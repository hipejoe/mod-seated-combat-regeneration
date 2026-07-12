/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ConfigValueCache.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <unordered_map>

#include "Chat.h"
#include "Spell.h"

namespace
{
    enum class SeatedCombatRegenConfig
    {
        Enabled,
        TimerSeconds,

        MountOverrideEnabled,
        MountOverrideAreaIds,
        MountOverrideMessage,

        Max
    };

    class SeatedCombatRegenConfigData
        : public ConfigValueCache<SeatedCombatRegenConfig>
    {
    public:
        SeatedCombatRegenConfigData()
            : ConfigValueCache(SeatedCombatRegenConfig::Max)
        {
        }

        void BuildConfigCache() override
        {
            SetConfigValue<bool>(
                SeatedCombatRegenConfig::Enabled,
                "SeatedCombatRegen.Enable",
                true);

            SetConfigValue<uint32>(
                SeatedCombatRegenConfig::TimerSeconds,
                "SeatedCombatRegen.TimerSeconds",
                30);

            SetConfigValue<bool>(
                SeatedCombatRegenConfig::MountOverrideEnabled,
                "SeatedCombatRegen.MountOverride.Enable",
                true);

            SetConfigValue<std::string>(
                SeatedCombatRegenConfig::MountOverrideAreaIds,
                "SeatedCombatRegen.MountOverride.AreaIds",
                "");

            SetConfigValue<std::string>(
                SeatedCombatRegenConfig::MountOverrideMessage,
                "SeatedCombatRegen.MountOverride.Message",
                "Mounting is allowed in this area.");
        }
    };

    SeatedCombatRegenConfigData SeatedCombatRegenConfigCache;

    struct SeatedCombatRegenState
    {
        uint32 SeatedTimer = 0;
    };

    std::unordered_map<ObjectGuid, SeatedCombatRegenState>
        SeatedCombatRegenStates;

    std::unordered_set<uint32> ParseAreaIds(std::string const& rawAreaIds)
    {
        std::unordered_set<uint32> areaIds;

        std::stringstream stream(rawAreaIds);
        std::string token;

        while (std::getline(stream, token, ','))
        {
            token.erase(
                std::remove_if(
                    token.begin(),
                    token.end(),
                    [](unsigned char character)
                    {
                        return std::isspace(character);
                    }),
                token.end());

            if (token.empty())
            {
                continue;
            }

            try
            {
                areaIds.insert(static_cast<uint32>(std::stoul(token)));
            }
            catch (...)
            {
            }
        }

        return areaIds;
    }

    bool IsMountOverrideArea(Player* player)
    {
        if (!player)
        {
            return false;
        }

        std::string rawAreaIds =
            SeatedCombatRegenConfigCache.GetConfigValue<std::string>(
                SeatedCombatRegenConfig::MountOverrideAreaIds);

        std::unordered_set<uint32> areaIds = ParseAreaIds(rawAreaIds);

        if (areaIds.empty())
        {
            return false;
        }

        return areaIds.find(player->GetAreaId()) != areaIds.end() ||
            areaIds.find(player->GetZoneId()) != areaIds.end();
    }

    bool IsMountSpell(SpellInfo const* spellInfo)
    {
        if (!spellInfo)
        {
            return false;
        }

        return IsMountSpell(spellInfo) ();
    }

    bool IsMountLocationFailure(SpellCastResult result)
    {
        switch (result)
        {
        case SPELL_FAILED_NOT_HERE:
        case SPELL_FAILED_ONLY_OUTDOORS:
        case SPELL_FAILED_NO_MOUNTS_ALLOWED:
            return true;
        default:
            return false;
        }
    }
}

class SeatedCombatRegenWorldScript : public WorldScript
{
public:
    SeatedCombatRegenWorldScript()
        : WorldScript(
            "SeatedCombatRegenWorldScript",
            {
                WORLDHOOK_ON_BEFORE_CONFIG_LOAD
            })
    {
    }

    void OnBeforeConfigLoad(bool reload) override
    {
        SeatedCombatRegenConfigCache.Initialize(reload);
    }
};

class SeatedCombatRegenPlayerScript : public PlayerScript
{
public:
    SeatedCombatRegenPlayerScript()
        : PlayerScript(
            "SeatedCombatRegenPlayerScript",
            {
                PLAYERHOOK_ON_UPDATE,
                PLAYERHOOK_ON_LOGOUT,
                PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT
            })
    {
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player)
        {
            return;
        }

        ObjectGuid playerGuid = player->GetGUID();

        if (!SeatedCombatRegenConfigCache.GetConfigValue<bool>(
            SeatedCombatRegenConfig::Enabled))
        {
            SeatedCombatRegenStates.erase(playerGuid);
            return;
        }

        if (!player->IsAlive())
        {
            SeatedCombatRegenStates.erase(playerGuid);
            return;
        }

        if (!player->IsInCombat() || !player->IsSitState())
        {
            SeatedCombatRegenStates.erase(playerGuid);
            return;
        }

        uint32 timerSeconds = std::max<uint32>(
            1,
            SeatedCombatRegenConfigCache.GetConfigValue<uint32>(
                SeatedCombatRegenConfig::TimerSeconds));

        uint32 seatedCombatRegenDelay =
            timerSeconds * IN_MILLISECONDS;

        auto& state = SeatedCombatRegenStates[playerGuid];

        state.SeatedTimer = std::min<uint32>(
            state.SeatedTimer + diff,
            seatedCombatRegenDelay);

        if (state.SeatedTimer < seatedCombatRegenDelay)
        {
            return;
        }

        player->CombatStop();

        SeatedCombatRegenStates.erase(playerGuid);
    }

    void OnPlayerLeaveCombat(Player* player) override
    {
        this->RemovePlayerState(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        this->RemovePlayerState(player);
    }

private:
    void RemovePlayerState(Player* player)
    {
        if (!player)
        {
            return;
        }

        SeatedCombatRegenStates.erase(player->GetGUID());
    }
};

class SeatedCombatRegenMountOverrideSpellScript : public AllSpellScript
{
public:
    SeatedCombatRegenMountOverrideSpellScript()
        : AllSpellScript(
            "SeatedCombatRegenMountOverrideSpellScript",
            {
                ALLSPELLHOOK_ON_SPELL_CHECK_CAST
            })
    {
    }

    void OnSpellCheckCast(
        Spell* spell,
        bool /*strict*/,
        SpellCastResult& result) override
    {
        if (!SeatedCombatRegenConfigCache.GetConfigValue<bool>(
            SeatedCombatRegenConfig::MountOverrideEnabled))
        {
            return;
        }

        if (!IsMountLocationFailure(result))
        {
            return;
        }

        if (!spell)
        {
            return;
        }

        Unit* caster = spell->GetCaster();

        if (!caster)
        {
            return;
        }

        Player* player = caster->ToPlayer();

        if (!player)
        {
            return;
        }

        if (!IsMountSpell(spell->GetSpellInfo()))
        {
            return;
        }

        if (!IsMountOverrideArea(player))
        {
            return;
        }

        result = SPELL_CAST_OK;

        std::string message =
            SeatedCombatRegenConfigCache.GetConfigValue<std::string>(
                SeatedCombatRegenConfig::MountOverrideMessage);

        if (!message.empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage(message);
        }
    }
};

void AddSC_mod_seated_combat_regeneration()
{
    new SeatedCombatRegenWorldScript();
    new SeatedCombatRegenPlayerScript();
    new SeatedCombatRegenMountOverrideSpellScript();
}

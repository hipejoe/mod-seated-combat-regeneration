/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ConfigValueCache.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <unordered_map>

namespace
{
    enum class SeatedCombatRegenConfig
    {
        Enabled,
        TimerSeconds,

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
        }
    };

    SeatedCombatRegenConfigData SeatedCombatRegenConfigCache;

    struct SeatedCombatRegenState
    {
        uint32 SeatedTimer = 0;
    };

    std::unordered_map<ObjectGuid, SeatedCombatRegenState>
        SeatedCombatRegenStates;
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

void AddSC_mod_seated_combat_regeneration()
{
    new SeatedCombatRegenWorldScript();
    new SeatedCombatRegenPlayerScript();
}

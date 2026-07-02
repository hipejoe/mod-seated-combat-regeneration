````markdown
# Seated Combat Regen

[English](README.md)

## Description

Seated Combat Regen is an AzerothCore module that allows a player to leave combat after remaining seated continuously for a configurable amount of time.

The module does not modify AzerothCore source files. It uses `PlayerScript` hooks to track how long each player remains seated while in combat.

Once the configured timer expires, the module calls:

```cpp
player->CombatStop();
```

This clears the player's active combat state and allows normal out-of-combat regeneration to resume.

## Behavior

The timer runs only while all of the following conditions are true:

- The module is enabled.
- The player is alive.
- The player is seated.
- The player is in combat.

The timer is reset when:

- The player stands up.
- The player leaves combat.
- The player dies.
- The player logs out.
- The module is disabled.

When the configured timer expires, the module calls `Player::CombatStop()` and removes the player's stored timer state.

> [!NOTE]
> `CombatStop()` clears the player's current combat relationships. A creature that is still actively attacking the player may place the player back into combat during a subsequent AI or combat update.

## Configuration

The module provides the following settings:

```ini
SeatedCombatRegen.Enable = 1
SeatedCombatRegen.TimerSeconds = 30
```

### `SeatedCombatRegen.Enable`

Enables or disables the module.

Available values:

```ini
0 = Disabled
1 = Enabled
```

Default:

```ini
SeatedCombatRegen.Enable = 1
```

### `SeatedCombatRegen.TimerSeconds`

Specifies how many continuous seconds a player must remain seated while in combat before combat is cleared.

The implementation enforces a minimum value of one second.

Default:

```ini
SeatedCombatRegen.TimerSeconds = 30
```

## Installation

Clone or copy the module into the AzerothCore `modules` directory:

```bash
cd /path/to/azerothcore/modules
git clone <repository-url> mod-seated-combat-regen
```

Reconfigure and rebuild AzerothCore:

```bash
cd /path/to/azerothcore/build
cmake ../
make -j$(nproc)
```

Copy the module configuration file into the server configuration directory if it is not copied automatically:

```bash
cp modules/mod-seated-combat-regen/conf/mod_seated_combat_regen.conf.dist \
    /path/to/azerothcore/env/dist/etc/modules/mod_seated_combat_regen.conf
```

Update the destination path as needed for your AzerothCore installation.

Restart `worldserver` after installing the module or changing its configuration.

## Module Structure

```text
mod-seated-combat-regen/
├── conf/
│   └── mod_seated_combat_regen.conf.dist
├── src/
│   ├── Loader.cpp
│   └── SeatedCombatRegen.cpp
├── CMakeLists.txt
└── README.md
```

## Implementation Details

The module uses a `PlayerScript` with the following hooks:

```cpp
PLAYERHOOK_ON_UPDATE
PLAYERHOOK_ON_LOGOUT
PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT
```

A per-player timer is stored in memory using the player's `ObjectGuid`.

During `OnPlayerUpdate`, the timer increments only while the player is alive, seated, and in combat.

When the configured duration is reached, the module:

1. Calls `Player::CombatStop()`.
2. Removes the player's stored timer state.
3. Allows AzerothCore's normal out-of-combat regeneration behavior to resume.

The stored state is also removed when the player logs out or leaves combat.

No database tables or SQL updates are required.

## Testing

Enable the module:

```ini
SeatedCombatRegen.Enable = 1
```

Use a short timer while testing:

```ini
SeatedCombatRegen.TimerSeconds = 5
```

Test the following behavior:

1. Enter combat with a creature.
2. Sit down.
3. Remain seated without interruption.
4. Confirm that combat clears after approximately five seconds.
5. Stand before the timer expires and confirm that the timer resets.
6. Sit again and confirm that the full configured duration is required.
7. Confirm that leaving combat removes the stored timer.
8. Confirm that logging out removes the stored timer.
9. Confirm that an actively attacking creature may immediately place the player back into combat.

It is also recommended to compile with precompiled headers disabled to identify missing includes:

```bash
cmake ../ -DNOPCH=1
make -j$(nproc)
```

## Compatibility

This module is intended for AzerothCore WotLK.

Compatibility may depend on the AzerothCore revision because script hook names and public method signatures may change over time.

## Licensing

This module is released under the GNU AGPL v3 license.

AzerothCore is also released under the GNU AGPL v3 license. See the AzerothCore license for additional information:

https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
````

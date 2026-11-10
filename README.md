# DAGGER - Dark's Adaptive Goam & GoamEER Ear Retractor

An SKSE plugin that hides the ears from **Goam's Elven Ears** and **Goam's Elven Ears Redone** (GoamEER) while your character wears a hood, cowl or helmet, and shows them again when it comes off.

It does not swap head parts and does not need an `.esp` or any Papyrus scripts. It marks the ear meshes as hidden on the player's loaded model, so the change is immediate, does not rebuild the head, and leaves nothing in your save game.

## Requirements
- Skyrim Special Edition / Anniversary Edition
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- Goam's Elven Ears and/or Goam's Elven Ears Redone (this mod ships none of their files)

Tested on game version 1.7.104 only. The plugin uses Address Library and CommonLibSSE-NG, so other SE and AE versions should work, but they have not been tested.

## Installation
Install the zip with Vortex or Mod Organizer 2. The FOMOD asks which ears mod you use:

| Option | Hides |
|---|---|
| Goam's Elven Ears Redone | head parts starting `GoamSm`, `GoamLrg`, `GoamXL` |
| Goam's Elven Ears (original) | head parts starting `GoamElvenEars` |
| Both | all of the above |

The installer pre-selects the option that matches your active plugins. If you pick the wrong one, edit `SKSE/Plugins/DAGGER.ini` (see below).

Load order does not matter: there is no plugin file.

## What counts as covered
- Armor in slot 30 (Head) or slot 31 (Hair) hides the ears. Vanilla hoods and cowls use slot 31, helmets use 30 and 31.
- Circlets do not hide the ears. Circlets use their own slot (42), and armor with the `ClothingCirclet` keyword is skipped even if a mod puts it in slot 30/31.
- Only the player character is handled. Followers and NPCs are not.

## Configuration (`Data/SKSE/Plugins/DAGGER.ini`)
| Key | Default | Meaning |
|---|---|---|
| `prefixes` | `GoamSm, GoamLrg, GoamXL, GoamElvenEars` (Both) | Ear geometry is matched by name prefix, not case sensitive. The game names ear geometry after the head part's editor ID, for example `GoamLrgDark`. |
| `nodes` | empty | Optional exact geometry names, in addition to the prefixes. |
| `slotMask` | `3` | Armor slots that count as covered: 1 = Head (30), 2 = Hair (31). Use `1` to ignore hair-slot-only items. |
| `ignoreCirclets` | `true` | Skip armor with the `ClothingCirclet` keyword. |
| `pollMs` / `fastPollMs` | `250` / `40` | Re-check interval while idle / right after an equip or load. |
| `logLevel` | `info` | `trace`, `debug`, `info`, `warn`, `error`. |

## Troubleshooting
The log is `Documents\My Games\Skyrim Special Edition\SKSE\DAGGER.log`.

- It logs every armor item you equip or unequip with its slot mask, so you can see why an item does or does not count.
- `Ear geometry: N matched (...)` shows which ear meshes were found. If N is 0 while covered, the log also lists all geometry on the player. Add the right prefix to `prefixes`.
- If a hood from another mod does not hide the ears, check the slot mask in the log. Items that use neither slot 30 nor 31 are not counted.

## Known limitations
- Player only.
- A mod that rebuilds the head model while covered can briefly show the ears. DAGGER re-applies the hide within a quarter of a second.
- If you previously used a script-based ear hider that swapped head parts, restore your ears first (remove the hood, wait, save) before uninstalling it.

## AI disclosure
DAGGER was developed with Claude Code, an AI coding assistant using Anthropic's Claude models. The AI wrote the plugin code, the installer files and this documentation. The author set the requirements and design, chose the name, and tested every build in game. The mod contains no AI-generated images, textures, meshes or audio.

## Version history
- **1.0.1** - README, LICENSE and NOTICE included in the package; version and revision are stamped in the DLL, log, installer and `.ini`. GPL-3.0-or-later.
- **1.0.0** - First tested build: hides Goam's Elven Ears and Redone ears under hoods and helmets (matched by head-part name prefix), FOMOD installer.

## Building
Needs Visual Studio 2022 (C++ workload), CMake 3.21+ and vcpkg with `VCPKG_ROOT` set. CommonLibSSE-NG (alandtse fork, v15.5.0) is a git submodule, so clone with submodules.

```
git clone --recurse-submodules https://github.com/DarkAttentater/DAGGER.git
cd DAGGER/Plugin
cmake --preset plugin
cmake --build --preset plugin
.\package.ps1
```

`package.ps1` builds `DAGGER-<version>-r<rev>.zip` next to the project. The revision is `DAGGER_REVISION` in `Plugin/CMakeLists.txt`.

## Credits
- Goam, author of Goam's Elven Ears and Goam's Elven Ears Redone.
- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) (alandtse fork) and Address Library.

## Licence
GPL-3.0-or-later, see `LICENSE` and `NOTICE`. DAGGER links [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) (GPL-3.0-or-later with a modding exception), which is why it uses the same licence. No files from Goam's mods are included.

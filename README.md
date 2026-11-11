# NPC Studio Native Plugin

The C++ SKSE plugin behind [NPC Studio](https://github.com/selenter-c/NPCStudio-GamePlugin), an in-game editor for permanent NPCs in Skyrim Special Edition on Steam and GOG.

NPC Studio was built primarily for [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin) playthroughs. It provides blank, persistent NPCs with editable identities, appearances, and equipment. They generally stand idle until an AI framework gives them actions or dialogue. Automatic SkyrimNet registration and biography generation are in development.

[**Download and install the beta**](https://github.com/selenter-c/NPCStudio-GamePlugin/releases) · [Build instructions](https://github.com/selenter-c/NPCStudio-Build)

## What the plugin handles

- Permanent reference assignment and restoration for 400 reserved NPC slots.
- Runtime editing for owned characters and supported world NPCs, with SKSE co-save persistence.
- Equipment presets, inventory operations, and optional NPCs Names Distributor integration.
- A Magelight v4 bridge for the editor and live previews and portraits through the public API in [Phantasm 1.0](https://github.com/Severause/Phantasm/blob/c0531d4eae1cd1ac04607c9513b2edea8f7b58da/docs/API.md).

## Build and runtime

The native target uses C++23, MSVC, CMake, Ninja, and vcpkg. It pins [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/8eaddedd1542290701ccb0e9e88b2af8b3cea597) and builds against the Skyrim AE runtime. Use the [Build repository](https://github.com/selenter-c/NPCStudio-Build) for the complete toolchain and packaging commands.

The same DLL targets Steam 1.6.1130, 1.6.1170, 1.7.99, and 1.7.104, plus GOG 1.6.1179. The runtime check uses the version reported by SKSE, including the GOG marker. The CommonLib pin reads Address Library formats 2 and 5. See the [player guide](https://github.com/selenter-c/NPCStudio-GamePlugin#requirements) for the SKSE and Magelight requirements.

## Credits and licensing

[Build a Follower](https://www.nexusmods.com/skyrimspecialedition/mods/185471) informed the permanent slot model. [RaceMenu](https://www.nexusmods.com/skyrimspecialedition/mods/19080) and [NPC Preset Applier](https://www.nexusmods.com/skyrimspecialedition/mods/193575) informed appearance editing. [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin) and [IntelEngine](https://github.com/galanx/IntelEngine-NativePlugin) provided integration references. [SeverActions](https://github.com/Severause/SeverActions) and Magelight UI are by Severause.

Developed with **OpenAI Codex**, which helped solve technical problems in the SKSE, rendering, save-state, and UI implementations.

NPC Studio's original native source uses the [MIT license](LICENSE), as declared in `vcpkg.json`. Vendored SDK headers retain their upstream notices in [include](include). The distributed DLL is a combined work with CommonLibSSE-NG under **GPL-3.0-or-later**, subject to [its upstream exceptions](https://github.com/alandtse/CommonLibVR/blob/8eaddedd1542290701ccb0e9e88b2af8b3cea597/EXCEPTIONS.md). Corresponding source is available alongside the beta download.

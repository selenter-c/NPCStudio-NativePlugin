# NPC Studio Native Plugin

The C++ SKSE plugin behind [NPC Studio](https://github.com/selenter-c/NPCStudio-GamePlugin), an in-game editor for permanent NPCs in Skyrim Special Edition 1.6.1170.

NPC Studio was built primarily for [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin) playthroughs. It provides blank, persistent NPCs with editable identities, appearances, and equipment. They generally stand idle until an AI framework gives them actions or dialogue. Automatic SkyrimNet registration and biography generation are outside the 0.8.7 beta.

[**Download and install the beta**](https://github.com/selenter-c/NPCStudio-GamePlugin/releases) · [Build instructions](https://github.com/selenter-c/NPCStudio-Build)

## What the plugin handles

- Permanent reference assignment and restoration for 400 reserved NPC slots.
- Runtime editing for owned characters and supported world NPCs, with SKSE co-save persistence.
- Equipment presets, inventory operations, and optional NPCs Names Distributor integration.
- A Magelight v4 bridge for the editor and native integration for 3D previews and portraits.

## Build and runtime

The native target uses C++23, MSVC, CMake, Ninja, and vcpkg. It pins [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/8eaddedd1542290701ccb0e9e88b2af8b3cea597) and builds against the Skyrim AE runtime. Use the [Build repository](https://github.com/selenter-c/NPCStudio-Build) for the complete toolchain and packaging commands.

The installed mod requires [SKSE64 2.2.6](https://skse.silverlock.org/), [AE Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444), and [Magelight UI](https://www.nexusmods.com/skyrimspecialedition/mods/193303) with its v4 API. See the [player guide](https://github.com/selenter-c/NPCStudio-GamePlugin#requirements) for optional dependencies.

## Credits and licensing

[Build a Follower](https://www.nexusmods.com/skyrimspecialedition/mods/185471) informed the permanent slot model. [RaceMenu](https://www.nexusmods.com/skyrimspecialedition/mods/19080) and [NPC Preset Applier](https://www.nexusmods.com/skyrimspecialedition/mods/193575) informed appearance editing. [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin) and [IntelEngine](https://github.com/galanx/IntelEngine-NativePlugin) provided integration references. [SeverActions](https://github.com/Severause/SeverActions) and Magelight UI are by Severause.

Developed with **OpenAI Codex**, which helped solve technical problems in the SKSE, rendering, save-state, and UI implementations.

NPC Studio's original native source uses the [MIT license](LICENSE), as declared in `vcpkg.json`. Vendored SDK headers retain their upstream notices in [include](include). The distributed DLL is a combined work with CommonLibSSE-NG under **GPL-3.0-or-later**, subject to [its upstream exceptions](https://github.com/alandtse/CommonLibVR/blob/8eaddedd1542290701ccb0e9e88b2af8b3cea597/EXCEPTIONS.md). Corresponding source is available alongside the beta download.

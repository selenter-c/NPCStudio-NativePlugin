# Очистка 0.7.0

Удалены старые файлы, отсутствующие в CMake SOURCES и графе include действующего плагина. Реальный редактор внешности и SkyrimNet будут реализованы через текущие core/preview/ui; удалённые заглушки не являлись работающей интеграцией. Собственный NIF viewer и его документация сохранены по просьбе не удалять созданные ранее наработки; stb_image_write.h используется текущим ActorPreview.

- include/PrismaUI_API.h
- include/PublicAPI.h
- src/api/PrismUIBridge.cpp
- src/api/PrismUIBridge.h
- src/api/SkyrimNetBridge.cpp
- src/api/SkyrimNetBridge.h
- src/bio/BioGenerator.cpp
- src/bio/BioGenerator.h
- src/input/InputHandler.cpp
- src/input/InputHandler.h
- src/naming/NameDistributor.cpp
- src/naming/NameDistributor.h
- src/npc/AppearanceManager.cpp
- src/npc/AppearanceManager.h
- src/npc/InventoryManager.cpp
- src/npc/InventoryManager.h
- src/npc/NPCFactory.cpp
- src/npc/NPCFactory.h
- src/preset/PresetManager.cpp
- src/preset/PresetManager.h
- src/preset/PresetSerializer.h
- src/ui/UIManager.cpp
- src/ui/UIManager.h
- Game/Legacy/0.1.0/Data/PrismaUI/views/NPCStudio/index.html
- Game/Legacy/0.1.0/Data/PrismaUI/views/NPCStudio/script.js
- Game/Legacy/0.1.0/Data/PrismaUI/views/NPCStudio/style.css
- Game/Legacy/0.1.0/Data/SKSE/Plugins/NPCStudio_DISTR.ini
- Game/Legacy/0.1.0/Data/SKSE/Plugins/NPCsNamesDistributor/NPCStudio_Names.json
- Game/Legacy/0.1.0/Data/Scripts/Source/NPCStudio_InputHandler.psc
- Game/Legacy/0.1.0/Data/Scripts/Source/NPCStudio_MCM.psc
- Game/Legacy/0.1.0/Data/Scripts/Source/NPCStudio_Manager.psc
- Game/Legacy/0.1.0/Data/Scripts/Source/NPCStudio_PlayerAlias.psc
- Game/Legacy/0.1.0/Data/Scripts/Source/NPCStudio_Utils.psc

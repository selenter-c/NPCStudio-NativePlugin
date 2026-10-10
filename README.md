# NPC Studio — Native Plugin (C++ SKSE)

> **Автор:** Selenter
> **Версия:** 0.1.0
> **Целевая игра:** Skyrim SE 1.6.1170

## Описание

C++ SKSE-плагин для мода NPC Studio. Обеспечивает:
- Инициализацию плагина и логирование
- Обработку ввода (SHIFT+8)
- Управление UI через PrismUI
- Спавн и управление NPC
- Интеграцию со SkyrimNet для генерации биографий
- Систему пресетов (JSON)

## Сборка

### Через NPCStudio-Build (рекомендуется)

```powershell
cd ../NPCStudio-Build
.\build.bat                 # Incremental build (~10s)
.\build-clean.bat           # Full rebuild (~3min)
.\compile.bat               # Только компиляция
```

### Вручную через CMake

```powershell
# Требуется vcpkg в C:\vcpkg с установленными зависимостями
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat"" x64 && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_OVERLAY_TRIPLETS=. -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_CXX_COMPILER=cl && cmake --build build"
```

Подробная инструкция в `BUILD.md`.

## Зависимости

| Библиотека | Тип | Назначение |
|-----------|-----|-----------|
| CommonLibSSE-NG | FetchContent | SKSE фреймворк |
| spdlog | vcpkg (static) | Логирование |
| fmt | vcpkg (static) | Форматирование строк |
| nlohmann_json | vcpkg (static) | JSON парсинг |
| rapidcsv | vcpkg (static) | CSV парсинг |
| PrismUI | header-only | UI фреймворк |
| SkyrimNet Public API | runtime | AI-генерация биографий |

**Примечание:** spdlog, fmt, nlohmann_json и rapidcsv линкуются статически (встроены в DLL). DLL зависит только от системных библиотек (MSVCP140, VCRUNTIME140).

## Структура

```
src/
├── plugin.cpp          # Точка входа SKSE
├── PCH.h               # Precompiled header
├── api/                # Обёртки над внешними API
│   ├── SkyrimNetBridge.h/.cpp
│   └── PrismUIBridge.h/.cpp
├── input/              # Обработка ввода
│   └── InputHandler.h/.cpp
├── npc/                # Управление NPC
│   ├── NPCFactory.h/.cpp
│   ├── AppearanceManager.h/.cpp
│   └── InventoryManager.h/.cpp
├── preset/             # Пресеты
│   ├── PresetManager.h/.cpp
│   └── PresetSerializer.h
├── bio/                # Генерация биографий
│   └── BioGenerator.h/.cpp
├── naming/             # Имена NPC
│   └── NameDistributor.h/.cpp
└── ui/                 # UI менеджер
    └── UIManager.h/.cpp
```

## Лицензия

MIT
# NPC Studio — Native Plugin Development Log

> **Автор:** Selenter
> **Проект:** NPC Studio
> **Репозиторий:** NPCStudio-NativePlugin (C++ SKSE-плагин)

---

## 📋 Состояние файлов проекта

> Обновлено: 2026-10-10 13:10

### C++ (NativePlugin)

| Файл | Этап | Статус | Комментарий |
|------|------|--------|-------------|
| `src/PCH.h` | 0 | ⬜ | Precompiled header, базовый |
| `src/plugin.cpp` | 1 | 🟢 | Точка входа SKSE, логирование |
| `include/PublicAPI.h` | 8 | ⬜ | SkyrimNet API, заглушка |
| `include/PrismaUI_API.h` | 2 | ⬜ | PrismUI API, заглушка |
| `src/api/SkyrimNetBridge.h` | 8 | ⬜ | Обёртка SkyrimNet, заглушка |
| `src/api/SkyrimNetBridge.cpp` | 8 | ⬜ | Реализация обёртки, заглушка |
| `src/api/PrismUIBridge.h` | 2 | ⬜ | Обёртка PrismUI, заглушка |
| `src/api/PrismUIBridge.cpp` | 2 | ⬜ | Реализация обёртки, заглушка |
| `src/input/InputHandler.h` | 2 | ⬜ | Обработка ввода, заглушка |
| `src/input/InputHandler.cpp` | 2 | ⬜ | Реализация ввода, заглушка |
| `src/npc/NPCFactory.h` | 3 | ⬜ | Спавн NPC, заглушка |
| `src/npc/NPCFactory.cpp` | 3 | ⬜ | Реализация спавна, заглушка |
| `src/npc/AppearanceManager.h` | 4 | ⬜ | Клонирование внешности, заглушка |
| `src/npc/AppearanceManager.cpp` | 4 | ⬜ | Реализация внешности, заглушка |
| `src/npc/InventoryManager.h` | 6 | ⬜ | Инвентарь, заглушка |
| `src/npc/InventoryManager.cpp` | 6 | ⬜ | Реализация инвентаря, заглушка |
| `src/ui/UIManager.h` | 2 | ⬜ | UI менеджер, заглушка |
| `src/ui/UIManager.cpp` | 2 | ⬜ | Реализация UI, заглушка |
| `src/preset/PresetManager.h` | 10 | ⬜ | Пресеты, заглушка |
| `src/preset/PresetManager.cpp` | 10 | ⬜ | Реализация пресетов, заглушка |
| `src/preset/PresetSerializer.h` | 10 | ⬜ | Сериализация пресетов, заглушка |
| `src/bio/BioGenerator.h` | 8 | ⬜ | Генерация био, заглушка |
| `src/bio/BioGenerator.cpp` | 8 | ⬜ | Реализация генерации био, заглушка |
| `src/naming/NameDistributor.h` | 9 | ⬜ | Имена NPC, заглушка |
| `src/naming/NameDistributor.cpp` | 9 | ⬜ | Реализация имён, заглушка |
| `CMakeLists.txt` | 1 | 🟢 | Конфигурация сборки (vcpkg + FetchContent) |
| `vcpkg.json` | 1 | 🟢 | Зависимости vcpkg (spdlog, nlohmann-json, fmt, rapidcsv) |
| `x64-windows-static.cmake` | 1 | 🟢 | Custom triplet (static libs + dynamic CRT) |
| `BUILD.md` | 1 | 🟢 | Инструкция по сборке |

**Прогресс:** 5 из 29 файлов реализовано (17%)

---

## 📝 Журнал разработки

### 2026-10-10 10:45 — Stage 0: Создание каркаса
- Создана структура NPCStudio-NativePlugin
- Все файлы созданы как заглушки с TODO + REF комментариями
- Изучены референсы: IntelEngine-NativePlugin, SkyrimNetDevKit
- CMakeLists.txt настроен с /utf-8, CommonLibSSE-NG, spdlog, nlohmann_json
- vcpkg.json содержит минимальные зависимости

### 2026-10-10 11:00 — Stage 1: Установка + вывод в консоль
- Реализован plugin.cpp с логированием
- Регистрация SKSE Messaging Interface (kDataLoaded, kNewGame, kPostLoadGame)
- Обновлён CMakeLists.txt со всеми исходными файлами
- Создана папка NPCStudio-Build с скриптами сборки
- Изучен sn-playwright-master референс

### 2026-10-10 11:30 — Компиляция и исправления
- Установлен vcpkg + зависимости (spdlog, nlohmann-json, fmt, rapidcsv)
- Настроен CMake через Ninja + MSVC + vcpkg toolchain
- Исправлены u8"" строки для совместимости с fmt v12
- Проект успешно скомпилирован (DLL 350 KB)

### 2026-10-10 12:35 — Исправление ошибки 0x7E
- DLL не загружалась из-за отсутствующих зависимостей (spdlog.dll, fmt.dll)
- Перешли на статическую линковку (custom triplet x64-windows-static)
- Исправлен CRT mismatch (MT vs MD) — установлен dynamic CRT
- DLL теперь 534 KB (всё встроено), зависит только от системных библиотек

### 2026-10-10 12:45 — Оптимизация сборки
- Убрана автоматическая очистка build/ в build-native.ps1
- Добавлен параметр -Clean для принудительной пересборки
- Incremental build: ~10s вместо ~3min (пересобираются только изменённые файлы)
- Создан BUILD.md с подробными инструкциями

### 2026-10-10 13:10 — Обновление документации
- Обновлён DEVELOPMENT_LOG.md с полной историей
- Обновлена таблица состояний файлов
- Добавлены записи о всех исправлениях
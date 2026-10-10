# NPC Studio — Инструкция по сборке NativePlugin

> **Автор:** Selenter
> **Обновлено:** 2026-10-10

## Требования

### Обязательные
- **Windows 10/11** (x64)
- **Visual Studio 2022+** с компонентом "Desktop development with C++"
- **CMake 3.24+**
- **Git** (для FetchContent)
- **Python 3.8+** (для генерации ESP)
- **vcpkg** (установлен в `C:\vcpkg`)

### Установка vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg --depth 1
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
C:\vcpkg\vcpkg.exe install spdlog:x64-windows-static nlohmann-json:x64-windows-static fmt:x64-windows-static rapidcsv:x64-windows-static
```

## Быстрая сборка

### Через NPCStudio-Build (рекомендуется)

```powershell
cd ../NPCStudio-Build
.\build.bat                 # Incremental build (~10s)
.\build-clean.bat           # Full rebuild (~3min)
.\compile.bat               # Только компиляция C++
```

### Вручную через CMake + vcpkg

```powershell
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat"" x64 && cmake -S NPCStudio-NativePlugin -B NPCStudio-NativePlugin/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_OVERLAY_TRIPLETS=NPCStudio-NativePlugin -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_CXX_COMPILER=cl && cmake --build NPCStudio-NativePlugin/build"
```

## Результат сборки

```
NPCStudio-NativePlugin/build/NPCStudio.dll   (534 KB, статическая линковка)
```

DLL зависит только от системных библиотек (MSVCP140.dll, VCRUNTIME140.dll). Никаких внешних spdlog.dll или fmt.dll не требуется.

## Установка в игру

### Через MO2 (рекомендуется)

```powershell
cd ../NPCStudio-Build
.\build.bat          # Полная сборка + упаковка
```

Результат: `NPCStudio-Build/dist/NPCStudio-0.1.0-fomod.zip`

### Вручную

1. Скопировать `NPCStudio.dll` в `<Skyrim>/Data/SKSE/Plugins/`
2. Скопировать `NPCStudio.esp` в `<Skyrim>/Data/`
3. Скопировать содержимое `NPCStudio-GamePlugin/Data/` в `<Skyrim>/Data/`

## Проверка работы

1. Запустить Skyrim SE
2. Проверить лог: `Documents/My Games/Skyrim Special Edition/SKSE/NPCStudio.log`
3. Должно содержать:
   ```
   ========================================
     NPC Studio v0.1.0
     Author: Selenter
     Target: Skyrim SE 1.6.1170
   ========================================
   Plugin loading...
   SKSE message listener registered
   NPC Studio loaded successfully!
   DataLoaded - all game data loaded
   NPC Studio ready
   ```

## Зависимости

| Библиотека | Версия | Тип линковки | Назначение |
|-----------|--------|-------------|-----------|
| CommonLibSSE-NG | main | FetchContent (static) | SKSE фреймворк |
| spdlog | 1.17+ | vcpkg (static) | Логирование |
| fmt | 12+ | vcpkg (static) | Форматирование строк |
| nlohmann_json | 3.12+ | vcpkg (static) | JSON парсинг |
| rapidcsv | 9+ | vcpkg (static) | CSV парсинг |

**Custom triplet:** `x64-windows-static.cmake` — статические библиотеки + динамический CRT (`/MD`). Это предотвращает конфликт с CommonLibSSE-NG и устраняет ошибку 0x7E при загрузке плагина.

## Troubleshooting

| Проблема | Решение |
|----------|---------|
| Ошибка 0x7E при загрузке | Используйте статическую линковку (custom triplet x64-windows-static) |
| LNK2038 RuntimeLibrary mismatch | Убедитесь что triplet использует `VCPKG_CRT_LINKAGE dynamic` |
| "vcpkg.json was rejected" | Удалите `"version>="` из vcpkg.json |
| "Could not find spdlog" | Убедитесь что vcpkg установлен и указан `-DCMAKE_TOOLCHAIN_FILE` |
| u8"" строки несовместимы с fmt | Не используйте `u8""` префикс, `/utf-8` флаг достаточно |
| MSB4102 (VCTargetsPath) | Используйте Ninja генератор через vcvarsall.bat |
| Пересборка занимает 3 минуты | Используйте `.\build.bat` без `-Clean` для incremental build |

## CI/CD (будущее)

Для автоматической сборки можно использовать GitHub Actions с:
- `windows-latest` runner
- vcpkg с binary caching
- Custom triplet x64-windows-static
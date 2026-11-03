## Обновление 0.6.1

Установка одним архивом `NPCStudio-Build/dist/NPCStudio-0.6.1-fomod.zip`: проверенный Phantasm, его SWF и лицензии включены в локальный комплект. Отдельный мод NPCStudio-Phantasm отключить, чтобы он не перекрывал файлы. Полный SeverActions не требуется. Phantasm остаётся отдельной DLL внутри одного установленного мода; это внешний компонент, не собственная разработка NPCStudio. Чужие бинарники не публикуются в Git.

Исправлено: строчный ID клиента Phantasm, поиск предметов через LookupForm + As (также аудит инвентаря), синхронизация цвета тела со слоем кожи головы после смены расы. Сборка и автоматические тесты не заменяют проверку в Skyrim; начать новую игру и проверить NPC/предметы/орка/возврат одежды.

Сборка: `NPCStudio-Build/build.bat`, инкрементально. Требуются Skyrim 1.6.1170, SKSE, Address Library и Magelight UI. Открытие Shift+8 (верхний ряд). Языки ru/en, четыре палитры, 200 женских и 200 мужских слотов; только текущая схема сохранения, без миграций. Пользовательские Equipment/user-*.json, categories.json и settings.json сохранить перед заменой установленной версии.

Проверки: CTest state-codec/preview-contract, Mutagen 400 NPC + 400 persistent ACHR, Chrome headless с имитацией игрового API, ZIP и хеш Phantasm. Последняя стабильная пользовательская точка — checkpoint-0.4.0-user-tested; 0.6.1 помечается только build-tested до игрового теста. Редактор головы/RaceMenu и SkyrimNet ещё не подключены.

Разбор рендера: NPCStudio-NativePlugin/docs/RENDERING_060.md, исправления: NPCStudio-NativePlugin/docs/RENDERING_061.md. Автоматизация не запускает MO2/Skyrim. Локальный установщик собирается из предоставленного пользователем SeverAction/97 Phantasm/00 Core; бинарник проверяется по точному SHA256, не добавляется в Git.

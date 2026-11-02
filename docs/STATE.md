# NPCStudio — состояние2026-11-03,0.5.1

## Правила

Русский, без обращения по имени. Пользователь разрешил commit/push/tag/откат для трёх NPCStudio main репозиториев (github.com/selenter-c). Computer-use/автозапуск Skyrim/MO2 запрещены; установка и игровые действия только пользователем. Разрешены терминал/файлы/read-onlyHouseCarl/headless тесты. Не менять MO2 профиль/не копировать DLL в работающую игру. Build/build.bat, NPCSTUDIO_NO_PAUSE=1, обычно без clean.

**Новое указание пользователя:** тестирует только с новой игры; поддержку предыдущих сохранений и миграции удалить. Не восстанавливать их при дальнейшей разработке. Не путать сохранение/загрузку текущей схемы с обратной совместимостью.

## Контрольные точки

0.4 подтверждена пользователем: checkpoint-0.4.0-user-tested (Build7009c76/Game0d9d53b/Native43b50e1). 0.5 build-tested: Build9930c28/Game0e15754/Native638a083, но VM SetRace в игре не сработал. Не считать0.5 user-tested. Теги в origin сохранены.

Текущая0.5.1 исправляет пул и путь расы. Build/dist/NPCStudio-0.5.1-fomod.zip,51файл. Работу прямого SwitchRace и рендера в Skyrim ещё должен проверить пользователь. Не утверждать, что игровые тесты выполнены.

## Что изменено0.5.1

- Последовательный пул: id0..199female, id200..399male, номер=id%200+1. Формы полностью переорганизованы по разрешению пользователя: CLFM900+id/NPCB00+id/ACHRD00+id. CELL800, XMarker801;1202 уникальные формы, максимумE8F, ESL. Manifest schema3; предыдущие схемы не принимаются.
- Co-save recordversion2, JSON poolVersion3. DecodePoolState только текущий формат, обязательные name/identity/race, назначения должны быть в usedSlots. Нет legacyHistoryUnknown, импорта160слотов, identity fallback и DecodeState wrapper. Свободные только fresh/used. История удалённых внутри текущей игры сохраняется, auto fresh→used. Settings Shift+F8 migration тоже удалена; обычные пользовательские настройки сохраняются.
- UI и переводы unknown/историидо0.4 удалены. Smoke использует новую раскладку и выбирает OrcRace. Слово «custom» от пользователя означало выбранную расу: орк — стандартная раса; настоящие модовые расы пока не включены.
- Причина0.5.0 по NPCStudio.log: DispatchMethodCall Actor.SetRace вернул false. VM/callback/Calls/Pending удалены полностью. Используем закреплённый CommonLib Actor::SwitchRace(TESRace*,bool), AE relocation37925 (SE36901), второй аргументfalse. Не придумывали offset и не меняли CommonLib.
- Create проверяет выбранную расу, MoveTo/Enable, сохраняет желаемый raceEDID, QueueRestore. Pump ждёт Is3DLoaded, вызывает SwitchRace один раз при несовпадении расы, затем ждёт фактическую расу/3D и стабилизацию~600мс до gear. Edit сохраняет loadout и ставит такую же задачу. AfterLoad ставит её для всех активных назначений. Нет вызовов смены расы на отключённых резервных actor и бесконечного ожидания VM callback. Общая попытка ограничена40тиками/6сек.

## Проверки и ручной тест

Финальный полный build.bat прошёл21сек, CTest2/2, Mutagen и ZIP (root docs/v051-build.log), compiler warnings отсутствуют. Chrome smoke прошёл, screenshots Native/docs/ui-051-*.png — MOCK RENDERER. Отдельно проверены все400 slots.json:200female+200male, уникальные1202 ID, максимумE8F. Unit tests перебирают все400 номеров/пол, текущий co-save roundtrip, удаление/reuse/history, malformed identity/race и отказ от poolVersion1/2/4. WARP renderer test остаётся.

Дать ZIP. При закрытой игре заменить мод целиком, сохранить user-*.json/categories.json/settings.json/overwrite. Начать именно новую игру после обновления, не загрузить старый стартовый сейв. Проверить список1..200 безunknown; создать орка с одеждой, дождаться завершения операции; поменять расу существующего NPC; save/reload текущей игры. При проблемах нужны NPCStudio.log строки SwitchRace/восстановление, скрин. FaceGen пользователь ранее исправил модами — не чинить его без новой причины.

## Сохраняемые возможности и ограничения

Shift+8(DIK9), собственный input sink и PostGameTask Magelight. Language ru/en при первом запуске по GetUserDefaultUILanguage, затем settings.json; четыре палитры huevariables. Локализация UI/i18n, встроенных комплектов, пользовательские имена/категории и игровые itemnames не переводятся. Советы при открытии и временные toast. Типизированные настройки проверяются CTest.

40 наборов, свои категории/наборы, каталог экипировки. Capture counts/worn передpark/save, Restore существующих вещей после3D с повторной проверкой~600мс, без автодовыдачи. DynamicFF пропускаются; переносимые presets не клонируют extras/temper/custom enchant/руку. PendingPreset сохраняется до выдачи. Delete очищает ownassignment+inventory/ресурсы/StopCombat/Disable/MoveToholding/defaultname, не чужие quests/relations/AI базы. Identity и worldEpoch защищают команды. Worker150ms только coalesced main task, безRE и self-requeue.

Names::Random читает Tamrielic/NND списки через VFS по полу/стандартной расе, не полный алгоритм фамилий. NND API RevealName и4SPID исключения. ЗаголовокGPL, MagelightheadersMIT. CommonLib alandtse/CommonLibVR ng8eaddedd, MSVC14.44staticCRT, AEonly1.6.1170.

Экспериментальный ActorPreview: собственный snapshot render загруженной модели, yaw/zoom/3lights/headportrait. Не анимации и не исходники SeverActions. Код ихnative закрыт, API/dumpbin не имеют rendererexports. Подробности root иNative docs/RENDERING_050.md. Current pose scenegraph, BSTriShape/dynamichead, boneweights, diffuse/tint, D3D11device Skyrim, deferred context ExecuteCommandList RestoreState, PNG InteropCall. Без Present hook/камеры/перемещения NPC. Ограниченный scene/vertex budget; пропущенные meshes в логе. FaceTint/makeup/normal/SSS/ENB/CS/alpha не воспроизводятся полностью. Capture может дать hitch. WARP проверяет backend, не реального NPC.

Портреты первых24 отфильтрованных loadedNPC, пауза450мс, остальные силуэты; очередь останавливается приclose. JS dataURI cache, неco-save; open сбрасывает. Полный редактор головы/RaceMenu и SkyrimNet НЕ подключены. LoadCharacterPreset Papyrus нельзяNPC; будущий nativeABI нужен. SkyrimNetSDKv14PublicRequestProfileUpdate — очередь без callback/exacttext. АудитисточниковTECHNICAL_AUDIT.md.

MO2: D:/SteamLibrary/steamapps/common/Skyrim Special Edition/MO2, пользователь меняет профиль сам. Лог C:/Users/Selenter/Documents/My Games/Skyrim Special Edition/SKSE/NPCStudio.log. В текущем логе0.5.0 также был аудит missing6из6 после bandit-melee; пользователь это сейчас не спрашивал, не считать решённым/не менять без исследования.

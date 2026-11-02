# NPCStudio: состояние на 2026-11-03

## Правила работы

- Общаться на русском, не обращаться по имени. Комментарии по-русски.
- Пользователь разрешил commit/push/tag/откат во всех трёх main репозиториях NPCStudio-{Build,GamePlugin,NativePlugin}, origin github.com/selenter-c.
- Computer-use и автозапуск Skyrim/MO2 запрещены. Установку и игровые действия выполняет пользователь. Разрешены терминал, файлы, read-only HouseCarl и headless проверки. Не менять профиль MO2 и не копировать DLL в работающую игру.
- Сборка через Build/build.bat, NPCSTUDIO_NO_PAUSE=1; обычно инкрементально, без clean.
- В игре подтверждена 0.4. Новые рендер и расы 0.5 ещё НЕ проверены в Skyrim.

## Версии и артефакты

Пользователь подтвердил 0.4 сообщением «Всё работает!». Тег checkpoint-0.4.0-user-tested: Build7009c76, Game0d9d53b, Native43b50e1. Более старые baseline и build/user-tested точки сохранены в Git. FaceGen пользователь исправил модами: без новой причины не чинить.

Новая 0.5.0: Build/dist/NPCStudio-0.5.0-fomod.zip,51файл. Планируемый тег checkpoint-0.5.0-build-tested. Документация возможностей и ограничений — Native/README.md; разбор SeverActions и собственного рендера — docs/RENDERING_050.md (root и Native).

## Новое в0.5

1. Shift+8, верхняя цифровая клавиша DIK9. Старый стандарт schema1 key66/mod1 мигрирует; пользовательские назначения и schema2 не меняются. SettingsCodec.h покрыт CTest.
2. English/Русский. Первый язык — GetUserDefaultUILanguage Windows, русский либо английский fallback. После выбора language хранится в settings.json независимо от системы. Общие настройки для всех сейвов, не co-save. Палитры amethyst/emerald/sapphire/ember, CSS hue variables. i18n.js переводит UI и встроенные комплекты; пользовательские имена и игровые item names не переводятся. Часть старых нестандартных диагностик может остаться в исходном виде.
3. Новый SVG знак и подпись «Персонажи, которые останутся в вашем мире», circle-точки кубика, «Не выбран» в actorPicker. Connection убрана. Случайные советы при открытии; операции показывают отдельный toast на6.5сек, ошибки14сек.
4. Выбор10обычных ванильных взрослых рас при создании и смена в roster. Races::Request вызывает штатный Actor.SetRace через VM DispatchMethodCall(handle,"Actor","SetRace",MakeFunctionArguments(race),callback). Сигнатура проверена по papyrus-reference. Callback лишь пишет logger/atomic completion, без RE. Calls поFormID изменяется только на game thread, clear на preload/newgame. Pump ждёт callback, фактическую расу и3D, затем восстанавливает gear. Если VM не ответит, слот останется restoring до reload; это ещё требует игрового теста. RaceEDID хранится в assignment.race, AfterLoad возобновляет. Пользовательские расы пока не включены. ESP и slots.json с0.4 не изменены.
5. Собственный ActorPreview.cpp. Native-рендер SeverActions закрыт и не экспортируется: не называть это переносом их кода. Захватывает текущую позу модели и одежду; поворот yaw, zoom,3lights, head portrait. Это3Dснимок, без проигрывания анимаций; после изменения одежды кнопка refresh.

## Рендер: важные детали

Только game-thread через Magelight PostGameTask. Actor.Get3D → NiNode traversal → BSTriShape/BSDynamicTriShape, app-cull/dismember visibility. VertexDesc и GPU staging readback, full/half position/half UV. Dynamic head float4 читается под BSSpinLock. Skinning: NiSkinData boneVert или SSE packed half4 weights+byte4 global bones; формат сверялся с nifly GetShapeBoneWeights/SSE conversion. inverse(actorRoot.world)*bone.world*skinToBone.

То же D3D11 device Skyrim, COM AddRef diffuse SRV, material UV, alpha-test, hair/face RGB tint. Простое geometric lighting. Deferred context → ExecuteCommandList(RestoreContextState=true), color readback → PNG/data URI → InteropCall. Нет Present hooks, второго устройства, перемещения NPC или камеры игры.

Лимиты:256meshes,250kvertices/mesh,500k/scene,3million indices/scene,32MBbuffer,4096nodes/depth64,128partitions/512bones. Неподдержанные части пропускаются, число в UI/логе. Один native scene cache по identity, clear на preload/newgame. First capture синхронный и может дать hitch. Не воспроизводятся полностью FaceTint/makeup, normal/SSS, ENB/Community Shaders и прозрачные материалы.

Портреты генерируются постепенно для первых24 NPC отфильтрованного списка, с паузой450мс, только у загруженных моделей. Незагруженные не телепортируются — показывается силуэт. UI data URI cache, не co-save. Identity защищает от старых команд, background ошибки quiet, очередь останавливается при закрытии. Открытие сбрасывает визуальный кэш. Background portrait может заменить native scene; следующая preview команда автоматически перезахватит выбранного NPC.

## Проверки

- Full build.bat exit0,46сек, CTest2/2, Mutagen400slots, ZIP51files.
- После clang-format новых C++ файлов: build-native exit0, CTest2/2, pack повторён. Логи root docs/v050-{build,final-native,pack}.log. Compiler warnings отсутствуют.
- State tests: прежние co-save/history/границы400 плюс язык системы/сохранённый выбор/миграция Shift+F8/валидация настроек.
- preview-graphics: тот же backend на реальном WARP D3D11 device. HLSL compile, разные PNG при yaw/light, портрет, восстановление viewport. Не читает игрового NPC.
- UI smoke: локали, палитра, race create/edit, портреты/камера, прежние операции, пять страниц720p/1080p, без горизонтального переполнения и pageerrors, close/reopen. Native/docs/ui-050-{create,roster,settings-en}.png просмотрены: это MOCK RENDERER, не Skyrim.

## Следующий ручной тест

Сделать отдельный сейв, сохранить Equipment/user-*.json/categories.json/settings.json и MO2 overwrite. Закрыть игру, обновить ZIP целиком. Проверить Shift+8 (неF8/неNum8), язык→перезапуск, палитру. Создать Nord/Bosmer/Khajiit в одежде, сменить расу, save/load. В roster проверить целую модель/лицо/волосы/доспех/портрет, drag/zoom/light/refresh, park→summon, save/reload. Если модель неполная — нужны скриншот и NPCStudio.log со строкой «Превью:Nчастей,Mпропущено,boundsZ». Не исправлять ESP/FaceGen вместо анализа фактических buffers/skin data.

## Сохраняемая основа

Пул200female+200male,1202формы ESL800..EEF, CELL800/XMarker801. Старые id0..159 и480 NPC_/ACHR/CLFM побайтово неизменны с0.3. Female0..79,160..279; male80..159,280..399. Старые color900+id/baseA00+id/refB00+id; новые offset=id-160,C00/D00/E00+offset. Каждое назначение получает новую identity, но повторный слот сохраняет referenceID.

Co-save UID5554534E,StateTag5354534E,recordversion1,JSON poolVersion2; читает v1(160). Assignments+usedSlots+legacyHistoryUnknown. Авто fresh→unknown→used; старые свободные160 при миграции unknown. С новыми race полями для отката нужен сейв до обновления. Missing co-save guard обнаруживает enabled unassigned; если все потерянные назначения скрыты, обнаружить это пока не может.

Equipment:40наборов/7категорий+свои, JSONschema1 name/category/items[plugin,localform,count]. Capture количества/надетого перед park/save; Restore существующих вещей после3D повторяется через600мс, аудит, без автодовыдачи. DynamicFF пропускается. Unique hand/temper/custom enchant не клонируются пресетами. PendingPreset хранится до успешной выдачи и возобновляется после load. Старый parked0.3 без снимка — один раз применить комплект.

Delete очищает собственное назначение/инвентарь, прекращает бой, восстанавливает ресурсы, Disable/MoveToholding/defaultname. Чужие quests/relations/AI базы не чистятся. История использования остаётся. SKSE pacer150мс только ставит coalesced main-thread task, без RE в worker и self-requeue.

NND RevealName+4SPIDисключения, random Tamrielic списки через VFS по полу/расе, базовое имя без полного алгоритма фамилий. NNDsource2986a406, headerGPL. Magelightv4 ABI headersMIT, минимум1.5; CommonLib alandtse/CommonLibVR ng8eaddedd, AEonly1.6.1170, MSVC14.44staticCRT.

Полный head editor/RaceMenu и SkyrimNet ещё НЕ подключены. RaceMenu Papyrus LoadCharacterPreset запрещён для NPC; нужна native integration. Старый niflyviewer исключён из target. NPA закрыт. SkyrimNet SDKv14 PublicRequestProfileUpdate — очередь без callback/exact text set; ABI/exports проверять. Детали в TECHNICAL_AUDIT.md.

MO2: D:/SteamLibrary/steamapps/common/Skyrim Special Edition/MO2. Лог: C:/Users/Selenter/Documents/My Games/Skyrim Special Edition/SKSE/NPCStudio.log. Пользователь меняет профиль сам: текущий HouseCarl видит Default176enabled/168active,NPCStudio.espactive. Ничего в Default не перезаписывать. Изолированный NPCStudio Development также существует.

# NPCStudio: текущее состояние

Обновлено 2026-11-03. Версия 0.4.0, готовится сборочная контрольная точка. Технический аудит исходных модов: TECHNICAL_AUDIT.md. Код и README находятся в трёх соседних репозиториях.

## Правила сессии

- Язык русский, не обращаться по имени. Код и терминал редактировать самостоятельно.
- Пользователь разрешил commit/push/tag/откат во всех трёх NPCStudio репозиториях. Main, origin github.com/selenter-c/NPCStudio-{Build,GamePlugin,NativePlugin}.
- Computer-use запрещён, не запускать Skyrim/MO2, не устанавливать автоматически. Игровые действия выполняет пользователь по инструкции. Не копировать DLL в работающую игру.
- Собирать через NPCStudio-Build/build.bat, обычно инкрементально, NPCSTUDIO_NO_PAUSE=1 для автоматизации. Полный clean дорогой и не нужен.
- Все реальные проверки новой версии в игре ещё впереди. Mock Chrome и успешная DLL — не доказательство игрового поведения.

## Подтверждённые точки

Baseline-before-rewrite-20261102 сохраняет пользовательское начальное состояние, включая исходные удалённые README/DEVLOG и правки старого nifviewer. 0.2 foundation и 0.3 equipment подтверждены пользователем в Skyrim; соответствующие user-tested/build-tested теги в origin. 0.3.1 (delete/NND) отправлена как checkpoint-0.3.1-build-tested; текущие скриншоты пользователя показывают её UI. Точный полный checklist 0.3.1 не подтверждён. FaceGen исправился после настройки модов, по сообщению пользователя: не чинить его без новой причины.

Последние HEAD до 0.4: Build b8c09b6, Game 41c1d8c, Native 5a92237. 0.4 готова к commit/push после финального pack. Архив уже создан: Build/dist/NPCStudio-0.4.0-fomod.zip, 50 файлов.

## Что реализовано в 0.4

1. Пул 200 женских и 200 мужских слотов. Старые id0..159, ActorBase/reference/color адреса и все 480 NPC_/ACHR/CLFM записи побайтово неизменны относительно 0.3.1. Новые id160..279 — female, id280..399 — male. Отдельные номера по полу1..200. Новые адреса: offset=id-160, CLFM C00+offset, NPC D00+offset, ACHR E00+offset. Всего1202 формы, диапазон800..EEF; ESL сохраняется. CELL800, XMarker801.
2. Co-save record version1, внутренний poolVersion2. DecodePoolState читает старый poolVersion1. Assignments + usedSlots + legacyHistoryUnknown. При миграции занятые старые слоты known-used, свободные старые160 unknown (старые удаления не регистрировались), новые fresh. Автовыбор fresh → unknown → used. Удалённые имена не сохраняются, только факт использования. Явный слот проверяется на пол и занятость. Новая identity при каждом назначении, stale команды отвергаются. Старый DLL0.3 не читает poolVersion2: для отката нужен сейв до обновления.
3. При создании можно выбрать обмундирование. Сначала проверяются зависимости, затем spawn; выдача после появления 3D. 40 готовых JSON-наборов, 7 категорий + «Мои наборы». Свои категории categories.json; свои наборы Equipment/user-*.json. Сохраняются базовые формы, без custom enchant/temper/выбора руки. Наборы не меняют faction/AI/class.
4. Перед park и при сохранении активного NPC Capture записывает inventory/count/worn в assignment.loadout. Summon и AfterLoad запускают RestoreJob. Pacer jthread150ms ставит только coalesced SKSE game task при наличии работы; Pump game-thread, worldEpoch+identity, до40 попыток/6сек. Ожидает3D, восстанавливает существующие вещи, повторяет спустя~600ms, проверяет count и IsWorn. Автоматической довыдачи нет. Audit missing/notEquipped/checked/skipped и предупреждения в UI/логе. FF dynamic items не сравниваются. Старый parked0.3 без снимка не позволяет точно восстановить прежний наряд — применить набор один раз. Если призывается активный NPC, снимок обновляется перед MoveTo. Повторный park уже disabled не стирает снимок.
5. Names::Random читает установленные файлы NND через VFS: NNDNord.json/Middle и NNDBosmer.json/First для стандартных female/male баз; sex, соответствия других vanilla race EDID. Это базовое имя из списка, не полный алгоритм NND для фамилий/титулов/inherit/honorifics. Исходные списки перевода не копируются в проект. Если списков нет, небольшой встроенный запас имён, источник виден. Full/short/display name + NND API RevealName и SPID исключения сохранены.
6. Default Shift+F8. Свой BSInputDeviceManager sink, IsDown основной клавиши, GetAsyncKeyState Shift/Ctrl/Alt, Magelight PostGameTask Toggle. Общие настройки settings.json и atomic key/modifiers. Старый BindHotkey больше не используется. CommonLib keyboard->IsPressed дал unresolved virtuals; исправлено без смены CommonLib. Все open/close должны проверить вручную. UI Toggle проверяет world/pause/console/dialogue/racesex, остальные ограничения хозяина Magelight остаются.
7. UI переписан: отдельные create/roster/equipment/presets/settings, SVG-иконки, compact cards, portal combo с поиском/клавиатурой. Новая JS в стиле CamelCase, Prettier semi=false. Сохранение settings/category через native. Head actor picker для выбора NPC. Превью/внешность/SkyrimNet пока не реализованы и помечены недоступными.

## Проверки 0.4

- Full build.bat exit0, последний полный проход36сек; CTest после миграции истории/границ, DLL без compiler warnings, Mutagen ESP, ZIP.
- Сравнение бинарных записей: все480 старых NPC_/ACHR/CLFM идентичны, ровно200female+200male,1202 различных собственныхFormID в800..FFF.
- Все ссылки предметов40 комплектов проверены по выгрузке HouseCarl research/equipment-records.jsonl. Её источник — Skyrim.esm defined-in записи, пользовательские победители применяются в runtime LookupForm.
- Mock UI smoke: custom combo/search, spawnpreset, auto избегает used слота, manual reused, крайний male399, категории, randomName, settings и Shift+F8 hint, rename escaping, stale identity; все5 страниц720p/1080p без horizontal overflow и pageerrors. Скрины Native/docs/ui-040-create.png и roster.png просмотрены, это Chrome/mock, не Skyrim.
- После полной сборки изменены лишь строка Console.WriteLine EspTool, README и UI подпись источника имени. ESP/pack повторён; после подписи выполняются последний UI smoke и pack.

## Установка и следующий ручной тест

Пользователю дать ZIP. Закрыть игру, обновить тестовый мод целиком (ESP/DLL/UI/slots.json должны совпадать), сохранить user-*.json/categories.json/settings.json, включая MO2 overwrite. Сделать отдельный сейв до обновления для отката. Проверить Shift+F8 и переназначение, старый сейв +400 слотов, создание с комплектом, выбор/историю слотов, park→summon с одеждой и count audit, save/reload, случайное русское имя, категории и сохранение набора. Ничего из этого не выдавать за уже выполненный игровой тест.

## Остальная архитектура и ограничения

- C++23, pinned alandtse/CommonLibVR ng 8eaddedd1542290701ccb0e9e88b2af8b3cea597, AE-only1.6.1170, MSVC14.44, static CRT. Magelight v4 C ABI, page callback → PostGameTask. Старые Prisma/Papyrus/renderer stubs исключены из CMake target; Game/Legacy0.1 не пакуется.
- Удаление очищает текущие данные NPCStudio + inventory, StopCombat/ресурсы/default name/Disable/MoveTo резерв. Чужие quests/relations/AI БД не стираются. Future SkyrimNet reuse identity cleanup ещё не решён.
- NND source research/NND 2986a406f8e07ae3c95e20614c58b115ad1b4c64; официальный API header vendored + GPL license, helper исправлен GetModuleHandleW. Native RevealName снимает cached Obscurity старых сейвов. SPID _DISTR лежит flat Data/; применяется после перезапуска. Общие NND настройки не меняются.
- MO2 D:/SteamLibrary/steamapps/common/Skyrim Special Edition/MO2, профиль NPCStudio Development. Skyrim Unbound/RaceMenu/SkyUI/PapyrusUtil/Address Library установлены, пользователь меняет свой тестовый профиль. Default не трогать. Логи Documents/My Games/Skyrim Special Edition/SKSE/NPCStudio.log.
- Исследования RaceMenu/SeverActions/SkyrimNet и технические риски описаны в TECHNICAL_AUDIT.md. NPA и renderer SeverActions закрыты; нельзя считать их код/API доступными. RaceMenu Papyrus LoadCharacterPreset запрещён для NPC по комментарию API. Нужна собственная корректная интеграция. SkyrimNet public profile update — очередь, не callback и не точная запись текста; SDKABI проверять перед интеграцией.

Финальная доработка: выбранный spawn-комплект хранится в assignment.pendingPreset до успешного применения. Если сохраниться до загрузки3D, AfterLoad возобновит выдачу. После Capture pendingPreset удаляется. Финальный полный build после этой правки: exit0,31сек, CTest/ESP/ZIP50файлов прошли, compilerwarnings нет; новый UI smoke после финальной подписи прошёл.

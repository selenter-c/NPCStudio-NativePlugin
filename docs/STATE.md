# Состояние исследования NPCStudio

Дата: 2026-11-02. Исторический аудит ниже; актуальное состояние — в последнем разделе. Реализация и ведение git разрешены пользователем.
Требования: Skyrim 1.6.1170, Magelight UI вместо PrismaUI, единый интерфейс создания и редактирования NPC, собственные 80 женских + 80 мужских слотов, внешность/волосы/цвет, живое 3D-превью, экипировка и пресеты ролей, биография через SkyrimNet.

Подтверждено:
- NPCStudio native: внешность, inventory, пресеты, SkyrimNet, биография, naming — заглушки. NPCFactory создаёт reference произвольного существующего ActorBase, пула нет. UI — PrismaUI.
- build-esp.ps1 пишет только TES4 и использует заголовок 20 байт вместо 24 байт Skyrim SE. Проверено: ESP 148 байт, size=128, ожидаемый конец 152. Заявленной QUST нет.
- BAF 2.6.6: houseCARL подтвердил 160 NPC_, 160 ACHR, 160 ColorRecord, 1 CELL, 1 QUST. Внешность через CharGen.LoadExternalCharacterEx mode=15 с полным jslot/nif/dds. Нельзя переносить ограничения этого пути на все реализации RaceMenu.
- NPA 3.1.0: закрытые исходники, публичный API в поставке не найден. Не рассчитывать на перенос его кода. README и LICENSE прочитаны.
- Magelight локально 1.7.1. Клон research/MagelightUI — tag v1.7.1, commit 05d05d8cf13164e0dead3596640f28c058b9b307. Прочитаны CPP.md, SDK.md, MANIFEST.md, PRISMAUI_MIGRATION.md, API header. JS callbacks не game-thread; использовать PostGameTask. Texture API требует texture на игровом D3D device; текущий NIF viewer создаёт отдельный device.
- В активном MO2 profile Default установлен победитель Magelight 1.5.1, а не 1.7.1. Мод NPC Studio отключён. MO2: D:/SteamLibrary/steamapps/common/Skyrim Special Edition/MO2. Профиль не изменялся.
- DevKit beta26-rc4: PublicSendCustomPromptToLLM v8+ — async, callback ThreadPool, const char* копировать. Генерация текста не равна установке биографии. Канал установки/перезагрузки био ещё исследуется.
- Изучены 4 вложенных скриншота и все 5 картинок Reference. Числа 32 и отдельные детали сгенерированных картинок — только референс, приоритет у требования 80+80.

Завершено:
- baseline build через NPCStudio-Build/build.bat: exit 0, 26 s, ZIP 14 файлов / ~674 KB. Лог docs/baseline-build.log. Проверена структура архива: PEX нет; ESP всё ещё некорректен. Не устанавливать как готовую версию.
- RaceMenu BSA распакован в research/RaceMenuBsa, прочитан настоящий scripts/source/chargen.psc. Дополнительно research/RaceMenuSource с тремя официальными upstream файлами. Upstream master не гарантирует ABI локальной DLL; SavePreset там возвращает false даже после попытки сохранения.
- SeverActions native renderer закрыт: официальный README прямо указывает private development repository. Полное клонирование остановлено; частичная research/SeverActions не является рабочим исходником. Полный README отдельно research/SeverActions-README.md; локальная поставка исходного проекта сохранна.
- Скачан официальный research/skyrimnet-devkit-beta29.1-rc2.zip; headers/docs в research/SkyrimNetDevKit-beta29.1. PublicRequestProfileUpdate v14: bool(formId,instructions,blocksCSV), асинхронная очередь без callback, сохраняет bio в текущем прохождении, нужен Actor. До спавна — CustomPrompt/draft. Точный импорт произвольного текста ещё не доказан.
- В активном MO2 SkyrimNet manifest 0.28.0; PublicAPI version выясняется в runtime, не по manifest.
- Новый SDK требует MSVC v143 и рекомендует /MT, старый утверждал /MD. Текущий toolset 14.50 + dynamic CRT. ОБЯЗАТЕЛЬНО проверить ABI до интеграции STL через DLL. В новом SDK отсутствовавший PublicAPIDiaryQuery.h есть; globals inline/namespace, старые headers только в одном .cpp.
- Основной отчёт docs/TECHNICAL_AUDIT.md: требования, факты, архитектурное предложение, риски, критерии проверки, очередность.

Следующий этап после обсуждения с пользователем:
1. Корректные ESP/build checks и минимальный Magelight.
2. Один постоянный слот + co-save и восстановление настроек базы.
3. Прототип actor preview и RaceMenu внешности с reload/save/restart.
4. Масштабирование 80+80, экипировка/пресеты, SkyrimNet, UI.

Открытые технические проверки: точный renderer actor; NPA-подобный jslot путь; точная запись редактированного bio/готовность profile update; очистка AI identity при переиспользовании слота. Не выдавать наличие API/успешную сборку за игровые проверки.

На этапе аудита код мода не переписывался, настройки MO2 и сохранения не менялись. Сборка обновила выходные артефакты. Не выполнялись commit/push/pull/создание веток.


## Реализация 0.2.0 — в работе
Пользователь разрешил commit/push/tag в трёх репозиториях. Исходная точка сохранена tag baseline-before-rewrite-20261102 в каждом.
Написаны Mutagen EspTool 0.54.4 (.NET10), фиксированный ESL-пул 80+80, Magelight bridge, StudioService с SKSE co-save, UI управления пулом. Старый UI/Papyrus вынесен в GamePlugin/Legacy/0.1.0; старые native подсистемы не входят в target. Библиотека alandtse/CommonLibVR pinned 8eaddedd1542290701ccb0e9e88b2af8b3cea597, AE-only, MSVC14.44, static CRT, без VR submodule.
Тесты: native StateCodec прошёл; UI smoke (mock bridge, Chrome) прошёл. Полный build доходил до packaging: исправлены Get-FileHash несовместимость PS и передача пустого --version; отдельный pack-mo2.ps1 уже создал проверенный ZIP 0.2.0 (9 файлов). Ещё нужен повторный build.bat для подтверждения всего pipeline.
В игре подтверждена загрузка DLL, 160 слотов и Magelight view по NPCStudio.log, но спавн/save/reload пока НЕ проверены. Тестовый профиль MO2 NPCStudio Development с локальными saves, отдельный мод NPCStudio Development 0.2. Default modlist SHA256 8292191189dd07d4bca8ae9d8c1d918709a4bf2e3676da26a5f32fb3ca7e7130, не менялся.
Пользователь запретил computer-use: больше не вызывать sky/cua и не выполнять действия в игре. Все игровые действия делает пользователь по инструкциям. Skyrim и MO2 пользователь закрыл. По запросу добавлены Skyrim Unbound Reborn 3.0.17 и PapyrusUtil в тестовый профиль. Ничего не запускать автоматически. F8 открывает NPCStudio только после входа в мир с закрытыми меню/консолью.
Основные функции пока: создать NPC, переименовать, призвать, скрыть без освобождения слота, телепортироваться к NPC. Preview/appearance/equipment/presets/bio ещё НЕ реализованы, UI помечает их недоступными.

Повторный полный NPCStudio-Build/build.bat завершился успешно: exit 0, 15 секунд, native tests + проверка ESP + архив. Игровые проверки спавна и save/load выполняет пользователь; результат пока не получен.

## Игровая проверка 0.2.0 подтверждена пользователем
Пользователь сообщил «всё работает», прислал открытый Magelight UI и созданного NPC aboba (женский слот 1). Конкретные шаги save/restart/load отдельно не перечислены, не считать их независимо проверенными. На скриншоте заметна тёмная голова; по прямой просьбе пользователя FaceGen отложен. Причина не установлена. Следующий этап — снаряжение и наборы ролей. Computer-use запрещён; не запускать игру и не обновлять используемый мод при работающей игре.

## 0.3.0 — снаряжение, готово к ручному тесту
Ветка main всех трёх репозиториев и baseline/checkpoint-0.2.0-build-tested отправлены в origin. Основа 0.2 подтверждена пользователем; FaceGen прямо отложен.
Добавлен EquipmentService.cpp: inventory, catalog (первые 100 совпадений, Unicode-поиск по имени/плагину), give+equip, unequip, применение и сохранение наборов. Все мутации на игровом потоке и только для собственных назначенных слотов; требуется живой enabled/3D-loaded actor. Полная проверка зависимостей до изменения; старые предметы не удаляются, выдаётся только недостающее. EquipObject/UnequipObject через CommonLib, queue=false/applyNow=true/force=false — игровая семантика пока не проверена.
16 JSON-наборов с конкретными vanilla-предметами, без leveled lists: 9 владений, 2 бандита, 2 гражданских, кузнец, шахтёр, маг. Адреса каждого проверены по выгрузке HouseCarl research/equipment-records.jsonl. Наборы не меняют faction/AI/class.
Custom-набор сохраняет базовые формы надетого, без custom enchant/temper/left hand. Адреса plugin+localID, динамические FF-формы не поддержаны. Файлы user-*.json через VFS могут попадать в overwrite, общие для сохранений. Сам инвентарь сохраняется движком в ESS, не дублируется в co-save. Фиксированные ID пула не менялись, версия co-save остаётся 1.
UI вкладка Снаряжение: наборы, инвентарь, поиск, выдача, снятие, сохранение. Автоматически проверены missing dependency, сохранение, смена вкладки, поиск, Unicode и 720p/1080p. Скриншот Native/docs/ui-equipment.png — Chrome mock, не игра. Финальный build.bat 0.3: exit 0, 28 секунд, CTest + ESP + проверенный ZIP (25 файлов). UI smoke повторно прошёл, предупреждений компилятора нет. Изменения ESP/slots.json относительно 0.2 отсутствуют.
Не развёртывать автоматически в MO2 и не запускать игру: пользователь выполняет тестовые действия сам. Предоставить ZIP и шаги установки с сохранением user-файлов. Требуется тест экипировки/повторного применения/сохранения набора/загрузки ESS. Preview, внешность, голоса/раса, SkyrimNet и авто-применение при создании ещё не реализованы.

## 0.3 проверена пользователем
Пользователь подтвердил выдачу/экипировку, прислал 2 одетых NPC и интерфейс набора. Лицо исправилось после изменения модов; исправление FaceGen не требуется по результату пользователя. Запрошены удаление с очисткой/повторным использованием слота и немедленное отображение имени с NND/Tamrielic Names/SPID. Найдена NND Obscurity (официальная документация и исходники research/NND).

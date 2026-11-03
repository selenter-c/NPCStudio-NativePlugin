# NPCStudio 0.6.1 — исправления после игрового теста 0.6.0

Пользователь подтвердил отказ превью: RegisterClient result 1; стальной меч Unsupported preview item; орк с зелёной головой и светлым телом. 0.6.0 не является стабильной версией.

## Причины и изменения
- Ghidra Phantasm RVA 2A320: длина имени клиента 1..24, символы [a-z0-9._-]. NPCStudio отвергается до создания клиента; теперь npcstudio, constexpr validation и regression test отвергают старое имя.
- Закреплённый CommonLib TESDataHandler.h:218..225 LookupForm<T> проверяет T::FORMTYPE, в отличие от TESForm::As<T>. Для общего TESBoundObject возвращался null. Превью предмета и аудит сохранённого инвентаря теперь используют нетипизированный LookupForm + As<TESBoundObject> с проверкой null. Это также объясняет ложную недостачу вещей на скриншотах.
- После завершения SwitchRace/ожидания 3D и перед экипировкой синхронизируется bodyTintColor уникального actor base с tintLayers: индекс слоя SkinTone берётся из faceRelatedData текущей расы/пола. Если слоя нет — presetDefault расы. Затем Actor::UpdateSkinColor обновляет материалы 3D. Работа на игровом потоке; игрок не меняется. Визуальная корректность требует проверки в игре, особенно для зверорас и сторонних заменителей тел.
- pack.py включает проверенный Phantasm Core (DLL, Interface/phantasm.swf, licenses) в тот же локальный FOMOD; Interface добавлен в requiredInstallFiles. Отдельная установка Phantasm больше не нужна. DLL не переименована и не изменена; внешний компонент может использоваться и другими модами.

## Проверено
build.bat: компиляция, CTest 2/2, Mutagen400slots, ZIP64files. Chrome headless mock UI успешно; это не тест настоящего Phantasm. Установку/игровой тест выполняет пользователь. Git checkpoint только build-tested.

## Следующий тест
Закрыть игру, отключить отдельный NPCStudio-Phantasm, установить NPCStudio-0.6.1-fomod.zip вместо предыдущего NPCStudio. Новая игра. Проверить создание орка и смену Nord→Orc→Nord, цвет кожи после одежды/возврата, NPC live view и портреты, меч/броню в item view, вращение/освещение, закрытие/повторное открытие. При отказе читать новые NPCStudio.log и Phantasm.log. Не заявлять исправленные FPS/краши без результатов.

---
Предыдущий контекст:

# NPCStudio — состояние 2026-11-03, 0.6.0

## Постоянные ограничения

Общение и комментарии на русском, без имени/ника. Пользователь явно разрешил commit/push/tag/откат трёх main репозиториев github.com/selenter-c. Computer-use, запуск Skyrim/MO2 и управление их окнами запрещены. Установка и игровые действия — пользователь. Терминал, файлы, read-only houseCARL, локальные headless тесты разрешены. Не менять MO2 профиль и не копировать DLL в активную игру. build через NPCStudio-Build/build.bat, NPCSTUDIO_NO_PAUSE=1, без clean.

Пользователь требует **никаких миграций старых сохранений**: каждый тест с новой игры. FaceGen был исправлен модами и исключён из задачи. Новый текущий билд0.6.0 прошёл сборку и headless, но **не подтверждён игрой**. Предыдущий0.5.1 визуально рисовал NPC, однако вращение~1FPS и GPU hang. Последний явно стабильный checkpoint:0.4.0-user-tested. 0.5.0/0.5.1 — build-tested, не stable.

## Последняя задача и результат

Запрос: устранить лаги/краши рендера, реально разобрать SeverActions и item renderer. Пользователь предложил https://github.com/morluto/rea. Прежнее утверждение о невозможности анализа закрытого DLL исправлено. Использованы REA6.8.0 CLI (inspect-pe-imports, execute-rizin-command), Rizin0.9.1, Ghidra11.4.2 headless/JDK21. Codex/MCP/system Node configs не менялись. Установленные инструменты/выводы/декомпиляция только в research/SeverReverse, вне публикуемых репозиториев. Подробные подтверждения и ABI: NativePlugin/docs/RENDERING_060.md.

**Найден отдельный Phantasm.dll** в SeverAction/97 Phantasm/00 Core — именно renderer NPC/items SeverActions4.7.0. Export RequestPluginAPI, v1 struct90/provider300,16 методов. Phantasm0.3.0 ccf916f72d59, SHA2568b9bf8341dee500c033c8777d3b616d968295676c91bbaf2946d7ae611bd7cff. Публичного repoPhantasm/исходников не найдено, но экспорт и вызовы восстановлены по DLL/Severclient. **Не придумывать приватные адресные вызовы**: адаптер получает только экспортированную API-таблицу и проверяет hash/header/все16RVA перед её использованием. Реализация src/preview/PhantasmAPI.h и ActorPreview.cpp.

Собственный небезопасный D3D-renderer0.5.1 удалён из ActorPreview.cpp/DLL target. Там были deferredExecuteCommandList, recapture, readback/PNG при каждом повороте, фоновые аватары перетирали одну сцену, каждая previewкоманда делала Snapshot→40JSON/полныйRender. Лог NPCStudio был stagingHRESULT887A0005 DEVICE_REMOVED, затем NVidiaGPUhangвMagelight; точный чужойхук не обвинён. Новый адаптер открывает Phantasm OpenActor/OpenItem, принимает liveimageUrl, Orbit/Zoom/6lights через API. Портреты144x144 — отдельные RequestStill callbacks; packedRGBA, metadata+28 НЕ stride (декомпиляцией подтверждено). После секунды простоя камеры фоновая очередь возобновляется, первые24, максимум4pending. Framing используется1 (допустимо0..2, композиция требует ручного теста).

Callback не обращается к RE, копирует строки/пиксели, PostGameTask передаёт UI. Generation/client cookie + UIEpoch отвергают старыеcallbacks, JSPreviewSequence отбрасывает поздние partial responses. Clear удаляет state подmutex, ReleaseClient внеmutex. Вызывается наclose/tab/selection/park/setRace/delete/preload/NewGame. Nativepreview/portrait/itemPreview не вызывают Snapshot. JS RAF + один запросвработе, Update только image/avatar, без всей страницы. Предметы3D открываются кнопкой3D у catalog/inventory, drag/yaw/zoom. Дляliveview не PNG.

## Файлы сборки и установка

NPCStudio-Build/dist/NPCStudio-0.6.0-fomod.zip — мод. NPCStudio-Phantasm-0.3.0-local.zip — отдельная личная локальная зависимость из предоставленного Core (DLL/SWF/VERSION/licences), **не добавляется в Git и не публикуется**. Инструмент Build/tools/phantasm.py --pack проверяет реальный PE/SHA/таблицу относительно nativeheader и упаковывает его. Полный SeverActions включать не нужно; вMO2 должен побеждать один проверенныйPhantasm. ЕслиDLLне загружена/иная — понятная ошибка, старогоrendererfallback нет.

Финальный build.bat:28сек, CTest2/2, Mutagen400слотов иZIP51files, compilerwarnings0. Лог rootdocs/v060-build.log. Проверка Phantasm PE прошла. Chrome headless smoke прошёл:partial/async,avatar,itemviewer,100camera inputscoalesced безпересозданияrosterDOM,locale/theme/race/slots/gear/settings/delete/staleidentity,720p/1080p. Native/docs/ui-060-* — **mock**, неSkyrim. WARPtest удалён: он проверялстарыйrenderer,теперьтестируютсяABI/zoom/yaw/размерпортрета. Нельзя обещатьFPSилиотсутствиевылетовбезручноготеста.

Следующийшаг пользователя:при закрытойигреустановить2ZIP/включитьPhantasm,новаяигра,Shift+8,создатьNPCсрасой/набором,вращать/zoom/6lights,сменятьNPC,закрыватьменю,предметы3D,park/summon/race/delete/save/load. Есликраш —свежиеNPCStudio.log/Phantasm.log/Magelight.log/CrashLogger. Не запускатьсамостоятельно.

## Сохраняемые механизмы

Единыйпул400:id0..199female,id200..399male,displayid%200+1. CLFM900+id,NPCB00+id,ACHRD00+id;CELL800/XMarker801.1202формыдоE8F,ESL. slotsmanifest3/co-saverecord2/JSONpoolVersion3,mandatoryname/identity/race+usedSlots. Толькоfresh/used,autoсначалановые;deleteсохраняетисторию. Старыеschemesотклоняютсябезмиграций.

Смена расы черезCommonLib Actor::SwitchRace(race,false),AEreloc37925,безVMDispatch(ранееотказывал). MoveTo/Enable→ожидание3D→одинSwitchRace→проверкафактическойрасы/стабилизация600мс→gear;40тиков/6секмаксимум.10стандартныхрас,модовыене включены.

40gearpresets/customcategory/userpresets. Capturecounts/wornпередpark/save,Restoreпосле3Dс600мспроверкойбезавтодовыдачи. DynamicFFпропускаются,presetsнепереносятtemper/enchant/extras/руку. PendingPresetсохраняется. Deleteочищаетassignment/inventory/ресурсы/StopCombat/Disable/MoveToholding/defaultname,нечужиеquests/relations. Worker150ms main-taskcoalesced,worldEpoch.

Shift+8DIK9+modifiermask1Shift2Ctrl4Alt.Settingsschema2 JSON,initialWindowsUILanguage→ru/en,persistentchoice,4themes. Names::RandomчитаетTamrielic/NNDlistчерезVFSпоstandardrace/sex,неполныйалгоритмфамилий. NNDRevealNameиSPIDисключения. Полныйредакторголовы/RaceMenu/SkyrimNetнеподключены. SkyrimNetPublicRequestProfileUpdate—очередьбезcallback. АудитисточниковTECHNICAL_AUDIT.md.

BuildMSVC14.44/staticCRT,AEonly1170,CommonLibalandtse/CommonLibVRpin8eaddedd1542290701ccb0e9e88b2af8b3cea597. NNDGPL/MagelightMIT/stblicencesсохранены. MO2 D:/SteamLibrary/steamapps/common/Skyrim Special Edition/MO2, SKSElogs C:/Users/Selenter/Documents/My Games/Skyrim Special Edition/SKSE.

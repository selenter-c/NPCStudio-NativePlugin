# Рендер NPCStudio 0.6.0: Phantasm

## Что удалось установить

Ранний вывод «без исходников нельзя разобраться» был неверен. В предоставленном SeverActions 4.7.0 рендер выделен в `97 Phantasm/00 Core`: Phantasm.dll, Interface/phantasm.swf, VERSION.txt и лицензии зависимостей. У DLL есть экспорт RequestPluginAPI. SeverActionsNative содержит клиент PhantasmClient, который вызывает этот API для NPC и предметов.

REA 6.8.0 (https://github.com/morluto/rea) действительно использован: inspect-pe-imports и execute-rizin-command. Для него локально вызван Node 24.22.1 через npx, системный Node/config/MCP не менялись. Rizin 0.9.1 дал дизассемблирование; его сборка без pdg. Ghidra 11.4.2 + существующий JDK21 в headless режиме выполнили анализ и декомпиляцию выбранных функций. Чужие DLL и декомпилированный код в Git не добавляются. Локальные доказательства и скрипты находятся в research/SeverReverse рядом с тремя репозиториями.

Провайдер: Phantasm 0.3.0 ccf916f72d59, timestamp 6AE6826F, SHA256 `8b9bf8341dee500c033c8777d3b616d968295676c91bbaf2946d7ae611bd7cff`.
RequestPluginAPI RVA1FBA0, таблица RVA1FDB40, header size90/version1/provider300. Вызовы идут только через экспортированную таблицу. Ни приватных вызовов по RVA, ни LoadLibrary, ни изменения кода DLL нет. SHA256 и все16 адресов таблицы проверяются до RegisterClient. Другие сборки отклоняются.

## Принцип и подтверждённый контракт

Провайдер использует D3D-устройство игры, carrier-меню с PostDisplay и сохранение GPU-состояния LiveD3DStateBackup. OpenActor/OpenItem возвращают view handle; ready-событие содержит imageUrl. Этот URL помещается в img Magelight, последующие движения меняют камеру провайдера. PNG при каждом повороте больше нет. Это интеграция рендера SeverActions через его отдельную библиотеку, а не копия закрытых шейдеров.

Восстановленный контракт — src/preview/PhantasmAPI.h. Сверены места вызовов SeverActionsNative OpenSlot VA180565CC0, Initialize1805661E0 и обработчик событий180565020. На стороне Phantasm декомпилированы OpenActor2B4E0, OpenItem2B610, SetHint2E420, Orbit/Zoom и их очереди, StillAnswer2BD90, RequestStill2DE30, RequestStillHinted2DFD0, RequestItemStill2E190, CancelStill2E330.

API offsets от начала таблицы:

| Offset | Функция |
| --- | --- |
| 10/18 | RegisterClient/ReleaseClient |
| 20/28/30 | OpenActor/CloseView/Resize |
| 38/40/48 | Orbit/Zoom/Refresh |
| 50/58 | SetHint/QueryHint |
| 60/68 | RequestStill/RebuildActor3D |
| 70/78 | OpenItem/CancelStill |
| 80/88 | RequestStillHinted/RequestItemStill |

Event: size0,kind4,view8,formC,reason10,imageUrl20. Используется только prefix28; kind1 ready,3 ended. Строки копируются до возврата callback. Orbit меняет yaw -= delta (градусы); yaw оборачивается через остаток360. Zoom меняется по pow(1.00150001049,delta); UI absolute scale переводится в delta через логарифм. Это подтверждено функциями58790/586F0. Освещение — простые строки studio/daylight/hearth/torchlight/moonlight/here, не JSON.

Still размер30: request4,success8,reason10,width18,height1C,pixels20,metadata28. **Поле28 не RowPitch**: TendPortrait записывает туда float-метаданные про проекцию, поэтому оно игнорируется. Readback DE860 копирует строки плотным width*4, EnsureLiveDrawResources9A3C0 создаёт color/SRV с DXGI_FORMAT_R8G8B8A8_UNORM (28). Resize E2C70 также упаковывает четыре байта на пиксель. Принимаются только144x144, PNG кодируется один раз для аватара пока pixels ещё валиден. После секунды простоя камеры фоновая очередь продолжает портреты первых24 видимых NPC, максимум4 ожидающих ответа. Имена framing0..2 полностью не восстановлены; используется допустимое значение1, композицию нужно оценить в игре.

## Что убрано из нашего DLL

Прежний ActorPreview0.5.1 выполнял recapture геометрии, создавал render/depth/staging ресурсы, ExecuteCommandList, GPU readback и PNG/base64 при повороте. Фоновые аватары перезаписывали единственную сцену. Каждая команда также делала Snapshot с чтением40 JSON и перестройкой страницы. В логе был HRESULT887A0005 DEVICE_REMOVED, затем зависание GPU-стека в Magelight.log. Это свидетельство ошибки пути рендера, но не точное доказательство конкретного виновного GPU-хука.

Теперь старый D3D-код исключён полностью из ActorPreview.cpp и DLL target, d3d11/d3dcompiler в target не нужны. Adapter не захватывает геометрию и не трогает контекст игры. preview/portrait/itemPreview возвращают маленькие partial responses без Snapshot; JS обновляет только превью/аватар. RAF и один запрос в работе объединяют быстрый ввод. Полученный imageUrl используется повторно.

Все RE/API-команды — игровой поток. Callback провайдера не читает RE; события копируются и передаются в PostGameTask. Client generation + UI epoch отвергают старые callbacks, JS sequence — запоздавшие подтверждения. Callback cookie портрета — номер, не указатель на уже освобождённый объект. Clear сначала удаляет наше состояние под mutex, затем вне mutex вызывает ReleaseClient. Close, смена вкладки/выбора, park, setRace, delete, preload, NewGame освобождают виды и запросы. Возврат к виду открывает новый handle.

## Установка и проверка

Отдельный **локальный** архив зависимости создаёт `python NPCStudio-Build/tools/phantasm.py --pack` из предоставленного пользователем SeverAction/97 Phantasm/00 Core. Он сохраняет SWF, DLL, VERSION и лицензии, проверяет SHA/header/таблицу относительно нашего header. Он игнорируется Git; это не публичная раздача библиотеки. Полный SeverActions включать не требуется, но один проверенный Phantasm должен быть доступен в VFS MO2. Не устанавливать несколько разных Phantasm.dll.

Нужны ручные проверки: новая игра, NPC разных рас и одежды, вращение/zoom/6lights, быстрая смена NPC, многократное закрытие, предметы ARMO/WEAP/AMMO, смена расы/park/delete при открытом виде, save/load. FPS, совместимость с CommunityShaders/framegen и отсутствие вылетов **не доказаны headless тестами**. При сбое нужны NPCStudio.log, Phantasm.log/Magelight.log и свежий CrashLogger.

Автотесты: CTest state/settings и ABI/layout/zoom/yaw/границы портретов; Python проверяет реальный PE провайдера; Chrome mock проверяет partial/async сообщения, аватары, item viewer,100 input событий с объединением без замены roster DOM, закрытие и остальные функции интерфейса. Прежний WARP тест удалён, потому что он проверял больше не используемый renderer.

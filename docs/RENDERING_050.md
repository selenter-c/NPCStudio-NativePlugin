# Рендер NPCStudio 0.5: исследование и прототип

## Что действительно доступно у SeverActions

Источник: локальная поставка SeverAction/00 Core, публичный https://github.com/Severause/SeverActions и API/NATIVE_API.md. Автор прямо указывает, что native source/build tooling находятся в private development repository. Повторно проверено 2026-11-03. Нативного исходного рендера в поставке нет.

В bundled main-jmS7MCkP.js видны mannequinLiveStatus → ok/imageUrl/state и обработчики stageStatus, mirrorIdles, mirrorExpression. stageOrbit отправляет dYaw/dElev/dZoom; pointermove даёт относительное вращение, wheel — zoom. Это контракт их страницы с их DLL, не публичный API для стороннего плагина. Вызвать эти JS-функции из нашей страницы недостаточно: у нашего View нет зарегистрированных native listeners SeverActions.

dumpbin /exports SeverActionsNative.dll: ровно11 SAAPI функций (GetVersion, BusyReasons/IsKnownChild, intimacy, background prompt, UI theme/language, history provider). Нет Render/Preview/Texture экспорта. NATIVE_API.md подтверждает эту поверхность. Возможность получить actor texture от SeverActions не обнаружена; адреса внутри DLL не угадывались.

Публичное описание различает live mannequin preview и Live Stage. Live Stage показывает самого NPC в мире и управляет камерой; это отдельная система, а не доказательство offscreen render API. Портреты автора допускают пользовательские картинки. Не приписывать собственной реализации их внутренние алгоритмы.

## Собственная реализация

Native/src/preview/ActorPreview.cpp. Только game-thread через существующий PostGameTask; ни C++ hooks, ни Papyrus PSC, ни перемещение actor/camera ради превью. Живая геометрия снимается один раз в текущей позе, затем рисуется независимая камера. Это 3D-снимок, не анимационный renderer.

1. Существующий Actor.Get3D, NiNode children, app-culled фильтр. Только BSTriShape/BSDynamicTriShape; dismember visibility учитывается.
2. BSGraphics::TriShape buffer/VertexDesc: staging GPU readback, проверка stride/count/индексов. Full/half position, half UV; dynamic head position из float4 под BSSpinLock. NiSkinData boneVert weights или SSE packed half4 weights + byte4 global bone indices. Формат подтверждён локальным nifly NifFile::GetShapeBoneWeights и SSE conversion, не по догадке о partition palette. Преобразование inverse(actorRoot.world)*bone.world*skinToBone. Чужие NiNodes не изменяются.
3. Diffuse SRV удерживается COM AddRef на том же D3D11 device Skyrim. Material UV scale/offset, alpha test, hair/face RGB tint. Нет полного игрового lighting shader: geometric normals и простой студийный key/fill/rim.
4. Deferred context, собственные color/depth/buffers/shaders, ExecuteCommandList(RestoreContextState=true); игровой backbuffer не трогается. Readback color → PNG → data URI через InteropCall. Нет второго несовместимого устройства и нет перехвата Present.
5. Один native scene cache по identity. Смена мира очищает его; кадры/портреты в JS памяти, co-save не раздувается. В списке очередь первых24 отфильтрованных NPC с паузой450мс; незагруженные не телепортируются. Закрытие UI останавливает очередь. Повторное открытие сбрасывает визуальный кэш.
6. Лимиты: 256 meshes, 250000 vertices/mesh, 500000 vertices/scene, 3million indices/scene, bounded raw buffer32MB, node walk4096/depth64, partitions128/bones512. Неподдержанные meshes пропускаются, итоги в UI/логе. Snapshot Frame команды имеют assignment identity и world/ready guards.

## Что проверено и что ещё нет

CTest preview-graphics реально создаёт WARP D3D11 device и запускает тот же код setup/shaders/render/PNG. Разные yaw/light дают разные PNG; сравнивается viewport до/после. Это проверка графического backend, а не способ получить тестового NPC вне Skyrim. UI mock обозначен MOCK RENDERER.

Реальное чтение SSE GPU buffers, skinning конкретных модовых голов/одежды, FaceTint, прозрачность волос и качество материалов требуют ручного игрового теста. Первый GPU snapshot синхронный и может дать hitch. Самый важный лог: NPCStudio.log, строки «Превью: N частей, M пропущено, bounds Z ...». Если отображаются только части, сначала разбирать фактические buffers/desc/skin data, не менять ESP и не лечить FaceGen.

Следующий этап после ручной проверки: точность головы/FaceTint/normal maps/alpha, эффективный GPU texture transport Magelight вместо PNG, затем анимации и полный редактор внешности. Приоритет — корректная модель и lifecycle, а не рекламировать паритет с закрытым SeverActions.

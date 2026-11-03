# NPCStudio 0.6.6 — ожидается игровой тест

Запрос: живой 3D редактор портрета, исправление Invalid portrait image, native pan, оформление scale slider; логотип оставить.

Портрет: удалён старый Canvas-кроп. На время dialog существующий live viewport переносится внутрь окна, поверх него подвижный круг (60..400px); после закрытия viewport возвращается на место. Save вызывает portrait с editorCapture, camera и нормализованным crop. Native RequestStillHinted 480x640/framing0; RGBA кропируется в256x256, PNG кодируется stb, сохраняется атомарно по identity. Пиксельный буфер копируется только внутри callback. Требует проверки соответствия framing0 живому виду и точности crop в Skyrim. HTML canvas для GPU image больше не используется. Проверить повторный запуск и повторную съёмку. Прежние .image кеши не мигрируем.

RE: Ghidra FUN180021a90 парсит строку turn=...;pitch=...;lighting=..., а не JSON. FUN18003ad80 Zoom передаёт pivot и вычисляет цель камеры; FUN180058a70 при scale→1 обнуляет цель. Новый native pan: anchored zoom после сброса цели; physical scale .95.. .5 (logical zoom .5..2.5). Pivot ограничен0..1, поэтому диапазон pan сужается на дальнем плане. Текстура/рамка HTML НЕ двигается. PNG crop компенсирует scale/pan математически, но соответствие реальному framing требует игрового теста. Не писать что runtime уже подтверждён. Запрещённые direct writes и private hooks не добавлялись: используются только проверенные API Phantasm.

CSS: оформлены uiScale/circle range WebKit track/thumb. Автотест проверяет живой viewport внутри dialog, возврат после save, editorCapture вместо savePortrait canvas, неподвижную текстуру при pan и передачу координат. Native build.bat + CTest2 + Mutagen + ZIP64files. MO2/Skyrim запускает только пользователь. Git публикация ранее разрешена.

Голова, пауза и остальные старые проблемы подтверждены пользователем в0.6.4; не трогать восстановление расы без новой причины. Расширенный внешний редактор RaceMenu и SkyrimNet биографии — следующие большие задачи. Нет поддержки старых схем сохранений.

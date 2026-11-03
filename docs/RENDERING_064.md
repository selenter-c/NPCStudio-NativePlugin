# NPCStudio 0.6.4 — проверено сборкой, игровой тест ожидается

Изменения: Magelight RequestUIMode pause flag; очередь восстановления временно снимает паузу, затем возвращает. Для экипировки даётся 3 тика worker (~450ms), ввод остаётся у UI. Закрытие освобождает UI mode. Проверить паузу/создание в игре.

Портрет: реальная причина из NPCStudio.log — view-drawing. Закрываем live view перед RequestStill, блокируем возобновление до callback (pending ACK не завершает операцию), до 3 повторов busy/view-drawing с 200ms задержкой, таймаут6s, ошибки видны. В игре ещё не подтверждено.

Голова: RaceMenuSource/PresetInterface.cpp ApplyPreset заменяет полный массив headParts базовым набором расы перед пресетом; вызывает полную пересборку узлов. Теперь восстанавливаем RNAM, массив через allocator движка, сбрасываем originalRace/faceNPC, применяем сохранённые части/морфы/пресеты лица и DoReset3D(false), а не Update3DModel (обновление экипировки). Ждём завершения SwitchRace перед этим. Проверить именно полный выход из процесса после Nord→Orc; предыдущие версии проблему НЕ исправили. Не считать исправление подтверждённым.

Источники: BAF_ActorScript.psc OnBuildAFollowerOnloadUpdate и LoadExternalCharacterEx; RaceMenuSource/PapyrusCharGen.cpp и PresetInterface.cpp. BAF явно восстанавливает несериализуемый mapped preset раз в сессию. NPA README требует bExternalHeads=0 и описывает замену внешности с сохранением игровой расы; это не наш SwitchRace. REA imports NPA сохранены research/SeverReverse/rea-npa-imports.json; это НЕ полный анализ его алгоритма внешности. Для будущего редактора нужны дальнейшее исследование NPA и интеграция RaceMenu.

Камера: предмет yaw180 по умолчанию; компактный крест+reset; ПКМ перемещает изображение внутри viewport (CSS composition, НЕ настоящая XYZ camera Phantasm), Shift+ПКМ меняет глубину через zoom. Нативный pan API ещё не найден. Не выдавать это за полноценный 3D pan.

Проверки: build.bat, CTest2, Mutagen ESP, ZIP64files; headless Chrome mock UI, pending portrait ACK, pan/reset, item yaw180, остальной smoke. Снимки ui-064 — MOCK, не Skyrim. ZIP dist/NPCStudio-0.6.4-fomod.zip. MO2 и игру запускает пользователь. Нет миграций старых сохранений.

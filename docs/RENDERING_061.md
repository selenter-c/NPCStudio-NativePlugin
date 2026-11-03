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

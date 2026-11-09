SET @MODULE_STRING := 'mod-aoe-loot';

-- module string
DELETE FROM `module_string` WHERE `module` = @MODULE_STRING;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
(@MODULE_STRING, 1, 'This server is running the |cff4CFF00Loot aoe|r module.'),
(@MODULE_STRING, 2, '|cff4CFF00[Loot aoe]|r Your items has been mailed to you.'),
(@MODULE_STRING, 3, 'AOE Loot module is active. Use .aoeloot on/off to toggle it.'),
(@MODULE_STRING, 4, 'AOE Loot: Quest item sent to your mailbox.'),
(@MODULE_STRING, 5, 'AOE Loot is already enabled for your character.'),
(@MODULE_STRING, 6, 'AOE Loot enabled for your character. Type .aoeloot off to disable it.'),
(@MODULE_STRING, 7, 'AOE Loot is already disabled for your character.'),
(@MODULE_STRING, 8, 'AOE Loot disabled for your character. Type .aoeloot on to enable it.');

-- localizations
DELETE FROM `module_string_locale` WHERE `module` = @MODULE_STRING;
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
-- ID 1: welcome
(@MODULE_STRING, 1, 'koKR', '이 서버는 |cff4CFF00Loot aoe|r 모듈을 실행 중입니다.'),
(@MODULE_STRING, 1, 'frFR', 'Ce serveur utilise le module |cff4CFF00Loot aoe|r.'),
(@MODULE_STRING, 1, 'deDE', 'Dieser Server verwendet das |cff4CFF00Loot aoe|r-Modul.'),
(@MODULE_STRING, 1, 'zhCN', '此服务器正在运行 |cff4CFF00Loot aoe|r 模块。'),
(@MODULE_STRING, 1, 'zhTW', '此伺服器正在執行 |cff4CFF00Loot aoe|r 模組。'),
(@MODULE_STRING, 1, 'esES', 'Este servidor está ejecutando el módulo |cff4CFF00Loot aoe|r.'),
(@MODULE_STRING, 1, 'esMX', 'Este servidor está ejecutando el módulo |cff4CFF00Loot aoe|r.'),
(@MODULE_STRING, 1, 'ruRU', 'На этом сервере запущен модуль |cff4CFF00Loot aoe|r.'),
-- ID 2: items mailed
(@MODULE_STRING, 2, 'koKR', '|cff4CFF00[Loot aoe]|r 아이템이 우편으로 발송되었습니다.'),
(@MODULE_STRING, 2, 'frFR', '|cff4CFF00[Loot aoe]|r Vos objets vous ont été envoyés par courrier.'),
(@MODULE_STRING, 2, 'deDE', '|cff4CFF00[Loot aoe]|r Deine Gegenstände wurden dir per Post zugesandt.'),
(@MODULE_STRING, 2, 'zhCN', '|cff4CFF00[Loot aoe]|r 您的物品已邮寄给您。'),
(@MODULE_STRING, 2, 'zhTW', '|cff4CFF00[Loot aoe]|r 您的物品已郵寄給您。'),
(@MODULE_STRING, 2, 'esES', '|cff4CFF00[Loot aoe]|r  Sus artículos le han sido enviados por correo.'),
(@MODULE_STRING, 2, 'esMX', '|cff4CFF00[Loot aoe]|r  Sus artículos le han sido enviados por correo.'),
(@MODULE_STRING, 2, 'ruRU', '|cff4CFF00[Loot aoe]|r Ваши предметы отправлены вам по почте.'),
-- ID 3: module active
(@MODULE_STRING, 3, 'koKR', 'AOE Loot 모듈이 활성화되었습니다. .aoeloot on/off로 전환하세요.'),
(@MODULE_STRING, 3, 'frFR', 'Le module AOE Loot est actif. Utilisez .aoeloot on/off pour le basculer.'),
(@MODULE_STRING, 3, 'deDE', 'Das AOE-Loot-Modul ist aktiv. Verwende .aoeloot on/off, um es umzuschalten.'),
(@MODULE_STRING, 3, 'zhCN', 'AOE Loot 模块已激活。使用 .aoeloot on/off 切换。'),
(@MODULE_STRING, 3, 'zhTW', 'AOE Loot 模組已啟用。使用 .aoeloot on/off 切換。'),
(@MODULE_STRING, 3, 'esES', 'El módulo de Botín AOE está activo. Usa .aoeloot on/off para activarlo o desactivarlo.'),
(@MODULE_STRING, 3, 'esMX', 'El módulo de Botín AOE está activo. Usa .aoeloot on/off para activarlo o desactivarlo.'),
(@MODULE_STRING, 3, 'ruRU', 'Модуль AOE Loot активен. Используйте .aoeloot on/off для переключения.'),
-- ID 4: quest item mailed
(@MODULE_STRING, 4, 'koKR', 'AOE Loot: 퀘스트 아이템이 우편함으로 발송되었습니다.'),
(@MODULE_STRING, 4, 'frFR', 'AOE Loot : objet de quête envoyé à votre boîte aux lettres.'),
(@MODULE_STRING, 4, 'deDE', 'AOE Loot: Questgegenstand an dein Postfach gesendet.'),
(@MODULE_STRING, 4, 'zhCN', 'AOE Loot：任务物品已发送至您的邮箱。'),
(@MODULE_STRING, 4, 'zhTW', 'AOE Loot：任務物品已寄送至您的信箱。'),
(@MODULE_STRING, 4, 'esES', 'Botín AOE: Objeto de misión enviado a tu buzón.'),
(@MODULE_STRING, 4, 'esMX', 'Botín AOE: Objeto de misión enviado a tu buzón.'),
(@MODULE_STRING, 4, 'ruRU', 'AOE Loot: предмет задания отправлен на вашу почту.'),
-- ID 5: already enabled
(@MODULE_STRING, 5, 'koKR', 'AOE Loot은 이미 캐릭터에 대해 활성화되어 있습니다.'),
(@MODULE_STRING, 5, 'frFR', 'AOE Loot est déjà activé pour votre personnage.'),
(@MODULE_STRING, 5, 'deDE', 'AOE Loot ist für deinen Charakter bereits aktiviert.'),
(@MODULE_STRING, 5, 'zhCN', 'AOE Loot 已为您的角色启用。'),
(@MODULE_STRING, 5, 'zhTW', 'AOE Loot 已為您的角色啟用。'),
(@MODULE_STRING, 5, 'esES', 'El Botín AOE ya está activado para tu personaje.'),
(@MODULE_STRING, 5, 'esMX', 'El Botín AOE ya está activado para tu personaje.'),
(@MODULE_STRING, 5, 'ruRU', 'AOE Loot уже включён для вашего персонажа.'),
-- ID 6: enabled
(@MODULE_STRING, 6, 'koKR', 'AOE Loot이 캐릭터에 대해 활성화되었습니다. 비활성화하려면 .aoeloot off를 입력하세요.'),
(@MODULE_STRING, 6, 'frFR', 'AOE Loot activé pour votre personnage. Tapez .aoeloot off pour le désactiver.'),
(@MODULE_STRING, 6, 'deDE', 'AOE Loot für deinen Charakter aktiviert. Gib .aoeloot off ein, um es zu deaktivieren.'),
(@MODULE_STRING, 6, 'zhCN', '已为您的角色启用 AOE Loot。输入 .aoeloot off 以禁用。'),
(@MODULE_STRING, 6, 'zhTW', '已為您的角色啟用 AOE Loot。輸入 .aoeloot off 以停用。'),
(@MODULE_STRING, 6, 'esES', 'Botín AOE activado para tu personaje. Escribe .aoeloot off para desactivarlo.'),
(@MODULE_STRING, 6, 'esMX', 'Botín AOE activado para tu personaje. Escribe .aoeloot off para desactivarlo.'),
(@MODULE_STRING, 6, 'ruRU', 'AOE Loot включён для вашего персонажа. Введите .aoeloot off, чтобы отключить.'),
-- ID 7: already disabled
(@MODULE_STRING, 7, 'koKR', 'AOE Loot은 이미 캐릭터에 대해 비활성화되어 있습니다.'),
(@MODULE_STRING, 7, 'frFR', 'AOE Loot est déjà désactivé pour votre personnage.'),
(@MODULE_STRING, 7, 'deDE', 'AOE Loot ist für deinen Charakter bereits deaktiviert.'),
(@MODULE_STRING, 7, 'zhCN', 'AOE Loot 已为您的角色禁用。'),
(@MODULE_STRING, 7, 'zhTW', 'AOE Loot 已為您的角色停用。'),
(@MODULE_STRING, 7, 'esES', 'El Botín AOE ya está desactivado para tu personaje.'),
(@MODULE_STRING, 7, 'esMX', 'El Botín AOE ya está desactivado para tu personaje.'),
(@MODULE_STRING, 7, 'ruRU', 'AOE Loot уже отключён для вашего персонажа.'),
-- ID 8: disabled
(@MODULE_STRING, 8, 'koKR', 'AOE Loot이 캐릭터에 대해 비활성화되었습니다. 활성화하려면 .aoeloot on을 입력하세요.'),
(@MODULE_STRING, 8, 'frFR', 'AOE Loot désactivé pour votre personnage. Tapez .aoeloot on pour l''activer.'),
(@MODULE_STRING, 8, 'deDE', 'AOE Loot für deinen Charakter deaktiviert. Gib .aoeloot on ein, um es zu aktivieren.'),
(@MODULE_STRING, 8, 'zhCN', '已为您的角色禁用 AOE Loot。输入 .aoeloot on 以启用。'),
(@MODULE_STRING, 8, 'zhTW', '已為您的角色停用 AOE Loot。輸入 .aoeloot on 以啟用。'),
(@MODULE_STRING, 8, 'esES', 'Botín AOE desactivado para tu personaje. Escribe .aoeloot on para activarlo.'),
(@MODULE_STRING, 8, 'esMX', 'Botín AOE desactivado para tu personaje. Escribe .aoeloot on para activarlo.'),
(@MODULE_STRING, 8, 'ruRU', 'AOE Loot отключён для вашего персонажа. Введите .aoeloot on, чтобы включить.');

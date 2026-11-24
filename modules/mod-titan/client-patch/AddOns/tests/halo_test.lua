-- Offline test for the Halo (Emberforge_Runes/Halo.lua): loads the rune addon against a mocked WoW 3.3.5 API and plays
-- the Runes of Enhancement window the way the server drives it (see src/rune_tiers.cpp). Run: lua5.1 halo_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Runes/"
local failures = 0
local function check(ok, what)
    print((ok and "ok    " or "FAIL  ") .. what)
    if not ok then
        failures = failures + 1
    end
end

-- Mock widgets: any capitalized method is a no-op unless defined below.
local Widget = {}
Widget.__index = function(self, key)
    local method = rawget(Widget, key)
    if method then
        return method
    end
    if type(key) == "string" and key:match("^[A-Z]") then
        return function() end
    end
end
local function NewWidget(name, parent)
    local widget = setmetatable({ shown = true, scripts = {}, text = "", width = 36, height = 36, name = name,
        parent = parent }, Widget)
    if name then
        _G[name] = widget
    end
    return widget
end
function Widget:GetName() return self.name end
function Widget:CreateTexture() return NewWidget(nil, self) end
function Widget:CreateFontString() return NewWidget(nil, self) end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:GetScript(event) return self.scripts[event] end
function Widget:Show()
    local was = self.shown
    self.shown = true
    if not was and self.scripts.OnShow then
        self.scripts.OnShow(self)
    end
end
function Widget:Hide()
    local was = self.shown
    self.shown = false
    if was and self.scripts.OnHide then
        self.scripts.OnHide(self)
    end
end
function Widget:IsShown() return self.shown end
function Widget:SetText(text) self.text = text or "" end
function Widget:GetText() return self.text end
function Widget:GetStringWidth() return #self.text * 8 end
function Widget:SetTexture(texture) self.texture = texture end
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:SetAlpha(alpha) self.alpha = alpha end
function Widget:GetWidth() return self.width end
function Widget:GetHeight() return self.height end
function Widget:GetFrameLevel() return 1 end
function Widget:Click(button) self.scripts.OnClick(self, button or "LeftButton") end

UIParent = NewWidget("UIParent")
UIParent.width, UIParent.height = 1600, 900
function CreateFrame(_, name, parent) return NewWidget(name, parent) end
local sent = {}
UISpecialFrames, SlashCmdList, StaticPopupDialogs = {}, {}, {}
GameTooltip = NewWidget("GameTooltip")
function hooksecurefunc() end
function SendAddonMessage(prefix, message) sent[#sent + 1] = { prefix, message } end
function UnitName() return "Tester" end
function UnitLevel() return 30 end
function GetSpellInfo(id) if id then return "Spell" .. id, "", "Interface\\Icons\\Test" end end
function UnitBuff() end
function GetActionInfo() end
function GetTime() return 100 end
function IsShiftKeyDown() return false end
tinsert = table.insert
function tContains(list, value) for _, v in ipairs(list) do if v == value then return true end end end
ACCEPT, CANCEL = "Accept", "Cancel"
local popup
function StaticPopup_Show(name, a) popup = { name = name, a = a } return popup end
DEFAULT_CHAT_FRAME = { AddMessage = function() end }
function geterrorhandler() return function() end end
function seterrorhandler() end

EmberforgeHalo_ForceEnabled = true      -- the tiers are off in game (Halo.lua, ENABLED); the window is still tested
for _, file in ipairs({ "Loader.lua", "Catalog.lua", "Art.lua", "Emberforge_Runes.lua", "Tiers.lua", "Halo.lua" }) do
    dofile(DIR .. file)
end
local debug, view = EmberforgeHalo_Debug, EmberforgeHalo_View
local function server(message) EmberforgeHalo_OnMessage(message) end
local function lastSent() return sent[#sent] and sent[#sent][1] .. " " .. sent[#sent][2] end

-- The catalog: Epics, Rares and Uncommons by id.
local info = debug.Info(1001)
check(info and info.rarity == "epic" and info.name == "Rune of the Open Guard" and info.spec == "Arms",
    "an Epic's words come from Tiers.lua")
info = debug.Info(1000000 + 133 * 8 + 0)
check(info and info.rarity == "uncommon" and info.name == "Rapid Fireball" and info.text:find("cast time"),
    "an Uncommon is its prefix on its ability (Rapid Fireball)")

-- /runes and the rune button open the Halo (the Legendary's panel stays shut).
debug.frame:Hide()
SlashCmdList["EMBERFORGERUNES"]()
check(debug.frame.shown and not EmberforgeLegendaryRunes.shown, "/runes opens the Halo")
EmberforgeRunes_Button:Click()
check(not debug.frame.shown, "the rune button closes it again")
EmberforgeRunes_Button:Click()
check(debug.frame.shown, "and opens it")

-- The server's state: 2 Epics (one dormant), a Rare, three Uncommons on two abilities.
local RAPID_FIREBALL, CRIT_FIREBALL = 1000000 + 133 * 8, 1000000 + 133 * 8 + 3
local PROVOKING_SLAM = 1000000 + 23922 * 8 + 5
server("S,30,5,10,10")
server("T,1001:1:0.1002:2:1.2001:1:0")
server(("T,%d:0:0.%d:0:0.%d:0:0"):format(RAPID_FIREBALL, CRIT_FIREBALL, PROVOKING_SLAM))
server("Z")
local sockets = debug.sockets
check(sockets.epic[1].rune == 1001 and sockets.epic[2].rune == 1002 and not sockets.epic[3].rune
    and sockets.rare[1].rune == 2001, "the runes sit in their sockets")
check(debug.ringLabels.epic.text:find("EPIC  2 / 5") and debug.ringLabels.rare.text:find("RARE  1 / 10"),
    "the rings count their runes")
check(debug.listTitle.text:find("2 / 10 ABILITIES") and debug.rows[1].name.text == "Fireball"
    and debug.rows[1].tags.text:find("Rapid") and debug.rows[2].name.text == "Shield Slam",
    "the Uncommons are listed by ability, their prefixes tagged")

-- Selecting: an Epic's words, a dormant one's reason, an empty socket, an ability's Uncommons.
sockets.epic[1]:Click()
check(debug.detail.name.text == "Rune of the Open Guard" and debug.detail.ability.text:find("Arms")
    and debug.detail.action.shown, "an Epic's words, and Release")
sockets.epic[2]:Click()
check(debug.detail.rarity.text:find("DORMANT") and debug.detail.note.text:find("no longer know"),
    "a dormant Epic says why")
sockets.epic[4]:Click()
check(debug.detail.name.text == "An empty socket" and not debug.detail.action.shown, "an empty socket explains itself")
debug.rows[1]:Click()
check(debug.detail.name.text == "Fireball" and debug.detail.text.text:find("Rapid")
    and debug.detail.text.text:find("Critical"), "an ability shows its Uncommons")

-- Releasing asks first, then tells the server.
debug.detail.action:Click()
check(popup and popup.name == "EMBERFORGE_RUNE_RELEASE", "releasing asks to confirm")
StaticPopupDialogs.EMBERFORGE_RUNE_RELEASE.OnAccept(nil, popup.data)
check(lastSent() == "EFRT RELA,133", "an ability's Uncommons are released together")
sockets.rare[1]:Click()
debug.detail.action:Click()
StaticPopupDialogs.EMBERFORGE_RUNE_RELEASE.OnAccept(nil, popup.data)
check(lastSent() == "EFRT REL,2001", "a Rare is released alone")

-- An offer: O shows its cards alone in the middle of the screen (the Halo stays shut); an Uncommon taken at once.
debug.frame:Hide()
server("S,30,5,10,10")
server("T,1001:1:0.1002:2:0.1003:3:0.1004:4:0.1005:5:0")
server(("Q,7,30,%d.2002.1006"):format(RAPID_FIREBALL + 1))
server("Z")
server("O")
local offer = debug.offer
check(not debug.frame.shown and offer.shown and offer:GetName() == "EmberforgeRuneOffer"
    and offer.cards[1].name.text == "Aggressive Fireball"
    and offer.cards[2].rarity.text == "RARE RUNE" and offer.cards[3].rarity.text == "EPIC RUNE",
    "an offer shows its three runes on their own")
check(offer.cards[1].fate.text == "Takes ability slot 1 of 10"
    and offer.cards[2].fate.text == "Fills Rare socket 1 of 10"
    and offer.cards[3].fate.text:find("Replaces one of your 5 epic runes"), "each card says what taking it does")
check(offer.eyebrow.text == "LEVEL 30 REWARD" and not offer.prev.shown, "one offer: no pager")
check(not offer.close and not offer.later and not tContains(UISpecialFrames, "EmberforgeRuneOffer"),
    "the offer can't be put off: no X, no Decide Later, no Escape")
local before = #sent
offer.skip:Click()
check(#sent == before and offer.skip.text.text == "CLICK AGAIN TO SKIP", "skipping asks first, on the button")
offer.skip:Click()
check(lastSent() == "EFRT SKIP,7" and offer.skip.text.text == "SKIP OFFER", "a second click skips the offer")
offer.cards[1]:Click()
check(lastSent() == "EFRT PICK,7,1", "an Uncommon is taken at once")

-- An Epic with every Epic socket full: a socket to replace is chosen first.
offer:Show()
offer.cards[3]:Click()
check(view.replace and not offer.shown and debug.frame.shown and debug.banner.text:find("Choose a socket"),
    "a full set of Epic sockets opens the Halo to ask which one to replace")
debug.frame:Hide()
check(not view.replace and offer.shown, "closing the Halo without a socket brings the offer back")
offer.cards[3]:Click()
sockets.epic[4]:Click()
check(lastSent() == "EFRT PICK,7,3,4" and not view.replace, "the chosen socket goes with the pick")

-- A held rune's socket shows its ability's icon.
check(sockets.epic[1].icon.alpha == 1 and sockets.epic[1].icon.texture == "Interface\\Icons\\Test"
    and sockets.epic[3].icon.alpha == 1, "held runes show their ability's icon")

-- The Legendary selected: its build, slot by slot.
EmberforgeRunes_State.rune, EmberforgeRunes_State.mods = 31, { 1, 0, 2 }
debug.frame:Show()
view.sel = { kind = "legend" }
EmberforgeHalo_Refresh()
check(debug.build.head.text == "YOUR BUILD  ·  2 / 3 CHOSEN" and debug.build[1].slot.text == "SUNSHARD"
    and debug.build[2].name.text == "Not chosen yet" and debug.build[3].text.text ~= "",
    "the Legendary's build is broken down under it")
view.sel = { kind = "epic", index = 1 }
EmberforgeHalo_Refresh()
check(debug.build.head.text == "" and debug.build[1].name.text == "", "and hidden for other runes")

-- The Legendary in the centre opens its own panel.
debug.frame:Show()
debug.legend:Click()
check(not debug.frame.shown, "the medallion leaves the Halo for the Legendary's panel")

-- Errors show at the bottom.
server("E,Your Epic sockets are full: choose one to replace.")
check(debug.note.text:find("sockets are full"), "the server's errors are shown")

print(failures == 0 and "All checks passed" or (failures .. " check(s) failed"))
os.exit(failures == 0 and 0 or 1)

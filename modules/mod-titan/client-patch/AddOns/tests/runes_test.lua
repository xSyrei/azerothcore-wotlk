-- Offline test for Emberforge_Runes: loads the addon against a mocked WoW 3.3.5 API and plays the panel the way
-- the server drives it (see src/runes.cpp), stepping its animation. Run: lua5.1 runes_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Runes/"
local failures = 0
local function check(ok, what)
    print((ok and "ok    " or "FAIL  ") .. what)
    if not ok then
        failures = failures + 1
    end
end

-- Mock widgets: any capitalized method is a no-op unless defined below; state the test reads is recorded.
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
function Widget:CreateTexture()
    local texture = NewWidget(nil, self)
    self.textures = self.textures or {}
    self.textures[#self.textures + 1] = texture
    return texture
end
function Widget:CreateFontString() return NewWidget(nil, self) end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:Show()
    local wasShown = self.shown
    self.shown = true
    if not wasShown and self.scripts.OnShow then
        self.scripts.OnShow(self)
    end
end
function Widget:Hide()
    local wasShown = self.shown
    self.shown = false
    if wasShown and self.scripts.OnHide then
        self.scripts.OnHide(self)
    end
end
function Widget:IsShown() return self.shown end
function Widget:SetText(text) self.text = text or "" end
function Widget:GetText() return self.text end
function Widget:GetStringWidth() return #self.text * 8 end
function Widget:SetTexture(texture) self.texture = texture end
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:SetDesaturated(on) self.desaturated = on end
function Widget:SetAlpha(alpha) self.alpha = alpha end
function Widget:GetWidth() return self.width end
function Widget:GetHeight() return self.height end
function Widget:SetFrameLevel(level) self.level = level end
function Widget:GetFrameLevel() return self.level or 1 end
function Widget:GetParent() return self.parent end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end

UIParent = NewWidget("UIParent")
UIParent.width, UIParent.height = 1600, 900
local frames = {}
function CreateFrame(_, name, parent)
    local widget = NewWidget(name, parent)
    frames[#frames + 1] = widget
    return widget
end
local sent = {}
UISpecialFrames, SlashCmdList = {}, {}
GameTooltip = NewWidget("GameTooltip")
-- The tooltip's aura methods and the hooks on them (only on tables: the action bar hooks stay unset here).
local tooltipLines = {}
function GameTooltip:SetUnitBuff() end
function GameTooltip:SetUnitDebuff() end
function GameTooltip:SetUnitAura() end
function GameTooltip:AddLine(text) tooltipLines[#tooltipLines + 1] = text end
function hooksecurefunc(target, method, hook)
    if type(target) ~= "table" then
        return
    end
    local original = target[method]
    target[method] = function(...)
        original(...)
        hook(...)
    end
end
local auras = {}                                -- index -> spell id, the aura tooltip's
function UnitAura(_, index)
    if auras[index] then
        return "Aura", "", "", 0, nil, 0, 0, "player", nil, nil, auras[index]
    end
end
function SendAddonMessage(prefix, message) sent[#sent + 1] = { prefix, message } end
function UnitName() return "Tester" end
function UnitLevel() return 12 end
local SPELLS = { [26573] = "Consecration", [90730] = "Rune of the Sanctified Paragon",
    [90111] = "Throw Glaive", [90701] = "Fel Command", [90116] = "Metamorphosis",
    [686] = "Shadow Bolt", [29722] = "Incinerate", [688] = "Summon Imp", [697] = "Summon Voidwalker" }
function GetSpellInfo(id) return SPELLS[id] end
tinsert = table.insert
StaticPopupDialogs = {}
local popup
function StaticPopup_Show(name, a, b) popup = { name = name, a = a, b = b } return popup end
ACCEPT, CANCEL = "Accept", "Cancel"
-- The player's buffs (name -> stacks) and the action bars (button -> spell id).
local buffs, actions = {}, {}
function UnitBuff(_, name)
    if buffs[name] then
        return name, "", "", buffs[name]
    end
end
function GetActionInfo(slot)
    if actions[slot] then
        return "spell", 40 + slot, "spell", actions[slot]       -- 3.3.5: a spellbook index, the spell's id fourth
    end
end
for index = 1, 12 do
    local button = NewWidget("ActionButton" .. index)
    button.action = index
    button.GetName = function() return "ActionButton" .. index end
    _G["ActionButton" .. index .. "Icon"] = button:CreateTexture(nil, "BORDER")
end

-- The loader first: until the rest loads, /runes says why the panel is missing and asks for the state.
local chat = {}
DEFAULT_CHAT_FRAME = { AddMessage = function(_, text) chat[#chat + 1] = text end }
local handler = function() end
function geterrorhandler() return handler end
function seterrorhandler(fn) handler = fn end
dofile(DIR .. "Loader.lua")
handler("Interface\\AddOns\\Emberforge_Runes\\Emberforge_Runes.lua:12: attempt to index nil")
SlashCmdList["EMBERFORGERUNES"]()
check(chat[1] and chat[1]:find("did not load") and chat[2] and chat[2]:find("Emberforge_Runes.lua:12", 1, true)
    and sent[#sent][2] == "STATE", "before the panel loads, /runes reports its load error and asks for the state")
dofile(DIR .. "Catalog.lua")
dofile(DIR .. "Art.lua")
dofile(DIR .. "Emberforge_Runes.lua")
local frame = EmberforgeLegendaryRunes
local view, debug = EmberforgeRunes_View, EmberforgeRunes_Debug
local function server(message) EmberforgeRunes_OnMessage(message) end
local function lastSent() return sent[#sent] and sent[#sent][2] end
local function run(seconds)
    for _ = 1, math.floor(seconds / 0.02 + 0.5) do
        debug.Step(0.02)
    end
end
frame:Hide()

-- The catalog and the art: only the runes the server builds (the first draft's placeholders are gone).
local function ById(id)
    for _, rune in ipairs(EmberforgeRunesCatalog) do
        if rune.id == id then
            return rune
        end
    end
end
check(#EmberforgeRunesCatalog == 13 and EmberforgeRunesCatalog[1].id == 21 and not ById(1) and not ById(20)
    and ById(22).name == "Rune of the Sanctified Paragon" and ById(23).name == "Rune of the Stormbound Conduit"
    and ById(24).name == "Rune of Echoing Thunder" and ById(25).name == "Rune of the Wellspring"
    and ById(26).name == "Rune of the Wandering Mist", "the catalog holds only the runes the server builds")
for _, rune in ipairs(EmberforgeRunesCatalog) do
    check(EmberforgeRunesArt.runes[rune.id] and EmberforgeRunesArt.runes[rune.id].built, "built: " .. rune.name)
end
-- The five after them: each built, with its three slots, nine options and three builds.
for id, name in pairs({ [27] = "Rune of the Threefold Grave", [28] = "Rune of the Unbroken Onslaught",
    [29] = "Rune of the Dawnbound Crusader", [30] = "Rune of the Hollow Congregation",
    [31] = "Rune of the Shattered Sun", [32] = "Rune of the Spectral Blade", [33] = "Rune of the Wild Hunt" }) do
    local entry, rune = EmberforgeRunesArt.runes[id], ById(id)
    check(rune.name == name and entry.built and #entry.slots == 3 and #entry.texts == 9 and #rune.mods == 9
        and #rune.builds == 3, "built with its words: " .. name)
    for _, build in ipairs(rune.builds) do
        for _, mod in ipairs(build.mods) do
            local found = false
            for _, option in ipairs(rune.mods) do
                found = found or option.name == mod
            end
            check(found, name .. ": build " .. build.name .. " names a real modifier (" .. mod .. ")")
        end
    end
end
for _, id in ipairs({ 25, 26 }) do
    local entry = EmberforgeRunesArt.runes[id]
    check(entry.built and #entry.texts == 9 and #ById(id).mods == 9
        and ById(id).roles[1] == "HEALER", "a healer's rune with its words: rune " .. id)
end
local thunder = EmberforgeRunesArt.runes[24]
check(thunder.built and thunder.slots[3] == "Resonance" and #thunder.texts == 9
    and ById(24).roles[1] == "TANK", "Art.lua has Echoing Thunder's slots and words; a tank's rune")
local storm = EmberforgeRunesArt.runes[23]
check(storm.built and storm.slots[2] == "Lightning Shield" and #storm.texts == 9 and #ById(23).mods == 9,
    "Art.lua has the Stormbound Conduit's slots (Arcs, Lightning Shield, Surge) and its words")
local paragon = EmberforgeRunesArt.runes[22]
check(paragon.built and paragon.slots[1] == "Presence" and paragon.slots[3] == "Covenant" and #paragon.texts == 9,
    "Art.lua has the Sanctified Paragon's slots (Presence, Manifestation, Covenant) and its words")
local fel = EmberforgeRunesArt.runes[21]
check(fel.built and fel.asks[2] == "What your Chaos Bolt Volley does" and #fel.texts == 9,
    "Art.lua has Fel Commandment's questions and option words")

-- Below level 10: nothing opens; /runes says when.
server("R,21,-")
server("S,8,10,1,0,0,0,0,-,0")
check(not frame:IsShown(), "below level 10 the panel stays closed")
SlashCmdList.EMBERFORGERUNES()
check(frame:IsShown() and view.mode == "locked" and debug.head.subtitle.text == "Legendary Runes unlock at level 10.",
    "/runes shows when the runes unlock")
check(lastSent() == "STATE", "opening asks the server for the state")
frame:Hide()

-- Level 10, no rune: the panel opens by itself once, the suggested cards rising in.
server("S,10,10,1,0,0,0,0,21,1")
check(frame:IsShown() and view.mode == "offers" and view.phase == "intro", "unlocked with no rune: the offers open")
run(1.6)
check(view.phase == "offer" and debug.cards[1].rune and debug.cards[1].rune.id == 21 and not debug.cards[2].rune,
    "one card: the rune the Hero's abilities fit, settled")
check(debug.browseAsk.target == 1, "Browse all is offered")
frame:Hide()
server("S,10,10,1,0,0,0,0,21,1")
check(not frame:IsShown(), "it opens by itself only once a session")

-- Browse all: every rune in one list, searched and filtered.
EmberforgeRunes_Open()
debug.browseAsk.pill:Click()
run(0.7)
local rows, browse = debug.rows, debug.browse
local function listed()
    local ids = {}
    for _, rune in ipairs(browse.list) do
        ids[#ids + 1] = rune.id
    end
    return table.concat(ids, ",")
end
check(view.mode == "browse" and view.phase == "browse" and debug.browser.target == 1 and #browse.list == 13
    and rows[1].shown and rows[5].shown and debug.browser.count.text == "13 runes",
    "Browse all lists every rune, five rows at a time")
check(rows[1].rune.id == 21 and rows[1].status.text == "Usable now" and rows[2].status.text == "Coming soon",
    "the usable rune comes first; each row says where it stands")
debug.browser.scripts.OnMouseWheel(debug.browser, -1)
check(browse.offset == 1 and rows[1].rune == browse.list[2], "the mouse wheel scrolls the list")
debug.browser.scripts.OnMouseWheel(debug.browser, -20)
check(browse.offset == 8, "it stops at the end")
local search = debug.search
search.text = "lightning"
search.scripts.OnTextChanged(search)
check(browse.offset == 0 and rows[1].rune.id == 23 and not search.hint.shown, "the search finds a rune by its name")
search.text = "warlock glaive"
search.scripts.OnTextChanged(search)
check(listed() == "21" and debug.browser.count.text == "1 of 13 runes", "and by the abilities it uses")
search.text = "soothing heal"
search.scripts.OnTextChanged(search)
check(listed() == "26", "every word must match: Soothing Mist's healing rune")
search.text = "rogue"
search.scripts.OnTextChanged(search)
check(listed() == "32", "by keyword: the rogue's rune")
search.text = "kill command"
search.scripts.OnTextChanged(search)
check(listed() == "33", "by ability: Kill Command's rune")
search.text = "xyzzy"
search.scripts.OnTextChanged(search)
check(#browse.list == 0 and not rows[1].shown and debug.empty.target == 1
    and debug.empty.text.text:find("No rune matches"), "nothing found says so")
search.text = ""
search.scripts.OnTextChanged(search)
check(search.hint.shown and #browse.list == 13, "an empty search lists every rune again")
debug.chips[3]:Click()
local healers = true
for _, rune in ipairs(browse.list) do
    local heals = false
    for _, role in ipairs(rune.roles or {}) do
        heals = heals or role == "HEALER"
    end
    healers = healers and heals
end
check(healers and #browse.list >= 3 and debug.chips[3].on and not debug.chips[1].on, "the Healing filter")
debug.chips[1]:Click()
debug.chips[5]:Click()
check(listed() == "21" and debug.chips[5].on, "Usable now: only the runes nothing is missing for")
debug.chips[5]:Click()
server("R,21,90111/688+697")
check(rows[1].status.text:find("Needs Throw Glaive"), "a rune missing abilities says what it needs")
rows[1].choose:Click()
check(popup and popup.name == "EMBERFORGE_RUNE_BIND_MISSING"
    and popup.b == "Throw Glaive, and Summon Imp or Summon Voidwalker",
    "a rune missing abilities warns, naming them, before it can be bound")
StaticPopupDialogs.EMBERFORGE_RUNE_BIND_MISSING.OnAccept(nil, 21)
check(lastSent() == "PICK,21,1", "binding it anyway says so to the server")
view.pending = nil
server("R,21,-")

-- Preview: the build as if bound, its picks tried here only; Back returns to the list as it was.
search.text = "fel"
search.scripts.OnTextChanged(search)
rows[1].preview:Click()
check(view.mode == "preview" and view.phase == "build" and debug.options[1].name.text == "Infernal Pact",
    "previewing shows its medallion and first options")
local sentBefore = #sent
debug.options[2]:Click()
run(0.7)
check(#sent == sentBefore and view.picks[1] == 2, "a pick in the preview is tried, not sent")
debug.previewBar.back:Click()
run(0.7)
check(view.mode == "browse" and view.phase == "browse" and not view.preview and browse.query == "fel"
    and rows[1].rune.id == 21, "Back returns to the list, its search kept")

-- Binding asks first; the server's bind fades the list and the chosen rune's card comes in and erupts.
rows[1].choose:Click()
check(popup and popup.name == "EMBERFORGE_RUNE_BIND" and popup.a == "Rune of Fel Commandment",
    "choosing asks to confirm the bind")
StaticPopupDialogs.EMBERFORGE_RUNE_BIND.OnAccept(nil, 21)
check(lastSent() == "PICK,21", "accepting sends PICK,21")
server("N,21,1,3,686+29722")
server("S,10,10,1,21,0,0,0,21,0")
check(view.phase == "arrive" and debug.browser.target == 0 and debug.cards[1].rune.id == 21,
    "bound: the list fades away and the chosen rune's card comes in")
run(0.7)
check(view.phase == "erupt" and view.chosen == 1, "then it erupts")
run(3.7)
check(view.medal ~= nil and view.phase == "split", "the card condenses into the medallion, which splits")
run(1.6)
check(view.phase == "build" and view.active == 1 and debug.choose.target == 1, "the slots, and the first choice")
local options = debug.options
check(options[1].name.text == "Infernal Pact" and options[1].text.text:find("twice as often")
    and options[1].ability.text == "YOUR DEMON", "the options in the mockup's words")
check(options[3].missing and options[3].ability.text:find("NEEDS SHADOW BOLT OR INCINERATE"),
    "Dreadfire Discipline, missing Shadow Bolt or Incinerate, says so")
local before = #sent
options[3]:Click()
check(#sent == before and debug.note.text.text:find("needs Shadow Bolt or Incinerate"), "a greyed option sends nothing")

-- A pick streams into its slot, and the next slot asks.
options[1]:Click()
check(lastSent() == "MOD,1,1", "picking Infernal Pact sends MOD,1,1")
run(0.6)
check(view.picks[1] == 1 and view.active == 2 and debug.slots[1].pick.text == "Infernal Pact",
    "the slot takes it; Manifestation is next")
server("S,10,10,1,21,1,0,0,21,0")
options[3]:Click()
run(0.6)
server("S,10,10,1,21,1,3,0,21,0")
options[2]:Click()
run(0.6)
server("S,10,10,1,21,1,3,2,21,0")
check(view.active == nil and debug.done.target == 1 and debug.done.name.text == "The Legion's Master",
    "the three picks of a doc build: complete, named")
check(debug.done.hint.text == "Click on a modifier to change it", "the done view says a modifier can be changed")

-- A done slot can be chosen again.
debug.slots[2]:Click()
check(view.active == 2 and debug.choose.target == 1 and options[3].picked, "a done slot opens its choices again")
server("E,Modifiers can't be changed in combat.")
check(debug.note.text.text:find("in combat"), "a refusal shows in the note")

-- Reopened, bound: the medallion straight away.
frame:Hide()
EmberforgeRunes_Open()
check(view.mode == "build" and view.phase == "build" and view.medal.split == 1, "reopened: the build, no eruption")
check(debug.slots[1].small.texture:find("SlotInvocation$") and debug.choose.labels[1].texture:find("ChooseInvocation$"),
    "Fel Commandment's slots carry the doc's names")
frame:Hide()

-- Another specialization bound to the Sanctified Paragon: its slots carry its own names.
server("R,22,-")
server("S,10,10,2,22,0,0,0,22,0")
EmberforgeRunes_Open()
run(0.2)
check(view.mode == "build" and debug.slots[1].small.texture:find("SlotPresence$")
    and debug.slots[3].small.texture:find("SlotCovenant$"), "the Sanctified Paragon's slots: Presence, Covenant")
check(debug.choose.labels[1].texture:find("ChoosePresence$") and debug.options[1].name.text == "Boundless Sanctity",
    "choosing its Presence: Boundless Sanctity first")
frame:Hide()

-- The Stormbound Conduit: slot names with a space find their labels (baked without it).
server("R,23,-")
server("S,10,10,3,23,0,0,0,23,0")
EmberforgeRunes_Open()
run(0.2)
check(view.mode == "build" and debug.slots[2].small.texture:find("SlotLightningShield$")
    and debug.slots[3].small.texture:find("SlotSurge$"), "the Stormbound Conduit's slots: Lightning Shield...")
check(debug.choose.labels[1].texture:find("ChooseArcs$")
    and debug.options[1].name.text == "Forked Tempest", "choosing its Arcs: Forked Tempest first")
frame:Hide()
server("S,10,10,1,21,1,3,1,21,0")

-- The glow: Throw Glaive on button 3 glows at 5 Fel Command, with the Felguard mark in Metamorphosis.
actions[3] = 90111
buffs["Fel Command"] = 4
local function fire(event, unit)
    for _, widget in ipairs(frames) do
        if widget.scripts.OnEvent then
            widget.scripts.OnEvent(widget, event, unit)
        end
    end
end
local function volleyShown(button)
    return _G[button:GetName() .. "Icon"].texture == "Interface\\Icons\\Ability_Warlock_ChaosBolt"
end
fire("UNIT_AURA", "player")
check(not volleyShown(ActionButton3), "4 stacks: no glow")
buffs["Fel Command"] = 5
fire("UNIT_AURA", "player")
check(volleyShown(ActionButton3) and not volleyShown(ActionButton4),
    "5 stacks: Throw Glaive's button (only it) shows Chaos Bolt Volley")
buffs["Fel Command"] = nil
fire("UNIT_AURA", "player")
check(not volleyShown(ActionButton3), "spent: the glow goes")

-- Consecration on button 4 shows the Sanctified Paragon's icon while that rune is bound (no glow).
local function iconShown(button, path)
    return _G[button:GetName() .. "Icon"].texture == path
end
local PARAGON_ICON = "Interface\\Icons\\Ability_Rune_SanctifiedParagon"
actions[4] = 26573
fire("UNIT_AURA", "player")
check(not iconShown(ActionButton4, PARAGON_ICON), "Consecration keeps its own icon without the rune")
server("S,10,5,1,22,0,0,0,-,0")
check(iconShown(ActionButton4, PARAGON_ICON) and not iconShown(ActionButton3, PARAGON_ICON),
    "with the Sanctified Paragon bound, Consecration's button shows the rune's icon")
server("S,10,5,1,21,1,3,1,21,0")
check(not iconShown(ActionButton4, PARAGON_ICON), "the rune gone, Consecration's own icon is back")
actions[4] = nil

-- The rune button: from the unlock level (5); unbound, its icon turns through the runes; bound, it settles.
local button = EmberforgeRunes_Button
server("S,4,5,1,0,0,0,0,-,0")
check(not button.shown, "below level 5 the rune button is hidden")
server("S,5,5,1,0,0,0,0,-,0")
check(button.shown and button.turning ~= nil and button.glow.on, "at level 5, unbound: shown, glowing, turning")
local first = button.turning.index
button.scripts.OnUpdate(button, 2.6)
button.scripts.OnUpdate(button, 0.7)
check(button.turning.index ~= first, "its icon turns to the next rune's")
server("S,10,5,1,21,1,3,1,21,0")
check(button.shown and button.turning == nil and not button.glow.on, "bound: it settles on the rune, no glow")

-- An error in the action bars' update (a stale spellbook slot as a level's spells change) never keeps the rune
-- button hidden; a level up asks for the state again, and once more 3 sec later.
local glowWas = EmberforgeRunes_UpdateGlow
EmberforgeRunes_UpdateGlow = function() error("Invalid spell slot") end
button:Hide()
server("S,11,5,1,21,1,3,1,21,0")
check(button.shown, "the rune button shows even when the bars' update fails")
EmberforgeRunes_UpdateGlow = glowWas
local before = #sent
fire("PLAYER_LEVEL_UP", "12")
check(#sent == before + 1 and lastSent() == "STATE", "a level up asks for the state")
EmberforgeRunes_Asking.scripts.OnUpdate(EmberforgeRunes_Asking, 3.1)
check(#sent == before + 2 and lastSent() == "STATE", "and once more when the level has settled")

-- A rune aura's amount (V): the server's, added to its tooltip; a stale one is replaced, others left alone.
server("V,90843,A,412")
auras[1], auras[2] = 90843, 26573
GameTooltip:SetUnitBuff("player", 1)
check(tooltipLines[#tooltipLines] == "Absorbs 412 damage.", "an absorb's tooltip shows the server's amount")
server("V,90841,D,57")
auras[3] = 90841
GameTooltip:SetUnitDebuff("target", 3)
check(tooltipLines[#tooltipLines] == "57 damage every second.", "a damage over time shows its tick")
local count = #tooltipLines
GameTooltip:SetUnitBuff("player", 2)
check(#tooltipLines == count, "another aura's tooltip is left alone")

print(failures == 0 and "All checks passed" or (failures .. " check(s) failed"))
os.exit(failures == 0 and 0 or 1)

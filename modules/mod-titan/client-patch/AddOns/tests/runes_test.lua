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
function SendAddonMessage(prefix, message) sent[#sent + 1] = { prefix, message } end
function UnitName() return "Tester" end
function UnitLevel() return 12 end
local SPELLS = { [90111] = "Throw Glaive", [90701] = "Fel Command", [90116] = "Metamorphosis",
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
        return "spell", actions[slot]
    end
end
for index = 1, 12 do
    NewWidget("ActionButton" .. index).action = index
end

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

-- The catalog and the art: the doc's 21 runes, the Sanctified Paragon and the Stormbound Conduit; Fel
-- Commandment's build view in the mockup's words.
check(#EmberforgeRunesCatalog == 26 and EmberforgeRunesCatalog[22].name == "Rune of the Sanctified Paragon"
    and EmberforgeRunesCatalog[23].name == "Rune of the Stormbound Conduit"
    and EmberforgeRunesCatalog[24].name == "Rune of Echoing Thunder"
    and EmberforgeRunesCatalog[25].name == "Rune of the Wellspring"
    and EmberforgeRunesCatalog[26].name == "Rune of the Wandering Mist",
    "the catalog holds the doc's 21 runes, then the Paragon, the Conduit, Echoing Thunder, the Wellspring, the Mist")
for _, id in ipairs({ 25, 26 }) do
    local entry = EmberforgeRunesArt.runes[id]
    check(entry.built and #entry.texts == 9 and #EmberforgeRunesCatalog[id].mods == 9
        and EmberforgeRunesCatalog[id].roles[1] == "HEALER", "a healer's rune with its words: rune " .. id)
end
local thunder = EmberforgeRunesArt.runes[24]
check(thunder.built and thunder.slots[3] == "Shield Resonance" and #thunder.texts == 9
    and EmberforgeRunesCatalog[24].roles[1] == "TANK", "Art.lua has Echoing Thunder's slots and words; a tank's rune")
local storm = EmberforgeRunesArt.runes[23]
check(storm.built and storm.slots[2] == "Arc Pattern" and #storm.texts == 9 and #EmberforgeRunesCatalog[23].mods == 9,
    "Art.lua has the Stormbound Conduit's slots (Conductivity, Arc Pattern, Lightning Shield) and its words")
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

-- Browse all: a page of three; the unbuilt ones shake and say they are coming.
EmberforgeRunes_Open()
debug.browseAsk.pill:Click()
run(1.6)
check(view.mode == "browse" and debug.cards[1].rune.id == 1 and debug.cards[3].rune.id == 3,
    "Browse all shows the first page")
check(debug.cards[1].status.text == "Coming soon", "an unbuilt rune says Coming soon")
debug.hits[1]:Click()
check(debug.cards[1].shake and debug.note.text.text:find("coming soon"), "choosing it shakes the card and says why")
for _ = 1, 6 do
    debug.pager.next:Click()
end
run(1.6)
check(view.page == 7 and debug.cards[3].rune.id == 21, "the last page ends with Fel Commandment")
check(debug.cards[3].roles[1].shown and not debug.cards[3].roles[2].shown
    and debug.cards[1].roles[1].shown, "each card shows its roles (Fel Commandment: damage only)")
server("R,21,90111/688+697")
debug.hits[3]:Click()
check(popup and popup.name == "EMBERFORGE_RUNE_BIND_MISSING"
    and popup.b == "Throw Glaive, and Summon Imp or Summon Voidwalker",
    "a rune missing abilities warns, naming them, before it can be bound")
StaticPopupDialogs.EMBERFORGE_RUNE_BIND_MISSING.OnAccept(nil, 21)
check(lastSent() == "PICK,21,1", "binding it anyway says so to the server")
view.pending = nil

-- Preview: the build as if bound, its picks tried here only; Back returns to the page.
check(debug.previews[3].on and not debug.previews[1].on, "Preview under a rune with its build words only")
debug.previews[3]:Click()
check(view.mode == "preview" and view.phase == "build" and debug.options[1].name.text == "Infernal Pact",
    "previewing shows its medallion and first options")
local sentBefore = #sent
debug.options[2]:Click()
run(0.7)
check(#sent == sentBefore and view.picks[1] == 2, "a pick in the preview is tried, not sent")
debug.previewBar.back:Click()
run(1.6)
check(view.mode == "browse" and view.page == 7 and not view.preview, "Back returns to the page it came from")
server("R,21,-")

-- Binding asks first; the server's bind sets the eruption off.
debug.hits[3]:Click()
check(popup and popup.name == "EMBERFORGE_RUNE_BIND" and popup.a == "Rune of Fel Commandment",
    "choosing asks to confirm the bind")
StaticPopupDialogs.EMBERFORGE_RUNE_BIND.OnAccept(nil, 21)
check(lastSent() == "PICK,21", "accepting sends PICK,21")
server("N,21,1,3,686+29722")
server("S,10,10,1,21,0,0,0,21,0")
check(view.phase == "erupt" and view.chosen == 3, "bound: the chosen card erupts")
run(1.2)
check(debug.cards[1].noise ~= nil, "the other cards burn")
run(2.5)
check(view.medal ~= nil and view.phase == "split", "the card condenses into the medallion, which splits")
run(1.6)
check(view.phase == "build" and view.active == 1 and debug.choose.target == 1, "the slots, and the first choice")
local options = debug.options
check(options[1].name.text == "Infernal Pact" and options[1].text.text:find("once every 2 sec")
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
options[1]:Click()
run(0.6)
server("S,10,10,1,21,1,3,1,21,0")
check(view.active == nil and debug.done.target == 1 and debug.done.name.text == "Demon Commander",
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
check(view.mode == "build" and debug.slots[2].small.texture:find("SlotArcPattern$")
    and debug.slots[3].small.texture:find("SlotLightningShield$"), "the Stormbound Conduit's slots: Arc Pattern...")
check(debug.choose.labels[1].texture:find("ChooseConductivity$")
    and debug.options[1].name.text == "Persistent Current", "choosing its Conductivity: Persistent Current first")
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
    for _, texture in ipairs(button.textures or {}) do
        if texture.texture == "Interface\\Icons\\Ability_Warlock_ChaosBolt" and texture.shown then
            return true
        end
    end
    return false
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

print(failures == 0 and "All checks passed" or (failures .. " check(s) failed"))
os.exit(failures == 0 and 0 or 1)

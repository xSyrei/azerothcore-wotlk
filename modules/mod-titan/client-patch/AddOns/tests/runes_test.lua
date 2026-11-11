-- Offline test for Emberforge_Runes: loads the addon against a mocked WoW 3.3.5 API and plays the panel the way
-- the server drives it (see src/runes.cpp). Run: lua5.1 runes_test.lua
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
local function NewWidget(name)
    local widget = setmetatable({ shown = true, scripts = {}, text = "", width = 36, height = 36, name = name },
        Widget)
    if name then
        _G[name] = widget
    end
    return widget
end
function Widget:GetName() return self.name end
function Widget:CreateTexture()
    local texture = NewWidget()
    self.textures = self.textures or {}
    self.textures[#self.textures + 1] = texture
    return texture
end
function Widget:CreateFontString() return NewWidget() end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:Show()
    local wasShown = self.shown
    self.shown = true
    if not wasShown and self.scripts.OnShow then
        self.scripts.OnShow(self)
    end
end
function Widget:Hide() self.shown = false end
function Widget:IsShown() return self.shown end
function Widget:SetText(text) self.text = text end
function Widget:GetText() return self.text end
function Widget:SetTexture(texture) self.texture = texture end
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:SetVertexColor(r, g, b) self.vertex = { r, g, b } end
function Widget:SetDesaturated(on) self.desaturated = on end
function Widget:GetWidth() return self.width end
function Widget:GetHeight() return self.height end
function Widget:GetParent() return self.parent end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end

UIParent = NewWidget("UIParent")
local frames = {}
function CreateFrame(_, name, parent)
    local widget = NewWidget(name)
    widget.parent = parent
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
function StaticPopup_Show(name, a) popup = { name = name, a = a } return popup end
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
dofile(DIR .. "Emberforge_Runes.lua")
local frame = EmberforgeLegendaryRunes
local state, view = EmberforgeRunes_State, EmberforgeRunes_View
local function server(message) EmberforgeRunes_OnMessage(message) end
local function lastSent() return sent[#sent] and sent[#sent][2] end

local function upvalues(fn)
    local values, i = {}, 1
    while true do
        local name, value = debug.getupvalue(fn, i)
        if not name then
            break
        end
        values[name] = value
        i = i + 1
    end
    return values
end
local panel = upvalues(EmberforgeRunes_Open)
local parts = upvalues(panel.Refresh)
local cards = upvalues(parts.RefreshOffers).cards
local rows = upvalues(parts.RefreshBrowse).rows
local columns = upvalues(parts.RefreshBuild).columns
local footer = parts.footer
frame:Hide()

-- The catalog: 21 runes, 9 modifiers each, Fel Commandment last.
check(#EmberforgeRunesCatalog == 21, "the catalog holds the doc's 21 runes")
local fel = EmberforgeRunesCatalog[21]
check(fel.name == "Rune of Fel Commandment" and #fel.mods == 9 and fel.mods[3].name == "Dreadfire Discipline",
    "Fel Commandment is rune 21, its modifiers in slot order")

-- Below level 10: nothing opens, the panel says when.
server("R,21,-")
server("S,8,10,1,0,0,0,0,-,0")
check(not frame:IsShown(), "below level 10 the panel stays closed")
SlashCmdList.EMBERFORGERUNES()
check(frame:IsShown() and parts.locked.text == "Legendary Runes unlock at level 10.", "/runes says when it unlocks")
check(lastSent() == "STATE", "opening asks the server for the state")
frame:Hide()

-- Level 10, no rune: the panel opens by itself once, with the offers.
server("S,10,10,1,0,0,0,0,21,1")
check(frame:IsShown() and view.page == "offers", "unlocked with no rune: the panel opens on the offers")
check(cards[1].shown and cards[1].name.text == "Rune of Fel Commandment" and not cards[2].shown,
    "one card: the rune the Hero's abilities fit")
frame:Hide()
server("S,10,10,1,0,0,0,0,21,1")
check(not frame:IsShown(), "it opens by itself only once a session")

-- Browse all: every rune; Fel Commandment available, the rest coming.
EmberforgeRunes_Open()
view.page = "browse"
panel.Refresh()
check(view.page == "browse" and rows[1].shown and rows[1].status.text == "Coming soon",
    "Browse all lists every rune; the unbuilt ones say Coming soon")
view.browseOffset = 12
panel.Refresh()
check(rows[9].name.text == "Rune of Fel Commandment" and rows[9].status.text == "Choose",
    "scrolled down: Fel Commandment can be chosen")
-- Missing an ability: greyed, naming it.
server("R,21,90111/688+697")
panel.Refresh()
check(rows[9].status.text == "Needs Throw Glaive, and Summon Imp or Summon Voidwalker",
    "a rune missing abilities names them")
server("R,21,-")
panel.Refresh()

-- Binding asks first.
rows[9]:Click()
check(popup and popup.name == "EMBERFORGE_RUNE_BIND" and popup.a == "Rune of Fel Commandment",
    "choosing asks to confirm the bind")
StaticPopupDialogs.EMBERFORGE_RUNE_BIND.OnAccept(nil, 21)
check(lastSent() == "PICK,21", "accepting sends PICK,21")

-- Bound: the build view, its three slots.
view.page = "offers"
server("N,21,1,3,686+29722")
server("S,10,10,1,21,0,0,0,21,0")
check(view.page == "build" and columns[1].options[1].name.text == "Infernal Pact"
    and columns[3].options[3].name.text == "Pact of Ruin", "bound: the build view shows its nine modifiers")
check(not columns[1].options[3].enabled and columns[1].options[3].text.text:find("Needs Shadow Bolt or Incinerate"),
    "Dreadfire Discipline, missing Shadow Bolt or Incinerate, is greyed and says so")
columns[1].options[3]:Click()
check(lastSent() == "PICK,21", "a greyed modifier sends nothing")
columns[1].options[1]:Click()
check(lastSent() == "MOD,1,1", "picking Infernal Pact sends MOD,1,1")
check(footer.text:find("Choose your Invocation"), "the footer asks for the first empty slot")

-- The three picks of a build: complete, named.
server("S,10,10,1,21,1,3,1,21,0")
check(footer.text == "Your rune is complete: Demon commander.", "a doc build is named when complete")
server("S,10,10,1,21,1,2,1,21,0")
check(footer.text == "Your rune is complete.", "other picks: complete, no name")
server("E,Modifiers can't be changed in combat.")
check(footer.text:find("in combat"), "a refusal shows in the footer")

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

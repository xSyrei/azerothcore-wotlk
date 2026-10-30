-- Offline test for Emberforge_Classless: loads the addon against a mocked WoW 3.3.5 API and plays a
-- session the way the server drives it (see src/classless.cpp). Run: lua5.1 classless_test.lua
local ADDON = (arg and arg[1]) or "../Emberforge_Classless/Emberforge_Classless.lua"
local failures = 0
local function check(ok, what)
    print((ok and "ok    " or "FAIL  ") .. what)
    if not ok then
        failures = failures + 1
    end
end

-- Mock widgets: any method is a no-op unless defined below; state the test reads is recorded.
local Widget = {}
Widget.__index = function(self, key)
    local method = rawget(Widget, key)
    if method then
        return method
    end
    -- Unknown methods (capitalized, like WoW's) do nothing; unknown fields are nil.
    if type(key) == "string" and key:match("^[A-Z]") then
        return function() end
    end
end
local function NewWidget(name)
    local widget = setmetatable({ shown = true, scripts = {}, text = "" }, Widget)
    if name then
        _G[name] = widget
    end
    return widget
end
function Widget:CreateTexture() return NewWidget() end
function Widget:CreateFontString() return NewWidget() end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:GetScript(event) return self.scripts[event] end
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
function Widget:SetTexCoord(...) self.coords = { ... } end
function Widget:SetVertexColor(...) self.vertex = { ... } end
function Widget:SetAlpha(alpha) self.alpha = alpha end
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:SetDesaturated(desaturated) self.desaturated = desaturated end
function Widget:Enable() self.enabled = true end
function Widget:Disable() self.enabled = false end
function Widget:GetChecked() return self.checked end
function Widget:IsOwned(owner) return self.owner == owner end
function Widget:SetOwner(owner) self.owner = owner end
function Widget:SetHyperlink(link) self.link = link end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end

UIParent = NewWidget("UIParent")
function CreateFrame(_, name, _, template)
    local widget = NewWidget(name)
    if name and template == "UICheckButtonTemplate" then
        NewWidget(name .. "Text")
    end
    return widget
end

local sent, popup, bindings = {}, nil, {}
local playerLevel = 1
UISpecialFrames, StaticPopupDialogs, SlashCmdList = {}, {}, {}
RAID_CLASS_COLORS = { MAGE = { r = 0.41, g = 0.8, b = 0.94 } }
local sounds = {}
function PlaySound(name) sounds[#sounds + 1] = name end
ALL, ACCEPT, CANCEL, OKAY = "All", "Accept", "Cancel", "Okay"
CLASS_ICON_TCOORDS = setmetatable({}, { __index = function() return { 0, 0.25, 0, 0.25 } end })
GameTooltip = NewWidget("GameTooltip")
Minimap = NewWidget("Minimap")
function Minimap:GetCenter() return 100, 100 end
function Minimap:GetEffectiveScale() return 1 end
function GetCursorPosition() return 100, 180 end
function GetBindingKey(action) for key, bound in pairs(bindings) do if bound == action then return key end end end
function SetBinding(key, action) bindings[key] = action end
function SaveBindings() end
function GetCurrentBindingSet() return 1 end
function SendAddonMessage(prefix, message, channel, target) sent[#sent + 1] = { prefix, message, channel, target } end
function UnitName() return "Tester" end
function UnitLevel() return playerLevel end
local SPELLS = {
    [133] = "Fireball", [116] = "Frostbolt", [78] = "Heroic Strike", [772] = "Rend", [5176] = "Wrath",
    [11069] = "Improved Fireball", [12338] = "Improved Fireball", [11078] = "Improved Fire Blast",
    [16462] = "Deflection", [11071] = "Frostbite", [90101] = "Chaos Strike",
}
function GetSpellInfo(id) return SPELLS[id], "", "Interface\\Icons\\Spell_" .. id end
function GetSpellLink(id) return "[" .. (SPELLS[id] or "?") .. "]" end
function IsModifiedClick() return false end
function ChatEdit_InsertLink() end
function StaticPopup_Show(which, text, _, data) popup = { which = which, text = text, data = data } end
local offsets = {}
function FauxScrollFrame_SetOffset(frame, offset) offsets[frame] = offset end
function FauxScrollFrame_GetOffset(frame) return offsets[frame] or 0 end
function FauxScrollFrame_Update() end
function FauxScrollFrame_OnVerticalScroll(_, _, _, update) update() end
function wipe(t) for k in pairs(t) do t[k] = nil end return t end
tinsert, tremove = table.insert, table.remove
function strsplit(sep, text, limit)
    local parts, start = {}, 1
    while true do
        if limit and #parts == limit - 1 then
            parts[#parts + 1] = text:sub(start)
            break
        end
        local i = text:find(sep, start, true)
        if not i then
            parts[#parts + 1] = text:sub(start)
            break
        end
        parts[#parts + 1] = text:sub(start, i - 1)
        start = i + 1
    end
    return unpack(parts)
end

dofile(ADDON)
local frame = EmberforgeClassless
local function event(name, ...) frame.scripts.OnEvent(frame, name, ...) end
local function server(message) event("CHAT_MSG_ADDON", "EFCL", message, "WHISPER", "Tester") end
local function lastSent() return sent[#sent] and sent[#sent][2] end

-- The addon keeps its widgets local; reach them through the upvalues of its functions.
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
local function shownCells(cells)
    local names = {}
    for _, cell in ipairs(cells) do
        if cell.shown and cell.ability then names[#names + 1] = cell.name.text end
    end
    return table.concat(names, ",")
end

-- First login: N is bound to the window and the minimap button is placed.
event("PLAYER_LOGIN")
check(bindings.N == "EMBERFORGE_CLASSLESS_TOGGLE", "first login binds N to the Classless window")
check(EmberforgeClasslessDB and EmberforgeClasslessDB.boundKey, "the binding is remembered per character")
bindings.N = "TOGGLETALENTS"
EmberforgeClasslessDB.boundKey = true
event("PLAYER_LOGIN")
check(bindings.N == "TOGGLETALENTS", "a player's own N binding is not taken back later")

-- The window starts hidden and asks for the list the first time it opens.
check(not frame:IsShown(), "window starts hidden")
SlashCmdList.EMBERFORGECLASSLESS("")
check(frame:IsShown(), "/classless opens the window")
check(lastSent() == "LIST" and sent[1][1] == "EFCL" and sent[1][3] == "WHISPER" and sent[1][4] == "Tester",
    "opening asks the server for the list (addon whisper to self)")

-- The server answers: spec skill lines, abilities, talent trees, known spells, talents and state.
server("B")
server("T,8,6,Fire")
server("T,8,8,Arcane")
server("T,8,16,Frost")
server("T,1,26,Arms")
server("T,12,2000,Havoc")
server("A,8.6.1.133;8.16.4.116;1.26.1.78;1.26.4.772;11.574.1.5176;12.2000.1.90101")
server("U,8,0,81")
server("U,8,1,41")
server("U,8,2,61")
server("N,41.34.0.1.11069+12338.0.0;41.36.0.2.11078.0.0;61.38.0.0.16462.0.0;61.39.1.0.11071.38.1")
server("E")
server("K0")
server("R0")
server("R,34:1")
server("S,4,0,9")

local refresh = upvalues(frame.scripts.OnShow).Refresh
local up = upvalues(refresh)
local grid = upvalues(up.RefreshGrid)
local classRow = upvalues(up.RefreshClassRow)
local specs = upvalues(up.RefreshSpecs)
local tree = upvalues(up.RefreshTree)
local list = upvalues(up.RefreshList)
local stat = upvalues(up.RefreshStatus)

local mageButton, yourButton, druidButton
for _, button in ipairs(classRow.classButtons) do
    if button.id == 8 then mageButton = button end
    if button.id == 11 then druidButton = button end
    if button.id == 0 then yourButton = button end
end
local function shownTabs(tabs)
    local names = {}
    for _, button in ipairs(tabs) do
        if button.shown then names[#names + 1] = button.text end
    end
    return table.concat(names, ",")
end
check(#classRow.classButtons == 13 and yourButton.label.text == "Abilities", "class row has Abilities + 12 classes")
local demonHunterButton, monkButton
for _, button in ipairs(classRow.classButtons) do
    if button.id == 12 then demonHunterButton = button end
    if button.id == 10 then monkButton = button end
end
check(demonHunterButton and demonHunterButton.label.text == "Demon Hunter" and monkButton.label.text == "Monk",
    "Demon Hunter and Monk join the class row")
check(stat.status.text:find("choose 4 more abilities", 1, true) ~= nil, "status shows 4 picks left")

-- Mage opens on its first tree (Arcane, no abilities here); tabs are the three trees.
mageButton:Click()
check(shownTabs(specs.specButtons) == "Arcane,Fire,Frost", "Mage spec tabs: Arcane, Fire, Frost (no General)")
check(shownCells(grid.cells) == "", "Arcane lists no abilities here")

-- Fire: Fire abilities only, and the Fire talent tree with its background and owned rank.
specs.specButtons[2]:Click()
check(shownCells(grid.cells) == "Fireball", "Fire spec lists only Fire abilities")
check(grid.cells[1].icon.desaturated and grid.cells[1].sub.text == "Click to choose",
    "unknown Fireball is greyed and pickable at level 1")
check(tree.treeTitle.text == "Fire  |cffffffff1 point|r", "tree title shows Fire and points owned")
check(tree.backgrounds[1].texture == "Interface\\TalentFrame\\MageFire-TopLeft", "tree uses the Mage Fire background")
local improvedFireball = tree.talentButtons[0 * 4 + 1 + 1]
local improvedFireBlast = tree.talentButtons[0 * 4 + 2 + 1]
check(improvedFireball.shown and improvedFireball.rank.text == 1 and not improvedFireball.icon.desaturated,
    "Improved Fireball sits at row 1 col 2, rank 1, in color")
check(improvedFireBlast.shown and improvedFireBlast.rank.text == 0 and improvedFireBlast.icon.desaturated,
    "Improved Fire Blast is unowned and greyed")
local otherShown = 0
for index, button in ipairs(tree.talentButtons) do
    if button.shown and button ~= improvedFireball and button ~= improvedFireBlast then otherShown = otherShown + 1 end
end
check(otherShown == 0, "only Fire talents are on the Fire tree")
check(tree.branches.used == 0 and tree.arrowheads.used == 0, "no arrows on a tree without prerequisites")

-- Frost: Frostbolt, and Frostbite under Deflection with an unlit arrow (Deflection not owned).
specs.specButtons[3]:Click()
check(shownCells(grid.cells) == "Frostbolt", "Frost spec lists Frostbolt (Frost skill line)")
check(grid.cells[1].sub.text == "Level 4" and grid.cells[1].sub.color[2] < 0.5, "Frostbolt shows a red Level 4")
check(tree.talentButtons[1 * 4 + 0 + 1].shown, "Frostbite sits on row 2 of the Frost tree")
local arrow = tree.arrowheads[1]
check(tree.branches.used == 1 and tree.arrowheads.used == 1 and arrow.shown,
    "Deflection -> Frostbite draws one line and one arrowhead")
check(arrow.texture:find("ArrowDown", 1, true) and arrow.vertex[1] == 0.5 and tree.branches[1].texture == 0.5,
    "the down arrow is grey while unmet")
server("R,38:1")
check(tree.arrowheads[1].vertex[1] == 1 and tree.branches[1].texture == 1, "and gold once Deflection is owned")
server("R0")
server("R,34:1")

-- Picking: Frostbolt (level 4) is not pickable at level 1, Fireball is.
popup = nil
grid.cells[1]:Click()
check(popup == nil, "clicking an ability above your level does nothing")
specs.specButtons[2]:Click()
grid.cells[1]:Click()
check(popup and popup.which == "EMBERFORGE_CLASSLESS_PICK" and popup.data == 133, "clicking Fireball asks to confirm")
StaticPopupDialogs.EMBERFORGE_CLASSLESS_PICK.OnAccept(nil, popup.data)
check(lastSent() == "PICK,133", "accepting sends PICK,133")
server("K0")
server("K,133")
server("S,3,0,9")
check(grid.cells[1].sub.text == "Known" and not grid.cells[1].icon.desaturated, "Fireball now shows as known")
check(list.listRows[1].shown and list.listRows[1].name.text == "Fireball" and not list.listRows[2].shown,
    "the Spells tab lists Fireball")

-- Learnable now: hides known and too-high abilities.
EmberforgeClasslessLearnable.checked = true
EmberforgeClasslessLearnable.scripts.OnClick(EmberforgeClasslessLearnable)
check(shownCells(grid.cells) == "", "learnable now hides known Fireball")
EmberforgeClasslessLearnable.checked = false
EmberforgeClasslessLearnable.scripts.OnClick(EmberforgeClasslessLearnable)

-- Demon Hunter and Monk: their own specs. The Demon Hunter's trees are on the warlock's art (no "no tree" note).
demonHunterButton:Click()
check(shownTabs(specs.specButtons) == "Havoc,Vengeance", "Demon Hunter specs: Havoc, Vengeance")
specs.specButtons[1]:Click()
check(shownCells(grid.cells) == "Chaos Strike", "Havoc lists the Demon Hunter's own Chaos Strike")
check(not tree.treeNote.shown and tree.backgrounds[1].texture == "Interface\\TalentFrame\\WarlockSummoning-TopLeft",
    "Havoc on the Demonology art, no note")
monkButton:Click()
check(shownTabs(specs.specButtons) == "Brewmaster,Mistweaver,Windwalker",
    "Monk specs: Brewmaster, Mistweaver, Windwalker")
check(grid.empty.text == "No abilities here.", "Monk has no abilities yet")

-- Abilities outside the three trees get a General tab (Wrath's skill line has no tree here).
druidButton:Click()
check(shownTabs(specs.specButtons) == "Balance,Feral Combat,Restoration,General", "Druid gets a General tab")
specs.specButtons[4]:Click()
check(shownCells(grid.cells) == "Wrath"
    and tree.backgrounds[1].texture == "Interface\\TalentFrame\\DruidBalance-TopLeft",
    "General lists Wrath and keeps the last tree shown")

-- Search covers every class.
EmberforgeClasslessSearch.text = "r"
EmberforgeClasslessSearch.scripts.OnTextChanged(EmberforgeClasslessSearch)
check(shownCells(grid.cells) == "Chaos Strike,Fireball,Heroic Strike,Wrath,Frostbolt,Rend",
    "search 'r' finds abilities of every class")
check(shownTabs(specs.specButtons) == "", "spec tabs hide while searching")
EmberforgeClasslessSearch.text = ""
EmberforgeClasslessSearch.scripts.OnTextChanged(EmberforgeClasslessSearch)

-- Right panel tabs: talents with ranks, then specs with points; a spec opens its tree.
specs.listTabs[2]:Click()
check(list.listRows[1].talent and list.listRows[1].name.text:find("Improved Fireball", 1, true) == 1,
    "Talents tab lists Improved Fireball")
specs.listTabs[3]:Click()
check(list.listRows[1].spec and list.listRows[1].name.text:find("Fire", 1, true) == 1, "Specs tab lists Fire")
list.listRows[1]:Click()
check(tree.treeTitle.text == "Fire  |cffffffff1 point|r" and shownCells(grid.cells) == "Fireball",
    "clicking a spec opens it")
specs.listTabs[1]:Click()

-- Abilities (your build): known abilities in the grid, owned talents in the tree panel.
yourButton:Click()
check(shownCells(grid.cells) == "Fireball" and grid.cells[1].sub.text == "Mage", "Abilities lists known abilities")
check(tree.treeTitle.text == "Your Talents  |cffffffff1|r" and tree.talentRows[1].shown
    and tree.talentRows[1].name.text == "Improved Fireball  1/2", "Abilities lists owned talents with ranks")

-- Wildcard roll: the die bursts, the reel lands on the prize, the die re-forms and everything fades.
local reveal = EmberforgeClasslessReveal
local before = #sent
server("G,133,133")
server("H,34,12338,2")
check(reveal.shown and reveal.spinning, "a level-up ability starts the roll")
local spin = upvalues(reveal.scripts.OnUpdate)
local card = upvalues(spin.Land)
check(card.revealName.text == "", "nothing is named while it spins")
check(card.slots[card.WINNER_SLOT].icon.texture == "Interface\\Icons\\Spell_133", "the prize waits in the winning slot")
reveal.scripts.OnUpdate(reveal, 1)
check(reveal.spinning and card.revealName.text == "", "one second in it is still rolling")
reveal.scripts.OnUpdate(reveal, 10)
check(not reveal.spinning and card.revealName.text == "Fireball" and card.revealNote.text == "Mage  -  Fire",
    "then lands on Fireball (Mage, Fire)")
check(card.burst.shown and not card.skip.shown, "the light burst plays")
check(GameTooltip.link == "spell:133" and GameTooltip:IsOwned(reveal), "the spell's tooltip sits beside the track")
local finale = upvalues(upvalues(reveal.scripts.OnUpdate).RebuildDie)
reveal.scripts.OnUpdate(reveal, 1.5)
check(finale.shards[1].shown and not finale.die.shown, "after the reveal the shards gather")
reveal.scripts.OnUpdate(reveal, 0.6)
check(finale.die.shown and not finale.shards[1].shown, "and re-form the die")
reveal.scripts.OnUpdate(reveal, 1)
check(not finale.die.shown and upvalues(reveal.scripts.OnUpdate).bar.alpha == 0,
    "which fades away with the track closed")
check(not GameTooltip:IsOwned(reveal) or not GameTooltip.shown, "the tooltip goes as the rest fades")
reveal.scripts.OnUpdate(reveal, 0.8)
check(#sent == before, "nothing is asked of the player or sent to the server")

-- The queued talent roll follows on its own; Skip lands it at once.
check(reveal.shown and reveal.spinning, "the queued talent roll starts by itself")
card.skip.scripts.OnClick(card.skip)
reveal.scripts.OnUpdate(reveal, 0.01)
check(card.revealName.text == "Improved Fireball" and card.revealNote.text == "Mage  -  Fire  -  Rank 2/2",
    "Skip lands the talent roll on the rank won")
reveal.scripts.OnUpdate(reveal, 4)
check(not reveal.shown, "and once it fades the roll is gone")

-- Instant rolls: "/cl instant" or the window's check box; the roll opens on the prize.
local chat = {}
DEFAULT_CHAT_FRAME = { AddMessage = function(_, text) chat[#chat + 1] = text end }
SlashCmdList.EMBERFORGECLASSLESS("instant")
check(EmberforgeClasslessDB.instantRolls and chat[#chat]:find("instant Wildcard rolls on", 1, true) ~= nil,
    "/cl instant turns instant rolls on")
server("G,116,116")
check(reveal.shown and reveal.spinning and not card.skip.shown, "an instant roll needs no Skip")
reveal.scripts.OnUpdate(reveal, 0.3)
check(reveal.spinning, "the die still bursts first")
reveal.scripts.OnUpdate(reveal, 0.25)
check(not reveal.spinning and card.revealName.text == "Frostbolt", "and the prize is out in half a second")
for _ = 1, 30 do
    if reveal.scripts.OnUpdate then reveal.scripts.OnUpdate(reveal, 0.1) end
end
check(not reveal.shown, "the whole instant roll is over in under four seconds")
EmberforgeClasslessInstant.checked = false
EmberforgeClasslessInstant.scripts.OnClick(EmberforgeClasslessInstant)
check(EmberforgeClasslessDB.instantRolls == false, "the window's check box turns them off again")

-- Lock at level 9.
playerLevel = 9
server("S,0,1,9")
check(stat.status.text == "", "no tome hints once the kit is locked")
mageButton:Click()
specs.specButtons[3]:Click()
popup = nil
grid.cells[1]:Click()
check(grid.cells[1].ability and popup == nil, "no picking once locked (Frostbolt at level 9)")

-- The server can open the window (the Tome of Selection does); N and the minimap toggle it.
frame:Hide()
server("O")
check(frame:IsShown(), "server message O opens the window")
EmberforgeClassless_Toggle()
check(not frame:IsShown(), "the N binding's toggle closes it")
EmberforgeClasslessMinimapButton.scripts.OnClick(EmberforgeClasslessMinimapButton)
check(frame:IsShown(), "the minimap button opens it")

-- Other addons' messages are ignored.
before = #sent
event("CHAT_MSG_ADDON", "OTHER", "O", "WHISPER", "Someone")
check(#sent == before, "messages with another prefix are ignored")

print(failures == 0 and "\nAll checks passed" or ("\n" .. failures .. " check(s) failed"))
os.exit(failures == 0 and 0 or 1)

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
    return function() end
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
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end

UIParent = NewWidget("UIParent")
function CreateFrame(_, name) return NewWidget(name) end

local sent, popup = {}, nil
local playerLevel = 1
UISpecialFrames, StaticPopupDialogs, SlashCmdList = {}, {}, {}
ALL, ACCEPT, CANCEL = "All", "Accept", "Cancel"
CLASS_ICON_TCOORDS = setmetatable({}, { __index = function() return { 0, 0.25, 0, 0.25 } end })
GameTooltip = NewWidget("GameTooltip")
function SendAddonMessage(prefix, message, channel, target) sent[#sent + 1] = { prefix, message, channel, target } end
function UnitName() return "Tester" end
function UnitLevel() return playerLevel end
local SPELLS = { [133] = "Fireball", [116] = "Frostbolt", [78] = "Heroic Strike", [772] = "Rend", [5176] = "Wrath" }
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
tinsert = table.insert
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
local function server(message) frame.scripts.OnEvent(frame, "CHAT_MSG_ADDON", "EFCL", message, "WHISPER", "Tester") end
local function lastSent() return sent[#sent] and sent[#sent][2] end

-- The addon starts hidden and asks for the list the first time it opens.
check(not frame:IsShown(), "window starts hidden")
SlashCmdList.EMBERFORGECLASSLESS("")
check(frame:IsShown(), "/classless opens the window")
check(lastSent() == "LIST" and sent[1][1] == "EFCL" and sent[1][3] == "WHISPER" and sent[1][4] == "Tester",
    "opening asks the server for the list (addon whisper to self)")

-- The server answers: tabs, abilities, known spells and Selection state (4 picks).
server("B")
server("T,8,6,Fire")
server("T,8,8,Arcane")
server("T,8,16,Frost")
server("T,1,26,Arms")
server("A,8.6.1.133;8.6.4.116;1.26.1.78;1.26.4.772;11.574.1.5176")
server("E")
server("K0")
server("S,4,0,9")
check(frame:IsShown(), "window still open after the list")


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
local refresh = upvalues(frame.scripts.OnShow).Refresh
local up = upvalues(refresh)
local grid = upvalues(up.RefreshGrid)
local classRow = upvalues(up.RefreshClassRow)
local tabs = upvalues(up.RefreshTabs)
local list = upvalues(up.RefreshList)
local stat = upvalues(up.RefreshStatus)

local mageButton, yourButton
for _, button in ipairs(classRow.classButtons) do
    if button.id == 8 then mageButton = button end
    if button.id == 0 then yourButton = button end
end
check(#classRow.classButtons == 11, "class row has Abilities + 10 classes")
check(stat.status.text:find("choose 4 more abilities", 1, true) ~= nil, "status shows 4 picks left")

mageButton:Click()
local shown = {}
for _, cell in ipairs(grid.cells) do
    if cell.shown and cell.ability then shown[#shown + 1] = cell.name.text end
end
check(#shown == 2 and shown[1] == "Fireball" and shown[2] == "Frostbolt", "Mage grid lists Fireball then Frostbolt (by level)")
check(grid.cells[1].sub.text:find("click to choose", 1, true) ~= nil, "Fireball (level 1) is pickable at level 1")
check(grid.cells[2].sub.text == "Level 4", "Frostbolt (level 4) shows its level, not pickable yet")

local tabNames = {}
for _, tab in ipairs(tabs.tabButtons) do
    if tab.shown then tabNames[#tabNames + 1] = tab.text end
end
check(table.concat(tabNames, ",") == "All,Arcane,Fire,Frost", "Mage tabs: All, then specs by name")

-- Click Frostbolt: nothing (too high). Click Fireball: confirm popup, accept sends PICK.
popup = nil
grid.cells[2]:Click()
check(popup == nil, "clicking an ability above your level does nothing")
grid.cells[1]:Click()
check(popup and popup.which == "EMBERFORGE_CLASSLESS_PICK" and popup.data == 133, "clicking Fireball asks to confirm")
StaticPopupDialogs.EMBERFORGE_CLASSLESS_PICK.OnAccept(nil, popup.data)
check(lastSent() == "PICK,133", "accepting sends PICK,133")

-- Server confirms: Fireball known, 3 picks left.
server("K0")
server("K,133")
server("S,3,0,9")
check(grid.cells[1].sub.text == "Known", "Fireball now shows as known")
check(list.listTitle.text == "Your Abilities (1)", "your abilities list counts 1")
check(stat.status.text:find("choose 3 more abilities", 1, true) ~= nil, "status shows 3 picks left")

-- Search filters your abilities.
EmberforgeClasslessSearch.text = "frost"
EmberforgeClasslessSearch.scripts.OnTextChanged(EmberforgeClasslessSearch)
local rows = 0
for _, row in ipairs(list.listRows) do
    if row.shown and row.ability then rows = rows + 1 end
end
check(rows == 0, "search 'frost' hides Fireball from your abilities")
EmberforgeClasslessSearch.text = "fire"
EmberforgeClasslessSearch.scripts.OnTextChanged(EmberforgeClasslessSearch)
rows = 0
for _, row in ipairs(list.listRows) do
    if row.shown and row.ability then rows = rows + 1 end
end
check(rows == 1, "search 'fire' shows Fireball")

-- Your Abilities view, and the lock at level 9.
yourButton:Click()
check(grid.cells[1].shown and grid.cells[1].name.text == "Fireball" and grid.cells[1].sub.text == "Mage",
    "Abilities view shows what you know, with its class")
playerLevel = 9
server("S,0,1,9")
check(stat.status.text == "", "no tome hints once the kit is locked")
mageButton:Click()
popup = nil
grid.cells[2]:Click()
check(popup == nil, "no picking once locked")

-- The server can open the window (the Tome of Selection does).
frame:Hide()
server("O")
check(frame:IsShown(), "server message O opens the window")

-- Other addons' messages are ignored.
local before = #sent
frame.scripts.OnEvent(frame, "CHAT_MSG_ADDON", "OTHER", "O", "WHISPER", "Someone")
check(#sent == before, "messages with another prefix are ignored")

print(failures == 0 and "\nAll checks passed" or ("\n" .. failures .. " check(s) failed"))
os.exit(failures == 0 and 0 or 1)

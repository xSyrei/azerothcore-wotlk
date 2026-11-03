-- Offline test for Emberforge_Journal: loads its data and the addon against a mocked WoW 3.3.5 API and plays a
-- session the way the server drives it (see src/journal.cpp). Run: lua5.1 journal_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Journal/"
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
    local widget = setmetatable({ shown = true, scripts = {}, text = "", width = 300, height = 20, name = name },
        Widget)
    if name then
        _G[name] = widget
    end
    return widget
end
function Widget:GetName() return self.name end
function Widget:CreateTexture() return NewWidget() end
function Widget:CreateFontString() return NewWidget() end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:HookScript(event, handler) self.scripts[event] = handler end
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
function Widget:SetWidth(width) self.width = width end
function Widget:GetWidth() return self.width end
function Widget:SetHeight(height) self.height = height end
function Widget:GetHeight() return self.height end
function Widget:GetStringWidth() return #(self.text or "") * 6 end
function Widget:GetStringHeight() return 14 end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end

UIParent = NewWidget("UIParent")
WorldFrame = NewWidget("WorldFrame")
local created = {}
function CreateFrame(_, name)
    local widget = NewWidget(name)
    created[#created + 1] = widget
    return widget
end
local sent = {}
UISpecialFrames, SlashCmdList = {}, {}
GameTooltip = NewWidget("GameTooltip")
ITEM_QUALITY_COLORS = setmetatable({}, { __index = function() return { hex = "|cffffffff" } end })
function SendAddonMessage(prefix, message) sent[#sent + 1] = { prefix, message } end
function UnitName() return "Tester" end
function SetPortraitToTexture() end
function GetSpellInfo(id) return "Spell " .. id, "", "Interface\\Icons\\Spell_" .. id end
function GetItemIcon() return "Interface\\Icons\\INV_Misc_QuestionMark" end
function GetCursorPosition() return 0, 0 end
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

dofile(DIR .. "Data.lua")
dofile(DIR .. "Emberforge_Journal.lua")
local frame = EmberforgeJournal
local events
for _, widget in ipairs(created) do
    if widget.scripts.OnEvent and widget ~= frame then
        events = widget
    end
end
local function server(message)
    events.scripts.OnEvent(events, "CHAT_MSG_ADDON", "EFDJ", message, "WHISPER", "Tester")
end

-- The addon keeps its widgets local; reach them through the upvalues of its functions.
local function upvalue(fn, wanted)
    local i = 1
    while true do
        local name, value = debug.getupvalue(fn, i)
        if not name then
            return nil
        end
        if name == wanted then
            return value
        end
        i = i + 1
    end
end
local refresh = upvalue(frame.scripts.OnShow, "Refresh")
local ui = upvalue(upvalue(refresh, "RefreshHome"), "ui")
local view = upvalue(refresh, "view")

-- Opening asks for the list; the server sends Wailing Caverns and its last boss.
frame:Hide()
frame:Show()
check(sent[#sent] and sent[#sent][2] == "LIST", "opening the Journal asks the server for the list")
server("B")
server("D,1~43~1~0~0~5~15~25~15~25~0~Wailing Caverns")
server("F,43~0~Lady Anacondra~3671;43~0~Mutanus the Devourer~3654;43~0~Unknown Boss~0")
server("Z")
local tile = ui.tiles[1]
check(tile and tile.shown and tile.name.text == "Wailing Caverns", "its tile is on the home page")
check(tile.fill.texture:find("UI%-EJ%-DUNGEONBUTTON%-WailingCaverns"), "the tile shows the Journal's button art")

-- The instance: its picture and story, its bosses with the Journal's portraits (a model when there is none).
tile:Click()
check(ui.plate.fill.texture:find("UI%-EJ%-LOREBG%-WailingCaverns"), "the instance page shows its Journal picture")
check(ui.about.shown and EmberforgeJournalLore[43] ~= nil, "the instance has its Journal story")
local rows = ui.bossList.rows
check(rows[2].art.shown and rows[2].art.texture:find("UI%-EJ%-BOSS%-Mutanus"), "a boss row shows its portrait")
check(not rows[3].art.shown and rows[3].portrait.shown, "a boss without a portrait keeps its model")

-- A boss: the Journal's abilities, with icons and flags.
rows[2]:Click()
check(view.page == "boss", "clicking a boss opens its page")
local abilities = ui.abilityList.rows
check(#abilities > 0 and abilities[1].name.text ~= "", "the boss page lists the Journal's abilities")
local flagged = false
for _, row in ipairs(abilities) do
    for _, flag in ipairs(row.flags) do
        flagged = flagged or (flag.shown and flag.label ~= nil)
    end
end
check(flagged, "abilities show their flags")

-- A wing: Scarlet Monastery's Cathedral lists its own three bosses, not the whole monastery's.
server("B")
server("D,164~189~1~0~0~5~30~40~35~45~0~Scarlet Monastery - Cathedral")
server("F,189~0~Interrogator Vishas~3983;189~0~Bloodmage Thalnos~4543;189~0~Houndmaster Loksey~3974;"
    .. "189~0~Arcanist Doan~6487;189~0~Herod~3975;189~0~High Inquisitor Fairbanks~4542;"
    .. "189~0~Scarlet Commander Mograine~3976;189~0~High Inquisitor Whitemane~3977")
server("W,163~1~2;164~6~8")
server("Z")
view.page, view.instance, view.boss, view.difficulty, view.tab = "home", nil, nil, 1, "bosses"
refresh()
ui.tiles[1]:Click()
local names = {}
for _, row in ipairs(ui.bossList.rows) do
    if row.shown then
        names[#names + 1] = row.name.text
    end
end
check(#names == 3 and names[1] == "High Inquisitor Fairbanks", "a wing lists its own bosses only")
check(EmberforgeJournalLore[189] and EmberforgeJournalLore[189] ~= "PH", "the old Scarlet Monastery has a story")

print(failures == 0 and "all passed" or (failures .. " failed"))
os.exit(failures == 0 and 0 or 1)

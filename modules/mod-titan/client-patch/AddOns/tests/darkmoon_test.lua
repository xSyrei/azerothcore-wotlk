-- Offline test for Emberforge_Darkmoon: loads the addon against a mocked WoW 3.3.5 API and plays the shop and the
-- reveal the way the server drives them (see src/cards.cpp). Run: lua5.1 darkmoon_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Darkmoon/"
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
function Widget:GetFrameLevel() return 1 end
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

StaticPopupDialogs = {}
local popup
function StaticPopup_Show(name, a, b) popup = { name = name, a = a, b = b } end
function StaticPopup_Hide() popup = nil end
ACCEPT, CANCEL = "Accept", "Cancel"
local xp, xpMax = 0, 400
function UnitXP() return xp end
function UnitXPMax() return xpMax end
function Widget:Enable() self.enabled = true end
function Widget:Disable() self.enabled = false end
function Widget:GetFrameLevel() return 1 end

StaticPopupDialogs = {}
local popup
function StaticPopup_Hide() popup = nil end
ACCEPT, CANCEL = "Accept", "Cancel"
function Widget:Enable() self.enabled = true end
function Widget:Disable() self.enabled = false end
function UnitXP() return 0 end
function UnitXPMax() return 1 end
function GetMoney() return 1000000 end
function PlaySound() end
function Widget:SetDesaturated() end
function Widget:EnableMouseWheel() end
function Widget:SetAutoFocus() end
function Widget:ClearFocus() end
function Widget:SetFrameLevel() end
local dialog = {}
function StaticPopup_Show(name, a, b) popup = { name = name, a = a, b = b }; return dialog end

dofile(DIR .. "Emberforge_Darkmoon.lua")
local frame = EmberforgeDarkmoon
local t = EmberforgeDarkmoon_State
local events
for _, widget in ipairs(created) do
    if widget.scripts.OnEvent then
        events = widget
    end
end
local function server(message)
    events.scripts.OnEvent(events, "CHAT_MSG_ADDON", "EFDM", message, "WHISPER", "Tester")
end

server("S,1500,100,125,100,5,750000,10000,13,10,50,1,2,4,8")
server("O")
check(frame:IsShown(), "O opens the shop")
check(t.state.spark == 1500 and t.state.prices[1] == 100 and t.state.prices[2] == 125 and t.state.prices[3] == 10000,
    "the prices are read")
check(t.state.duplicates == 13 and t.state.goldLeft == 5, "the tallies are read")

t.view.tab = 2
server("K0")
server("K,0.133.133.0.1.1.0;0.27243.27243.3.0.1.0")
server("K,1.2300.90200.1.2.5.1")
server("KE")
check(#t.catalog == 3, "the catalog arrives in parts")
server("U,0.27243.1.0")
local seed
for _, card in ipairs(t.catalog) do
    if card.id == 27243 then seed = card end
end
check(seed and seed.owned == 1, "an update marks a card owned")

server("R,0,0.133.133.0.0.40;0.27243.27243.3.1.0;0.5.5.1.1.0")
check(t.reveal:IsShown(), "a pack opened plays the reveal")
t.reveal.scripts.OnUpdate(t.reveal, 3)
t.reveal.scripts.OnMouseDown(t.reveal)
check(not t.reveal:IsShown(), "a click after the reveal closes it")
server("B,1.2300.90200.1")
check(t.reveal:IsShown(), "a card bought shows too")

print(failures == 0 and "all passed" or (failures .. " failed"))
os.exit(failures == 0 and 0 or 1)

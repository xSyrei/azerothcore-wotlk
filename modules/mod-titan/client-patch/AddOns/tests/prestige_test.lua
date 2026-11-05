-- Offline test for Emberforge_Prestige: loads the addon against a mocked WoW 3.3.5 API and plays the window the
-- way the server drives it (see src/prestige.cpp). Run: lua5.1 prestige_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Prestige/"
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
function Widget:Enable() self.enabled = true end
function Widget:Disable() self.enabled = false end
function Widget:GetFrameLevel() return 1 end

dofile(DIR .. "Emberforge_Prestige.lua")
local frame = EmberforgePrestige
local button = EmberforgePrestigeButton
local events
for _, widget in ipairs(created) do
    if widget.scripts.OnEvent then
        events = widget
    end
end
local function server(message)
    events.scripts.OnEvent(events, "CHAT_MSG_ADDON", "EFPR", message, "WHISPER", "Tester")
end
local state = EmberforgePrestige_State

check(not frame:IsShown(), "the window starts hidden")
server("S,0,42,60,1500,0,0,0,0,500,5000,2,10,Come back at level 60.")
server("O")
check(frame:IsShown(), "O opens the window")
check(state.level == 42 and state.spark == 1500 and not state.ready, "the state is read")
check(state.why == "Come back at level 60.", "the reason is read")
check(button.enabled == false, "below the cap the button is off")
button.scripts.OnClick(button)
check(popup == nil, "an off button asks nothing")

server("S,2,60,60,12000,0,0,9000,1,500,5000,2,10,")
check(state.ready and state.rank == 2 and state.fastest == 9000, "at the cap it is ready")
check(button.enabled == true, "and the button is on")
button.scripts.OnClick(button)
check(popup and popup.name == "EMBERFORGE_PRESTIGE" and popup.a == "3", "the prestige asks first, for rank 3")
StaticPopupDialogs.EMBERFORGE_PRESTIGE.OnAccept()
check(sent[#sent] and sent[#sent][1] == "EFPR" and sent[#sent][2] == "PRESTIGE", "accepting asks the server")
server("C")
check(not frame:IsShown() and popup == nil, "C closes the window")

server("S,1,60,60,0,0,0,0,0,500,5000,2,10,Not now: you must be alive, out of combat, and on the ground.")
check(state.why == "Not now: you must be alive, out of combat, and on the ground.", "a reason with commas stays whole")
frame.scripts.OnUpdate(frame, 0.1)

print(failures == 0 and "all passed" or (failures .. " failed"))
os.exit(failures == 0 and 0 or 1)

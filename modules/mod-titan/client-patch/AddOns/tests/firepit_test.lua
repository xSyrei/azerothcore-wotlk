-- Offline test for Emberforge_Firepit: loads the addon against a mocked WoW 3.3.5 API and plays a run the way the
-- server drives it (see src/firepit.cpp). Run: lua5.1 firepit_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Firepit/"
local failures = 0
local function check(ok, what)
    print((ok and "ok    " or "FAIL  ") .. what)
    if not ok then
        failures = failures + 1
    end
end

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
    local widget = setmetatable({ shown = true, scripts = {}, text = "", name = name }, Widget)
    if name then
        _G[name] = widget
    end
    return widget
end
function Widget:CreateTexture() return NewWidget() end
function Widget:CreateFontString() return NewWidget() end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:Show() self.shown = true end
function Widget:Hide() self.shown = false end
function Widget:IsShown() return self.shown end
function Widget:SetText(text) self.text = text end
function Widget:GetText() return self.text end
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end

UIParent = NewWidget("UIParent")
local frames = {}
function CreateFrame(_, name)
    local widget = NewWidget(name)
    frames[#frames + 1] = widget
    return widget
end
UISpecialFrames, SlashCmdList = {}, {}
tinsert = table.insert
GameTooltip = NewWidget("GameTooltip")
local now = 100
function GetTime() return now end
function UnitName() return "Tester" end
local chat = {}
function SendChatMessage(text, channel) chat[#chat + 1] = { text, channel } end
function GetNumRaidMembers() return 0 end
function GetNumPartyMembers() return 2 end
DEFAULT_CHAT_FRAME = { AddMessage = function() end }

dofile(DIR .. "Emberforge_Firepit.lua")
local debug = EmberforgeFirepitDebug
local events = frames[#frames]
local function server(message) events.scripts.OnEvent(events, "CHAT_MSG_ADDON", "EFFP", message, "WHISPER", "Tester") end

check(not debug.hud.shown and not debug.results.shown, "nothing shows before a run")

server("H,0,0,0,0,3,3,8")
check(debug.hud.shown and debug.hud.timer.text == "0:08", "the countdown shows on the wave bar")
server("H,10,42,37,1,2,3,0")
check(debug.hud.title.text:find("BOSS", 1, true) and debug.hud.title.text:find("Wave 10", 1, true),
    "a boss wave says so")
check(debug.hud.timer.text == "0:42" and debug.hud.line.text:find("37", 1, true)
    and debug.hud.line.text:find("2|r/3", 1, true), "the bar shows time left, enemies and Heroes standing")
now = now + 10
debug.hud.scripts.OnUpdate(debug.hud)
check(not debug.hud.shown, "the bar goes when the news stops")

server("RUN,31,1520,3,412,180,1")
server("P,Tester,42,31,58,74,1234567,0,456789,140,3,9876,4321,1,31,0,2,Grizzle")
server("P,Healer,40,29,40,61,98000,2500000,300000,12,0,1500,6000,2,29,1,1,Anvilrage Marshal")
check(not debug.results.shown, "the results wait for END")
server("END")
check(debug.results.shown, "END shows the results")
check(debug.results.summary.text:find("31", 1, true) and debug.results.summary.text:find("25:20", 1, true)
    and debug.results.summary.text:find("412", 1, true), "the summary has the wave, the time and the kills")
check(debug.results.detail.text:find("x1.8", 1, true), "the detail has the group's monster health")
local first = debug.rows[1].cells
check(first[1].text:find("Tester", 1, true) and first[2].text == "31" and first[5].text == "1.23M",
    "sorted by peak wave: Tester first, damage shortened")
check(first[12].text == "1 (w31)" and first[14].text == "2" and first[15].text == "Grizzle",
    "falls, spoils and killer")
check(first[1].color and first[1].color[1] == 1 and first[1].color[3] < 1, "your own row stands out")
check(not debug.rows[3].shown, "rows past the run's Heroes are hidden")

debug.headers[6]:Click()
check(debug.rows[1].cells[1].text:find("Healer", 1, true), "sorting by healing puts the healer first")
debug.headers[6]:Click()
check(debug.rows[1].cells[1].text:find("Tester", 1, true), "clicking again reverses it")

local share
for _, frame in ipairs(frames) do
    if frame.text == "Share in chat" then
        share = frame
    end
end
share:Click()
check(chat[1] and chat[1][2] == "PARTY" and chat[1][1]:find("wave 31", 1, true)
    and chat[1][1]:find("Tester", 1, true), "Share posts the result to the party")

debug.results:Hide()
SlashCmdList.EMBERFORGEFIREPIT()
check(debug.results.shown and EmberforgeFirepitLast.reached == 31, "/firepit opens the last results, saved")

if failures > 0 then
    print(failures .. " check(s) failed")
    os.exit(1)
end
print("All checks passed")

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
function Widget:Show()
    local was = self.shown
    self.shown = true
    if not was and self.scripts.OnShow then
        self.scripts.OnShow(self)
    end
end
function Widget:Hide() self.shown = false end
function Widget:IsShown() return self.shown end
function Widget:SetText(text) self.text = text end
function Widget:GetText() return self.text end
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end
function Widget:Enable() self.enabled = true end
function Widget:SetValue(value) self.value = value end
function Widget:SetBackdropBorderColor(...) self.border = { ... } end
function Widget:SetTexture(texture) self.texture = texture end
function Widget:SetScale(scale) self.scale = scale end
function Widget:SetPoint(point) self.point = self.point or point end
function Widget:Disable() self.enabled = false end
function Widget:GetScript(event) return self.scripts[event] end

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
sounds = {}
function PlaySound(name) sounds[#sounds + 1] = name end
local party = { { "Healer", 40, true }, { "Lowbie", 8, true } }
local leader = true
function UnitName(unit)
    local index = unit and tonumber(unit:match("^party(%d)$"))
    return index and party[index] and party[index][1] or "Tester"
end
function UnitLevel(unit)
    local index = unit and tonumber(unit:match("^party(%d)$"))
    return index and party[index] and party[index][2] or 42
end
function UnitIsConnected(unit)
    local index = unit and tonumber(unit:match("^party(%d)$"))
    return not index or party[index][3]
end
function IsPartyLeader() return leader end
local sent, popups = {}, {}
function SendAddonMessage(prefix, message) sent[#sent + 1] = message end
StaticPopupDialogs = {}
function StaticPopup_Show(which, text) popups[#popups + 1] = { which, text } end
function StaticPopup_Hide(which) popups.hidden = which end
function ShowUIPanel(frame) frame:Show(); if frame.scripts.OnShow then frame.scripts.OnShow(frame) end end
function HideUIPanel(frame) frame:Hide() end
local chat = {}
function SendChatMessage(text, channel) chat[#chat + 1] = { text, channel } end
function GetNumRaidMembers() return 0 end
local partySize = 2
function GetNumPartyMembers() return partySize end
DEFAULT_CHAT_FRAME = { AddMessage = function() end }
local instanceName, instanceKind = "Stormwind City", "none"
function GetInstanceInfo() return instanceName, instanceKind end
function GetLFGMode() return nil end
local music
function PlayMusic(file) music = file end
function StopMusic() music = nil end
local menus = {}
function EasyMenu(menu) menus[#menus + 1] = menu end
MiniMapLFGFrame = NewWidget("MiniMapLFGFrame")
MiniMapLFGFrame.shown = false
local dungeonFinderClicked = false
MiniMapLFGFrame.scripts.OnClick = function() dungeonFinderClicked = true end

dofile(DIR .. "Emberforge_Firepit.lua")
local debug = EmberforgeFirepitDebug
local events = frames[#frames]
local function server(message)
    events.scripts.OnEvent(events, "CHAT_MSG_ADDON", "EFFP", message, "WHISPER", "Tester")
end

debug.finder:Hide()
check(not debug.hud.shown and not debug.results.shown, "nothing shows before a run")

-- The queue: the group finder's Firepit tab.
server("OPEN")
check(debug.finder.shown and sent[#sent] == "STATE", "OPEN shows the queue, which asks for its state")
local escapes = false
for _, name in ipairs(UISpecialFrames) do
    escapes = escapes or name == "EmberforgeFirepitFinder"
end
check(debug.finder.scale == 1.5 and debug.finder.point == "CENTER" and escapes,
    "the queue is a window of its own: larger, in the screen's middle, closed by Escape")
server("S,23,31,0,12,0,10,30")
server("L,1,1,Solo Star,57,1")
server("L,4,1,Tester,41,4")
server("LE")
check(debug.members[1].member.name == "Tester" and debug.members[3].member.name == "Lowbie"
    and not debug.members[4].member, "a slot for you and each party member, the rest open")
check(debug.members[3].why == "Needs level 10." and not debug.queueButton.enabled,
    "a member under level 10 keeps the group out")
check(debug.records[1].value.text == "23" and debug.records[2].value.text == "31"
    and debug.records[4].value.text == "12",
    "the records: solo, duo, trio, group")
GameTooltip.lines = {}
function GameTooltip:AddLine(text) self.lines[#self.lines + 1] = text end
function GameTooltip:AddDoubleLine(left, right) self.lines[#self.lines + 1] = left .. " " .. right end
debug.records[4].scripts.OnEnter(debug.records[4])
check(table.concat(GameTooltip.lines, "|"):find("Tester", 1, true)
    and table.concat(GameTooltip.lines, "|"):find("wave 41", 1, true), "hovering a record shows its leaderboard")
check(debug.finder.difficulty.text:find("x1.8", 1, true) and debug.finder.difficulty.text:find("x1.4", 1, true),
    "the bottom bar: the difficulty for a group of 3")
check(debug.start.text.text:find("Wave 1", 1, true) and not debug.startPrev.enabled and debug.startNext.enabled,
    "the run starts at wave 1, later checkpoints offered")
debug.startNext:Click()
debug.startNext:Click()
check(debug.start.text.text:find("Wave 10", 1, true), "the next checkpoints: 5, 10")
for _ = 1, 10 do debug.startNext:Click() end
check(debug.start.text.text:find("Wave 30", 1, true) and not debug.startNext.enabled,
    "no further than the latest checkpoint reached")
for _ = 1, 10 do debug.startPrev:Click() end
check(debug.start.text.text:find("Wave 1", 1, true), "back to wave 1")
debug.startNext:Click()

party[2][2] = 12
debug.finder.scripts.OnEvent(debug.finder, "PARTY_MEMBERS_CHANGED")
check(debug.queueButton.enabled and debug.queueButton.text == "Queue group (3)", "the leader can queue the group")
debug.queueButton:Click()
check(sent[#sent] == "QUEUE,5", "queuing asks the server, from the wave chosen")
server("QP,Tester,3,40,5")
server("QA,Tester,1")
server("QA,Healer,0")
check(#popups == 0, "the leader is not asked to accept their own queue")
check(debug.members[1].label.text == "Ready" and debug.members[2].label.text == "Pending"
    and debug.finder.status.text:find("1 of 3", 1, true), "each member's answer shows on their slot")
check(debug.queueButton.text == "Leave queue", "the leader can leave the queue")
server("QC,Healer declined.")
check(debug.finder.status.text:find("Healer declined", 1, true) and debug.queueButton.text == "Queue group (3)",
    "a decline ends the queue, and says why")

leader = false
server("QP,Healer,3,40,1")
check(popups[1] and popups[1][1] == "EMBERFORGE_FIREPIT_PROPOSAL" and popups[1][2] == "Healer",
    "a member is asked to accept")
StaticPopupDialogs.EMBERFORGE_FIREPIT_PROPOSAL.OnAccept()
check(sent[#sent] == "ACCEPT", "accepting tells the server")
server("QG")
check(not debug.finder.shown and popups.hidden == "EMBERFORGE_FIREPIT_PROPOSAL", "going in closes the queue")
leader = true

MiniMapLFGFrame.scripts.OnClick(MiniMapLFGFrame, "LeftButton")
check(dungeonFinderClicked and #menus == 0, "outside the Firepit, the eye is the dungeon finder's own")
server("INTRO")
check(debug.intro.shown and debug.intro.title.text == "THE FIREPIT", "arriving shows the Firepit's title")
check(MiniMapLFGFrame.shown and debug.Inside(), "inside the Firepit, the minimap's eye shows")
check(music and music:find("PressurePoint.mp3", 1, true), "and the Firepit's music plays")
MiniMapLFGFrame.scripts.OnClick(MiniMapLFGFrame, "LeftButton")
local menu = menus[1]
check(menu and menu[2].text:find("Teleport out", 1, true), "the eye's menu offers the way out")
menu[2].func()
check(popups[#popups][1] == "EMBERFORGE_FIREPIT_EXIT", "leaving asks first")
StaticPopupDialogs.EMBERFORGE_FIREPIT_EXIT.OnAccept()
check(sent[#sent] == "EXIT", "and then asks the server to send you out")
events.scripts.OnEvent(events, "PLAYER_ENTERING_WORLD")
check(not MiniMapLFGFrame.shown and not debug.Inside() and not music, "out of the Firepit, the eye and music go")
instanceName, instanceKind = "The Firepit", "party"
events.scripts.OnEvent(events, "PLAYER_ENTERING_WORLD")
check(MiniMapLFGFrame.shown, "a Hero who logs in inside sees the eye")
server("H,0,0,0,0,3,3,8")
check(debug.hud.shown and debug.hud.timer.text == "0:08" and debug.hud.wave.text == "Get ready",
    "the countdown shows on the wave tracker")
server("H,10,42,37,1,2,3,0")
check(debug.hud.wave.text == "Wave 10" and debug.hud.boss.text == "BOSS WAVE", "a boss wave says so")
check(debug.hud.timer.text == "0:42" and debug.hud.bar.value == 42 and debug.hud.enemies.text:find("37", 1, true)
    and debug.hud.standing.text:find("2|r / 3", 1, true), "time left on the bar, enemies and Heroes standing")
server("H,12,30,16,0,2,2,0,2,16,5")
check(debug.hud.boss.text:find("PRESSURE +10%", 1, true)
    and debug.hud.enemies.text:find("16", 1, true) and debug.hud.enemies.text:find("/ 16 +5", 1, true),
    "pressure: its stacks, the enemies against the cap and the reinforcements waiting")
server("H,12,30,20,0,2,2,0,0,50")
check(debug.hud.boss.text == "" and not debug.hud.enemies.text:find("/", 1, true), "back under: no warning")
server("A,Wave 10: Grizzle enters the Ring!,1")
check(debug.hud.event.text:find("Grizzle", 1, true) and sounds[#sounds] == "RaidWarning",
    "a boss's news on the tracker, with the raid warning's sound")
now = now + 10
debug.hud.scripts.OnUpdate(debug.hud)
check(not debug.hud.shown, "the tracker goes when the news stops")

server("RUN,31,1520,3,412,180,1")
server("P,Tester,42,31,58,74,1234567,0,456789,140,3,9876,4321,1,31,0,2,Grizzle")
server("P,Healer,40,29,40,61,98000,2500000,300000,12,0,1500,6000,2,29,1,1,Anvilrage Marshal")
check(not debug.results.shown, "the recap waits for END")
server("END")
check(debug.results.shown, "END shows the recap")
local stats = debug.results.stats
check(stats[1].value.text == "31" and stats[2].value.text == "25:20" and stats[3].value.text == "412"
    and stats[4].value.text == "3" and stats[5].value.text == "x1.8", "the run in big numbers")
check(debug.results.highlights.text:find("Top healing", 1, true)
    and debug.results.highlights.text:find("Healer", 1, true), "who stood out")
local first = debug.rows[1].cells
check(first[1].text:find("Tester", 1, true) and first[2].text == "31" and first[5].text == "1.23M",
    "sorted by peak wave: Tester first, damage shortened")
check(first[12].text == "1 (w31)" and first[14].text == "2" and first[15].text == "Grizzle",
    "falls, spoils and killer")
check(first[1].color and first[1].color[1] == 1 and first[1].color[3] < 0.6, "your own row stands out")
check(first[5].color[2] < 0.7 and debug.rows[2].cells[6].color[2] < 0.7, "the best in a column is lit")
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

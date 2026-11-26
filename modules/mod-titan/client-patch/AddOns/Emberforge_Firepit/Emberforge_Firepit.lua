-- Emberforge Firepit: the Firepit's queue (a tab of the group finder, beside Dungeons and Player vs. Player: see
-- Emberforge_Journal/MicroMenu.lua), the wave bar during a run (wave, time left, enemies alive, Heroes standing), and
-- the results window when the run ends: the run's totals and a row for each Hero, sortable by any column, with a
-- button that shares the result in group chat. /firepit opens the last results again.
--
-- The server (mod-titan, src/firepit.cpp) talks in addon whispers to the player, prefix EFFP:
--   H,<wave>,<sec left>,<enemies alive>,<boss wave 0|1>,<Heroes standing>,<Heroes in the run>,<countdown sec>
--   RUN,<wave reached>,<sec survived>,<group size>,<kills>,<monster health %>,<start wave>
--   P,<name>,<level>,<peak wave>,<enemies at peak>,<most enemies>,<damage>,<healing>,<taken>,<kills>,<boss kills>,
--     <biggest hit>,<biggest hit taken>,<deaths>,<wave fallen on>,<times resurrected>,<spoils>,<killer>
--   END   the results are complete: show them
--   OPEN  open the queue (the Firepit Keeper sent you)
--   S,<best wave>,<its group size>,<in a proposal 0|1>,<level it opens at>    your state, then the leaderboard:
--   L,<solo 0|group 1>,<rank>,<name>,<wave>,<group size> ... LE
--   QP,<leader>,<Heroes>,<sec>   your group's leader queued: accept or decline
--   QA,<name>,<0 waiting|1 accepted|2 declined>    QC,<why>   the queue ended    QG   everyone accepted: going in
-- To the server: STATE, QUEUE (alone: straight in; a leader: the group is asked), ACCEPT, DECLINE, LEAVE.

local PREFIX = "EFFP"
local GOLD = { 1, 0.82, 0 }
local EMBER = { 1, 0.5, 0.15 }
local HUD_TIMEOUT = 4          -- sec without news: the run is over, the bar goes
local GROUP_HEALTH, GROUP_COUNT = 0.4, 0.2     -- src/firepit.cpp: each Hero past the first
local MOST_HEROES = 10
local ICON = "Interface\\Icons\\Spell_Fire_Burnout"
local READY = "Interface\\RaidFrame\\ReadyCheck-Ready"
local WAITING = "Interface\\RaidFrame\\ReadyCheck-Waiting"
local NOT_READY = "Interface\\RaidFrame\\ReadyCheck-NotReady"

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

EmberforgeFirepitLast = EmberforgeFirepitLast or nil

local function Thousands(value)
    local text = tostring(math.floor(value or 0))
    while true do
        local done
        text, done = text:gsub("^(%d+)(%d%d%d)", "%1,%2")
        if done == 0 then
            return text
        end
    end
end

-- 1234567 -> "1.23M", 12345 -> "12.3K"
local function Short(value)
    value = tonumber(value) or 0
    if value >= 1000000 then
        return string.format("%.2fM", value / 1000000)
    elseif value >= 10000 then
        return string.format("%.1fK", value / 1000)
    end
    return Thousands(value)
end

local function Clock(seconds)
    seconds = math.max(0, tonumber(seconds) or 0)
    return string.format("%d:%02d", math.floor(seconds / 60), seconds % 60)
end

local function Split(text)
    local fields = {}
    for field in string.gmatch(text .. ",", "([^,]*),") do
        fields[#fields + 1] = field
    end
    return fields
end

local function Backdrop(frame, alpha)
    frame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 14,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    frame:SetBackdropColor(0.06, 0.03, 0.02, alpha or 0.92)
    frame:SetBackdropBorderColor(EMBER[1], EMBER[2], EMBER[3], 0.9)
end

-- The wave bar --------------------------------------------------------------------------------------------------

local hud = CreateFrame("Frame", "EmberforgeFirepitHud", UIParent)
hud:SetWidth(300)
hud:SetHeight(58)
hud:SetPoint("TOP", UIParent, "TOP", 0, -110)
Backdrop(hud, 0.8)
hud:Hide()

hud.title = hud:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
hud.title:SetPoint("TOP", 0, -8)
hud.timer = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlightLarge")
hud.timer:SetPoint("TOPRIGHT", -12, -9)
hud.line = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
hud.line:SetPoint("BOTTOM", 0, 9)

local hudState = { seen = 0, left = 0 }

local function ShowHud(fields)
    local wave, left, alive, boss = tonumber(fields[2]) or 0, tonumber(fields[3]) or 0, tonumber(fields[4]) or 0,
        fields[5] == "1"
    local standing, total, countdown = tonumber(fields[6]) or 0, tonumber(fields[7]) or 0, tonumber(fields[8]) or 0
    if countdown > 0 or wave == 0 then
        hud.title:SetText("|cffff7f2aThe Firepit|r")
        hud.timer:SetText(Clock(countdown))
        hud.line:SetText("The first wave is coming...")
    else
        hud.title:SetText((boss and "|cffff3020BOSS|r  " or "") .. "|cffff7f2aWave " .. wave .. "|r")
        hud.timer:SetText(Clock(left))
        hud.line:SetText(string.format("Enemies |cffff6040%d|r    Standing |cff60ff60%d|r/%d", alive, standing,
            total))
    end
    hudState.seen = GetTime()
    hudState.left = left
    hud:Show()
end

hud:SetScript("OnUpdate", function(self)
    if GetTime() - hudState.seen > HUD_TIMEOUT then
        self:Hide()
    end
end)

-- The results window ----------------------------------------------------------------------------------------------

-- The columns: header, the record's field, a formatter, width, the tooltip that explains it.
local COLUMNS = {
    { "Hero", "name", nil, 104, "The Hero and their level." },
    { "Peak", "peak", nil, 40, "The last wave they stood in." },
    { "Foes", "peakAlive", nil, 40, "The most enemies alive during their peak wave." },
    { "Most", "mostAlive", nil, 40, "The most enemies alive at once while they stood." },
    { "Damage", "damage", Short, 62, "Damage done (pets included)." },
    { "Healing", "healing", Short, 62, "Healing done." },
    { "Taken", "taken", Short, 62, "Damage taken." },
    { "Kills", "kills", nil, 40, "Killing blows (pets included)." },
    { "Boss", "bossKills", nil, 34, "Killing blows on bosses." },
    { "Hit", "hit", Short, 54, "Their biggest hit." },
    { "Hit taken", "hitTaken", Short, 62, "The biggest hit they took." },
    { "Fell", "fell", nil, 52, "Times they fell, and the wave of the last." },
    { "Rez", "rezzes", nil, 32, "Times they were brought back during the run." },
    { "Spoils", "loot", nil, 44, "Pieces of gear won (one every 15 waves survived)." },
    { "Felled by", "killer", nil, 120, "What dealt the last blow." },
}
local ROW_HEIGHT, MAX_ROWS = 20, 10

local results = CreateFrame("Frame", "EmberforgeFirepitResults", UIParent)
local width = 32
for _, column in ipairs(COLUMNS) do
    width = width + column[4]
end
results:SetWidth(width)
results:SetHeight(150 + ROW_HEIGHT * MAX_ROWS)
results:SetPoint("CENTER", 0, 40)
results:SetFrameStrata("DIALOG")
results:EnableMouse(true)
results:SetMovable(true)
results:RegisterForDrag("LeftButton")
results:SetScript("OnDragStart", results.StartMoving)
results:SetScript("OnDragStop", results.StopMovingOrSizing)
Backdrop(results)
results:Hide()
tinsert(UISpecialFrames, "EmberforgeFirepitResults")

results.title = results:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
results.title:SetPoint("TOP", 0, -14)
results.title:SetText("|cffff7f2aThe Firepit|r")
results.summary = results:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
results.summary:SetPoint("TOP", results.title, "BOTTOM", 0, -8)
results.detail = results:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
results.detail:SetPoint("TOP", results.summary, "BOTTOM", 0, -4)

local close = CreateFrame("Button", nil, results, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -2, -2)

local run = { players = {} }
local sortKey, sortDown = "peak", true

local headers, rows = {}, {}
local x = 16
for index, column in ipairs(COLUMNS) do
    local header = CreateFrame("Button", nil, results)
    header:SetWidth(column[4])
    header:SetHeight(18)
    header:SetPoint("TOPLEFT", x, -84)
    header.text = header:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    header.text:SetAllPoints()
    header.text:SetJustifyH(index == 1 and "LEFT" or "RIGHT")
    header.column = column
    header:SetScript("OnClick", function(self)
        if sortKey == self.column[2] then
            sortDown = not sortDown
        else
            sortKey, sortDown = self.column[2], self.column[2] ~= "name" and self.column[2] ~= "killer"
        end
        results:Refresh()
    end)
    header:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:AddLine(self.column[1], GOLD[1], GOLD[2], GOLD[3])
        GameTooltip:AddLine(self.column[5], 1, 1, 1, true)
        GameTooltip:AddLine("Click to sort.", 0.6, 0.6, 0.6)
        GameTooltip:Show()
    end)
    header:SetScript("OnLeave", function() GameTooltip:Hide() end)
    headers[index] = header
    x = x + column[4]
end

for index = 1, MAX_ROWS do
    local row = CreateFrame("Frame", nil, results)
    row:SetWidth(width - 32)
    row:SetHeight(ROW_HEIGHT)
    row:SetPoint("TOPLEFT", 16, -104 - (index - 1) * ROW_HEIGHT)
    row.shade = row:CreateTexture(nil, "BACKGROUND")
    row.shade:SetAllPoints()
    row.shade:SetTexture(1, 0.5, 0.15, index % 2 == 0 and 0.06 or 0)
    row.cells = {}
    local cellX = 0
    for column = 1, #COLUMNS do
        local cell = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        cell:SetWidth(COLUMNS[column][4])
        cell:SetHeight(ROW_HEIGHT)
        cell:SetPoint("LEFT", cellX, 0)
        cell:SetJustifyH(column == 1 and "LEFT" or "RIGHT")
        row.cells[column] = cell
        cellX = cellX + COLUMNS[column][4]
    end
    rows[index] = row
end

local function Sorted()
    local list = {}
    for _, player in ipairs(run.players) do
        list[#list + 1] = player
    end
    table.sort(list, function(a, b)
        local left, right = a[sortKey], b[sortKey]
        if left == right then
            return (a.peak or 0) > (b.peak or 0)
        end
        if type(left) == "string" or type(right) == "string" then
            left, right = tostring(left), tostring(right)
        end
        if sortDown then
            return left > right
        end
        return left < right
    end)
    return list
end

function results:Refresh()
    self.summary:SetText(string.format("Wave |cffff7f2a%d|r reached in %s  -  %d %s  -  %s kills", run.reached or 0,
        Clock(run.seconds), run.size or 1, (run.size or 1) == 1 and "Hero" or "Heroes", Thousands(run.kills)))
    self.detail:SetText(string.format("Monster health x%.1f for the group%s", (run.health or 100) / 100,
        (run.start or 1) > 1 and ("  -  started at wave " .. run.start) or ""))
    for index, header in ipairs(headers) do
        local column = COLUMNS[index]
        header.text:SetText(column[1] .. (sortKey == column[2] and (sortDown and " v" or " ^") or ""))
    end
    local list = Sorted()
    local me = UnitName("player")
    for index, row in ipairs(rows) do
        local player = list[index]
        if player then
            for column, cell in ipairs(row.cells) do
                local spec = COLUMNS[column]
                local value = player[spec[2]]
                if spec[2] == "name" then
                    value = player.name .. " |cff999999" .. player.level .. "|r"
                elseif spec[2] == "fell" then
                    value = player.deaths > 0 and (player.deaths .. " (w" .. player.deathWave .. ")") or "-"
                elseif spec[3] then
                    value = spec[3](value)
                end
                cell:SetText(value == "" and "-" or tostring(value))
                if player.name == me then
                    cell:SetTextColor(1, 0.82, 0.3)
                else
                    cell:SetTextColor(1, 1, 1)
                end
            end
            row:Show()
        else
            row:Hide()
        end
    end
end

-- Share: the result in group chat (or to yourself, alone).
local share = CreateFrame("Button", nil, results, "UIPanelButtonTemplate")
share:SetWidth(130)
share:SetHeight(24)
share:SetPoint("BOTTOMRIGHT", -16, 14)
share:SetText("Share in chat")
share:SetScript("OnClick", function()
    local best
    for _, player in ipairs(run.players) do
        if not best or player.damage > best.damage then
            best = player
        end
    end
    local text = string.format("The Firepit: wave %d reached by %d in %s, %s kills.", run.reached or 0,
        run.size or 1, Clock(run.seconds), Thousands(run.kills))
    if best then
        text = text .. string.format(" Top damage: %s (%s).", best.name, Short(best.damage))
    end
    local channel = (GetNumRaidMembers() > 0 and "RAID") or (GetNumPartyMembers() > 0 and "PARTY") or nil
    if channel then
        SendChatMessage(text, channel)
    else
        DEFAULT_CHAT_FRAME:AddMessage("|cffff7f2a" .. text .. "|r")
    end
end)

local hint = results:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
hint:SetPoint("BOTTOMLEFT", 18, 20)
hint:SetText("Claim your spoils chest, then speak to the Keeper to leave.  /firepit shows this again.")

-- The queue: the group finder's Firepit tab ------------------------------------------------------------------------

local finder = CreateFrame("Frame", "EmberforgeFirepitFinder", UIParent)
finder:SetWidth(352)
finder:SetHeight(480)
finder:SetPoint("TOPLEFT", 16, -116)
finder:EnableMouse(true)
finder:SetToplevel(true)
Backdrop(finder, 0.96)
finder:Hide()
UIPanelWindows = UIPanelWindows or {}
UIPanelWindows["EmberforgeFirepitFinder"] = { area = "left", pushable = 0, whileDead = 1 }

finder.icon = finder:CreateTexture(nil, "ARTWORK")
finder.icon:SetWidth(44)
finder.icon:SetHeight(44)
finder.icon:SetPoint("TOPLEFT", 14, -12)
finder.icon:SetTexture(ICON)
finder.title = finder:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
finder.title:SetPoint("TOPLEFT", finder.icon, "TOPRIGHT", 10, -2)
finder.title:SetText("|cffff7f2aThe Firepit|r")
finder.subtitle = finder:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
finder.subtitle:SetPoint("TOPLEFT", finder.title, "BOTTOMLEFT", 0, -4)
finder.subtitle:SetText("Endless waves in the Ring of Law. Solo, or up to 10.")
local finderClose = CreateFrame("Button", nil, finder, "UIPanelCloseButton")
finderClose:SetPoint("TOPRIGHT", -2, -2)
finderClose:SetScript("OnClick", function() HideUIPanel(finder) end)

finder.rules = finder:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
finder.rules:SetPoint("TOPLEFT", 16, -68)
finder.rules:SetWidth(320)
finder.rules:SetJustifyH("LEFT")
finder.rules:SetText("A wave every |cffff7f2a60 sec|r, ready or not; the ones you don't kill stay. Each wave is "
    .. "|cffff7f2a10%|r stronger, a boss every 5th. You fight until you all fall. Gear for every 15 waves you live "
    .. "through, epic from 45.")

local function Label(text, y)
    local label = finder:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    label:SetPoint("TOPLEFT", 16, y)
    label:SetText(text)
    return label
end

finder.groupLabel = Label("Your group", -126)
finder.difficulty = finder:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
finder.difficulty:SetPoint("TOPRIGHT", -16, -128)

local MEMBER_HEIGHT = 15
local members = {}
for index = 1, MOST_HEROES do
    local row = CreateFrame("Frame", nil, finder)
    row:SetWidth(320)
    row:SetHeight(MEMBER_HEIGHT)
    row:SetPoint("TOPLEFT", 16, -144 - (index - 1) * MEMBER_HEIGHT)
    row.status = row:CreateTexture(nil, "ARTWORK")
    row.status:SetWidth(13)
    row.status:SetHeight(13)
    row.status:SetPoint("LEFT", 0, 0)
    row.name = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    row.name:SetPoint("LEFT", 18, 0)
    row.note = row:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    row.note:SetPoint("RIGHT", 0, 0)
    members[index] = row
end

finder.bestLabel = Label("Your best", -300)
finder.best = finder:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
finder.best:SetPoint("LEFT", finder.bestLabel, "RIGHT", 8, 0)

finder.boardLabel = Label("Leaderboard", -322)
local board = { side = 0, rows = { [0] = {}, [1] = {} }, entries = { [0] = {}, [1] = {} } }
local sideButtons = {}
for side, text in pairs({ [0] = "Solo", [1] = "Groups" }) do
    local button = CreateFrame("Button", nil, finder, "UIPanelButtonTemplate")
    button:SetWidth(64)
    button:SetHeight(18)
    button:SetPoint("TOPRIGHT", side == 0 and -84 or -16, -320)
    button:SetText(text)
    button.side = side
    sideButtons[side] = button
end
local boardRows = {}
for index = 1, 5 do
    local row = finder:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    row:SetPoint("TOPLEFT", 22, -340 - (index - 1) * 13)
    row:SetWidth(300)
    row:SetJustifyH("LEFT")
    boardRows[index] = row
end

finder.status = finder:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
finder.status:SetPoint("BOTTOM", 0, 44)
finder.status:SetWidth(320)
local queueButton = CreateFrame("Button", nil, finder, "UIPanelButtonTemplate")
queueButton:SetWidth(180)
queueButton:SetHeight(26)
queueButton:SetPoint("BOTTOM", 0, 14)

local queue = { state = nil, best = 0, bestSize = 0, level = 10, answers = {}, leader = nil, why = nil }

-- The group as the client knows it: { name, level, online } (yourself first when in a party).
local function Roster()
    local list = {}
    local raid = GetNumRaidMembers and GetNumRaidMembers() or 0
    if raid > 0 then
        for i = 1, raid do
            local name, _, _, level, _, _, _, online = GetRaidRosterInfo(i)
            if name then
                list[#list + 1] = { name = name, level = level or 0, online = online }
            end
        end
    else
        list[1] = { name = UnitName("player"), level = UnitLevel("player"), online = true }
        for i = 1, (GetNumPartyMembers and GetNumPartyMembers() or 0) do
            local unit = "party" .. i
            list[#list + 1] = { name = UnitName(unit) or "?", level = UnitLevel(unit) or 0,
                online = UnitIsConnected(unit) }
        end
    end
    return list
end

local function Leading(list)
    if #list <= 1 then
        return true
    end
    if GetNumRaidMembers and GetNumRaidMembers() > 0 then
        return IsRaidLeader and IsRaidLeader()
    end
    return IsPartyLeader and IsPartyLeader()
end

function finder:Refresh()
    local list = Roster()
    local problem
    for index, row in ipairs(members) do
        local member = list[index]
        if member then
            local answer = queue.answers[member.name]
            row.status:SetTexture(answer == 1 and READY or answer == 2 and NOT_READY
                or (queue.state == "proposal" and WAITING) or nil)
            row.name:SetText(member.name .. "  |cff999999" .. member.level .. "|r")
            if not member.online then
                row.note:SetText("|cffff4040Offline|r")
            elseif member.level < queue.level then
                row.note:SetText("|cffff4040Needs level " .. queue.level .. "|r")
                problem = problem or (member.name .. " must be level " .. queue.level .. ".")
            else
                row.note:SetText("")
            end
            row:Show()
        else
            row:Hide()
        end
    end
    local size = math.max(1, math.min(#list, MOST_HEROES))
    if #list > MOST_HEROES then
        problem = "The Firepit takes at most 10."
    end
    self.difficulty:SetText(string.format("Monsters: health x%.1f, numbers x%.1f", 1 + GROUP_HEALTH * (size - 1),
        1 + GROUP_COUNT * (size - 1)))
    self.best:SetText(queue.best > 0 and ("wave " .. queue.best .. (queue.bestSize > 1
        and (" (group of " .. queue.bestSize .. ")") or " (solo)")) or "|cff999999none yet|r")

    for side, button in pairs(sideButtons) do
        if side == board.side then
            button:LockHighlight()
        else
            button:UnlockHighlight()
        end
    end
    local entries = board.entries[board.side]
    for index, row in ipairs(boardRows) do
        local entry = entries[index]
        row:SetText(entry and string.format("%d.  %s  -  wave |cffff7f2a%d|r%s", entry.rank, entry.name, entry.wave,
            entry.size > 1 and (" |cff999999(" .. entry.size .. ")|r") or "") or (index == 1 and
            "|cff999999No one yet.|r" or ""))
    end

    local leading = Leading(list)
    if queue.state == "proposal" then
        local accepted = 0
        for _, member in ipairs(list) do
            if queue.answers[member.name] == 1 then
                accepted = accepted + 1
            end
        end
        self.status:SetText(string.format("Waiting for the group: %d of %d ready.", accepted, #list))
        queueButton:SetText(leading and "Leave Queue" or "Decline")
        queueButton:Enable()
    else
        self.status:SetText(queue.why and ("|cffff4040" .. queue.why .. "|r") or (problem and ("|cffff4040"
            .. problem .. "|r") or ""))
        queueButton:SetText(#list > 1 and "Queue my group" or "Enter the Firepit")
        if leading and not problem and UnitLevel("player") >= queue.level then
            queueButton:Enable()
        else
            queueButton:Disable()
            if not leading then
                self.status:SetText("Only your group's leader can queue.")
            end
        end
    end
end

for side, button in pairs(sideButtons) do
    button:SetScript("OnClick", function(self)
        board.side = self.side
        finder:Refresh()
    end)
end

queueButton:SetScript("OnClick", function()
    if queue.state == "proposal" then
        Send(Leading(Roster()) and "LEAVE" or "DECLINE")
    else
        queue.why = nil
        Send("QUEUE")
    end
end)

finder:SetScript("OnShow", function(self)
    queue.why = nil
    self:Refresh()
    Send("STATE")
end)
finder:RegisterEvent("PARTY_MEMBERS_CHANGED")
finder:RegisterEvent("RAID_ROSTER_UPDATE")
finder:RegisterEvent("PLAYER_LEVEL_UP")
finder:SetScript("OnEvent", function(self)
    if self:IsShown() then
        self:Refresh()
    end
end)

StaticPopupDialogs = StaticPopupDialogs or {}
StaticPopupDialogs["EMBERFORGE_FIREPIT_PROPOSAL"] = {
    text = "%s is taking your group into the Firepit. Waves come every 60 seconds and never stop: you fight until "
        .. "you all fall. Enter?",
    button1 = ACCEPT or "Accept",
    button2 = DECLINE or "Decline",
    OnAccept = function() Send("ACCEPT") end,
    OnCancel = function(_, _, reason)
        if reason ~= "override" then
            Send("DECLINE")
        end
    end,
    timeout = 40,
    whileDead = 1,
    hideOnEscape = 0,
}

local function OpenFinder()
    if ShowUIPanel then
        ShowUIPanel(finder)
    else
        finder:Show()
    end
end

local function OnQueueMessage(kind, fields)
    if kind == "OPEN" then
        OpenFinder()
    elseif kind == "S" then
        queue.best, queue.bestSize = tonumber(fields[2]) or 0, tonumber(fields[3]) or 0
        if fields[4] ~= "1" and queue.state == "proposal" then
            queue.state = nil
        end
        queue.level = tonumber(fields[5]) or queue.level
        board.entries = { [0] = {}, [1] = {} }
    elseif kind == "L" then
        local side = tonumber(fields[2]) or 0
        board.entries[side] = board.entries[side] or {}
        table.insert(board.entries[side], { rank = tonumber(fields[3]) or 0, name = fields[4] or "?",
            wave = tonumber(fields[5]) or 0, size = tonumber(fields[6]) or 1 })
    elseif kind == "QP" then
        queue.state, queue.leader, queue.why, queue.answers = "proposal", fields[2], nil, {}
        if fields[2] ~= UnitName("player") then
            if PlaySound then
                PlaySound("ReadyCheck")
            end
            StaticPopup_Show("EMBERFORGE_FIREPIT_PROPOSAL", fields[2])
        end
    elseif kind == "QA" then
        queue.answers[fields[2] or "?"] = tonumber(fields[3]) or 0
    elseif kind == "QC" then
        queue.state, queue.answers, queue.why = nil, {}, fields[2]
        StaticPopup_Hide("EMBERFORGE_FIREPIT_PROPOSAL")
        DEFAULT_CHAT_FRAME:AddMessage("|cffff7f2aThe Firepit:|r " .. (fields[2] or "the queue ended."))
    elseif kind == "QG" then
        queue.state, queue.answers, queue.why = nil, {}, nil
        StaticPopup_Hide("EMBERFORGE_FIREPIT_PROPOSAL")
        if HideUIPanel then
            HideUIPanel(finder)
        else
            finder:Hide()
        end
    else
        return false
    end
    if finder:IsShown() then
        finder:Refresh()
    end
    return true
end

-- Messages ----------------------------------------------------------------------------------------------------------

local pending

local function OnMessage(message)
    local fields = Split(message)
    local kind = fields[1]
    if OnQueueMessage(kind, fields) then
        return
    end
    if kind == "H" then
        ShowHud(fields)
    elseif kind == "RUN" then
        pending = { reached = tonumber(fields[2]) or 0, seconds = tonumber(fields[3]) or 0,
            size = tonumber(fields[4]) or 1, kills = tonumber(fields[5]) or 0, health = tonumber(fields[6]) or 100,
            start = tonumber(fields[7]) or 1, players = {} }
    elseif kind == "P" and pending then
        local n = function(i) return tonumber(fields[i]) or 0 end
        pending.players[#pending.players + 1] = {
            name = fields[2] or "?", level = n(3), peak = n(4), peakAlive = n(5), mostAlive = n(6), damage = n(7),
            healing = n(8), taken = n(9), kills = n(10), bossKills = n(11), hit = n(12), hitTaken = n(13),
            deaths = n(14), deathWave = n(15), rezzes = n(16), loot = n(17), killer = fields[18] or "",
            fell = n(14) * 1000 + n(15),
        }
    elseif kind == "END" and pending then
        run = pending
        pending = nil
        EmberforgeFirepitLast = run
        hud:Hide()
        results:Refresh()
        results:Show()
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("ADDON_LOADED")
events:SetScript("OnEvent", function(_, event, prefix, message)
    if event == "ADDON_LOADED" then
        if prefix == "Emberforge_Firepit" and EmberforgeFirepitLast then
            run = EmberforgeFirepitLast
        end
    elseif prefix == PREFIX and message then
        OnMessage(message)
    end
end)

SLASH_EMBERFORGEFIREPIT1 = "/firepit"
SlashCmdList.EMBERFORGEFIREPIT = function()
    if #run.players == 0 then
        DEFAULT_CHAT_FRAME:AddMessage("|cffff7f2aThe Firepit:|r no run yet. Find the Firepit Keeper beside Silas "
            .. "Darkmoon in Stormwind or Orgrimmar.")
        return
    end
    results:Refresh()
    results:Show()
end

-- For the tests.
EmberforgeFirepitDebug = { hud = hud, results = results, rows = rows, headers = headers, OnMessage = OnMessage,
    Run = function() return run end, finder = finder, members = members, boardRows = boardRows,
    queueButton = queueButton, sideButtons = sideButtons }

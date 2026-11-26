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
--   S,<best solo>,<duo>,<trio>,<group>,<in a proposal 0|1>,<level it opens at>,<latest start wave>   then each
--     bracket's leaderboard: L,<bracket 1-4>,<rank>,<name>,<wave>,<group size> ... LE
--   QP,<leader>,<Heroes>,<sec>,<start wave>   your group's leader queued: accept or decline
--   QA,<name>,<0 waiting|1 accepted|2 declined>    QC,<why>   the queue ended    QG   everyone accepted: going in
-- To the server: STATE, QUEUE[,<start wave>] (alone: straight in; a leader: the group is asked), ACCEPT, DECLINE,
-- LEAVE.

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
-- Two columns: on the left what the Firepit is, its rewards and your group (a slot a Hero, their answer on it); on
-- the right your records by group size over the fire (each one's leaderboard on hover), where to start from, and the
-- button. Along the bottom, how much tougher your group's size makes it.

local ART = "Interface\\AddOns\\Emberforge_Firepit\\Art\\"
local BRACKETS = { "Best Solo", "Best Duo", "Best Trio", "Best Group" }
local CHECKPOINT_EVERY = 5
local REWARDS = {
    { "Interface\\Icons\\INV_Box_01", "Spoils of the Firepit",
        "Your own chest when the run ends: nobody rolls on your loot." },
    { "Interface\\Icons\\INV_Sword_04", "Rare gear",
        "A piece of gear at your level for every 15 waves you live through." },
    { "Interface\\Icons\\INV_Shield_23", "Epic gear", "From wave 45, the pieces are epic." },
    { "Interface\\Icons\\INV_Misc_Head_Dragon_Red", "Records",
        "Your best wave, solo, as a duo, a trio and a group, on the leaderboard." },
}

local finder = CreateFrame("Frame", "EmberforgeFirepitFinder", UIParent)
finder:SetWidth(560)
finder:SetHeight(344)
finder:SetPoint("TOPLEFT", 16, -116)
finder:EnableMouse(true)
finder:SetToplevel(true)
Backdrop(finder, 0.97)
finder:Hide()
UIPanelWindows = UIPanelWindows or {}
UIPanelWindows["EmberforgeFirepitFinder"] = { area = "left", pushable = 0, whileDead = 1 }

-- The background: the Ring of Law burning (Art\Background: a 16:9 painting squeezed into 1024x512, unsqueezed
-- here to the panel's shape), darker under the left column's words.
finder.art = finder:CreateTexture(nil, "BACKGROUND")
finder.art:SetPoint("TOPLEFT", 4, -4)
finder.art:SetPoint("BOTTOMRIGHT", -4, 4)
finder.art:SetTexture(ART .. "Background")
finder.art:SetTexCoord(0.042, 0.958, 0, 1)
finder.shade = finder:CreateTexture(nil, "BORDER")
finder.shade:SetPoint("TOPLEFT", 4, -4)
finder.shade:SetPoint("BOTTOMLEFT", 4, 4)
finder.shade:SetWidth(360)
finder.shade:SetTexture(1, 1, 1, 1)
if finder.shade.SetGradientAlpha then
    finder.shade:SetGradientAlpha("HORIZONTAL", 0.03, 0.015, 0.01, 0.88, 0.03, 0.015, 0.01, 0)
end

local finderClose = CreateFrame("Button", nil, finder, "UIPanelCloseButton")
finderClose:SetPoint("TOPRIGHT", -2, -2)
finderClose:SetScript("OnClick", function() HideUIPanel(finder) end)

local function Text(font, x, y, text)
    local line = finder:CreateFontString(nil, "OVERLAY", font)
    line:SetPoint("TOPLEFT", x, y)
    line:SetJustifyH("LEFT")
    if text then
        line:SetText(text)
    end
    return line
end

finder.title = Text("GameFontNormalHuge", 16, -14, "|cffff9a3aThe Firepit|r")
finder.rules = Text("GameFontHighlightSmall", 16, -44)
finder.rules:SetWidth(314)
finder.rules:SetText("An endless wave arena in a private Ring of Law. Enter solo, or with a group of up to 10. A wave "
    .. "comes every |cffff9a3a60 sec|r, ready or not, and what you don't kill stays. Each wave is |cffff9a3a10%|r "
    .. "stronger than the last; every 5th brings a boss. You fight until you all fall.\n\nMonsters and spoils are "
    .. "scaled to your level.")

finder.rewardsLabel = Text("GameFontNormalLarge", 16, -140, "|cffff9a3aPotential Rewards|r")
local rewardButtons = {}
for index, reward in ipairs(REWARDS) do
    local button = CreateFrame("Button", nil, finder)
    button:SetWidth(36)
    button:SetHeight(36)
    button:SetPoint("TOPLEFT", 16 + (index - 1) * 42, -164)
    button.icon = button:CreateTexture(nil, "ARTWORK")
    button.icon:SetAllPoints()
    button.icon:SetTexture(reward[1])
    button.reward = reward
    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:AddLine(self.reward[2], 1, 0.82, 0)
        GameTooltip:AddLine(self.reward[3], 1, 1, 1, true)
        GameTooltip:Show()
    end)
    button:SetScript("OnLeave", function() GameTooltip:Hide() end)
    rewardButtons[index] = button
end

finder.groupLabel = Text("GameFontNormalLarge", 16, -214, "|cffff9a3aYour Group|r")
finder.groupNote = Text("GameFontHighlightSmall", 16, -236, "Up to 10 Heroes. Everyone must accept the queue.")

-- A slot a Hero: their portrait, their answer in the corner, red when they cannot come.
local SLOT, SLOT_STEP = 26, 30
local members = {}
for index = 1, MOST_HEROES do
    local slot = CreateFrame("Button", nil, finder)
    slot:SetWidth(SLOT)
    slot:SetHeight(SLOT)
    slot:SetPoint("TOPLEFT", 16 + (index - 1) * SLOT_STEP, -254)
    slot:SetBackdrop({ bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 8,
        insets = { left = 2, right = 2, top = 2, bottom = 2 } })
    slot:SetBackdropColor(0, 0, 0, 0.8)
    slot.portrait = slot:CreateTexture(nil, "ARTWORK")
    slot.portrait:SetPoint("TOPLEFT", 3, -3)
    slot.portrait:SetPoint("BOTTOMRIGHT", -3, 3)
    slot.empty = slot:CreateFontString(nil, "OVERLAY", "GameFontDisableLarge")
    slot.empty:SetPoint("CENTER", 0, 1)
    slot.empty:SetText("+")
    slot.status = slot:CreateTexture(nil, "OVERLAY")
    slot.status:SetWidth(13)
    slot.status:SetHeight(13)
    slot.status:SetPoint("BOTTOMRIGHT", 4, -4)
    slot:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        if self.member then
            GameTooltip:AddLine(self.member.name .. "  |cff999999level " .. self.member.level .. "|r", 1, 1, 1)
            if self.why then
                GameTooltip:AddLine(self.why, 1, 0.25, 0.25, true)
            elseif self.answer == 1 then
                GameTooltip:AddLine("Ready.", 0.3, 1, 0.3)
            elseif self.answer == 0 then
                GameTooltip:AddLine("Deciding...", 1, 0.82, 0)
            end
        else
            GameTooltip:AddLine("An open slot", 0.7, 0.7, 0.7)
            GameTooltip:AddLine("Invite a Hero to bring them along.", 1, 1, 1, true)
        end
        GameTooltip:Show()
    end)
    slot:SetScript("OnLeave", function() GameTooltip:Hide() end)
    members[index] = slot
end
finder.status = Text("GameFontNormalSmall", 16, -296)
finder.status:SetWidth(314)

-- Records over the fire, each with its leaderboard on hover.
local board = { [1] = {}, [2] = {}, [3] = {}, [4] = {} }
local records = {}
for index, title in ipairs(BRACKETS) do
    local record = CreateFrame("Frame", nil, finder)
    record:SetWidth(180)
    record:SetHeight(46)
    record:SetPoint("TOPRIGHT", -18, -14 - (index - 1) * 52)
    record:EnableMouse(true)
    record.label = record:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    record.label:SetPoint("TOPRIGHT", 0, 0)
    record.label:SetText("|cffffd100" .. title .. "|r")
    record.value = record:CreateFontString(nil, "OVERLAY", "GameFontHighlightHuge")
    record.value:SetPoint("TOPRIGHT", record.label, "BOTTOMRIGHT", 0, -3)
    record.value:SetText("0")
    record.bracket = index
    record:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        GameTooltip:AddLine(BRACKETS[self.bracket]:gsub("Best ", "") .. " leaderboard", 1, 0.6, 0.2)
        local entries = board[self.bracket]
        if #entries == 0 then
            GameTooltip:AddLine("No one yet.", 0.7, 0.7, 0.7)
        end
        for _, entry in ipairs(entries) do
            GameTooltip:AddDoubleLine(entry.rank .. ". " .. entry.name .. (self.bracket == 4 and
                (" |cff999999(" .. entry.size .. ")|r") or ""), "wave " .. entry.wave, 1, 1, 1, 1, 0.6, 0.2)
        end
        GameTooltip:Show()
    end)
    record:SetScript("OnLeave", function() GameTooltip:Hide() end)
    records[index] = record
end

-- Where the run starts: wave 1, or any 5th wave everyone in the group has reached.
local start = CreateFrame("Frame", nil, finder)
start:SetWidth(200)
start:SetHeight(24)
start:SetPoint("BOTTOMRIGHT", -18, 74)
start:SetBackdrop({ bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10,
    insets = { left = 2, right = 2, top = 2, bottom = 2 } })
start:SetBackdropColor(0, 0, 0, 0.85)
start:SetBackdropBorderColor(0.6, 0.45, 0.25, 1)
start.text = start:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
start.text:SetPoint("CENTER")
local startPrev = CreateFrame("Button", nil, start)
local startNext = CreateFrame("Button", nil, start)
for _, pair in ipairs({ { startPrev, "Prev", "LEFT", 2 }, { startNext, "Next", "RIGHT", -2 } }) do
    local button = pair[1]
    button:SetWidth(20)
    button:SetHeight(20)
    button:SetPoint(pair[3], pair[4], 0)
    button:SetNormalTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. pair[2] .. "Page-Up")
    button:SetPushedTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. pair[2] .. "Page-Down")
    button:SetDisabledTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. pair[2] .. "Page-Disabled")
end

local queueButton = CreateFrame("Button", nil, finder, "UIPanelButtonTemplate")
queueButton:SetWidth(200)
queueButton:SetHeight(28)
queueButton:SetPoint("BOTTOMRIGHT", -18, 40)

-- The bottom bar: the group's difficulty and the level it opens at.
finder.bar = CreateFrame("Frame", nil, finder)
finder.bar:SetPoint("BOTTOMLEFT", 6, 6)
finder.bar:SetPoint("BOTTOMRIGHT", -6, 6)
finder.bar:SetHeight(24)
finder.bar:SetBackdrop({ bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10,
    insets = { left = 2, right = 2, top = 2, bottom = 2 } })
finder.bar:SetBackdropColor(0, 0, 0, 0.8)
finder.bar:SetBackdropBorderColor(0.6, 0.45, 0.25, 1)
finder.difficulty = finder.bar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
finder.difficulty:SetPoint("LEFT", 10, 0)
finder.requirement = finder.bar:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
finder.requirement:SetPoint("RIGHT", -10, 0)

local queue = { state = nil, best = { 0, 0, 0, 0 }, level = 10, checkpoint = 1, start = 1, answers = {},
    leader = nil, why = nil }

-- The group as the client knows it: { name, level, online, unit } (yourself first when in a party).
local function Roster()
    local list = {}
    local raid = GetNumRaidMembers and GetNumRaidMembers() or 0
    if raid > 0 then
        for i = 1, raid do
            local name, _, _, level, _, _, _, online = GetRaidRosterInfo(i)
            if name then
                list[#list + 1] = { name = name, level = level or 0, online = online, unit = "raid" .. i }
            end
        end
    else
        list[1] = { name = UnitName("player"), level = UnitLevel("player"), online = true, unit = "player" }
        for i = 1, (GetNumPartyMembers and GetNumPartyMembers() or 0) do
            local unit = "party" .. i
            list[#list + 1] = { name = UnitName(unit) or "?", level = UnitLevel(unit) or 0,
                online = UnitIsConnected(unit), unit = unit }
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
    for index, slot in ipairs(members) do
        local member = list[index]
        slot.member, slot.why, slot.answer = member, nil, nil
        if member then
            slot.empty:Hide()
            if SetPortraitTexture then
                SetPortraitTexture(slot.portrait, member.unit)
            end
            slot.portrait:Show()
            if not member.online then
                slot.why = "Offline."
            elseif member.level < queue.level then
                slot.why = "Needs level " .. queue.level .. "."
                problem = problem or (member.name .. " must be level " .. queue.level .. ".")
            end
            local answer = queue.answers[member.name]
            slot.answer = queue.state == "proposal" and (answer or 0) or nil
            slot.status:SetTexture(slot.why and NOT_READY or answer == 1 and READY or answer == 2 and NOT_READY
                or (queue.state == "proposal" and WAITING) or nil)
            slot:SetBackdropBorderColor(slot.why and 1 or 0.86, slot.why and 0.2 or 0.64, slot.why and 0.2 or 0.32, 1)
        else
            slot.empty:Show()
            slot.portrait:Hide()
            slot.status:SetTexture(nil)
            slot:SetBackdropBorderColor(0.35, 0.3, 0.25, 1)
        end
    end
    local size = math.max(1, math.min(#list, MOST_HEROES))
    if #list > MOST_HEROES then
        problem = "The Firepit takes at most 10."
    end
    self.difficulty:SetText(string.format("Monsters for %d: health |cffff9a3ax%.1f|r, numbers |cffff9a3ax%.1f|r",
        size, 1 + GROUP_HEALTH * (size - 1), 1 + GROUP_COUNT * (size - 1)))
    self.requirement:SetText("Level " .. queue.level .. "+  -  a wave every 60 sec")
    for index, record in ipairs(records) do
        record.value:SetText(tostring(queue.best[index] or 0))
    end

    queue.start = math.min(queue.start, queue.checkpoint)
    start.text:SetText("Start at |cffff9a3aWave " .. queue.start .. "|r")
    if queue.start > 1 then
        startPrev:Enable()
    else
        startPrev:Disable()
    end
    if queue.start < queue.checkpoint then
        startNext:Enable()
    else
        startNext:Disable()
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
        queueButton:SetText(#list > 1 and ("Queue Group (" .. #list .. ")") or "Enter the Firepit")
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

startPrev:SetScript("OnClick", function()
    queue.start = queue.start <= CHECKPOINT_EVERY and 1 or queue.start - CHECKPOINT_EVERY
    finder:Refresh()
end)
startNext:SetScript("OnClick", function()
    queue.start = queue.start == 1 and CHECKPOINT_EVERY or queue.start + CHECKPOINT_EVERY
    finder:Refresh()
end)

queueButton:SetScript("OnClick", function()
    if queue.state == "proposal" then
        Send(Leading(Roster()) and "LEAVE" or "DECLINE")
    else
        queue.why = nil
        Send(queue.start > 1 and ("QUEUE," .. queue.start) or "QUEUE")
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
    text = "%s is taking your group into the Firepit%s. Waves come every 60 seconds and never stop: you fight until "
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
        for index = 1, 4 do
            queue.best[index] = tonumber(fields[index + 1]) or 0
        end
        if fields[6] ~= "1" and queue.state == "proposal" then
            queue.state = nil
        end
        queue.level = tonumber(fields[7]) or queue.level
        queue.checkpoint = math.max(1, tonumber(fields[8]) or 1)
        board = { [1] = {}, [2] = {}, [3] = {}, [4] = {} }
    elseif kind == "L" then
        local bracket = tonumber(fields[2]) or 1
        board[bracket] = board[bracket] or {}
        table.insert(board[bracket], { rank = tonumber(fields[3]) or 0, name = fields[4] or "?",
            wave = tonumber(fields[5]) or 0, size = tonumber(fields[6]) or 1 })
    elseif kind == "QP" then
        queue.state, queue.leader, queue.why, queue.answers = "proposal", fields[2], nil, {}
        if fields[2] ~= UnitName("player") then
            if PlaySound then
                PlaySound("ReadyCheck")
            end
            local from = tonumber(fields[5]) or 1
            StaticPopup_Show("EMBERFORGE_FIREPIT_PROPOSAL", fields[2], from > 1 and (" from wave " .. from) or "")
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
    Run = function() return run end, finder = finder, members = members, records = records,
    queueButton = queueButton, start = start, startPrev = startPrev, startNext = startNext }

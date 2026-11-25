-- Emberforge Firepit: the wave bar during a Firepit run (wave, time left, enemies alive, Heroes standing), and the
-- results window when the run ends: the run's totals and a row for each Hero, sortable by any column, with a button
-- that shares the result in group chat. /firepit opens the last results again.
--
-- The server (mod-titan, src/firepit.cpp) talks in addon whispers to the player, prefix EFFP:
--   H,<wave>,<sec left>,<enemies alive>,<boss wave 0|1>,<Heroes standing>,<Heroes in the run>,<countdown sec>
--   RUN,<wave reached>,<sec survived>,<group size>,<kills>,<monster health %>,<start wave>
--   P,<name>,<level>,<peak wave>,<enemies at peak>,<most enemies>,<damage>,<healing>,<taken>,<kills>,<boss kills>,
--     <biggest hit>,<biggest hit taken>,<deaths>,<wave fallen on>,<times resurrected>,<spoils>,<killer>
--   END   the results are complete: show them

local PREFIX = "EFFP"
local GOLD = { 1, 0.82, 0 }
local EMBER = { 1, 0.5, 0.15 }
local HUD_TIMEOUT = 4          -- sec without news: the run is over, the bar goes

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

-- Messages ----------------------------------------------------------------------------------------------------------

local pending

local function OnMessage(message)
    local fields = Split(message)
    local kind = fields[1]
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
    Run = function() return run end }

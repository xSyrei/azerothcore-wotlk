-- Emberforge Firepit: the Firepit's queue (a tab of the group finder, beside Dungeons and Player vs. Player: see
-- Emberforge_Journal/MicroMenu.lua), the wave tracker on the right during a run (wave, its time on a bar, enemies
-- alive, Heroes standing, the run's news), and the recap when the run ends: the run in big numbers, who stood out,
-- and a row for each Hero, sortable by any column, with a button that shares the result in group chat. /firepit
-- opens the last recap again. Inside the Firepit, the minimap's dungeon finder eye shows, as in any dungeon: its menu
-- takes you out; and the Firepit's music plays (PressurePoint.mp3, over the zone's).
--
-- The server (mod-titan, src/firepit.cpp) talks in addon whispers to the player, prefix EFFP:
--   X,<affix name>,... the world affixes active now
--   F,<melee>,<ranged>,<healers>,<bosses>,<other>,<queued>,<badges>: the Enemy Forces
--   H,<wave>,<sec left>,<enemies alive>,<boss wave 0|1>,<Heroes standing>,<Heroes in the run>,<countdown sec>,
--     <Pressure stacks (5% more enemy damage each)>,<the field's cap>,<reinforcements waiting>,
--     <Marked for Carnage: sec left, 0 none>
--   RUN,<wave reached>,<sec survived>,<group size>,<kills>,<monster health %>,<start wave>
--   P,<name>,<level>,<peak wave>,<enemies at peak>,<most enemies>,<damage>,<healing>,<taken>,<kills>,<boss kills>,
--     <biggest hit>,<biggest hit taken>,<deaths>,<wave fallen on>,<times resurrected>,<spoils>,<killer>
--   END   the results are complete: show them
--   A,<text>,<urgent 0|1>   the run's news, on the wave tracker    INTRO   you arrived: the Firepit's title
--   OPEN  open the queue (the Firepit Keeper sent you)
--   S,<best solo>,<duo>,<trio>,<group>,<in a proposal 0|1>,<level it opens at>,<latest start wave>   then each
--     bracket's leaderboard: L,<bracket 1-4>,<rank>,<name>,<wave>,<group size> ... LE
--   QP,<leader>,<Heroes>,<sec>,<start wave>   your group's leader queued: accept or decline
--   QA,<name>,<0 waiting|1 accepted|2 declined>    QC,<why>   the queue ended    QG   everyone accepted: going in
-- To the server: STATE, QUEUE[,<start wave>] (alone: straight in; a leader: the group is asked), ACCEPT, DECLINE,
-- LEAVE, EXIT (the eye's Teleport out: home, where you came in from).

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

-- The look the three windows share: the painting of the ring, a dark veil, thin bronze lines, cream capitals, ember.
local ART = "Interface\\AddOns\\Emberforge_Firepit\\Art\\"
-- The Enemy Forces tracker's icons, in the server's order (src/firepit.cpp LookOf): boss, tank, melee, ranged,
-- caster, healer.
local ROLE_ICONS = { "RoleBoss", "RoleTank", "RoleMelee", "RoleRanged", "RoleCaster", "RoleHealer" }
local FONT = "Fonts\\FRIZQT__.TTF"
local CREAM, PALE, BODY = { 0.98, 0.88, 0.72 }, { 0.84, 0.76, 0.66 }, { 0.98, 0.95, 0.9 }
local BRONZE, EMBER_LINE = { 0.55, 0.41, 0.3 }, { 1, 0.5, 0.16 }

local function Label(parent, size, color, layer)
    local text = parent:CreateFontString(nil, layer or "OVERLAY", "GameFontNormal")
    text:SetFont(FONT, size)
    text:SetTextColor(color[1], color[2], color[3])
    text:SetShadowOffset(1, -1)
    text:SetShadowColor(0, 0, 0, 0.9)
    return text
end

-- A thin line: bronze, or ember fading out to the right.
local function Line(parent, x, y, width, height, ember)
    local line = parent:CreateTexture(nil, "ARTWORK")
    line:SetPoint("TOPLEFT", x, -y)
    line:SetWidth(width)
    line:SetHeight(height or 1)
    line:SetTexture(1, 1, 1, 1)
    local color = ember and EMBER_LINE or BRONZE
    if line.SetGradientAlpha then
        line:SetGradientAlpha("HORIZONTAL", color[1], color[2], color[3], ember and 1 or 0.75, color[1], color[2],
            color[3], ember and 0 or 0.75)
    end
    return line
end

-- The painting under its veil, a bronze edge all round.
local function Dress(frame, veil)
    frame.art = frame:CreateTexture(nil, "BACKGROUND")
    frame.art:SetAllPoints()
    frame.art:SetTexture(ART .. "Background")
    frame.veil = frame:CreateTexture(nil, "BORDER")
    frame.veil:SetAllPoints()
    frame.veil:SetTexture(0.04, 0.02, 0.015, veil)
    for _, edge in ipairs({ { "TOPLEFT", "TOPRIGHT", true }, { "BOTTOMLEFT", "BOTTOMRIGHT", true },
        { "TOPLEFT", "BOTTOMLEFT", false }, { "TOPRIGHT", "BOTTOMRIGHT", false } }) do
        local line = frame:CreateTexture(nil, "ARTWORK")
        line:SetTexture(BRONZE[1], BRONZE[2], BRONZE[3], 0.9)
        line:SetPoint(edge[1])
        line:SetPoint(edge[2])
        if edge[3] then
            line:SetHeight(1)
        else
            line:SetWidth(1)
        end
    end
end

local function Movable(frame)
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:SetClampedToScreen(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", frame.StartMoving)
    frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
end

-- The wave tracker ------------------------------------------------------------------------------------------------
-- On the right, under the minimap (as a dungeon's objectives): the wave, its time running out on a bar, the enemies
-- alive, the Heroes standing, and the run's news (a boss coming, the last one falling). Drag it anywhere.

local HUD_W, HUD_H = 236, 196
local hud = CreateFrame("Frame", "EmberforgeFirepitHud", UIParent)
hud:SetWidth(HUD_W)
hud:SetHeight(HUD_H)
hud:SetPoint("TOPRIGHT", UIParent, "TOPRIGHT", -24, -250)
hud:SetFrameStrata("MEDIUM")
Movable(hud)
hud:Hide()
hud.shade = hud:CreateTexture(nil, "BACKGROUND")
hud.shade:SetAllPoints()
hud.shade:SetTexture(1, 1, 1, 1)
if hud.shade.SetGradientAlpha then
    hud.shade:SetGradientAlpha("HORIZONTAL", 0.04, 0.02, 0.015, 0, 0.04, 0.02, 0.015, 0.85)
end

hud.title = Label(hud, 11, CREAM)
hud.title:SetPoint("TOPLEFT", 12, -8)
hud.title:SetText("THE FIREPIT")
hud.boss = Label(hud, 10, { 1, 0.3, 0.22 })
hud.boss:SetPoint("TOPRIGHT", -12, -9)
Line(hud, 12, 24, HUD_W - 24, 1, true)

hud.wave = Label(hud, 20, BODY)
hud.wave:SetPoint("TOPLEFT", 12, -31)
hud.timer = Label(hud, 13, CREAM)
hud.timer:SetPoint("TOPRIGHT", -12, -36)

hud.bar = CreateFrame("StatusBar", nil, hud)
hud.bar:SetPoint("TOPLEFT", 12, -58)
hud.bar:SetWidth(HUD_W - 24)
hud.bar:SetHeight(8)
hud.bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
hud.bar:SetStatusBarColor(1, 0.45, 0.12)
hud.bar:SetMinMaxValues(0, 60)
hud.bar.back = hud.bar:CreateTexture(nil, "BACKGROUND")
hud.bar.back:SetAllPoints()
hud.bar.back:SetTexture(0, 0, 0, 0.6)

local function Row(y, name)
    local label = Label(hud, 11, PALE)
    label:SetPoint("TOPLEFT", 12, -y)
    label:SetText(name)
    local value = Label(hud, 12, BODY)
    value:SetPoint("TOPRIGHT", -12, -y)
    return value
end
hud.enemies = Row(76, "Enemies")
hud.standing = Row(94, "Standing")
Line(hud, 12, 114, HUD_W - 24, 1)
hud.event = Label(hud, 10, { 1, 0.62, 0.3 })
hud.event:SetPoint("TOPLEFT", 12, -120)
hud.event:SetWidth(HUD_W - 24)
hud.event:SetJustifyH("LEFT")
-- The Enemy Forces (F): the living enemies by role, the reinforcements apart, the priority badges.
hud.forces = Label(hud, 10, BODY)
hud.forces:SetPoint("BOTTOMLEFT", 12, 26)
hud.forces:SetWidth(HUD_W - 24)
hud.forces:SetJustifyH("LEFT")
hud.forces:SetText("")
-- The world affixes active now (X), at the bottom.
hud.affixes = Label(hud, 10, { 1, 0.78, 0.45 })
hud.affixes:SetPoint("BOTTOMLEFT", 12, 8)
hud.affixes:SetWidth(HUD_W - 24)
hud.affixes:SetJustifyH("LEFT")
hud.affixes:SetText("")

local hudState = { seen = 0, left = 0, eventAt = 0 }

local function ShowHud(fields)
    local wave, left, alive, boss = tonumber(fields[2]) or 0, tonumber(fields[3]) or 0, tonumber(fields[4]) or 0,
        fields[5] == "1"
    local standing, total, countdown = tonumber(fields[6]) or 0, tonumber(fields[7]) or 0, tonumber(fields[8]) or 0
    local pressure, cap, waiting = tonumber(fields[9]) or 0, tonumber(fields[10]) or 0, tonumber(fields[11]) or 0
    local carnage = tonumber(fields[12]) or 0
    if countdown > 0 or wave == 0 then
        hud.wave:SetText("Get ready")
        hud.timer:SetText(Clock(countdown))
        hud.bar:SetMinMaxValues(0, 10)
        hud.bar:SetValue(countdown)
        hud.boss:SetText("")
    else
        hud.wave:SetText("Wave " .. wave)
        hud.timer:SetText(Clock(left))
        hud.bar:SetMinMaxValues(0, 75)
        hud.bar:SetValue(left)
        hud.boss:SetText(boss and "BOSS WAVE" or "")
    end
    -- Pressure (the field full too long): its stacks in red over the boss line; at the cap, the count shows it and
    -- the reinforcements waiting.
    if pressure > 0 then
        hud.boss:SetText("|cffff3b30PRESSURE +" .. (pressure * 5) .. "%|r")
    end
    -- Marked for Carnage: the boss's deadline, over everything else.
    if carnage > 0 then
        hud.boss:SetText("|cffff3b30CARNAGE " .. Clock(carnage) .. "|r")
    end
    if cap > 0 and alive >= cap * 0.75 then
        hud.enemies:SetText("|cffff3b30" .. alive .. "|r / " .. cap .. (waiting > 0 and (" +" .. waiting) or ""))
    else
        hud.enemies:SetText(alive > 0 and ("|cffff6a4a" .. alive .. "|r") or "0")
    end
    hud.standing:SetText(string.format("|cff7ee07a%d|r / %d", standing, total))
    hudState.seen = GetTime()
    hudState.left = left
    hud:Show()
end

-- The run's news (A,<text>,<urgent 0|1>): on the tracker, a boss's in red with the raid warning's sound.
local function HudNews(text, urgent)
    hud.event:SetText(text)
    hud.event:SetTextColor(urgent and 1 or 1, urgent and 0.32 or 0.62, urgent and 0.24 or 0.3)
    hudState.eventAt = GetTime()
    if urgent and PlaySound then
        PlaySound("RaidWarning")
    end
end

hud:SetScript("OnUpdate", function(self)
    if GetTime() - hudState.seen > HUD_TIMEOUT then
        self:Hide()
    elseif hudState.eventAt > 0 and GetTime() - hudState.eventAt > 10 then
        self.event:SetText("")
        hudState.eventAt = 0
    end
end)

-- The way in: the Firepit's own title as you arrive (fades in, holds, fades out), over the middle of the screen.
local intro = CreateFrame("Frame", "EmberforgeFirepitIntro", UIParent)
intro:SetWidth(600)
intro:SetHeight(90)
intro:SetPoint("CENTER", UIParent, "CENTER", 0, 120)
intro:SetFrameStrata("HIGH")
intro:Hide()
intro.title = Label(intro, 36, CREAM)
intro.title:SetPoint("TOP", 0, 0)
intro.title:SetText("THE FIREPIT")
intro.line = intro:CreateTexture(nil, "ARTWORK")
intro.line:SetTexture(1, 1, 1, 1)
intro.line:SetPoint("TOP", 0, -46)
intro.line:SetWidth(300)
intro.line:SetHeight(2)
if intro.line.SetGradientAlpha then
    intro.line:SetGradientAlpha("HORIZONTAL", 1, 0.5, 0.16, 0, 1, 0.5, 0.16, 1)
end
intro.glow = intro:CreateTexture(nil, "ARTWORK")
intro.glow:SetTexture(1, 1, 1, 1)
intro.glow:SetPoint("LEFT", intro.line, "RIGHT")
intro.glow:SetWidth(300)
intro.glow:SetHeight(2)
if intro.glow.SetGradientAlpha then
    intro.glow:SetGradientAlpha("HORIZONTAL", 1, 0.5, 0.16, 1, 1, 0.5, 0.16, 0)
end
intro.line:ClearAllPoints()
intro.line:SetPoint("TOPRIGHT", intro, "TOP", 0, -46)
intro.subtitle = Label(intro, 13, PALE)
intro.subtitle:SetPoint("TOP", 0, -56)
intro.subtitle:SetText("The Ring of Law burns. Hold as long as you can.")
local INTRO_IN, INTRO_HOLD, INTRO_OUT = 0.6, 3.6, 1.2
intro:SetScript("OnUpdate", function(self)
    local t = GetTime() - (self.started or 0)
    local alpha = t < INTRO_IN and t / INTRO_IN or t < INTRO_IN + INTRO_HOLD and 1
        or 1 - (t - INTRO_IN - INTRO_HOLD) / INTRO_OUT
    if alpha <= 0 then
        self:Hide()
    else
        self:SetAlpha(alpha)
    end
end)

local function ShowIntro()
    intro.started = GetTime()
    intro:SetAlpha(0)
    intro:Show()
end

-- The recap ------------------------------------------------------------------------------------------------------
-- When the run ends: the run in big numbers along the top, who stood out, then a row a Hero (sortable by any
-- column; the best in each lit in ember, your own row in gold), and Share.

-- The columns: header, the record's field, a formatter, width, the tooltip that explains it, best is the most.
local COLUMNS = {
    { "HERO", "name", nil, 110, "The Hero and their level." },
    { "PEAK", "peak", nil, 40, "The last wave they stood in.", true },
    { "FOES", "peakAlive", nil, 40, "The most enemies alive during their peak wave.", true },
    { "MOST", "mostAlive", nil, 40, "The most enemies alive at once while they stood.", true },
    { "DAMAGE", "damage", Short, 60, "Damage done (pets included).", true },
    { "HEALING", "healing", Short, 60, "Healing done.", true },
    { "TAKEN", "taken", Short, 60, "Damage taken." },
    { "KILLS", "kills", nil, 38, "Killing blows (pets included).", true },
    { "BOSS", "bossKills", nil, 34, "Killing blows on bosses.", true },
    { "BIG HIT", "hit", Short, 52, "Their biggest hit.", true },
    { "HIT TAKEN", "hitTaken", Short, 58, "The biggest hit they took." },
    { "FELL", "fell", nil, 50, "Times they fell, and the wave of the last." },
    { "REZ", "rezzes", nil, 30, "Times they were brought back during the run." },
    { "SPOILS", "loot", nil, 42, "Pieces of gear won (one every 15 waves survived).", true },
    { "FELLED BY", "killer", nil, 100, "What dealt the last blow." },
}
local RESULTS_W, RESULTS_H = 860, 470
local ROW_HEIGHT, MAX_ROWS, TABLE_X, TABLE_Y = 22, 10, 26, 186

local results = CreateFrame("Frame", "EmberforgeFirepitResults", UIParent)
results:SetWidth(RESULTS_W)
results:SetHeight(RESULTS_H)
results:SetPoint("CENTER", 0, 30)
results:SetFrameStrata("DIALOG")
results:SetToplevel(true)
Movable(results)
Dress(results, 0.82)
results:Hide()
tinsert(UISpecialFrames, "EmberforgeFirepitResults")

results.title = Label(results, 20, CREAM)
results.title:SetPoint("TOPLEFT", 26, -16)
results.title:SetText("THE FIREPIT")
local bar = results:CreateTexture(nil, "ARTWORK")
bar:SetTexture(BRONZE[1], BRONZE[2], BRONZE[3], 0.9)
bar:SetPoint("TOPLEFT", 168, -18)
bar:SetWidth(1)
bar:SetHeight(16)
results.subtitle = Label(results, 9, PALE)
results.subtitle:SetPoint("TOPLEFT", 180, -22)
results.subtitle:SetText("R U N   C O M P L E T E")
Line(results, 20, 44, RESULTS_W - 40, 1)
Line(results, 20, 44, 150, 2, true)
local close = CreateFrame("Button", nil, results)
close:SetWidth(18)
close:SetHeight(18)
close:SetPoint("TOPRIGHT", -14, -14)
close:SetNormalTexture(ART .. "Close")
close:SetHighlightTexture(ART .. "Close", "ADD")
close:SetScript("OnClick", function() results:Hide() end)

-- The run in big numbers.
local STATS = { "WAVE REACHED", "TIME SURVIVED", "ENEMIES SLAIN", "HEROES", "ENEMY HEALTH" }
local STAT_W = (RESULTS_W - 40) / #STATS
results.stats = {}
for index, name in ipairs(STATS) do
    local x = 20 + (index - 1) * STAT_W
    local label = Label(results, 9, PALE)
    label:SetPoint("TOP", results, "TOPLEFT", x + STAT_W / 2, -60)
    label:SetText(name)
    local value = Label(results, index == 1 and 30 or 24, index == 1 and { 1, 0.6, 0.23 } or CREAM)
    value:SetPoint("TOP", results, "TOPLEFT", x + STAT_W / 2, -76)
    results.stats[index] = { label = label, value = value }
    if index > 1 then
        local divider = results:CreateTexture(nil, "ARTWORK")
        divider:SetTexture(BRONZE[1], BRONZE[2], BRONZE[3], 0.6)
        divider:SetPoint("TOPLEFT", x, -58)
        divider:SetWidth(1)
        divider:SetHeight(56)
    end
end
results.start = Label(results, 9, PALE)
results.start:SetPoint("TOP", results, "TOPLEFT", 20 + STAT_W / 2, -110)
results.highlights = Label(results, 11, BODY)
results.highlights:SetPoint("TOP", results, "TOP", 0, -134)
Line(results, 20, 156, RESULTS_W - 40, 1)

local run = { players = {} }
local sortKey, sortDown = "peak", true

local headers, rows = {}, {}
local x = TABLE_X
for index, column in ipairs(COLUMNS) do
    local header = CreateFrame("Button", nil, results)
    header:SetWidth(column[4])
    header:SetHeight(16)
    header:SetPoint("TOPLEFT", x, -164)
    header.text = Label(header, 9, CREAM)
    header.text:SetAllPoints()
    header.text:SetJustifyH(index == 1 and "LEFT" or index == #COLUMNS and "RIGHT" or "CENTER")
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
local TABLE_W = x - TABLE_X
Line(results, TABLE_X - 6, 182, TABLE_W + 12, 1)

for index = 1, MAX_ROWS do
    local row = CreateFrame("Frame", nil, results)
    row:SetWidth(TABLE_W + 12)
    row:SetHeight(ROW_HEIGHT)
    row:SetPoint("TOPLEFT", TABLE_X - 6, -TABLE_Y - (index - 1) * ROW_HEIGHT)
    row.shade = row:CreateTexture(nil, "BACKGROUND")
    row.shade:SetAllPoints()
    row.shade:SetTexture(1, 0.62, 0.3, index % 2 == 0 and 0.05 or 0)
    row.cells = {}
    local cellX = 6
    for column = 1, #COLUMNS do
        local cell = Label(row, column == 1 and 11 or 10, BODY)
        cell:SetWidth(COLUMNS[column][4])
        cell:SetHeight(ROW_HEIGHT)
        cell:SetPoint("LEFT", cellX, 0)
        cell:SetJustifyH(column == 1 and "LEFT" or column == #COLUMNS and "RIGHT" or "CENTER")
        row.cells[column] = cell
        cellX = cellX + COLUMNS[column][4]
    end
    rows[index] = row
end

results.footer = results:CreateTexture(nil, "ARTWORK")
results.footer:SetTexture(BRONZE[1], BRONZE[2], BRONZE[3], 0.75)
results.footer:SetPoint("BOTTOMLEFT", 20, 44)
results.footer:SetWidth(RESULTS_W - 40)
results.footer:SetHeight(1)
local hint = Label(results, 9, PALE)
hint:SetPoint("BOTTOMLEFT", 26, 18)
hint:SetText("Claim your spoils chest, then speak to the Keeper to leave.  /firepit shows this again.")

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

-- The Hero who did the most of a field (nil when no one did any).
local function Best(field)
    local best
    for _, player in ipairs(run.players) do
        if (player[field] or 0) > 0 and (not best or player[field] > best[field]) then
            best = player
        end
    end
    return best
end

function results:Refresh()
    -- As tall as its Heroes need (three rows at least).
    self:SetHeight(TABLE_Y + math.max(3, math.min(#run.players, MAX_ROWS)) * ROW_HEIGHT + 60)
    local stats = self.stats
    stats[1].value:SetText(tostring(run.reached or 0))
    stats[2].value:SetText(Clock(run.seconds))
    stats[3].value:SetText(Thousands(run.kills))
    stats[4].value:SetText(tostring(run.size or 1))
    stats[5].value:SetText(string.format("x%.1f", (run.health or 100) / 100))
    self.start:SetText((run.start or 1) > 1 and ("started at wave " .. run.start) or "")

    local parts = {}
    for _, pick in ipairs({ { "Top damage", "damage" }, { "Top healing", "healing" }, { "Most kills", "kills" } }) do
        local best = Best(pick[2])
        if best then
            parts[#parts + 1] = string.format("|cffbfae99%s|r  %s |cffff9a3a%s|r", pick[1], best.name,
                Short(best[pick[2]]))
        end
    end
    local last = Best("peak")
    if last and #run.players > 1 then
        parts[#parts + 1] = "|cffbfae99Last standing|r  " .. last.name
    end
    self.highlights:SetText(table.concat(parts, "      "))

    for index, header in ipairs(headers) do
        local column = COLUMNS[index]
        header.text:SetText(column[1] .. (sortKey == column[2] and (sortDown and " v" or " ^") or ""))
    end
    local tops = {}
    for _, column in ipairs(COLUMNS) do
        if column[6] and #run.players > 1 then
            tops[column[2]] = Best(column[2])
        end
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
                    value = player.name .. " |cff998877" .. player.level .. "|r"
                elseif spec[2] == "fell" then
                    value = player.deaths > 0 and (player.deaths .. " (w" .. player.deathWave .. ")") or "-"
                elseif spec[3] then
                    value = spec[3](value)
                end
                cell:SetText(value == "" and "-" or tostring(value))
                if tops[spec[2]] == player then
                    cell:SetTextColor(1, 0.6, 0.23)
                elseif player.name == me then
                    cell:SetTextColor(1, 0.84, 0.5)
                else
                    cell:SetTextColor(BODY[1], BODY[2], BODY[3])
                end
            end
            row:Show()
        else
            row:Hide()
        end
    end
end

-- Share: the result in group chat (or to yourself, alone).
local share = CreateFrame("Button", nil, results)
share:SetWidth(150)
share:SetHeight(26)
share:SetPoint("BOTTOMRIGHT", -24, 12)
share:SetNormalTexture(ART .. "Button")
share:SetPushedTexture(ART .. "ButtonPushed")
share:SetHighlightTexture(ART .. "ButtonHighlight", "ADD")
share.label = Label(share, 12, BODY)
share.label:SetPoint("CENTER")
share:SetFontString(share.label)
share:SetText("Share in chat")
share:SetScript("OnClick", function()
    local best = Best("damage")
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

-- The queue: the group finder's Firepit tab ------------------------------------------------------------------------
-- A painting of the ring under a dark veil, thin bronze lines, two columns: how it works, the rewards and your group
-- (a portrait a Hero, their answer under it) on the left; your records by group size (each one's leaderboard on
-- hover), the starting wave and the button on the right; the group's difficulty along the bottom. The art:
-- build_firepit_art.py, drawn at this layout.

local BRACKETS = { "Solo", "Duo", "Trio", "Group" }
local CHECKPOINT_EVERY = 5
local REWARDS = {
    { "Interface\\Icons\\INV_Box_04", "Spoils of the Firepit",
        "Beat the boss of wave 15, 30, 45... and your share waits in the great chest when the run ends. The wave " ..
        "you start on never pays out." },
    { "Interface\\Icons\\INV_Chest_Plate16", "Rare Fireforged gear",
        "Waves 15 and 30: a Rare piece for one of your weakest slots, above what you wear there." },
    { "Interface\\Icons\\INV_Sword_62", "Epic Fireforged gear",
        "From wave 45, every 15 waves: an Epic piece with one bonus effect chosen for your role and build." },
    { "Interface\\Icons\\INV_Misc_Gem_Variety_02", "Titanforged",
        "A rare roll on an Epic piece: it comes with a prismatic socket for a gem of your choice." },
}
-- The layout, in the panel's units (16:9, as the painting): build_firepit_art.py draws its lines at the same places.
local PANEL_W, PANEL_H = 560, 315
local CHROME_ROWS = 576 / 1024                  -- the panel's share of Chrome's texture
local REWARD_X, REWARD_Y, REWARD_SIZE, REWARD_STEP = 35, 164, 29, 41.6
local SLOT_CX, SLOT_CY, SLOT_SIZE, SLOT_STEP = 40, 236.5, 25, 30.6
local CELLS = { { 403, 64 }, { 488, 64 }, { 403, 105 }, { 488, 105 } }    -- a record's label (its centre, top)
local PICKER_X, PICKER_Y, PICKER_W, PICKER_H = 360, 186, 177, 23
local BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H = 360, 223, 177, 26
local FINDER_SCALE = 1.5                        -- a window of its own, in the screen's middle: larger than a panel

local finder = CreateFrame("Frame", "EmberforgeFirepitFinder", UIParent)
finder:SetWidth(PANEL_W)
finder:SetHeight(PANEL_H)
finder:SetPoint("CENTER", UIParent, "CENTER", 0, 40)
finder:SetScale(FINDER_SCALE)
finder:SetFrameStrata("HIGH")
finder:SetClampedToScreen(true)
finder:EnableMouse(true)
finder:SetMovable(true)
finder:RegisterForDrag("LeftButton")
finder:SetScript("OnDragStart", finder.StartMoving)
finder:SetScript("OnDragStop", finder.StopMovingOrSizing)
finder:SetToplevel(true)
finder:Hide()
tinsert(UISpecialFrames, "EmberforgeFirepitFinder")   -- Escape closes it

-- The painting of the ring (16:9, squeezed into 1024x512: the panel's own shape again here), and over it the veil,
-- the frame and the lines (Chrome).
finder.art = finder:CreateTexture(nil, "BACKGROUND")
finder.art:SetAllPoints()
finder.art:SetTexture(ART .. "Background")
finder.chrome = finder:CreateTexture(nil, "ARTWORK")
finder.chrome:SetAllPoints()
finder.chrome:SetTexture(ART .. "Chrome")
finder.chrome:SetTexCoord(0, 1, 0, CHROME_ROWS)

local function Text(x, y, size, color, anchor)
    local line = finder:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    line:SetFont(FONT, size)
    line:SetPoint(anchor or "TOPLEFT", finder, "TOPLEFT", x, -y)
    line:SetJustifyH(anchor == "TOP" and "CENTER" or anchor == "TOPRIGHT" and "RIGHT" or "LEFT")
    line:SetTextColor(color[1], color[2], color[3])
    line:SetShadowOffset(1, -1)
    line:SetShadowColor(0, 0, 0, 0.9)
    return line
end

local finderClose = CreateFrame("Button", nil, finder)
finderClose:SetWidth(18)
finderClose:SetHeight(18)
finderClose:SetPoint("TOPRIGHT", -13, -10)
finderClose:SetNormalTexture(ART .. "Close")
finderClose:SetHighlightTexture(ART .. "Close", "ADD")
finderClose:SetScript("OnClick", function() finder:Hide() end)

finder.title = Text(24, 11, 19, CREAM)
finder.title:SetText("THE FIREPIT")
finder.subtitle = Text(171, 17, 9, PALE)
finder.subtitle:SetText("E N D L E S S   W A V E S")

Text(26, 44, 12, CREAM):SetText("HOW IT WORKS")
for index, line in ipairs({ "A new wave every 60 seconds", "Surviving enemies remain", "Each wave is 10% stronger",
    "A boss every 5th wave" }) do
    Text(33, 62 + (index - 1) * 14, 10, BODY):SetText(line)
end
Text(31, 118, 9, PALE):SetText("Fight until your group falls. Enemies and rewards match your level.")

Text(26, 142, 12, CREAM):SetText("POTENTIAL REWARDS")
local rewardButtons = {}
for index, reward in ipairs(REWARDS) do
    local button = CreateFrame("Button", nil, finder)
    button:SetWidth(REWARD_SIZE)
    button:SetHeight(REWARD_SIZE)
    button:SetPoint("TOPLEFT", REWARD_X + (index - 1) * REWARD_STEP, -REWARD_Y)
    button.icon = button:CreateTexture(nil, "BACKGROUND")
    button.icon:SetAllPoints()
    button.icon:SetTexture(reward[1])
    button.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    button:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
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

Text(26, 202, 12, CREAM):SetText("YOUR GROUP")
finder.readyCount = Text(326, 204, 10, BODY, "TOPRIGHT")

-- A place a Hero (its outline drawn in Chrome): their portrait in a gold ring, their answer under it.
local members = {}
for index = 1, MOST_HEROES do
    local slot = CreateFrame("Button", nil, finder)
    slot:SetWidth(SLOT_SIZE)
    slot:SetHeight(SLOT_SIZE)
    slot:SetPoint("CENTER", finder, "TOPLEFT", SLOT_CX + (index - 1) * SLOT_STEP, -SLOT_CY)
    slot.portrait = slot:CreateTexture(nil, "BACKGROUND")
    slot.portrait:SetAllPoints()
    slot.vacant = slot:CreateTexture(nil, "BACKGROUND")
    slot.vacant:SetAllPoints()
    slot.vacant:SetTexture(ART .. "Slot")
    slot.ring = slot:CreateTexture(nil, "OVERLAY")
    slot.ring:SetPoint("CENTER")
    slot.ring:SetWidth(SLOT_SIZE + 4)
    slot.ring:SetHeight(SLOT_SIZE + 4)
    slot.ring:SetTexture(ART .. "Ring")
    slot.status = slot:CreateTexture(nil, "OVERLAY")
    slot.status:SetWidth(10)
    slot.status:SetHeight(10)
    slot.status:SetPoint("BOTTOMRIGHT", 2, -1)
    slot.label = slot:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    slot.label:SetFont(FONT, 7.5)
    slot.label:SetPoint("TOP", slot, "BOTTOM", 0, -1)
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
            GameTooltip:AddLine("An open place", 0.7, 0.7, 0.7)
            GameTooltip:AddLine("Invite a Hero to bring them along.", 1, 1, 1, true)
        end
        GameTooltip:Show()
    end)
    slot:SetScript("OnLeave", function() GameTooltip:Hide() end)
    members[index] = slot
end
Text(27, 262, 9, PALE):SetText("Up to 10 players. Everyone must accept.")

-- The records, four to a cross of lines (Chrome), each with its leaderboard on hover.
Text(358, 44, 12, CREAM):SetText("YOUR RECORDS")
local board = { [1] = {}, [2] = {}, [3] = {}, [4] = {} }
local records = {}
for index, cell in ipairs(CELLS) do
    local record = CreateFrame("Frame", nil, finder)
    record:SetWidth(78)
    record:SetHeight(38)
    record:SetPoint("TOP", finder, "TOPLEFT", cell[1], -(cell[2] - 2))
    record:EnableMouse(true)
    record.label = record:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    record.label:SetFont(FONT, 10)
    record.label:SetPoint("TOP", 0, -2)
    record.label:SetTextColor(CREAM[1], CREAM[2], CREAM[3])
    record.label:SetText(BRACKETS[index])
    record.value = record:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    record.value:SetFont(FONT, 21)
    record.value:SetPoint("TOP", 0, -13)
    record.value:SetTextColor(0.97, 0.92, 0.84)
    record.value:SetShadowOffset(1, -1)
    record.value:SetText("0")
    record.bracket = index
    record:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        GameTooltip:AddLine(BRACKETS[self.bracket] .. " leaderboard", 1, 0.6, 0.2)
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
Text(448.5, 140, 9, PALE, "TOP"):SetText("Hover a record to view its leaderboard.")

-- Where the run starts: wave 1, or any 5th wave everyone in the group has reached.
Text(358, 160, 12, CREAM):SetText("STARTING WAVE")
local start = CreateFrame("Frame", nil, finder)
start:SetWidth(PICKER_W)
start:SetHeight(PICKER_H)
start:SetPoint("TOPLEFT", PICKER_X, -PICKER_Y)
start.text = start:CreateFontString(nil, "OVERLAY", "GameFontNormal")
start.text:SetFont(FONT, 14)
start.text:SetPoint("CENTER", 0, 0)
start.text:SetTextColor(0.97, 0.92, 0.84)
local startPrev = CreateFrame("Button", nil, start)
local startNext = CreateFrame("Button", nil, start)
for _, pair in ipairs({ { startPrev, "LEFT", 6, false }, { startNext, "RIGHT", -6, true } }) do
    local button, flip = pair[1], pair[4]
    button:SetWidth(16)
    button:SetHeight(16)
    button:SetPoint(pair[2], pair[3], 0)
    for _, state in ipairs({ { "SetNormalTexture", "Arrow" }, { "SetDisabledTexture", "ArrowDisabled" },
        { "SetHighlightTexture", "Arrow" } }) do
        button[state[1]](button, ART .. state[2], state[1] == "SetHighlightTexture" and "ADD" or nil)
    end
    if flip then
        for _, texture in ipairs({ button:GetNormalTexture(), button:GetDisabledTexture(),
            button:GetHighlightTexture() }) do
            if texture then
                texture:SetTexCoord(1, 0, 0, 1)
            end
        end
    end
end

-- The big button.
local queueButton = CreateFrame("Button", nil, finder)
queueButton:SetWidth(BUTTON_W)
queueButton:SetHeight(BUTTON_H)
queueButton:SetPoint("TOPLEFT", BUTTON_X, -BUTTON_Y)
queueButton:SetNormalTexture(ART .. "Button")
queueButton:SetPushedTexture(ART .. "ButtonPushed")
queueButton:SetDisabledTexture(ART .. "ButtonDisabled")
queueButton:SetHighlightTexture(ART .. "ButtonHighlight", "ADD")
queueButton.label = queueButton:CreateFontString(nil, "OVERLAY", "GameFontNormal")
queueButton.label:SetFont(FONT, 14)
queueButton.label:SetPoint("CENTER", 0, 0)
queueButton.label:SetShadowOffset(1, -1)
queueButton:SetFontString(queueButton.label)
queueButton:SetPushedTextOffset(1, -1)
finder.status = Text(448.5, 255, 10, BODY, "TOP")

-- The bottom line: the group's difficulty, the level it opens at.
finder.difficulty = Text(23, 288, 10.5, BODY)
finder.requirement = Text(537, 288, 10.5, BODY, "TOPRIGHT")

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
            slot.vacant:Hide()
            if SetPortraitTexture then
                SetPortraitTexture(slot.portrait, member.unit)
            end
            slot.portrait:Show()
            slot.ring:Show()
            if not member.online then
                slot.why = "Offline."
            elseif member.level < queue.level then
                slot.why = "Needs level " .. queue.level .. "."
                problem = problem or (member.name .. " must be level " .. queue.level .. ".")
            end
            local answer = queue.answers[member.name]
            slot.answer = queue.state == "proposal" and (answer or 0) or nil
            slot.status:SetTexture(slot.why and NOT_READY or answer == 2 and NOT_READY or nil)
            slot.portrait:SetVertexColor(1, slot.why and 0.45 or 1, slot.why and 0.45 or 1)
            if slot.why then
                slot.label:SetText(member.online and ("Level " .. member.level) or "Offline")
                slot.label:SetTextColor(1, 0.35, 0.3)
            elseif slot.answer == 1 then
                slot.label:SetText("Ready")
                slot.label:SetTextColor(0.55, 0.95, 0.5)
            elseif slot.answer == 0 then
                slot.label:SetText("Pending")
                slot.label:SetTextColor(0.93, 0.84, 0.71)
            else
                slot.label:SetText("")
            end
        else
            slot.vacant:Show()
            slot.portrait:Hide()
            slot.ring:Hide()
            slot.status:SetTexture(nil)
            slot.label:SetText("")
        end
    end
    local size = math.max(1, math.min(#list, MOST_HEROES))
    if #list > MOST_HEROES then
        problem = "The Firepit takes at most 10."
    end
    self.difficulty:SetText(string.format("%d %s:  enemy health x%.1f   |cff8c6a4c|||r   enemy count x%.1f", size,
        size == 1 and "player" or "players", 1 + GROUP_HEALTH * (size - 1), 1 + GROUP_COUNT * (size - 1)))
    self.requirement:SetText("Level " .. queue.level .. "+")
    for index, record in ipairs(records) do
        record.value:SetText(tostring(queue.best[index] or 0))
    end

    queue.start = math.min(queue.start, queue.checkpoint)
    start.text:SetText("Wave " .. queue.start)
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
    local enabled
    if queue.state == "proposal" then
        local accepted = 0
        for _, member in ipairs(list) do
            if queue.answers[member.name] == 1 then
                accepted = accepted + 1
            end
        end
        self.readyCount:SetText(string.format("%d of %d ready", accepted, #list))
        self.status:SetText(string.format("Waiting for your group: %d of %d ready.", accepted, #list))
        queueButton:SetText(leading and "Leave queue" or "Decline")
        enabled = true
    else
        self.readyCount:SetText(#list > 1 and (#list .. " players") or "")
        self.status:SetText(queue.why and ("|cffff5a48" .. queue.why .. "|r") or (problem and ("|cffff5a48"
            .. problem .. "|r") or ""))
        queueButton:SetText(#list > 1 and ("Queue group (" .. #list .. ")") or "Enter the Firepit")
        enabled = leading and not problem and UnitLevel("player") >= queue.level
        if not leading then
            self.status:SetText("Only your group's leader can queue.")
        end
    end
    if enabled then
        queueButton:Enable()
        queueButton.label:SetTextColor(0.97, 0.92, 0.84)
    else
        queueButton:Disable()
        queueButton.label:SetTextColor(0.6, 0.55, 0.5)
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

-- In the group finder's place (it and Player vs. Player close; their tabs follow this window).
local function OpenFinder()
    for _, name in ipairs({ "LFDParentFrame", "PVPParentFrame" }) do
        local frame = _G[name]
        if frame and frame:IsShown() then
            HideUIPanel(frame)
        end
    end
    finder:Show()
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
        finder:Hide()
    else
        return false
    end
    if finder:IsShown() then
        finder:Refresh()
    end
    return true
end

-- The eye -----------------------------------------------------------------------------------------------------------
-- The dungeon finder's eye on the minimap (MiniMapLFGFrame) shows only for the dungeon finder's own groups: in the
-- Firepit it shows too, its menu offering the way out. The dungeon finder's own uses of it are left as they were.

local FIREPIT_MAP_NAME = "The Firepit"     -- the client patch's Map.dbc (client-patch/hd/make_atmosphere.py)
local MUSIC = "Interface\\AddOns\\Emberforge_Firepit\\PressurePoint.mp3"  -- the Firepit's own, on a loop inside
local inside = false
local eye = MiniMapLFGFrame
local eyeMenu = CreateFrame("Frame", "EmberforgeFirepitEyeMenu", UIParent, "UIDropDownMenuTemplate")

StaticPopupDialogs["EMBERFORGE_FIREPIT_EXIT"] = {
    text = "Leave the Firepit?\n\nIf the run is still going, you fall where you stand.",
    button1 = YES or "Yes",
    button2 = NO or "No",
    OnAccept = function() Send("EXIT") end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
}

local function Firepit()
    return inside and not (GetLFGMode and GetLFGMode())
end

local function UpdateEye()
    if not eye then
        return
    end
    if Firepit() then
        eye:Show()
        if EyeTemplate_StopAnimating and eye.eye then
            EyeTemplate_StopAnimating(eye.eye)
        end
    elseif not (GetLFGMode and GetLFGMode()) then
        eye:Hide()
    end
end

local function SetInside(now)
    if inside ~= now then
        inside = now
        UpdateEye()
        if now and PlayMusic then
            PlayMusic(MUSIC)
        elseif not now and StopMusic then
            StopMusic()
        end
    end
end

local function CheckInside()
    local name, kind = nil, nil
    if GetInstanceInfo then
        name, kind = GetInstanceInfo()
    end
    SetInside(name == FIREPIT_MAP_NAME and kind ~= "none")
end

local function EyeMenu()
    return {
        { text = FIREPIT_MAP_NAME, isTitle = true, notCheckable = true },
        { text = TELEPORT_OUT_OF_DUNGEON or "Teleport out of Dungeon", notCheckable = true,
          func = function() StaticPopup_Show("EMBERFORGE_FIREPIT_EXIT") end },
        { text = "Show last results", notCheckable = true,
          func = function() SlashCmdList.EMBERFORGEFIREPIT() end },
    }
end

if eye then
    local onEvent, onClick = eye:GetScript("OnEvent"), eye:GetScript("OnClick")
    local onEnter, onLeave = eye:GetScript("OnEnter"), eye:GetScript("OnLeave")
    eye:SetScript("OnEvent", function(...)
        if onEvent then
            onEvent(...)
        end
        UpdateEye()
    end)
    eye:SetScript("OnClick", function(self, button)
        if not Firepit() then
            return onClick and onClick(self, button)
        end
        PlaySound("igMainMenuOpen")
        EasyMenu(EyeMenu(), eyeMenu, self, 0, -5, "MENU")
    end)
    eye:SetScript("OnEnter", function(self)
        if not Firepit() then
            return onEnter and onEnter(self)
        end
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        GameTooltip:SetText(FIREPIT_MAP_NAME)
        GameTooltip:AddLine("You are in the Firepit. Click to leave.", 1, 0.82, 0, 1)
        GameTooltip:Show()
    end)
    eye:SetScript("OnLeave", function(self)
        if onLeave then
            onLeave(self)
        end
        GameTooltip:Hide()
    end)
    if hooksecurefunc and MiniMapLFG_UpdateIsShown then
        hooksecurefunc("MiniMapLFG_UpdateIsShown", UpdateEye)    -- the dungeon finder's own calls
    end
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
        SetInside(true)
        ShowHud(fields)
    elseif kind == "A" then
        HudNews(fields[2] or "", fields[3] == "1")
    elseif kind == "F" then
        -- F,<bosses>,<tanks>,<melee>,<ranged>,<casters>,<healers>,<queued>,<badges>: each kind its icon and count
        -- (a skull, a shield, a sword, a bow, a wand, a cross; Art/Role*.tga), the ones there are.
        local n = function(i) return tonumber(fields[i]) or 0 end
        local total, parts = 0, {}
        for i, icon in ipairs(ROLE_ICONS) do
            local count = n(i + 1)
            total = total + count
            if count > 0 then
                parts[#parts + 1] = "|T" .. ART .. icon .. ":14:14:0:-1|t" .. count
            end
        end
        local queued = n(8)
        local text = "ENEMY FORCES " .. total .. (#parts > 0 and ("  " .. table.concat(parts, "  ")) or "")
        if queued > 0 then
            text = text .. (total == 0 and " |cffffd060INCOMING " or " |cffffd060Queued ") .. queued .. "|r"
        end
        if (fields[9] or "") ~= "" then
            text = text .. " |cffff6a4a[" .. fields[9] .. "]|r"
        end
        hud.forces:SetText(text)
    elseif kind == "X" then
        local names = {}
        for i = 2, #fields do
            names[#names + 1] = fields[i]
        end
        hud.affixes:SetText(#names > 0 and ("Affixes: " .. table.concat(names, ", ")) or "")
    elseif kind == "INTRO" then
        SetInside(true)
        ShowIntro()
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
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:RegisterEvent("ZONE_CHANGED_NEW_AREA")
events:SetScript("OnEvent", function(_, event, prefix, message)
    if event == "PLAYER_ENTERING_WORLD" or event == "ZONE_CHANGED_NEW_AREA" then
        CheckInside()
    elseif event == "ADDON_LOADED" then
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
EmberforgeFirepitDebug = { intro = intro, hud = hud, results = results, rows = rows, headers = headers,
    OnMessage = OnMessage,
    Run = function() return run end, finder = finder, members = members, records = records,
    queueButton = queueButton, start = start, startPrev = startPrev, startNext = startNext,
    Inside = function() return inside end }

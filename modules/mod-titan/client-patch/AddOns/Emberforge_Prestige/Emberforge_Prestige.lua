-- Emberforge Prestige: the Prestige window, opened by talking to the Keeper of the Ember. Your prestige (rank, level,
-- this run and the fastest), what a prestige takes, keeps and gives, the reward at level 60, and the prestige itself
-- (confirmed first). The look is the Classless window's (its Art folder).
--
-- The server (mod-titan, src/prestige.cpp) talks in addon whispers to the player, prefix EFPR:
--   from server   S,<rank>,<level>,<max level>,<Emberspark>,<on a run 0|1>,<this run, sec>,<fastest run, sec>,
--                   <can prestige 0|1>,<Emberspark now>,<Emberspark at max level>,<souls>,<experience %>,<why not>
--                 O   open the window      C   close it (the prestige is done)
--   to server     PRESTIGE

local PREFIX = "EFPR"
local ART = "Interface\\AddOns\\Emberforge_Classless\\Art\\"
local SPARK_ICON = "Interface\\Icons\\INV_Emberforge_Emberspark"
local SOUL_ITEM = 910001
local WIDTH, HEIGHT = 760, 470
local GOLD = { 1, 0.82, 0 }
local SUBTEXT = { 0.86, 0.64, 0.32 }
local WINDOW_FILL, WINDOW_EDGE = { 0.045, 0.032, 0.026, 0.97 }, { 0.86, 0.64, 0.32, 1 }
local PANEL_FILL, PANEL_EDGE = { 0.02, 0.014, 0.011, 0.85 }, { 0.55, 0.41, 0.2, 1 }
local BANNER_FILL, BANNER_EDGE = { 0.16, 0.06, 0.015, 0.95 }, { 1, 0.62, 0.22, 1 }
local BUTTON_FILL, BUTTON_EDGE = { 0.3, 0.13, 0.03, 0.96 }, { 1, 0.72, 0.32, 1 }
local OFF_FILL, OFF_EDGE = { 0.08, 0.07, 0.06, 0.96 }, { 0.4, 0.37, 0.33, 1 }
local LOSE_COLOR, KEEP_COLOR = { 1, 0.61, 0.54 }, { 0.61, 0.94, 0.56 }
local CROSS = "Interface\\RaidFrame\\ReadyCheck-NotReady"
local TICK = "Interface\\RaidFrame\\ReadyCheck-Ready"

local state = { rank = 0, level = 1, maxLevel = 60, spark = 0, onRun = false, run = 0, fastest = 0, ready = false,
    sparkNow = 500, sparkMax = 5000, souls = 2, xp = 10, why = "" }

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

-- 1234567 -> "1,234,567"
local function Thousands(value)
    local text = tostring(value)
    while true do
        local done
        text, done = text:gsub("^(%d+)(%d%d%d)", "%1,%2")
        if done == 0 then
            return text
        end
    end
end

-- 11520 -> "3h 12m"; 0 -> "--"
local function Duration(seconds)
    if not seconds or seconds <= 0 then
        return "--"
    end
    local hours, minutes = math.floor(seconds / 3600), math.floor(seconds / 60) % 60
    return hours > 0 and string.format("%dh %dm", hours, minutes) or string.format("%dm", minutes)
end

-- Rounded boxes, as the Classless window draws them (Art\FillCorner, LineCorner, LineEdge).
local FLIPS = { { 0, 1, 0, 1 }, { 1, 0, 0, 1 }, { 0, 1, 1, 0 }, { 1, 0, 1, 0 } }
local CORNERS = { "TOPLEFT", "TOPRIGHT", "BOTTOMLEFT", "BOTTOMRIGHT" }

local function BoxColor(target, fill, edge)
    for _, piece in ipairs(target.boxFill) do
        if piece.solid then
            piece:SetTexture(unpack(fill))
        else
            piece:SetVertexColor(unpack(fill))
        end
    end
    for _, piece in ipairs(target.boxEdge) do
        piece:SetVertexColor(unpack(edge))
    end
end

local function Box(target, fill, edge, small)
    local size, suffix = small and 8 or 16, small and "Small" or ""
    target.boxFill, target.boxEdge = {}, {}
    local function Piece(list, layer, file, flip)
        local texture = target:CreateTexture(nil, layer)
        if file then
            texture:SetTexture(ART .. file .. suffix)
            texture:SetTexCoord(unpack(flip or FLIPS[1]))
        else
            texture.solid = true
        end
        list[#list + 1] = texture
        return texture
    end
    for index, corner in ipairs(CORNERS) do
        for _, piece in ipairs({ Piece(target.boxFill, "BACKGROUND", "FillCorner", FLIPS[index]),
            Piece(target.boxEdge, "BORDER", "LineCorner", FLIPS[index]) }) do
            piece:SetWidth(size)
            piece:SetHeight(size)
            piece:SetPoint(corner)
        end
    end
    local middle = Piece(target.boxFill, "BACKGROUND")
    middle:SetPoint("TOPLEFT", size, -0.5)
    middle:SetPoint("BOTTOMRIGHT", -size, 0.5)
    local left = Piece(target.boxFill, "BACKGROUND")
    left:SetPoint("TOPLEFT", 0.5, -size)
    left:SetPoint("BOTTOMRIGHT", target, "BOTTOMLEFT", size, size)
    local right = Piece(target.boxFill, "BACKGROUND")
    right:SetPoint("TOPLEFT", target, "TOPRIGHT", -size, -size)
    right:SetPoint("BOTTOMRIGHT", -0.5, size)
    local top = Piece(target.boxEdge, "BORDER", "LineEdge")
    top:SetPoint("TOPLEFT", size, 0)
    top:SetPoint("BOTTOMRIGHT", target, "TOPRIGHT", -size, -size)
    local bottom = Piece(target.boxEdge, "BORDER", "LineEdge", FLIPS[3])
    bottom:SetPoint("TOPLEFT", target, "BOTTOMLEFT", size, size)
    bottom:SetPoint("BOTTOMRIGHT", -size, 0)
    local leftEdge = Piece(target.boxEdge, "BORDER", "LineEdgeV")
    leftEdge:SetPoint("TOPLEFT", 0, -size)
    leftEdge:SetPoint("BOTTOMRIGHT", target, "BOTTOMLEFT", size, size)
    local rightEdge = Piece(target.boxEdge, "BORDER", "LineEdgeV", FLIPS[2])
    rightEdge:SetPoint("TOPLEFT", target, "TOPRIGHT", -size, -size)
    rightEdge:SetPoint("BOTTOMRIGHT", 0, size)
    BoxColor(target, fill, edge)
end

local function Text(parent, size, color, font)
    local text = parent:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    text:SetFont(font or "Fonts\\FRIZQT__.TTF", size)
    text:SetShadowOffset(1, -1)
    text:SetTextColor(unpack(color or { 1, 1, 1 }))
    return text
end

local function Panel(parent, x, y, width, height, fill, edge)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetWidth(width)
    panel:SetHeight(height)
    panel:SetPoint("TOPLEFT", x, y)
    Box(panel, fill or PANEL_FILL, edge or PANEL_EDGE)
    return panel
end

-- Window -----------------------------------------------------------------------------------------------------------

local frame = CreateFrame("Frame", "EmberforgePrestige", UIParent)
frame:SetWidth(WIDTH)
frame:SetHeight(HEIGHT)
frame:SetPoint("CENTER")
frame:SetFrameStrata("HIGH")
frame:SetToplevel(true)
frame:EnableMouse(true)
frame:SetMovable(true)
frame:SetClampedToScreen(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
Box(frame, WINDOW_FILL, WINDOW_EDGE)
frame:Hide()
tinsert(UISpecialFrames, "EmberforgePrestige")     -- Escape closes it

local headerGlow = frame:CreateTexture(nil, "BORDER")
headerGlow:SetTexture(ART .. "HeaderGlow")
headerGlow:SetBlendMode("ADD")
headerGlow:SetWidth(560)
headerGlow:SetHeight(130)
headerGlow:SetPoint("TOP", 0, -1)

local title = Text(frame, 22, GOLD, "Fonts\\MORPHEUS.TTF")
title:SetPoint("TOP", 0, -14)
title:SetText("Prestige")
local divider = frame:CreateTexture(nil, "ARTWORK")
divider:SetTexture(ART .. "Divider")
divider:SetWidth(380)
divider:SetHeight(16)
divider:SetPoint("TOP", 0, -36)

local close = CreateFrame("Button", nil, frame)
close:SetWidth(28)
close:SetHeight(28)
close:SetPoint("TOPRIGHT", -12, -12)
close:SetNormalTexture(ART .. "Close")
close:SetHighlightTexture(ART .. "RingHover", "ADD")
close:SetScript("OnClick", function() frame:Hide() end)

-- The Emberspark balance, top left.
local balance = CreateFrame("Frame", nil, frame)
balance:SetWidth(150)
balance:SetHeight(28)
balance:SetPoint("TOPLEFT", 16, -14)
Box(balance, PANEL_FILL, PANEL_EDGE, true)
local balanceIcon = balance:CreateTexture(nil, "ARTWORK")
balanceIcon:SetTexture(SPARK_ICON)
balanceIcon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
balanceIcon:SetWidth(20)
balanceIcon:SetHeight(20)
balanceIcon:SetPoint("LEFT", 5, 0)
local balanceText = Text(balance, 13, { 1, 1, 1 })
balanceText:SetPoint("LEFT", balanceIcon, "RIGHT", 6, 0)
balance:EnableMouse(true)
balance:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_BOTTOMRIGHT")
    GameTooltip:AddLine("Emberspark", 1, 0.55, 0.12)
    GameTooltip:AddLine("Your account's server currency.", 1, 1, 1, true)
    GameTooltip:Show()
end)
balance:SetScript("OnLeave", function() GameTooltip:Hide() end)

-- Left: you, as a prestige.
local TOP = -64
local you = Panel(frame, 20, TOP, 250, 386)
local rays = you:CreateTexture(nil, "BORDER")
rays:SetTexture(ART .. "Rays")
rays:SetBlendMode("ADD")
rays:SetVertexColor(1, 0.55, 0.15)
rays:SetWidth(230)
rays:SetHeight(230)
rays:SetPoint("TOP", 0, 12)
local emblem = you:CreateTexture(nil, "ARTWORK")
emblem:SetTexture(ART .. "HeroEmblem")
emblem:SetWidth(96)
emblem:SetHeight(96)
emblem:SetPoint("TOP", 0, -38)
local ring = you:CreateTexture(nil, "OVERLAY")
ring:SetTexture(ART .. "RingSelected")
ring:SetWidth(132)
ring:SetHeight(132)
ring:SetPoint("CENTER", emblem, "CENTER")
local rankBadge = CreateFrame("Frame", nil, you)
rankBadge:SetWidth(44)
rankBadge:SetHeight(26)
rankBadge:SetPoint("TOP", emblem, "BOTTOM", 0, 2)
rankBadge:SetFrameLevel(you:GetFrameLevel() + 3)
Box(rankBadge, BANNER_FILL, BANNER_EDGE, true)
local rankText = Text(rankBadge, 15, GOLD, "Fonts\\MORPHEUS.TTF")
rankText:SetPoint("CENTER", 0, 0)

local label = Text(you, 12, SUBTEXT)
label:SetPoint("TOP", 0, -178)
label:SetText("P R E S T I G E")
local nameText = Text(you, 18, GOLD, "Fonts\\MORPHEUS.TTF")
nameText:SetPoint("TOP", label, "BOTTOM", 0, -4)

local function StatRow(y, name)
    local left = Text(you, 12, SUBTEXT)
    left:SetPoint("TOPLEFT", 22, y)
    left:SetText(name)
    local right = Text(you, 12, { 1, 1, 1 })
    right:SetPoint("TOPRIGHT", -22, y)
    return right
end
local levelValue = StatRow(-226, "Level")
local bar = CreateFrame("StatusBar", nil, you)
bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
bar:SetStatusBarColor(1, 0.5, 0.1)
bar:SetWidth(206)
bar:SetHeight(8)
bar:SetPoint("TOPLEFT", 22, -244)
bar:SetMinMaxValues(0, 1)
local barBack = bar:CreateTexture(nil, "BACKGROUND")
barBack:SetTexture(0, 0, 0, 0.7)
barBack:SetAllPoints()
local runValue = StatRow(-262, "This run")
local fastestValue = StatRow(-282, "Fastest run")

local lock = you:CreateTexture(nil, "ARTWORK")
lock:SetTexture("Interface\\Buttons\\LockButton-Locked-Up")
lock:SetWidth(26)
lock:SetHeight(26)
lock:SetPoint("TOPLEFT", 14, -312)
local lockText = Text(you, 11, { 0.9, 0.86, 0.78 })
lockText:SetPoint("TOPLEFT", lock, "TOPRIGHT", 2, -3)
lockText:SetWidth(190)
lockText:SetJustifyH("LEFT")

-- Right: lose, keep, get.
local RIGHT = 286
local RIGHT_WIDTH = WIDTH - RIGHT - 20

local function List(panel, heading, color, entries, top)
    local head = Text(panel, 13, color, "Fonts\\MORPHEUS.TTF")
    head:SetPoint("TOPLEFT", 14, -10)
    head:SetText(heading)
    local rows = {}
    local y = top or -32
    for index, entry in ipairs(entries) do
        local icon = panel:CreateTexture(nil, "ARTWORK")
        icon:SetWidth(entry.size or 14)
        icon:SetHeight(entry.size or 14)
        icon:SetPoint("TOPLEFT", 14, y)
        icon:SetTexture(entry.icon)
        if entry.crop then
            icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
        end
        local text = Text(panel, 12, { 0.95, 0.92, 0.86 })
        text:SetPoint("TOPLEFT", icon, "TOPRIGHT", 6, entry.size and -((entry.size - 12) / 2) or 0)
        text:SetWidth(panel:GetWidth() - 44 - (entry.size or 14))
        text:SetJustifyH("LEFT")
        rows[index] = { icon = icon, text = text }
        y = y - (entry.step or 34)
    end
    return rows
end

local half = math.floor((RIGHT_WIDTH - 8) / 2)
local losePanel = Panel(frame, RIGHT, TOP, half, 162)
local lose = List(losePanel, "You lose", LOSE_COLOR, { { icon = CROSS }, { icon = CROSS }, { icon = CROSS } })
lose[1].text:SetText("Your level, back to 1.")
lose[2].text:SetText("Your abilities and talents, rerolled.")
lose[3].text:SetText("Your quests, to do and earn again.")

local keepPanel = Panel(frame, RIGHT + half + 8, TOP, RIGHT_WIDTH - half - 8, 162)
local keep = List(keepPanel, "You keep", KEEP_COLOR,
    { { icon = TICK }, { icon = TICK, step = 22 }, { icon = TICK }, { icon = TICK } })
keep[1].text:SetText("All your gear, back on at its level.")
keep[2].text:SetText("Gold and Emberspark.")
keep[3].text:SetText("Professions, mounts and reputation.")
keep[4].text:SetText("Runic Enhancements |cff9a8f80(coming soon)|r")

local getPanel = Panel(frame, RIGHT, TOP - 170, RIGHT_WIDTH, 106)
local get = List(getPanel, "You get", GOLD, {
    { icon = SPARK_ICON, crop = true, size = 20, step = 24 },
    { icon = GetItemIcon(SOUL_ITEM) or "Interface\\Icons\\INV_Potion_93", crop = true, size = 20, step = 24 },
    { icon = SPARK_ICON, crop = true, size = 20, step = 24 },
}, -30)

-- The level cap's reward.
local banner = Panel(frame, RIGHT, TOP - 284, RIGHT_WIDTH, 56, BANNER_FILL, BANNER_EDGE)
local bannerGlow = banner:CreateTexture(nil, "BORDER")
bannerGlow:SetTexture(ART .. "HeaderGlow")
bannerGlow:SetBlendMode("ADD")
bannerGlow:SetAllPoints()
local bannerIcon = banner:CreateTexture(nil, "ARTWORK")
bannerIcon:SetTexture(SPARK_ICON)
bannerIcon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
bannerIcon:SetWidth(36)
bannerIcon:SetHeight(36)
bannerIcon:SetPoint("LEFT", 12, 0)
local bannerHead = Text(banner, 11, SUBTEXT)
bannerHead:SetPoint("TOPLEFT", bannerIcon, "TOPRIGHT", 10, -1)
local bannerText = Text(banner, 17, GOLD, "Fonts\\MORPHEUS.TTF")
bannerText:SetPoint("BOTTOMLEFT", bannerIcon, "BOTTOMRIGHT", 10, 0)
local bannerNote = Text(banner, 11, { 0.9, 0.86, 0.78 })
bannerNote:SetPoint("RIGHT", -14, 0)
bannerNote:SetWidth(180)
bannerNote:SetJustifyH("RIGHT")
bannerNote:SetText("Spend it on rested experience, heirlooms, appearances and mounts.")

-- The prestige button, and why it is off.
local button = CreateFrame("Button", "EmberforgePrestigeButton", frame)
button:SetWidth(190)
button:SetHeight(40)
button:SetPoint("BOTTOMRIGHT", -20, 20)
Box(button, BUTTON_FILL, BUTTON_EDGE)
button:SetHighlightTexture(ART .. "HeaderGlow", "ADD")
local buttonText = Text(button, 18, GOLD, "Fonts\\MORPHEUS.TTF")
buttonText:SetPoint("CENTER", 0, 0)
buttonText:SetText("Prestige")
local status = Text(frame, 12, SUBTEXT)
status:SetPoint("RIGHT", button, "LEFT", -14, 0)
status:SetWidth(RIGHT_WIDTH - 210)
status:SetJustifyH("RIGHT")

StaticPopupDialogs["EMBERFORGE_PRESTIGE"] = {
    text = "Return to level 1 as Prestige %s?\n\nYour Wildcard abilities and talents are rerolled, and every quest "
        .. "can be done again. Your gear waits in your bags until you reach its level, and your role is locked "
        .. "until level %s.",
    button1 = ACCEPT,
    button2 = CANCEL,
    OnAccept = function() Send("PRESTIGE") end,
    timeout = 0,
    whileDead = false,
    hideOnEscape = true,
}
button:SetScript("OnClick", function()
    if state.ready then
        StaticPopup_Show("EMBERFORGE_PRESTIGE", tostring(state.rank + 1), tostring(state.maxLevel))
    end
end)

-- The rays turn slowly and the ring breathes.
local elapsed = 0
frame:SetScript("OnUpdate", function(_, delta)
    elapsed = elapsed + delta
    rays:SetAlpha(0.45 + 0.2 * math.sin(elapsed * 1.6))
    ring:SetAlpha(0.75 + 0.25 * math.sin(elapsed * 2.2))
end)

local function Refresh()
    balanceText:SetText(Thousands(state.spark))
    rankText:SetText(state.rank > 0 and tostring(state.rank) or "-")
    nameText:SetText(UnitName("player"))
    levelValue:SetText(state.level .. " / " .. state.maxLevel)
    bar:SetValue(math.min(1, state.level / math.max(1, state.maxLevel)))
    runValue:SetText(state.onRun and Duration(state.run) or "--")
    fastestValue:SetText(Duration(state.fastest))
    lockText:SetText(state.onRun and ("Your role is locked for this prestige. At level " .. state.maxLevel
        .. ", you can change it again.") or ("A prestige locks your role until you reach level " .. state.maxLevel
        .. " again."))
    get[1].text:SetText("|cffffffff" .. Thousands(state.sparkNow) .. " Emberspark|r right away")
    get[2].text:SetText("|cff1eff00" .. state.souls .. " Titan Souls of Rested Experience|r")
    get[3].text:SetText("|cffffd100Prestigious|r: +" .. state.xp .. "% experience from everything, until level "
        .. state.maxLevel)
    bannerHead:SetText("REACH LEVEL " .. state.maxLevel .. " AGAIN")
    bannerText:SetText(Thousands(state.sparkMax) .. " Emberspark")
    if state.ready then
        BoxColor(button, BUTTON_FILL, BUTTON_EDGE)
        buttonText:SetTextColor(unpack(GOLD))
        button:Enable()
        status:SetText("Ready: Prestige " .. (state.rank + 1) .. " awaits.")
    else
        BoxColor(button, OFF_FILL, OFF_EDGE)
        buttonText:SetTextColor(0.5, 0.5, 0.5)
        button:Disable()
        status:SetText(state.why ~= "" and state.why or ("Available at level " .. state.maxLevel .. "."))
    end
end

local function OnServerMessage(message)
    local kind = message:sub(1, 1)
    if kind == "S" then
        local fields = {}
        local rest = message:sub(3)
        for _ = 1, 13 do
            local value, after = rest:match("^([^,]*),?(.*)$")
            fields[#fields + 1] = value
            rest = after
        end
        state.rank, state.level, state.maxLevel = tonumber(fields[1]) or 0, tonumber(fields[2]) or 1,
            tonumber(fields[3]) or 60
        state.spark, state.onRun = tonumber(fields[4]) or 0, fields[5] == "1"
        state.run, state.fastest, state.ready = tonumber(fields[6]) or 0, tonumber(fields[7]) or 0, fields[8] == "1"
        state.sparkNow, state.sparkMax = tonumber(fields[9]) or 0, tonumber(fields[10]) or 0
        state.souls, state.xp = tonumber(fields[11]) or 0, tonumber(fields[12]) or 0
        state.why = rest ~= "" and (fields[13] .. "," .. rest) or (fields[13] or "")
        Refresh()
    elseif kind == "O" then
        Refresh()
        frame:Show()
    elseif kind == "C" then
        StaticPopup_Hide("EMBERFORGE_PRESTIGE")
        frame:Hide()
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix == PREFIX and sender == UnitName("player") then
        OnServerMessage(message)
    end
end)

EmberforgePrestige_State = state     -- for tests

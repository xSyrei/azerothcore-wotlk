-- Emberforge Journal: the Dungeon Journal. Every dungeon and raid of Classic, the Burning Crusade and Wrath, by
-- expansion and type: its creature levels as the server has them (after the level squish), the queue levels the
-- game's dungeon list gives it, its difficulties and sizes, and its bosses. Open with /journal or /dj.
--
-- The server (mod-titan, src/journal.cpp) sends the data as addon whispers to the player, prefix EFDJ:
--   to server     LIST
--   from server   B   D,<lfg id>~<map>~<type>~<expansion>~<difficulty>~<players>~<creature min>~<creature max>
--                       ~<queue min>~<queue max>~<seasonal>~<name>;...   F,<map>~<difficulty>~<boss>;...   Z

local PREFIX = "EFDJ"
local ART = "Interface\\AddOns\\Emberforge_Resources\\Art\\"
local WIDTH, HEIGHT = 860, 560
local LIST_WIDTH, ROW_HEIGHT, ROWS = 330, 24, 16
local GOLD, SUBTEXT = { 1, 0.82, 0 }, { 0.86, 0.64, 0.32 }
local EXPANSIONS = { [0] = "Classic", [1] = "Burning Crusade", [2] = "Wrath of the Lich King" }
local TYPES = { [1] = "Dungeon", [2] = "Raid" }

-- The instances, one per name and map (its difficulties inside), and the bosses by map and difficulty.
local data = { instances = {}, order = {}, bosses = {}, loaded = false, loading = false }
local view = { expansion = 0, type = 1, selected = nil, offset = 0 }
local frame, Refresh

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function OnServerMessage(message)
    local kind = message:sub(1, 1)
    if message == "B" then
        wipe(data.instances)
        wipe(data.order)
        wipe(data.bosses)
    elseif kind == "D" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local id, map, kindId, expansion, difficulty, players, low, high, qmin, qmax, seasonal, name =
                strsplit("~", entry, 12)
            map = tonumber(map)
            local key = map .. ":" .. name
            local instance = data.instances[key]
            if not instance then
                instance = { key = key, name = name, map = map, type = tonumber(kindId),
                    expansion = tonumber(expansion), low = tonumber(low), high = tonumber(high),
                    seasonal = seasonal == "1", difficulties = {} }
                data.instances[key] = instance
                data.order[#data.order + 1] = instance
            end
            instance.difficulties[#instance.difficulties + 1] = { id = tonumber(id), difficulty = tonumber(difficulty),
                players = tonumber(players), qmin = tonumber(qmin), qmax = tonumber(qmax) }
        end
    elseif kind == "F" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local map, difficulty, boss = strsplit("~", entry, 3)
            local key = map .. ":" .. difficulty
            data.bosses[key] = data.bosses[key] or {}
            tinsert(data.bosses[key], boss)
        end
    elseif message == "Z" then
        table.sort(data.order, function(a, b)
            if a.low ~= b.low then
                return a.low < b.low
            end
            return a.name < b.name
        end)
        for _, instance in ipairs(data.order) do
            table.sort(instance.difficulties, function(a, b) return a.difficulty < b.difficulty end)
        end
        data.loaded, data.loading = true, false
        Refresh()
    end
end

-- Look: rounded, gold-edged boxes (Emberforge_Resources' art, the Classless window's look).
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
local function Box(target, fill, edge)
    local size = 8
    target.boxFill, target.boxEdge = {}, {}
    local function Piece(list, layer, file, flip)
        local texture = target:CreateTexture(nil, layer)
        if file then
            texture:SetTexture(ART .. file .. "Small")
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
local PANEL_FILL, PANEL_EDGE = { 0.02, 0.014, 0.011, 0.85 }, { 0.55, 0.41, 0.2, 1 }
local CELL_FILL, CELL_EDGE = { 0.075, 0.055, 0.043, 0.96 }, { 0.38, 0.29, 0.16, 1 }
local ON_FILL, ON_EDGE = { 0.3, 0.13, 0.03, 0.96 }, { 1, 0.72, 0.32, 1 }

-- The window.
frame = CreateFrame("Frame", "EmberforgeJournal", UIParent)
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
frame:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 24,
    insets = { left = 6, right = 6, top = 6, bottom = 6 },
})
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeJournal")

local title = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
title:SetFont("Fonts\\MORPHEUS.TTF", 22)
title:SetPoint("TOP", 0, -16)
title:SetTextColor(unpack(GOLD))
title:SetText("Dungeon Journal")

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -6, -6)

-- A tab: a small rounded box with its label, lit when selected.
local function Tab(parent, label, width, onClick)
    local tab = CreateFrame("Button", nil, parent)
    tab:SetWidth(width)
    tab:SetHeight(26)
    Box(tab, CELL_FILL, CELL_EDGE)
    tab.label = tab:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    tab.label:SetPoint("CENTER")
    tab.label:SetText(label)
    local hover = tab:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.6, 0.2, 0.1)
    hover:SetPoint("TOPLEFT", 2, -2)
    hover:SetPoint("BOTTOMRIGHT", -2, 2)
    tab:SetScript("OnClick", onClick)
    return tab
end

local function Selected(tab, on)
    BoxColor(tab, on and ON_FILL or CELL_FILL, on and ON_EDGE or CELL_EDGE)
    tab.label:SetTextColor(on and 1 or GOLD[1], on and 1 or GOLD[2], on and 1 or GOLD[3])
end

local expansionTabs, typeTabs = {}, {}
for expansion = 0, 2 do
    local tab = Tab(frame, EXPANSIONS[expansion], 190, function()
        view.expansion, view.selected, view.offset = expansion, nil, 0
        Refresh()
    end)
    tab:SetPoint("TOPLEFT", 20 + expansion * 196, -50)
    tab.expansion = expansion
    expansionTabs[#expansionTabs + 1] = tab
end
for index, kind in ipairs({ 1, 2 }) do
    local tab = Tab(frame, TYPES[kind] .. "s", 110, function()
        view.type, view.selected, view.offset = kind, nil, 0
        Refresh()
    end)
    tab:SetPoint("TOPRIGHT", -20 - (2 - index) * 116, -50)
    tab.kind = kind
    typeTabs[#typeTabs + 1] = tab
end

-- The list (left).
local list = CreateFrame("Frame", nil, frame)
list:SetWidth(LIST_WIDTH)
list:SetPoint("TOPLEFT", 20, -86)
list:SetPoint("BOTTOMLEFT", 20, 40)
Box(list, PANEL_FILL, PANEL_EDGE)

local function Shown()
    local shown = {}
    for _, instance in ipairs(data.order) do
        if instance.expansion == view.expansion and instance.type == view.type then
            shown[#shown + 1] = instance
        end
    end
    return shown
end

local rows = {}
local scroll = CreateFrame("ScrollFrame", "EmberforgeJournalScroll", list, "FauxScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", 6, -8)
scroll:SetPoint("BOTTOMRIGHT", -28, 8)
scroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, ROW_HEIGHT, Refresh)
end)
for index = 1, ROWS do
    local row = CreateFrame("Button", nil, list)
    row:SetHeight(ROW_HEIGHT - 2)
    row:SetPoint("TOPLEFT", 8, -8 - (index - 1) * ROW_HEIGHT)
    row:SetPoint("RIGHT", list, "RIGHT", -30, 0)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    row.name:SetPoint("LEFT", 8, 0)
    row.name:SetJustifyH("LEFT")
    row.levels = row:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    row.levels:SetPoint("RIGHT", -6, 0)
    row.name:SetPoint("RIGHT", row.levels, "LEFT", -6, 0)
    row.selected = row:CreateTexture(nil, "BACKGROUND")
    row.selected:SetTexture(1, 0.55, 0.15, 0.22)
    row.selected:SetAllPoints()
    local hover = row:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.6, 0.2, 0.1)
    hover:SetAllPoints()
    row:SetScript("OnClick", function(self)
        view.selected = self.instance and self.instance.key
        Refresh()
    end)
    rows[index] = row
end

-- The details (right).
local details = CreateFrame("Frame", nil, frame)
details:SetPoint("TOPLEFT", list, "TOPRIGHT", 12, 0)
details:SetPoint("BOTTOMRIGHT", -20, 40)
Box(details, PANEL_FILL, PANEL_EDGE)
local name = details:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
name:SetPoint("TOPLEFT", 18, -16)
name:SetPoint("RIGHT", -18, 0)
name:SetJustifyH("LEFT")
name:SetTextColor(unpack(GOLD))
local info = details:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
info:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 0, -12)
info:SetPoint("RIGHT", -18, 0)
info:SetJustifyH("LEFT")
info:SetJustifyV("TOP")
info:SetSpacing(4)
local bossTitle = details:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
bossTitle:SetPoint("TOPLEFT", info, "BOTTOMLEFT", 0, -18)
bossTitle:SetTextColor(unpack(GOLD))
local bossList = details:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
bossList:SetPoint("TOPLEFT", bossTitle, "BOTTOMLEFT", 0, -8)
bossList:SetPoint("RIGHT", -18, 0)
bossList:SetJustifyH("LEFT")
bossList:SetJustifyV("TOP")
bossList:SetSpacing(3)

local footer = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
footer:SetPoint("BOTTOM", 0, 16)
footer:SetTextColor(unpack(SUBTEXT))

local function Levels(low, high)
    if not low or low == 0 then
        return "-"
    end
    return low == high and tostring(low) or (low .. "-" .. high)
end

local function DifficultyName(instance, difficulty)
    if instance.type == 2 then
        return (difficulty.players > 0 and (difficulty.players .. "-player") or "Raid")
            .. (difficulty.difficulty >= 2 and " Heroic" or "")
    end
    return (difficulty.difficulty == 1 and "Heroic" or "Normal")
        .. (difficulty.players > 0 and (" (" .. difficulty.players .. " players)") or "")
end

local function ShowDetails(instance)
    if not instance then
        name:SetText(data.loaded and "Choose a dungeon or raid" or "Loading...")
        info:SetText("")
        bossTitle:SetText("")
        bossList:SetText("")
        return
    end
    name:SetText(instance.name .. (instance.seasonal and " |cff999999(holiday event)|r" or ""))
    local lines = {
        TYPES[instance.type] .. ", " .. EXPANSIONS[instance.expansion],
        "Creature levels: |cffffffff" .. Levels(instance.low, instance.high) .. "|r",
        "Map ID: " .. instance.map,
        "",
        "|cffffd100Difficulties|r",
    }
    for _, difficulty in ipairs(instance.difficulties) do
        lines[#lines + 1] = "  " .. DifficultyName(instance, difficulty) .. "  |cff999999queue levels "
            .. Levels(difficulty.qmin, difficulty.qmax) .. ", dungeon ID " .. difficulty.id .. "|r"
    end
    info:SetText(table.concat(lines, "\n"))
    -- Bosses: the first difficulty's that has any (the others' are the same encounters).
    local bosses
    for _, difficulty in ipairs(instance.difficulties) do
        bosses = bosses or data.bosses[instance.map .. ":" .. difficulty.difficulty]
    end
    bossTitle:SetText("Bosses")
    bossList:SetText(bosses and table.concat(bosses, "\n") or "|cff999999None listed|r")
end

function Refresh()
    if not frame:IsShown() then
        return
    end
    for _, tab in ipairs(expansionTabs) do
        Selected(tab, tab.expansion == view.expansion)
    end
    for _, tab in ipairs(typeTabs) do
        Selected(tab, tab.kind == view.type)
    end
    local shown = Shown()
    FauxScrollFrame_Update(scroll, #shown, ROWS, ROW_HEIGHT)
    local offset = FauxScrollFrame_GetOffset(scroll)
    local selected
    for index, row in ipairs(rows) do
        local instance = shown[index + offset]
        row.instance = instance
        if instance then
            row.name:SetText(instance.name .. (instance.seasonal and " |cff999999(event)|r" or ""))
            row.levels:SetText(Levels(instance.low, instance.high))
            if instance.key == view.selected then
                row.selected:Show()
                selected = instance
            else
                row.selected:Hide()
            end
            row:Show()
        else
            row:Hide()
        end
    end
    if not selected and view.selected then
        selected = data.instances[view.selected]
    end
    ShowDetails(selected)
    local counts = { 0, 0 }
    for _, instance in ipairs(data.order) do
        if instance.expansion == view.expansion then
            counts[instance.type] = counts[instance.type] + 1
        end
    end
    footer:SetText(EXPANSIONS[view.expansion] .. ": " .. counts[1] .. " dungeons, " .. counts[2]
        .. " raids. Creature levels are the server's, after the level squish.")
end

frame:SetScript("OnShow", function()
    if not data.loaded and not data.loading then
        data.loading = true
        Send("LIST")
    end
    Refresh()
end)

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix == PREFIX and sender == UnitName("player") then
        OnServerMessage(message)
    end
end)

SLASH_EMBERFORGEJOURNAL1 = "/journal"
SLASH_EMBERFORGEJOURNAL2 = "/dj"
SlashCmdList.EMBERFORGEJOURNAL = function()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
    end
end

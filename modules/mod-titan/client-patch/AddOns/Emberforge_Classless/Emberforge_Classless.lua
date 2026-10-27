-- Emberforge Classless: every class's abilities and talent trees by spec, the Hero's own abilities
-- and talents, choosing the starting kit after a Titan Tome of Selection, and Wildcard roll reveals.
-- Open with N, the minimap button, /classless or /cl.
--
-- Layout: the class row on top; spec tabs over the ability grid (left), the spec's talent tree with
-- its prerequisite arrows (middle), and your spells, talents and specs with a search box (right).
--
-- The server (mod-titan, src/classless.cpp) sends the data as addon whispers to the player, prefix
-- EFCL, and the window answers the same way:
--   to server     LIST, PICK,<spell id>, KEEP,<spell id>, REROLL,<spell id>
--   from server   B / T,<class>,<skill line>,<name> / A,<class>.<skill line>.<level>.<spell id>;...
--                 U,<class>,<tree order>,<tab id>
--                 N,<tab>.<talent>.<row>.<col>.<rank1>+<rank2>....<requires talent>.<its rank>;... / E
--                 K0 / K,<spell id>,...   R0 / R,<talent id>:<rank>,...   S,<picks left>,<locked>,<lock level>
--                 G,<spell id>,<first rank id>,<can reroll>   O

local PREFIX = "EFCL"
local ART = "Interface\\AddOns\\Emberforge_Classless\\Art\\"
local WIDTH, HEIGHT = 1160, 720
local PANEL_TOP, PANEL_HEIGHT = -182, 478
local GRID_X, GRID_WIDTH, TREE_X, TREE_WIDTH, LIST_X, LIST_WIDTH = 20, 600, 628, 300, 936, 204
local CELL_WIDTH, CELL_HEIGHT, CELL_STEP_X, CELL_STEP_Y, COLUMNS, ROWS = 182, 48, 188, 56, 3, 8
local LIST_ROW_HEIGHT, LIST_ROWS = 42, 10
local TALENT_ROWS, TALENT_COLUMNS, TALENT_SIZE = 11, 4, 32
local TALENT_X, TALENT_Y, TALENT_STEP_X, TALENT_STEP_Y = 41, 14, 62, 41
local EMBER = "|cffff8a1e"
local YOUR_ABILITIES = 0
local GENERAL = 4                   -- the spec tab for abilities outside the class's three trees
local SPELLS, TALENTS, SPECS = 1, 2, 3

-- Server class ids, alphabetical like the class row in game. Names are spelled out because the
-- client's own class list calls the Paladin "Hero" on Emberforge.
local CLASSES = {
    { id = 6, token = "DEATHKNIGHT", name = "Death Knight" },
    { id = 11, token = "DRUID", name = "Druid" },
    { id = 3, token = "HUNTER", name = "Hunter" },
    { id = 8, token = "MAGE", name = "Mage" },
    { id = 2, token = "PALADIN", name = "Paladin" },
    { id = 5, token = "PRIEST", name = "Priest" },
    { id = 4, token = "ROGUE", name = "Rogue" },
    { id = 7, token = "SHAMAN", name = "Shaman" },
    { id = 9, token = "WARLOCK", name = "Warlock" },
    { id = 1, token = "WARRIOR", name = "Warrior" },
}
local CLASS_NAMES, CLASS_TOKENS = {}, {}
for _, class in ipairs(CLASSES) do
    CLASS_NAMES[class.id] = class.name
    CLASS_TOKENS[class.id] = class.token
end

-- Each class's talent trees in the client's order: name and Interface\TalentFrame background.
local TREES = {
    [1] = { { "Arms", "WarriorArms" }, { "Fury", "WarriorFury" }, { "Protection", "WarriorProtection" } },
    [2] = { { "Holy", "PaladinHoly" }, { "Protection", "PaladinProtection" }, { "Retribution", "PaladinCombat" } },
    [3] = { { "Beast Mastery", "HunterBeastMastery" }, { "Marksmanship", "HunterMarksmanship" },
        { "Survival", "HunterSurvival" } },
    [4] = { { "Assassination", "RogueAssassination" }, { "Combat", "RogueCombat" }, { "Subtlety", "RogueSubtlety" } },
    [5] = { { "Discipline", "PriestDiscipline" }, { "Holy", "PriestHoly" }, { "Shadow", "PriestShadow" } },
    [6] = { { "Blood", "DeathKnightBlood" }, { "Frost", "DeathKnightFrost" }, { "Unholy", "DeathKnightUnholy" } },
    [7] = { { "Elemental", "ShamanElementalCombat" }, { "Enhancement", "ShamanEnhancement" },
        { "Restoration", "ShamanRestoration" } },
    [8] = { { "Arcane", "MageArcane" }, { "Fire", "MageFire" }, { "Frost", "MageFrost" } },
    [9] = { { "Affliction", "WarlockCurses" }, { "Demonology", "WarlockSummoning" },
        { "Destruction", "WarlockDestruction" } },
    [11] = { { "Balance", "DruidBalance" }, { "Feral Combat", "DruidFeralCombat" },
             { "Restoration", "DruidRestoration" } },
}
-- Skill lines whose name differs from their talent tree.
local SKILL_TO_TREE = { ["Elemental Combat"] = "Elemental", ["Shadow Magic"] = "Shadow" }

local data = {
    abilities = {},     -- spell id -> { id, class, skill, level }
    skillNames = {},    -- skill line -> name
    trees = {},         -- class -> order (1-3) -> tab id
    talents = {},       -- tab id -> { { id, row, col, ranks = { spell ids }, requires, requiresRank }, ... }
    talentRanks = {},   -- talent id -> owned rank
    known = {},         -- spell id -> true
    picks = 0,
    locked = false,
    lockLevel = 9,
    loaded = false,
    loading = false,
}
-- spec: the spec tab (1-3, or GENERAL); tree: the talent tree shown (follows the last spec tab).
local view = { class = nil, spec = 1, tree = 1, search = "", learnable = false, list = SPELLS,
    gridOffset = 0, listOffset = 0 }
local reveals = {}      -- queued roll reveals: { spell, firstRank, canReroll }
local frame, Refresh, ShowNextReveal

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function SpellName(id)
    return GetSpellInfo(id) or ("Spell " .. id)
end

local function SpellIcon(id)
    local _, _, icon = GetSpellInfo(id)
    return icon or "Interface\\Icons\\INV_Misc_QuestionMark"
end

local function Picking()
    return data.picks > 0 and not data.locked
end

local function CanPick(ability)
    return Picking() and not data.known[ability.id] and ability.level <= UnitLevel("player")
end

-- The talent tree an ability's skill line belongs to (1-3), or GENERAL when it is none of them.
local function AbilityTree(ability)
    local name = data.skillNames[ability.skill]
    name = name and (SKILL_TO_TREE[name] or name)
    for index, info in ipairs(TREES[ability.class] or {}) do
        if info[1] == name then
            return index
        end
    end
    return GENERAL
end

local function HasGeneral(class)
    for _, ability in pairs(data.abilities) do
        if ability.class == class and AbilityTree(ability) == GENERAL then
            return true
        end
    end
    return false
end

local function ByLevelThenName(a, b)
    if a.level ~= b.level then
        return a.level < b.level
    end
    return SpellName(a.id) < SpellName(b.id)
end

local function ByName(a, b)
    return SpellName(a.id or a.ranks[1]) < SpellName(b.id or b.ranks[1])
end

-- What the ability grid shows for the current view, search and filter.
local function GridAbilities()
    local list, search, level = {}, view.search:lower(), UnitLevel("player")
    for id, ability in pairs(data.abilities) do
        local show
        if search ~= "" then
            show = SpellName(id):lower():find(search, 1, true) ~= nil
        elseif view.class == YOUR_ABILITIES then
            show = data.known[id]
        else
            show = ability.class == view.class and AbilityTree(ability) == view.spec
        end
        if show and view.learnable then
            show = not data.known[id] and ability.level <= level
        end
        if show then
            list[#list + 1] = ability
        end
    end
    table.sort(list, ByLevelThenName)
    return list
end

local function KnownAbilities()
    local list = {}
    for id in pairs(data.known) do
        if data.abilities[id] then
            list[#list + 1] = data.abilities[id]
        end
    end
    table.sort(list, ByName)
    return list
end

local function OwnedTalents()
    local list = {}
    for _, talents in pairs(data.talents) do
        for _, talent in ipairs(talents) do
            if (data.talentRanks[talent.id] or 0) > 0 then
                list[#list + 1] = talent
            end
        end
    end
    table.sort(list, ByName)
    return list
end

local function TreePoints(tab)
    local points = 0
    for _, talent in ipairs(data.talents[tab] or {}) do
        points = points + (data.talentRanks[talent.id] or 0)
    end
    return points
end

-- Trees you have talent points in: { class, order, points }, most points first.
local function OwnedSpecs()
    local list = {}
    for _, class in ipairs(CLASSES) do
        for order, tab in pairs(data.trees[class.id] or {}) do
            local points = TreePoints(tab)
            if points > 0 then
                list[#list + 1] = { class = class.id, order = order, points = points }
            end
        end
    end
    table.sort(list, function(a, b)
        if a.points ~= b.points then
            return a.points > b.points
        end
        return TREES[a.class][a.order][1] < TREES[b.class][b.order][1]
    end)
    return list
end

-- Messages from the server --------------------------------------------------------------------------

local function OnServerMessage(message)
    local kind = message:sub(1, 1)
    if message == "B" then
        wipe(data.abilities)
        wipe(data.skillNames)
        wipe(data.trees)
        wipe(data.talents)
    elseif kind == "T" then
        local _, _, skill, name = strsplit(",", message, 4)
        data.skillNames[tonumber(skill)] = name
    elseif kind == "A" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local class, skill, level, id = strsplit(".", entry)
            id = tonumber(id)
            data.abilities[id] = { id = id, class = tonumber(class), skill = tonumber(skill), level = tonumber(level) }
        end
    elseif kind == "U" then
        local _, class, order, tab = strsplit(",", message)
        class = tonumber(class)
        data.trees[class] = data.trees[class] or {}
        data.trees[class][tonumber(order) + 1] = tonumber(tab)
    elseif kind == "N" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local tab, id, row, col, ranks, requires, requiresRank = strsplit(".", entry)
            local talent = { id = tonumber(id), row = tonumber(row), col = tonumber(col), ranks = {},
                requires = tonumber(requires) or 0, requiresRank = tonumber(requiresRank) or 0 }
            for rank in ranks:gmatch("%d+") do
                talent.ranks[#talent.ranks + 1] = tonumber(rank)
            end
            tab = tonumber(tab)
            data.talents[tab] = data.talents[tab] or {}
            tinsert(data.talents[tab], talent)
        end
    elseif message == "E" then
        data.loaded, data.loading = true, false
        Refresh()
    elseif message == "K0" then
        wipe(data.known)
    elseif kind == "K" then
        for id in message:sub(3):gmatch("%d+") do
            data.known[tonumber(id)] = true
        end
        Refresh()
    elseif message == "R0" then
        wipe(data.talentRanks)
        Refresh()
    elseif kind == "R" then
        for id, rank in message:sub(3):gmatch("(%d+):(%d+)") do
            data.talentRanks[tonumber(id)] = tonumber(rank)
        end
        Refresh()
    elseif kind == "S" then
        local _, picks, locked, lockLevel = strsplit(",", message)
        data.picks, data.locked, data.lockLevel = tonumber(picks) or 0, locked == "1", tonumber(lockLevel) or 9
        Refresh()
    elseif kind == "G" then
        local _, spell, firstRank, canReroll = strsplit(",", message)
        tinsert(reveals, { spell = tonumber(spell), firstRank = tonumber(firstRank), canReroll = canReroll == "1" })
        ShowNextReveal()
    elseif message == "O" then
        frame:Show()
    end
end

-- Window --------------------------------------------------------------------------------------------
-- The character creation screen's look: rounded near-black panels with a thin gold edge, gold
-- portrait rings that glow ember when selected, a Morpheus title over a gem divider.

-- A rounded box drawn on target: fill and edge pieces (Art\FillCorner, LineCorner, LineEdge) tinted
-- by BoxColor. Small boxes (cells, tabs, rows) use 8 px corners, panels 16 px.
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
    -- The flat fill stops half a pixel in, where the corner pieces' fill starts.
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

-- Colors (the creation screen's panel fill and gold edge).
local GOLD = { 1, 0.82, 0 }
local WINDOW_FILL, WINDOW_EDGE = { 0.045, 0.032, 0.026, 0.97 }, { 0.86, 0.64, 0.32, 1 }
local PANEL_FILL, PANEL_EDGE = { 0.02, 0.014, 0.011, 0.85 }, { 0.55, 0.41, 0.2, 1 }
local CELL_FILL, CELL_EDGE = { 0.075, 0.055, 0.043, 0.96 }, { 0.38, 0.29, 0.16, 1 }
local KNOWN_EDGE = { 0.95, 0.72, 0.35, 1 }
local TAB_ON_FILL, TAB_ON_EDGE = { 0.3, 0.13, 0.03, 0.96 }, { 1, 0.72, 0.32, 1 }
local SUBTEXT = { 0.86, 0.64, 0.32 }

local function Panel(parent, x, width)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetWidth(width)
    panel:SetHeight(PANEL_HEIGHT)
    panel:SetPoint("TOPLEFT", x, PANEL_TOP)
    Box(panel, PANEL_FILL, PANEL_EDGE)
    return panel
end

-- A square icon inside a rounded gold frame, with an ember glow behind it when lit.
local function FramedIcon(parent, size)
    local icon = parent:CreateTexture(nil, "ARTWORK")
    icon:SetWidth(size)
    icon:SetHeight(size)
    icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    icon.frame = parent:CreateTexture(nil, "OVERLAY")
    icon.frame:SetTexture(ART .. "IconFrame")
    icon.frame:SetWidth(size * 1.14)
    icon.frame:SetHeight(size * 1.14)
    icon.frame:SetPoint("CENTER", icon, "CENTER", 0, 0)
    icon.glow = parent:CreateTexture(nil, "BORDER")
    icon.glow:SetTexture(ART .. "IconGlow")
    icon.glow:SetBlendMode("ADD")
    icon.glow:SetWidth(size * 1.7)
    icon.glow:SetHeight(size * 1.7)
    icon.glow:SetPoint("CENTER", icon, "CENTER", 0, 0)
    icon.glow:Hide()
    return icon
end

local function LightIcon(icon, lit, frameColor)
    icon:SetDesaturated(not lit)
    if lit then
        icon:SetVertexColor(1, 1, 1)
        icon.glow:Show()
    else
        icon:SetVertexColor(0.7, 0.7, 0.7)
        icon.glow:Hide()
    end
    icon.frame:SetVertexColor(unpack(frameColor or (lit and KNOWN_EDGE or { 0.45, 0.42, 0.38, 1 })))
end

-- Scrolling: the mouse wheel over a panel moves its list; a thin gold thumb shows where you are.
local function Scrollbar(panel, top, height, key)
    local bar = { height = height, key = key }
    bar.track = panel:CreateTexture(nil, "ARTWORK")
    bar.track:SetTexture(0, 0, 0, 0.5)
    bar.track:SetWidth(4)
    bar.track:SetHeight(height)
    bar.track:SetPoint("TOPRIGHT", -9, top)
    bar.thumb = panel:CreateTexture(nil, "OVERLAY")
    bar.thumb:SetTexture(0.86, 0.64, 0.32, 0.9)
    bar.thumb:SetWidth(4)
    bar.top = top
    panel:EnableMouseWheel(true)
    panel:SetScript("OnMouseWheel", function(_, delta)
        local offset = math.max(0, math.min(view[key] - delta, bar.max or 0))
        if offset ~= view[key] then
            view[key] = offset
            Refresh()
        end
    end)
    return bar
end

-- Clamps the view's offset to the list, places the thumb and returns the offset.
local function UpdateScrollbar(bar, total, visible)
    bar.max = math.max(0, total - visible)
    view[bar.key] = math.min(view[bar.key], bar.max)
    if bar.max == 0 then
        bar.track:Hide()
        bar.thumb:Hide()
    else
        local thumb = math.max(24, bar.height * visible / total)
        bar.thumb:SetHeight(thumb)
        bar.thumb:ClearAllPoints()
        bar.thumb:SetPoint("TOP", bar.track, "TOP", 0, -(bar.height - thumb) * view[bar.key] / bar.max)
        bar.track:Show()
        bar.thumb:Show()
    end
    return view[bar.key]
end

frame = CreateFrame("Frame", "EmberforgeClassless", UIParent)
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
tinsert(UISpecialFrames, "EmberforgeClassless")     -- Escape closes it

local headerGlow = frame:CreateTexture(nil, "BORDER")
headerGlow:SetTexture(ART .. "HeaderGlow")
headerGlow:SetBlendMode("ADD")
headerGlow:SetWidth(760)
headerGlow:SetHeight(150)
headerGlow:SetPoint("TOP", 0, -1)

local title = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
title:SetFont("Fonts\\MORPHEUS.TTF", 22)
title:SetShadowOffset(1, -1)
title:SetPoint("TOP", 0, -14)
title:SetTextColor(unpack(GOLD))
title:SetText("Classless")
local divider = frame:CreateTexture(nil, "ARTWORK")
divider:SetTexture(ART .. "Divider")
divider:SetWidth(460)
divider:SetHeight(16)
divider:SetPoint("TOP", 0, -36)

local close = CreateFrame("Button", nil, frame)
close:SetWidth(28)
close:SetHeight(28)
close:SetPoint("TOPRIGHT", -12, -12)
close:SetNormalTexture(ART .. "Close")
local closePushed = close:CreateTexture(nil, "ARTWORK")
closePushed:SetTexture(ART .. "Close")
closePushed:SetVertexColor(0.7, 0.7, 0.7)
closePushed:SetAllPoints()
close:SetPushedTexture(closePushed)
close:SetHighlightTexture(ART .. "RingHover", "ADD")
close:SetScript("OnClick", function() frame:Hide() end)

local function ResetScrolls()
    view.gridOffset = 0
end

-- Class row: Abilities (your build, the Hero emblem), then the ten classes, as ringed portraits.
local classButtons = {}
local function ClassButton(index, id, coords, label)
    local button = CreateFrame("Button", nil, frame)
    button:SetWidth(54)
    button:SetHeight(54)
    button:SetPoint("TOP", frame, "TOPLEFT", WIDTH / 2 + (index - 6) * 96, -60)
    button.id = id
    local portrait = button:CreateTexture(nil, "ARTWORK")
    if coords then
        portrait:SetAllPoints()
        portrait:SetTexture("Interface\\TargetingFrame\\UI-Classes-Circles")
        -- Inside each circle's own rim, so the gold ring frames it.
        portrait:SetTexCoord(coords[1] + 0.012, coords[2] - 0.012, coords[3] + 0.012, coords[4] - 0.012)
        local ring = button:CreateTexture(nil, "OVERLAY")
        ring:SetTexture(ART .. "Ring")
        ring:SetWidth(68)
        ring:SetHeight(68)
        ring:SetPoint("CENTER")
    else
        portrait:SetTexture(ART .. "HeroEmblem")
        portrait:SetWidth(68)
        portrait:SetHeight(68)
        portrait:SetPoint("CENTER")
    end
    local hover = button:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(ART .. "RingHover")
    hover:SetBlendMode("ADD")
    hover:SetWidth(74)
    hover:SetHeight(74)
    hover:SetPoint("CENTER")
    local glow = button:CreateTexture(nil, "OVERLAY")
    glow:SetTexture(ART .. "RingSelected")
    glow:SetBlendMode("ADD")
    glow:SetWidth(78)
    glow:SetHeight(78)
    glow:SetPoint("CENTER")
    button.glow = glow
    local text = button:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    text:SetPoint("TOP", button, "BOTTOM", 0, -10)
    text:SetText(label)
    button.label = text
    button:SetScript("OnClick", function(self)
        view.class, view.spec, view.tree = self.id, 1, 1
        ResetScrolls()
        Refresh()
    end)
    classButtons[#classButtons + 1] = button
end

ClassButton(1, YOUR_ABILITIES, nil, "Abilities")
for index, class in ipairs(CLASSES) do
    ClassButton(index + 1, class.id, CLASS_ICON_TCOORDS[class.token], class.name)
end

-- Tabs: small rounded boxes; the selected one glows ember.
local function Tab(x, width, onClick)
    local tab = CreateFrame("Button", nil, frame)
    tab:SetWidth(width)
    tab:SetHeight(26)
    tab:SetPoint("BOTTOMLEFT", frame, "TOPLEFT", x, PANEL_TOP + 6)
    Box(tab, CELL_FILL, CELL_EDGE, true)
    tab.label = tab:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    tab.label:SetPoint("CENTER", 0, 0)
    tab:SetFontString(tab.label)
    local hover = tab:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.6, 0.2, 0.1)
    hover:SetPoint("TOPLEFT", 2, -2)
    hover:SetPoint("BOTTOMRIGHT", -2, 2)
    tab:SetScript("OnClick", onClick)
    return tab
end

local function SelectTab(tab, selected)
    if selected then
        BoxColor(tab, TAB_ON_FILL, TAB_ON_EDGE)
        tab.label:SetTextColor(1, 1, 1)
    else
        BoxColor(tab, CELL_FILL, CELL_EDGE)
        tab.label:SetTextColor(unpack(GOLD))
    end
end

-- Spec tabs over the ability grid: the class's three trees, then General when it has abilities
-- outside them. The talent tree follows the last tree picked.
local specButtons = {}
for index = 1, 4 do
    local button = Tab(GRID_X + (index - 1) * 128, 122, function(self)
        view.spec = self.spec
        if self.spec ~= GENERAL then
            view.tree = self.spec
        end
        ResetScrolls()
        Refresh()
    end)
    button.spec = index
    specButtons[index] = button
end

-- Right panel tabs: your spells, your talents, the specs you have points in.
local listTabs = {}
for index, label in ipairs({ "Spells", "Talents", "Specs" }) do
    local tab = Tab(LIST_X + (index - 1) * 69, 66, function(self)
        view.list = self.list
        view.listOffset = 0
        Refresh()
    end)
    tab.list = index
    tab:SetText(label)
    listTabs[index] = tab
end

-- Ability grid ------------------------------------------------------------------------------------

local grid = Panel(frame, GRID_X, GRID_WIDTH)
local cells = {}
local function OnCellEnter(self)
    if not self.ability then
        return
    end
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("spell:" .. self.ability.id)
    GameTooltip:AddLine((CLASS_NAMES[self.ability.class] or "") .. " - level " .. self.ability.level, 0.7, 0.7, 0.7)
    if CanPick(self.ability) then
        GameTooltip:AddLine("Click to choose this ability.", 0.3, 1, 0.3)
    end
    GameTooltip:Show()
end

local function OnCellClick(self)
    local ability = self.ability
    if not ability then
        return
    end
    if IsModifiedClick("CHATLINK") then
        ChatEdit_InsertLink(GetSpellLink(ability.id))
    elseif CanPick(ability) then
        StaticPopup_Show("EMBERFORGE_CLASSLESS_PICK", SpellName(ability.id), nil, ability.id)
    end
end

local function RowHover(button)
    local hover = button:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.6, 0.2, 0.08)
    hover:SetPoint("TOPLEFT", 2, -2)
    hover:SetPoint("BOTTOMRIGHT", -2, 2)
end

for index = 1, COLUMNS * ROWS do
    local cell = CreateFrame("Button", nil, grid)
    cell:SetWidth(CELL_WIDTH)
    cell:SetHeight(CELL_HEIGHT)
    local column, row = (index - 1) % COLUMNS, math.floor((index - 1) / COLUMNS)
    cell:SetPoint("TOPLEFT", 14 + column * CELL_STEP_X, -14 - row * CELL_STEP_Y)
    Box(cell, CELL_FILL, CELL_EDGE, true)
    RowHover(cell)
    cell.icon = FramedIcon(cell, 32)
    cell.icon:SetPoint("LEFT", 9, 0)
    cell.name = cell:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    cell.name:SetPoint("TOPLEFT", cell.icon, "TOPRIGHT", 9, -1)
    cell.name:SetWidth(CELL_WIDTH - 60)
    cell.name:SetHeight(14)
    cell.name:SetJustifyH("LEFT")
    cell.sub = cell:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    cell.sub:SetPoint("BOTTOMLEFT", cell.icon, "BOTTOMRIGHT", 9, 1)
    cell:SetScript("OnEnter", OnCellEnter)
    cell:SetScript("OnLeave", function() GameTooltip:Hide() end)
    cell:SetScript("OnClick", OnCellClick)
    cells[index] = cell
end
local gridBar = Scrollbar(grid, -14, ROWS * CELL_STEP_Y - 8, "gridOffset")

-- Over the grid where the spec tabs sit for a class: what the Abilities view lists.
local gridTitle = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
gridTitle:SetPoint("BOTTOMLEFT", grid, "TOPLEFT", 6, 13)

local empty = grid:CreateFontString(nil, "ARTWORK", "GameFontDisable")
empty:SetPoint("TOP", 0, -40)

-- Talent tree -------------------------------------------------------------------------------------

local tree = Panel(frame, TREE_X, TREE_WIDTH)
local treeTitle = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
treeTitle:SetPoint("BOTTOM", tree, "TOP", 0, 13)

-- The art sits on its own layer inside the panel's edge: the spec background, darkened toward the
-- edges, then the prerequisite lines; the talent buttons above it all.
local art = CreateFrame("Frame", nil, tree)
art:SetPoint("TOPLEFT", 3, -3)
art:SetPoint("BOTTOMRIGHT", -3, 3)
local ART_WIDTH, ART_HEIGHT = TREE_WIDTH - 6, PANEL_HEIGHT - 6
local backgrounds = {}
for index, part in ipairs({ { "TopLeft", 0.8, 2 / 3 }, { "TopRight", 0.2, 2 / 3 }, { "BottomLeft", 0.8, 1 / 3 },
    { "BottomRight", 0.2, 1 / 3 } }) do
    local texture = art:CreateTexture(nil, "BACKGROUND")
    texture:SetWidth(ART_WIDTH * part[2])
    texture:SetHeight(ART_HEIGHT * part[3])
    texture.part = part[1]
    backgrounds[index] = texture
end
backgrounds[1]:SetPoint("TOPLEFT")
backgrounds[2]:SetPoint("TOPLEFT", backgrounds[1], "TOPRIGHT")
backgrounds[3]:SetPoint("TOPLEFT", backgrounds[1], "BOTTOMLEFT")
backgrounds[4]:SetPoint("TOPLEFT", backgrounds[1], "BOTTOMRIGHT")
local shade = art:CreateTexture(nil, "BORDER")
shade:SetTexture(ART .. "Shade")
shade:SetAllPoints()

local function TalentLeft(col) return TALENT_X + col * TALENT_STEP_X end
local function TalentTop(row) return -(TALENT_Y + row * TALENT_STEP_Y) end

local talentButtons = {}
local function OnTalentEnter(self)
    local talent = self.talent
    if not talent then
        return
    end
    local rank = data.talentRanks[talent.id] or 0
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("spell:" .. talent.ranks[math.max(math.min(rank, #talent.ranks), 1)])
    GameTooltip:AddLine("Rank " .. rank .. "/" .. #talent.ranks, 1, 0.82, 0)
    if rank < #talent.ranks then
        GameTooltip:AddLine("Wildcard grants talents at random from level 10.", 0.6, 0.6, 0.6)
    end
    GameTooltip:Show()
end

for row = 1, TALENT_ROWS do
    for column = 1, TALENT_COLUMNS do
        local button = CreateFrame("Button", nil, art)
        button:SetWidth(TALENT_SIZE)
        button:SetHeight(TALENT_SIZE)
        button:SetPoint("TOPLEFT", TalentLeft(column - 1), TalentTop(row - 1))
        button.icon = FramedIcon(button, TALENT_SIZE)
        button.icon:SetPoint("CENTER")
        local badge = button:CreateTexture(nil, "OVERLAY")
        badge:SetTexture(0.02, 0.015, 0.01, 0.9)
        badge:SetWidth(14)
        badge:SetHeight(12)
        badge:SetPoint("BOTTOMRIGHT", 5, -4)
        button.rank = button:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
        button.rank:SetPoint("CENTER", badge, "CENTER", 0, 0)
        local hover = button:CreateTexture(nil, "HIGHLIGHT")
        hover:SetTexture(ART .. "IconGlow")
        hover:SetBlendMode("ADD")
        hover:SetWidth(TALENT_SIZE * 1.9)
        hover:SetHeight(TALENT_SIZE * 1.9)
        hover:SetPoint("CENTER")
        button:SetScript("OnEnter", OnTalentEnter)
        button:SetScript("OnLeave", function() GameTooltip:Hide() end)
        button:SetScript("OnClick", function(self)
            if self.talent and IsModifiedClick("CHATLINK") then
                ChatEdit_InsertLink(GetSpellLink(self.talent.ranks[1]))
            end
        end)
        talentButtons[(row - 1) * TALENT_COLUMNS + column] = button
    end
end

-- Prerequisite arrows: lines between the talents and an arrowhead at the talent that needs the
-- other, gold once the prerequisite's rank is owned.
local LIT, UNLIT = { 1, 0.74, 0.3, 1 }, { 0.5, 0.48, 0.45, 0.9 }
local branches, arrowheads = { used = 0 }, { used = 0 }

local function Pooled(pool, layer)
    pool.used = pool.used + 1
    local texture = pool[pool.used]
    if not texture then
        texture = art:CreateTexture(nil, layer)
        pool[pool.used] = texture
    end
    texture:ClearAllPoints()
    texture:Show()
    return texture
end

local function Branch(x1, y1, x2, y2, lit)
    local line = Pooled(branches, "ARTWORK")
    line:SetTexture(unpack(lit and LIT or UNLIT))
    if x1 == x2 then
        line:SetWidth(3)
        line:SetHeight(math.abs(y1 - y2))
        line:SetPoint("TOPLEFT", x1 - 1.5, math.max(y1, y2))
    else
        line:SetWidth(math.abs(x1 - x2))
        line:SetHeight(3)
        line:SetPoint("TOPLEFT", math.min(x1, x2), y1 + 1.5)
    end
end

local function Arrowhead(direction, x, y, lit)
    local arrow = Pooled(arrowheads, "ARTWORK")
    arrow:SetTexture(ART .. (direction == "down" and "ArrowDown" or "ArrowRight"))
    if direction == "left" then
        arrow:SetTexCoord(1, 0, 0, 1)
    else
        arrow:SetTexCoord(0, 1, 0, 1)
    end
    arrow:SetVertexColor(unpack(lit and LIT or UNLIT))
    arrow:SetWidth(14)
    arrow:SetHeight(14)
    arrow:SetPoint("CENTER", art, "TOPLEFT", x, y)
end

local function DrawArrow(from, to)
    local lit = (data.talentRanks[from.id] or 0) >= to.requiresRank
    local half = TALENT_SIZE / 2
    local fromX, fromY = TalentLeft(from.col) + half, TalentTop(from.row) - half
    local toX, toY = TalentLeft(to.col) + half, TalentTop(to.row) - half
    if from.row == to.row then
        local side = to.col > from.col and 1 or -1
        Branch(fromX + side * (half + 3), fromY, toX - side * (half + 8), toY, lit)
        Arrowhead(side > 0 and "right" or "left", toX - side * (half + 7), toY, lit)
    else
        if from.col ~= to.col then
            local side = to.col > from.col and 1 or -1
            Branch(fromX + side * (half + 3), fromY, toX + 1.5 * side, fromY, lit)
            Branch(toX, fromY, toX, toY + half + 8, lit)
        else
            Branch(fromX, fromY - half - 3, toX, toY + half + 8, lit)
        end
        Arrowhead("down", toX, toY + half + 7, lit)
    end
end

-- Your talents (Abilities view), listed over the dark tree panel.
local talentRows = {}
for index = 1, 13 do
    local row = CreateFrame("Button", nil, art)
    row:SetWidth(ART_WIDTH - 20)
    row:SetHeight(32)
    row:SetPoint("TOPLEFT", 10, -12 - (index - 1) * 35)
    Box(row, CELL_FILL, CELL_EDGE, true)
    row.icon = FramedIcon(row, 26)
    row.icon:SetPoint("LEFT", 7, 0)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 9, 0)
    row.name:SetWidth(ART_WIDTH - 70)
    row.name:SetJustifyH("LEFT")
    row:SetScript("OnEnter", OnTalentEnter)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    talentRows[index] = row
end

-- Right panel: search, then your spells, talents or specs -------------------------------------------

local list = Panel(frame, LIST_X, LIST_WIDTH)
local searchBox = CreateFrame("Frame", nil, list)
searchBox:SetHeight(26)
searchBox:SetPoint("TOPLEFT", 10, -10)
searchBox:SetPoint("TOPRIGHT", -10, -10)
Box(searchBox, { 0, 0, 0, 0.7 }, CELL_EDGE, true)
local magnifier = searchBox:CreateTexture(nil, "ARTWORK")
magnifier:SetTexture(ART .. "Search")
magnifier:SetVertexColor(unpack(SUBTEXT))
magnifier:SetWidth(14)
magnifier:SetHeight(14)
magnifier:SetPoint("LEFT", 8, 0)
local search = CreateFrame("EditBox", "EmberforgeClasslessSearch", searchBox)
search:SetPoint("TOPLEFT", 26, 0)
search:SetPoint("BOTTOMRIGHT", -6, 0)
search:SetFontObject(GameFontHighlightSmall)
search:SetAutoFocus(false)
search:SetScript("OnTextChanged", function(self)
    view.search = self:GetText() or ""
    ResetScrolls()
    Refresh()
end)
search:SetScript("OnEscapePressed", search.ClearFocus)
search:SetScript("OnEnterPressed", search.ClearFocus)
local searchHint = search:CreateFontString(nil, "ARTWORK", "GameFontDisableSmall")
searchHint:SetPoint("LEFT", 0, 0)
searchHint:SetText("Search all abilities")

local function OnListEnter(self)
    if self.talent then
        OnTalentEnter(self)
    else
        OnCellEnter(self)
    end
end

local function OnListClick(self)
    if self.spec then
        view.class, view.spec, view.tree = self.spec.class, self.spec.order, self.spec.order
        ResetScrolls()
        Refresh()
    else
        OnCellClick(self)
    end
end

local listRows = {}
for index = 1, LIST_ROWS do
    local row = CreateFrame("Button", nil, list)
    row:SetWidth(LIST_WIDTH - 36)
    row:SetHeight(LIST_ROW_HEIGHT - 6)
    row:SetPoint("TOPLEFT", 10, -46 - (index - 1) * LIST_ROW_HEIGHT)
    Box(row, CELL_FILL, CELL_EDGE, true)
    RowHover(row)
    row.icon = FramedIcon(row, 26)
    row.icon:SetPoint("LEFT", 7, 0)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 9, 0)
    row.name:SetWidth(LIST_WIDTH - 84)
    row.name:SetJustifyH("LEFT")
    row:SetScript("OnEnter", OnListEnter)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    row:SetScript("OnClick", OnListClick)
    listRows[index] = row
end
local listBar = Scrollbar(list, -46, LIST_ROWS * LIST_ROW_HEIGHT - 6, "listOffset")

local listEmpty = list:CreateFontString(nil, "ARTWORK", "GameFontDisableSmall")
listEmpty:SetPoint("TOPLEFT", 14, -52)
listEmpty:SetWidth(LIST_WIDTH - 28)
listEmpty:SetJustifyH("LEFT")

-- Footer: the learnable filter and the tome status.
local learnable = CreateFrame("CheckButton", "EmberforgeClasslessLearnable", frame)
learnable:SetWidth(18)
learnable:SetHeight(18)
learnable:SetPoint("BOTTOMLEFT", 26, 22)
Box(learnable, { 0, 0, 0, 0.7 }, CELL_EDGE, true)
learnable:SetCheckedTexture("Interface\\Buttons\\UI-CheckBox-Check")
learnable:SetHighlightTexture(ART .. "RingHover", "ADD")
local learnableText = learnable:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
learnableText:SetPoint("LEFT", learnable, "RIGHT", 8, 0)
learnableText:SetText("Learnable now only")
learnable:SetScript("OnClick", function(self)
    view.learnable = self:GetChecked() and true or false
    ResetScrolls()
    Refresh()
end)

local status = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
status:SetPoint("BOTTOM", 0, 26)
status:SetTextColor(unpack(SUBTEXT))

StaticPopupDialogs["EMBERFORGE_CLASSLESS_PICK"] = {
    text = "Choose %s for your starting abilities?",
    button1 = ACCEPT,
    button2 = CANCEL,
    OnAccept = function(self, spellId)
        Send("PICK," .. spellId)
    end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
}

-- Refresh -------------------------------------------------------------------------------------------

local function RefreshClassRow()
    for _, button in ipairs(classButtons) do
        if button.id == view.class and view.search == "" then
            button.glow:Show()
            button.label:SetTextColor(unpack(GOLD))
        else
            button.glow:Hide()
            button.label:SetTextColor(0.9, 0.9, 0.9)
        end
    end
end

local function RefreshSpecs()
    local trees = TREES[view.class]
    for index, button in ipairs(specButtons) do
        if not trees or view.search ~= "" or (index == GENERAL and not HasGeneral(view.class)) then
            button:Hide()
        else
            button:SetText(index == GENERAL and "General" or trees[index][1])
            SelectTab(button, button.spec == view.spec)
            button:Show()
        end
    end
    for _, tab in ipairs(listTabs) do
        SelectTab(tab, tab.list == view.list)
    end
end

local function RefreshGrid()
    local abilities = GridAbilities()
    local offset = UpdateScrollbar(gridBar, math.ceil(#abilities / COLUMNS), ROWS) * COLUMNS
    local level = UnitLevel("player")
    for index, cell in ipairs(cells) do
        local ability = abilities[offset + index]
        cell.ability = ability
        if ability then
            local known = data.known[ability.id]
            cell.icon:SetTexture(SpellIcon(ability.id))
            LightIcon(cell.icon, known, not known and ability.level <= level and CELL_EDGE or nil)
            cell.name:SetText(SpellName(ability.id))
            if view.class == YOUR_ABILITIES and view.search == "" then
                cell.sub:SetText(CLASS_NAMES[ability.class] or "")
                cell.sub:SetTextColor(unpack(SUBTEXT))
            elseif known then
                cell.sub:SetText("Known")
                cell.sub:SetTextColor(1, 0.82, 0)
            elseif CanPick(ability) then
                cell.sub:SetText("Click to choose")
                cell.sub:SetTextColor(1, 0.55, 0.12)
            else
                cell.sub:SetText("Level " .. ability.level)
                if ability.level > level then
                    cell.sub:SetTextColor(0.9, 0.25, 0.2)
                else
                    cell.sub:SetTextColor(unpack(SUBTEXT))
                end
            end
            BoxColor(cell, CELL_FILL, known and KNOWN_EDGE or CELL_EDGE)
            cell:Show()
        else
            cell:Hide()
        end
    end

    if view.search ~= "" then
        gridTitle:SetText("Search results  |cffffffff" .. #abilities .. "|r")
    elseif view.class == YOUR_ABILITIES then
        gridTitle:SetText("Your Abilities  |cffffffff" .. #abilities .. "|r")
    else
        gridTitle:SetText("")
    end
    if not data.loaded then
        empty:SetText("Loading abilities...")
    elseif #abilities == 0 then
        empty:SetText(view.search ~= "" and "No ability matches your search."
            or view.class == YOUR_ABILITIES and "You know no abilities yet." or "No abilities here.")
    else
        empty:SetText("")
    end
end

local function RefreshTree()
    for _, button in ipairs(talentButtons) do
        button.talent = nil
        button:Hide()
    end
    for _, row in ipairs(talentRows) do
        row.talent = nil
        row:Hide()
    end
    for _, pool in ipairs({ branches, arrowheads }) do
        for index = 1, pool.used do
            pool[index]:Hide()
        end
        pool.used = 0
    end

    if view.class == YOUR_ABILITIES or view.search ~= "" then
        for _, texture in ipairs(backgrounds) do
            texture:SetTexture(nil)
        end
        local owned = OwnedTalents()
        local points = 0
        for _, talent in ipairs(owned) do
            points = points + data.talentRanks[talent.id]
        end
        treeTitle:SetText("Your Talents  |cffffffff" .. points .. "|r")
        for index, row in ipairs(talentRows) do
            local talent = owned[index]
            if talent then
                row.talent = talent
                row.icon:SetTexture(SpellIcon(talent.ranks[1]))
                LightIcon(row.icon, true)
                local text = SpellName(talent.ranks[1]) .. "  " .. data.talentRanks[talent.id] .. "/" .. #talent.ranks
                if index == #talentRows and #owned > index then
                    text = "+" .. (#owned - index + 1) .. " more"
                end
                row.name:SetText(text)
                row:Show()
            end
        end
        return
    end

    local trees = TREES[view.class]
    local tab = data.trees[view.class] and data.trees[view.class][view.tree]
    local info = trees and trees[view.tree]
    for _, texture in ipairs(backgrounds) do
        texture:SetTexture(info and ("Interface\\TalentFrame\\" .. info[2] .. "-" .. texture.part) or nil)
    end
    local byId = {}
    for _, talent in ipairs(tab and data.talents[tab] or {}) do
        byId[talent.id] = talent
        local rank = data.talentRanks[talent.id] or 0
        local button = talentButtons[talent.row * TALENT_COLUMNS + talent.col + 1]
        if button then
            button.talent = talent
            button.icon:SetTexture(SpellIcon(talent.ranks[math.max(math.min(rank, #talent.ranks), 1)]))
            LightIcon(button.icon, rank > 0)
            button.rank:SetText(rank)
            if rank == #talent.ranks then
                button.rank:SetTextColor(1, 0.82, 0)
            elseif rank > 0 then
                button.rank:SetTextColor(0.4, 1, 0.4)
            else
                button.rank:SetTextColor(0.8, 0.8, 0.8)
            end
            button:Show()
        end
    end
    for _, talent in pairs(byId) do
        if byId[talent.requires] then
            DrawArrow(byId[talent.requires], talent)
        end
    end
    local points = TreePoints(tab)
    treeTitle:SetText(info and (info[1] .. "  |cffffffff" .. points .. (points == 1 and " point" or " points") .. "|r")
        or "")
end

local function RefreshList()
    local entries, emptyText
    if view.list == TALENTS then
        entries, emptyText = OwnedTalents(), "No talents yet. Wildcard rolls them from level 10."
    elseif view.list == SPECS then
        entries, emptyText = OwnedSpecs(), "No talent points spent yet."
    else
        entries, emptyText = KnownAbilities(), "You know no abilities yet."
    end
    local offset = UpdateScrollbar(listBar, #entries, LIST_ROWS)
    listEmpty:SetText(#entries == 0 and data.loaded and emptyText or "")
    for index, row in ipairs(listRows) do
        local entry = entries[offset + index]
        row.ability, row.talent, row.spec = nil, nil, nil
        if entry then
            if view.list == TALENTS then
                row.talent = entry
                row.icon:SetTexture(SpellIcon(entry.ranks[1]))
                row.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
                row.name:SetText(SpellName(entry.ranks[1]) .. "  |cffdca352" .. data.talentRanks[entry.id] .. "/"
                    .. #entry.ranks .. "|r")
            elseif view.list == SPECS then
                row.spec = entry
                row.icon:SetTexture("Interface\\TargetingFrame\\UI-Classes-Circles")
                row.icon:SetTexCoord(unpack(CLASS_ICON_TCOORDS[CLASS_TOKENS[entry.class]]))
                row.name:SetText(TREES[entry.class][entry.order][1] .. "  |cffffffff" .. entry.points .. "|r\n"
                    .. "|cffdca352" .. CLASS_NAMES[entry.class] .. "|r")
            else
                row.ability = entry
                row.icon:SetTexture(SpellIcon(entry.id))
                row.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
                row.name:SetText(SpellName(entry.id))
            end
            LightIcon(row.icon, true)
            row:Show()
        else
            row:Hide()
        end
    end
    if view.search == "" then
        searchHint:Show()
    else
        searchHint:Hide()
    end
end

local function RefreshStatus()
    if Picking() then
        status:SetText(EMBER .. "Titan Tome of Selection:|r choose " .. data.picks .. " more "
            .. (data.picks == 1 and "ability" or "abilities")
            .. " - your abilities lock at level " .. data.lockLevel)
    elseif not data.locked then
        status:SetText("Reroll with the Tome of Randomization or choose with the Tome of Selection until level "
            .. data.lockLevel)
    else
        status:SetText("")
    end
end

function Refresh()
    if not frame:IsShown() then
        return
    end
    if view.class == nil then
        view.class = next(data.known) and not Picking() and YOUR_ABILITIES or CLASSES[1].id
    end
    RefreshClassRow()
    RefreshSpecs()
    RefreshGrid()
    RefreshTree()
    RefreshList()
    RefreshStatus()
end

frame:SetScript("OnShow", function(self)
    -- Fit small screens (4:3 has about 1024 units across).
    local width, height = UIParent:GetWidth(), UIParent:GetHeight()
    if width and height then
        self:SetScale(math.min(1, (width - 20) / WIDTH, (height - 20) / HEIGHT))
    end
    if not data.loaded and not data.loading then
        data.loading = true
        Send("LIST")
    end
    Refresh()
end)

function EmberforgeClassless_Toggle()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
    end
end

-- Roll reveal: a quick shuffle of icons, then the rolled ability with its tooltip, Keep or Reroll.

local reveal = CreateFrame("Frame", "EmberforgeClasslessReveal", UIParent)
reveal:SetWidth(320)
reveal:SetHeight(160)
reveal:SetPoint("TOP", 0, -150)
reveal:SetFrameStrata("DIALOG")
reveal:EnableMouse(true)
Box(reveal, WINDOW_FILL, WINDOW_EDGE)
reveal:Hide()

local revealGlow = reveal:CreateTexture(nil, "BORDER")
revealGlow:SetTexture(ART .. "HeaderGlow")
revealGlow:SetBlendMode("ADD")
revealGlow:SetWidth(300)
revealGlow:SetHeight(70)
revealGlow:SetPoint("TOP", 0, -1)
local revealTitle = reveal:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
revealTitle:SetFont("Fonts\\MORPHEUS.TTF", 18)
revealTitle:SetShadowOffset(1, -1)
revealTitle:SetTextColor(unpack(GOLD))
revealTitle:SetPoint("TOP", 0, -12)
local revealDivider = reveal:CreateTexture(nil, "ARTWORK")
revealDivider:SetTexture(ART .. "Divider")
revealDivider:SetWidth(240)
revealDivider:SetHeight(12)
revealDivider:SetPoint("TOP", 0, -32)
local revealIcon = FramedIcon(reveal, 48)
revealIcon:SetPoint("TOPLEFT", 28, -50)
local revealName = reveal:CreateFontString(nil, "ARTWORK", "GameFontHighlightLarge")
revealName:SetPoint("TOPLEFT", revealIcon, "TOPRIGHT", 16, -6)
revealName:SetWidth(200)
revealName:SetJustifyH("LEFT")
local revealNote = reveal:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
revealNote:SetTextColor(unpack(SUBTEXT))
revealNote:SetPoint("TOPLEFT", revealName, "BOTTOMLEFT", 0, -4)

local keep = CreateFrame("Button", nil, reveal, "UIPanelButtonTemplate")
keep:SetWidth(110)
keep:SetHeight(24)
keep:SetPoint("BOTTOMLEFT", 40, 18)
local reroll = CreateFrame("Button", nil, reveal, "UIPanelButtonTemplate")
reroll:SetWidth(110)
reroll:SetHeight(24)
reroll:SetPoint("BOTTOMRIGHT", -40, 18)
reroll:SetText("Reroll")

local SHUFFLE_TIME, SHUFFLE_STEP = 0.8, 0.07
local function Shuffle(self, elapsed)
    self.shuffle = self.shuffle - elapsed
    self.step = self.step - elapsed
    if self.shuffle <= 0 then
        self:SetScript("OnUpdate", nil)
        local current = reveals[1]
        revealIcon:SetTexture(SpellIcon(current.spell))
        LightIcon(revealIcon, true)
        revealName:SetText(SpellName(current.spell))
        revealNote:SetText(current.canReroll and "Keep it, or reroll once for another." or "")
        keep:Enable()
        if current.canReroll then
            reroll:Enable()
        end
        GameTooltip:SetOwner(reveal, "ANCHOR_NONE")
        GameTooltip:SetPoint("TOPLEFT", reveal, "TOPRIGHT", 0, 0)
        GameTooltip:SetHyperlink("spell:" .. current.spell)
        GameTooltip:Show()
    elseif self.step <= 0 then
        self.step = SHUFFLE_STEP
        local ids = {}
        for id in pairs(data.abilities) do
            ids[#ids + 1] = id
        end
        revealIcon:SetTexture(#ids > 0 and SpellIcon(ids[math.random(#ids)]) or "Interface\\Icons\\INV_Misc_Dice_01")
    end
end

function ShowNextReveal()
    local current = reveals[1]
    if not current then
        reveal:Hide()
        return
    end
    if reveal:IsShown() and reveal.current == current then
        return
    end
    reveal.current = current
    revealTitle:SetText("Wildcard Roll")
    LightIcon(revealIcon, false)
    revealName:SetText("Rolling...")
    revealNote:SetText("")
    keep:SetText(current.canReroll and "Keep" or OKAY)
    keep:Disable()
    reroll:Disable()
    if current.canReroll then
        reroll:Show()
        keep:ClearAllPoints()
        keep:SetPoint("BOTTOMLEFT", 40, 18)
    else
        reroll:Hide()
        keep:ClearAllPoints()
        keep:SetPoint("BOTTOM", 0, 18)
    end
    reveal.shuffle, reveal.step = SHUFFLE_TIME, 0
    reveal:SetScript("OnUpdate", Shuffle)
    reveal:Show()
end

local function Answer(message)
    local current = tremove(reveals, 1)
    if GameTooltip:IsOwned(reveal) then
        GameTooltip:Hide()
    end
    if message and current and current.canReroll then
        Send(message .. "," .. current.firstRank)
    end
    reveal.current = nil
    ShowNextReveal()
end
keep:SetScript("OnClick", function() Answer("KEEP") end)
reroll:SetScript("OnClick", function() Answer("REROLL") end)

-- Minimap button: drag it around the minimap; the position is kept per character.

local minimapButton = CreateFrame("Button", "EmberforgeClasslessMinimapButton", Minimap)
minimapButton:SetWidth(32)
minimapButton:SetHeight(32)
minimapButton:SetFrameStrata("MEDIUM")
minimapButton:SetFrameLevel(8)
minimapButton:RegisterForDrag("LeftButton")
minimapButton:SetHighlightTexture(ART .. "RingHover", "ADD")
local minimapIcon = minimapButton:CreateTexture(nil, "ARTWORK")
minimapIcon:SetTexture(ART .. "HeroEmblem")
minimapIcon:SetAllPoints()

local function PlaceMinimapButton(angle)
    minimapButton:ClearAllPoints()
    minimapButton:SetPoint("CENTER", Minimap, "CENTER", 80 * math.cos(angle), 80 * math.sin(angle))
end

local function MinimapDrag()
    local x, y = GetCursorPosition()
    local scale = Minimap:GetEffectiveScale()
    local cx, cy = Minimap:GetCenter()
    local angle = math.atan2(y / scale - cy, x / scale - cx)
    EmberforgeClasslessDB.minimapAngle = angle
    PlaceMinimapButton(angle)
end

minimapButton:SetScript("OnDragStart", function(self) self:SetScript("OnUpdate", MinimapDrag) end)
minimapButton:SetScript("OnDragStop", function(self) self:SetScript("OnUpdate", nil) end)
minimapButton:SetScript("OnClick", EmberforgeClassless_Toggle)
minimapButton:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine(EMBER .. "Classless|r")
    GameTooltip:AddLine("Click: open the Classless window (N)", 1, 1, 1)
    GameTooltip:AddLine("Drag: move this button", 0.7, 0.7, 0.7)
    GameTooltip:Show()
end)
minimapButton:SetScript("OnLeave", function() GameTooltip:Hide() end)

-- Key binding: N opens the window (Heroes have no talent window to open with it). Set once per
-- character, so a player who rebinds it keeps their choice.
BINDING_HEADER_EMBERFORGE = "Project Emberforge"
BINDING_NAME_EMBERFORGE_CLASSLESS_TOGGLE = "Classless window"

local function OnLogin()
    EmberforgeClasslessDB = EmberforgeClasslessDB or {}
    PlaceMinimapButton(EmberforgeClasslessDB.minimapAngle or math.rad(200))
    if not EmberforgeClasslessDB.boundKey and not GetBindingKey("EMBERFORGE_CLASSLESS_TOGGLE") then
        SetBinding("N", "EMBERFORGE_CLASSLESS_TOGGLE")
        SaveBindings(GetCurrentBindingSet())
    end
    EmberforgeClasslessDB.boundKey = true
end

-- Events and slash command ----------------------------------------------------------------------------

frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("PLAYER_LEVEL_UP")
frame:RegisterEvent("PLAYER_LOGIN")
frame:SetScript("OnEvent", function(self, event, prefix, message, channel, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            OnServerMessage(message)
        end
    elseif event == "PLAYER_LOGIN" then
        OnLogin()
    else
        Refresh()
    end
end)

SLASH_EMBERFORGECLASSLESS1 = "/classless"
SLASH_EMBERFORGECLASSLESS2 = "/cl"
SlashCmdList["EMBERFORGECLASSLESS"] = EmberforgeClassless_Toggle

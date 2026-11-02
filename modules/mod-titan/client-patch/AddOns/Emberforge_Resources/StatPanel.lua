-- Emberforge Resources: the character window's stats as Cataclysm's character pane, for every class.
--
-- A column on the right of the character window (Character tab only), one scrolling list of sections: General
-- (health, each power you use, the average item level of what you wear), Attributes, Melee, Ranged, Spell and
-- Defenses. Each section folds with its - / + button; the five Blizzard ones are filled by Blizzard's own stat code
-- (UpdatePaperdollStats), tooltips included. The arrow at the bottom of the model hides or shows the column. What is
-- folded and whether the column shows are kept per character. The 3.3.5 stat area under the model is hidden.

local WIDTH, ROW_HEIGHT, HEADER_HEIGHT, ROWS = 206, 15, 20, 6
local INNER = WIDTH - 44                -- the list's width, beside the scrollbar
local GOLD = { 1, 0.82, 0 }
local EDGE = { 0.86, 0.64, 0.32 }
-- The slots that count for the average (shirt and tabard do not); a two-hander counts for the off hand too.
local SLOTS = { 1, 2, 3, 15, 5, 9, 10, 6, 7, 8, 11, 12, 13, 14, 16, 17, 18 }
-- Powers shown under General: a Hero has every one of these; anyone else, their own.
local POWERS = { { 0, MANA or "Mana" }, { 1, RAGE or "Rage" }, { 3, ENERGY or "Energy" } }
local SECTIONS = {
    { key = "GENERAL", text = "General" },
    { key = "PLAYERSTAT_BASE_STATS", text = "Attributes" },
    { key = "PLAYERSTAT_MELEE_COMBAT", text = PLAYERSTAT_MELEE_COMBAT or "Melee" },
    { key = "PLAYERSTAT_RANGED_COMBAT", text = PLAYERSTAT_RANGED_COMBAT or "Ranged" },
    { key = "PLAYERSTAT_SPELL_COMBAT", text = PLAYERSTAT_SPELL_COMBAT or "Spell" },
    { key = "PLAYERSTAT_DEFENSES", text = PLAYERSTAT_DEFENSES or "Defenses" },
}

local function DB()
    EmberforgeResourcesDB = EmberforgeResourcesDB or {}
    EmberforgeResourcesDB.folded = EmberforgeResourcesDB.folded or {}
    return EmberforgeResourcesDB
end

-- The column: flush against the window's right edge, its full height.
local panel = CreateFrame("Frame", "EmberforgeStatPanel", PaperDollFrame)
panel:SetWidth(WIDTH)
panel:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", -37, -13)
panel:SetPoint("BOTTOMLEFT", CharacterFrame, "BOTTOMRIGHT", -37, 77)
panel:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 32, edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 },
})
panel:SetBackdropBorderColor(0.55, 0.45, 0.35, 1)

local scroll = CreateFrame("ScrollFrame", "EmberforgeStatPanelScroll", panel, "UIPanelScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", 10, -10)
scroll:SetPoint("BOTTOMRIGHT", -30, 10)
local list = CreateFrame("Frame", nil, scroll)
list:SetWidth(INNER)
list:SetHeight(1)
scroll:SetScrollChild(list)

-- A row: a label on the left, its value on the right, a stripe behind every other one.
local function NewRow(name)
    local row = CreateFrame("Frame", name, list)
    row:SetWidth(INNER)
    row:SetHeight(ROW_HEIGHT)
    row:EnableMouse(true)
    local label = row:CreateFontString(name and name .. "Label", "ARTWORK", "GameFontNormalSmall")
    label:SetPoint("LEFT", 6, 0)
    local value = row:CreateFontString(name and name .. "StatText", "ARTWORK", "GameFontHighlightSmall")
    value:SetPoint("RIGHT", -4, 0)
    row.stripe = row:CreateTexture(nil, "BACKGROUND")
    row.stripe:SetTexture(1, 1, 1, 0.06)
    row.stripe:SetAllPoints()
    row.label, row.value = label, value
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    return row
end

-- A section's title bar: dark, a thin gold line under it, the - / + button on its left.
local function NewHeader(section)
    local header = CreateFrame("Button", nil, list)
    header:SetWidth(INNER)
    header:SetHeight(HEADER_HEIGHT)
    local fill = header:CreateTexture(nil, "BACKGROUND")
    fill:SetTexture(1, 1, 1, 1)
    fill:SetGradientAlpha("VERTICAL", 0.08, 0.06, 0.05, 1, 0.2, 0.16, 0.12, 1)
    fill:SetAllPoints()
    local line = header:CreateTexture(nil, "BORDER")
    line:SetTexture(EDGE[1], EDGE[2], EDGE[3], 0.6)
    line:SetHeight(1)
    line:SetPoint("BOTTOMLEFT")
    line:SetPoint("BOTTOMRIGHT")
    header.toggle = header:CreateTexture(nil, "ARTWORK")
    header.toggle:SetWidth(14)
    header.toggle:SetHeight(14)
    header.toggle:SetPoint("LEFT", 3, 0)
    local text = header:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    text:SetPoint("LEFT", header.toggle, "RIGHT", 4, 0)
    text:SetTextColor(unpack(GOLD))
    text:SetText(section.text)
    local hover = header:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.6, 0.2, 0.1)
    hover:SetAllPoints()
    return header
end

local sections = {}
local Refresh
for index, info in ipairs(SECTIONS) do
    local section = { key = info.key, rows = {}, header = NewHeader(info) }
    local prefix = "EmberforgeStatPanel" .. index .. "Row"
    -- Blizzard's sections: six rows named for UpdatePaperdollStats. General: its own, as many as it needs.
    for row = 1, info.key == "GENERAL" and 5 or ROWS do
        section.rows[row] = NewRow(prefix .. row)
    end
    section.header:SetScript("OnClick", function()
        local folded = DB().folded
        folded[info.key] = not folded[info.key] or nil
        Refresh()
    end)
    sections[index] = section
end

local function AverageItemLevel()
    local total = 0
    for _, slot in ipairs(SLOTS) do
        local link = GetInventoryItemLink("player", slot)
        if link then
            local level, equipLoc = select(4, GetItemInfo(link)), select(9, GetItemInfo(link))
            total = total + (level or 0)
            -- Nothing in the off hand behind a two-hander: the two-hander counts twice.
            if level and slot == 16 and equipLoc == "INVTYPE_2HWEAPON" and not GetInventoryItemLink("player", 17) then
                total = total + level
            end
        end
    end
    return math.floor(total / #SLOTS + 0.5)
end

local function SetGeneral(rows)
    local lines = { { HEALTH or "Health", UnitHealthMax("player") } }
    local _, class = UnitClass("player")
    for _, power in ipairs(POWERS) do
        if class == "PALADIN" or UnitPowerType("player") == power[1] then
            lines[#lines + 1] = { power[2], UnitPowerMax("player", power[1]) }
        end
    end
    lines[#lines + 1] = { "Item Level", "|cffbf6bff" .. AverageItemLevel() .. "|r" }
    for index, row in ipairs(rows) do
        local line = lines[index]
        if line then
            row.label:SetText(line[1] .. ":")
            row.value:SetText(line[2])
            row:Show()
        else
            row:Hide()
        end
    end
end

-- Fills and stacks the sections: folded ones show their title only, rows a section hides are skipped.
Refresh = function()
    if not panel:IsVisible() then
        return
    end
    local folded = DB().folded
    local y = 0
    for index, section in ipairs(sections) do
        local header = section.header
        header:ClearAllPoints()
        header:SetPoint("TOPLEFT", list, "TOPLEFT", 0, -y)
        y = y + HEADER_HEIGHT + 2
        local open = not folded[section.key]
        header.toggle:SetTexture("Interface\\Buttons\\UI-" .. (open and "Minus" or "Plus") .. "Button-Up")
        if open then
            if section.key == "GENERAL" then
                SetGeneral(section.rows)
            elseif UpdatePaperdollStats then
                UpdatePaperdollStats("EmberforgeStatPanel" .. index .. "Row", section.key)
            end
        end
        local shown = 0
        for _, row in ipairs(section.rows) do
            if open and row:IsShown() then
                row:ClearAllPoints()
                row:SetPoint("TOPLEFT", list, "TOPLEFT", 0, -y)
                row.stripe:SetAlpha(shown % 2 == 0 and 1 or 0)
                y = y + ROW_HEIGHT
                shown = shown + 1
            else
                row:Hide()
            end
        end
        y = y + 6
    end
    list:SetHeight(math.max(1, y))
end

-- The arrow at the bottom right of the model: hides or shows the column.
local toggle = CreateFrame("Button", "EmberforgeStatPanelToggle", PaperDollFrame)
toggle:SetWidth(26)
toggle:SetHeight(26)
toggle:SetPoint("BOTTOMRIGHT", CharacterFrame, "BOTTOMRIGHT", -42, 84)
local function SetArrow()
    local shown = not DB().hidden
    local page = shown and "Next" or "Prev"
    toggle:SetNormalTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. page .. "Page-Up")
    toggle:SetPushedTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. page .. "Page-Down")
    toggle:SetHighlightTexture("Interface\\Buttons\\UI-Common-MouseHilight", "ADD")
    if shown then
        panel:Show()
    else
        panel:Hide()
    end
end
toggle:SetScript("OnClick", function()
    DB().hidden = not DB().hidden or nil
    SetArrow()
end)
toggle:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetText(DB().hidden and "Show stats" or "Hide stats")
    GameTooltip:Show()
end)
toggle:SetScript("OnLeave", function() GameTooltip:Hide() end)

-- The 3.3.5 stat area under the model (its dropdowns and two columns) gives way to the column.
local function HideOldStats()
    for _, name in ipairs({ "PlayerStatFrameLeftDropDown", "PlayerStatFrameRightDropDown", "PlayerStatLeftTop",
        "PlayerStatLeftMiddle", "PlayerStatLeftBottom", "PlayerStatRightTop", "PlayerStatRightMiddle",
        "PlayerStatRightBottom" }) do
        if _G[name] then
            _G[name]:Hide()
        end
    end
    for _, side in ipairs({ "Left", "Right" }) do
        for row = 1, ROWS do
            local frame = _G["PlayerStatFrame" .. side .. row]
            if frame then
                frame:Hide()
            end
        end
    end
end

if PaperDollFrame_UpdateStats then
    hooksecurefunc("PaperDollFrame_UpdateStats", function()
        HideOldStats()
        Refresh()
    end)
end
PaperDollFrame:HookScript("OnShow", function()
    HideOldStats()
    SetArrow()
end)
panel:SetScript("OnShow", Refresh)
for _, event in ipairs({ "PLAYER_EQUIPMENT_CHANGED", "UNIT_INVENTORY_CHANGED", "UNIT_STATS", "UNIT_AURA",
    "UNIT_MAXHEALTH", "UNIT_MAXPOWER", "PLAYER_LOGIN" }) do
    panel:RegisterEvent(event)
end
panel:SetScript("OnEvent", function(_, event, unit)
    if event == "PLAYER_LOGIN" then
        SetArrow()
    elseif not unit or unit == "player" then
        Refresh()
    end
end)

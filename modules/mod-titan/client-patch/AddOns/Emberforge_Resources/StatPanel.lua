-- Emberforge Resources: the character window's stats, Cataclysm style, for every class.
--
-- A column on the right of the character window (Character tab only), inside the one window CharacterSkin.lua draws:
-- the average item level of what you wear, a standalone number on top; then two sections, each a filter: click its
-- title to choose Attributes, Melee, Ranged, Spell or Defenses (Blizzard's own stat lines and tooltips,
-- UpdatePaperdollStats). Fixed in size, it fits the column: nothing scrolls or is cut. The choices, and whether the
-- column shows (the arrow at the bottom of the model), are kept per character. The 3.3.5 stat area under the model
-- is hidden.

local CATEGORIES = {
    { key = "PLAYERSTAT_BASE_STATS", text = "Attributes" },
    { key = "PLAYERSTAT_MELEE_COMBAT", text = PLAYERSTAT_MELEE_COMBAT or "Melee" },
    { key = "PLAYERSTAT_RANGED_COMBAT", text = PLAYERSTAT_RANGED_COMBAT or "Ranged" },
    { key = "PLAYERSTAT_SPELL_COMBAT", text = PLAYERSTAT_SPELL_COMBAT or "Spell" },
    { key = "PLAYERSTAT_DEFENSES", text = PLAYERSTAT_DEFENSES or "Defenses" },
}
local DEFAULTS = { "PLAYERSTAT_BASE_STATS", "PLAYERSTAT_MELEE_COMBAT" }
local WIDTH, ROWS, ROW_HEIGHT, HEADER_HEIGHT = 206, 6, 20, 24
local INNER = WIDTH - 20
local GOLD = { 1, 0.82, 0 }
local EDGE = { 0.86, 0.64, 0.32 }
-- The slots that count for the average (shirt and tabard do not); a two-hander counts for the off hand too.
local SLOTS = { 1, 2, 3, 15, 5, 9, 10, 6, 7, 8, 11, 12, 13, 14, 16, 17, 18 }

local function DB()
    EmberforgeResourcesDB = EmberforgeResourcesDB or {}
    EmberforgeResourcesDB.stats = EmberforgeResourcesDB.stats or {}
    return EmberforgeResourcesDB
end

local panel = CreateFrame("Frame", "EmberforgeStatPanel", PaperDollFrame)
panel:SetWidth(WIDTH)
panel:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", -40, -44)
panel:SetPoint("BOTTOMLEFT", CharacterFrame, "BOTTOMRIGHT", -40, 72)
local shade = panel:CreateTexture(nil, "BACKGROUND")
shade:SetTexture(0, 0, 0, 0.25)
shade:SetAllPoints()

-- A title bar: a warm gradient between thin gold lines, its text centered; a filter's has an arrow.
local function Header(text, clickable)
    local header = CreateFrame(clickable and "Button" or "Frame", nil, panel)
    header:SetWidth(INNER)
    header:SetHeight(HEADER_HEIGHT)
    local fill = header:CreateTexture(nil, "BACKGROUND")
    fill:SetTexture(1, 1, 1, 1)
    fill:SetGradientAlpha("VERTICAL", 0.08, 0.06, 0.05, 1, 0.24, 0.18, 0.12, 1)
    fill:SetAllPoints()
    for _, side in ipairs({ "TOP", "BOTTOM" }) do
        local line = header:CreateTexture(nil, "BORDER")
        line:SetTexture(EDGE[1], EDGE[2], EDGE[3], 0.7)
        line:SetHeight(1)
        line:SetPoint(side .. "LEFT")
        line:SetPoint(side .. "RIGHT")
    end
    header.text = header:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    header.text:SetPoint("CENTER")
    header.text:SetTextColor(unpack(GOLD))
    header.text:SetText(text)
    if clickable then
        local arrow = header:CreateTexture(nil, "ARTWORK")
        arrow:SetTexture("Interface\\ChatFrame\\UI-ChatIcon-ScrollDown-Up")
        arrow:SetWidth(18)
        arrow:SetHeight(18)
        arrow:SetPoint("RIGHT", -3, 0)
        local hover = header:CreateTexture(nil, "HIGHLIGHT")
        hover:SetTexture(1, 0.6, 0.2, 0.12)
        hover:SetAllPoints()
    end
    return header
end

-- Item level: its title, then the number, standalone, big.
local levelHeader = Header("Item Level")
levelHeader:SetPoint("TOP", 0, -6)
local itemLevel = panel:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
itemLevel:SetPoint("TOP", levelHeader, "BOTTOM", 0, -8)
itemLevel:SetTextColor(0.75, 0.42, 1)

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

-- A row: Blizzard's stat frame names (prefix .. index, its Label and StatText), a stripe behind every other one.
local function Row(prefix, index, anchor)
    local row = CreateFrame("Frame", prefix .. index, panel)
    row:SetWidth(INNER)
    row:SetHeight(ROW_HEIGHT)
    row:SetPoint("TOP", anchor, "BOTTOM", 0, index == 1 and -2 or 0)
    row:EnableMouse(true)
    local label = row:CreateFontString(prefix .. index .. "Label", "ARTWORK", "GameFontNormalSmall")
    label:SetPoint("LEFT", 6, 0)
    local value = row:CreateFontString(prefix .. index .. "StatText", "ARTWORK", "GameFontHighlightSmall")
    value:SetPoint("RIGHT", -6, 0)
    if index % 2 == 1 then
        local stripe = row:CreateTexture(nil, "BACKGROUND")
        stripe:SetTexture(1, 1, 1, 0.06)
        stripe:SetAllPoints()
    end
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    return row
end

local menu = CreateFrame("Frame", "EmberforgeStatPanelMenu", UIParent, "UIDropDownMenuTemplate")
local sections = {}

local function Choice(index)
    return DB().stats[index] or DEFAULTS[index]
end

local function CategoryText(key)
    for _, category in ipairs(CATEGORIES) do
        if category.key == key then
            return category.text
        end
    end
    return key
end

local function UpdateSection(index)
    local section = sections[index]
    local key = Choice(index)
    section.header.text:SetText(CategoryText(key))
    if UpdatePaperdollStats then
        UpdatePaperdollStats(section.prefix, key)
    end
end

local function Refresh()
    if not panel:IsVisible() then
        return
    end
    itemLevel:SetText(AverageItemLevel())
    for index in ipairs(sections) do
        UpdateSection(index)
    end
end

-- The two filters, spaced to fill the column.
local anchor = itemLevel
for index = 1, 2 do
    local prefix = "EmberforgeStatPanel" .. index .. "Row"
    local header = Header("", true)
    header:SetPoint("TOP", anchor, "BOTTOM", 0, index == 1 and -14 or -22)
    local section = { header = header, prefix = prefix }
    anchor = header
    for row = 1, ROWS do
        anchor = Row(prefix, row, anchor)
    end
    header:SetScript("OnClick", function(self)
        local items = {}
        for _, category in ipairs(CATEGORIES) do
            items[#items + 1] = {
                text = category.text,
                checked = Choice(index) == category.key,
                func = function()
                    DB().stats[index] = category.key
                    UpdateSection(index)
                end,
            }
        end
        EasyMenu(items, menu, self, 0, 0, "MENU")
    end)
    sections[index] = section
end

-- The arrow at the bottom right of the model: hides or shows the column.
local toggle = CreateFrame("Button", "EmberforgeStatPanelToggle", PaperDollFrame)
toggle:SetWidth(26)
toggle:SetHeight(26)
toggle:SetPoint("BOTTOMRIGHT", CharacterFrame, "BOTTOMRIGHT", -44, 76)
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

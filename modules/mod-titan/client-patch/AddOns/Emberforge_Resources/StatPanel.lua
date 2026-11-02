-- Emberforge Resources: the character window's stats, Cataclysm style, for every class.
--
-- A panel on the right of the character window (Character tab only): the average item level of what you wear on
-- top, then two stat sections. Click a section's title to pick what it shows: Base Stats, Melee, Ranged, Spell or
-- Defenses, the same five as the 3.3.5 dropdowns, filled by Blizzard's own stat code (UpdatePaperdollStats), tooltips
-- included. The choices are kept per character. The old stat area under the model is hidden.

local CATEGORIES = {
    { key = "PLAYERSTAT_BASE_STATS", text = PLAYERSTAT_BASE_STATS or "Base Stats" },
    { key = "PLAYERSTAT_MELEE_COMBAT", text = PLAYERSTAT_MELEE_COMBAT or "Melee" },
    { key = "PLAYERSTAT_RANGED_COMBAT", text = PLAYERSTAT_RANGED_COMBAT or "Ranged" },
    { key = "PLAYERSTAT_SPELL_COMBAT", text = PLAYERSTAT_SPELL_COMBAT or "Spell" },
    { key = "PLAYERSTAT_DEFENSES", text = PLAYERSTAT_DEFENSES or "Defenses" },
}
local DEFAULTS = { "PLAYERSTAT_BASE_STATS", "PLAYERSTAT_MELEE_COMBAT" }
local ROWS = 6
local WIDTH, ROW_HEIGHT = 212, 18
-- The slots that count for the average (shirt and tabard do not); a two-hander counts for the off hand too.
local SLOTS = { 1, 2, 3, 15, 5, 9, 10, 6, 7, 8, 11, 12, 13, 14, 16, 17, 18 }

local GOLD = { 1, 0.82, 0 }
local EDGE = { 0.86, 0.64, 0.32 }
local TOP, SECTION_GAP, HEADER_HEIGHT = -16, 22, 24

local panel = CreateFrame("Frame", "EmberforgeStatPanel", PaperDollFrame)
panel:SetWidth(WIDTH)
panel:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", -36, -13)
panel:SetPoint("BOTTOMLEFT", CharacterFrame, "BOTTOMRIGHT", -36, 76)
panel:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 24,
    insets = { left = 6, right = 6, top = 6, bottom = 6 },
})

-- A title bar: a warm gradient under a thin gold edge, its text centered (Cataclysm's look).
local function Header(parent, text, clickable)
    local header = CreateFrame(clickable and "Button" or "Frame", nil, parent)
    header:SetWidth(WIDTH - 22)
    header:SetHeight(HEADER_HEIGHT)
    header:SetBackdrop({
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    header:SetBackdropBorderColor(EDGE[1], EDGE[2], EDGE[3], 1)
    local fill = header:CreateTexture(nil, "BACKGROUND")
    fill:SetTexture(1, 1, 1, 1)
    fill:SetGradientAlpha("VERTICAL", 0.1, 0.06, 0.03, 1, 0.33, 0.21, 0.08, 1)
    fill:SetPoint("TOPLEFT", 3, -3)
    fill:SetPoint("BOTTOMRIGHT", -3, 3)
    header.text = header:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    header.text:SetPoint("CENTER")
    header.text:SetTextColor(unpack(GOLD))
    header.text:SetText(text)
    if clickable then
        local arrow = header:CreateTexture(nil, "ARTWORK")
        arrow:SetTexture("Interface\\ChatFrame\\UI-ChatIcon-ScrollDown-Up")
        arrow:SetWidth(18)
        arrow:SetHeight(18)
        arrow:SetPoint("RIGHT", -4, 0)
        local hover = header:CreateTexture(nil, "HIGHLIGHT")
        hover:SetTexture(1, 0.6, 0.2, 0.12)
        hover:SetPoint("TOPLEFT", 3, -3)
        hover:SetPoint("BOTTOMRIGHT", -3, 3)
    end
    return header
end

-- Item level: its title, then the number in a dark inset bar.
local levelHeader = Header(panel, "Item Level")
levelHeader:SetPoint("TOP", 0, TOP)
local levelBar = CreateFrame("Frame", nil, panel)
levelBar:SetWidth(WIDTH - 40)
levelBar:SetHeight(34)
levelBar:SetPoint("TOP", levelHeader, "BOTTOM", 0, -4)
local levelFill = levelBar:CreateTexture(nil, "BACKGROUND")
levelFill:SetTexture(1, 1, 1, 1)
levelFill:SetGradientAlpha("HORIZONTAL", 0.05, 0.03, 0.02, 0.2, 0.05, 0.03, 0.02, 0.2)
levelFill:SetAllPoints()
local levelGlow = levelBar:CreateTexture(nil, "BORDER")
levelGlow:SetTexture(0.35, 0.16, 0.45, 0.35)
levelGlow:SetPoint("TOPLEFT", 30, -4)
levelGlow:SetPoint("BOTTOMRIGHT", -30, 4)
local itemLevel = levelBar:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
itemLevel:SetPoint("CENTER", 0, 0)
itemLevel:SetTextColor(0.75, 0.42, 1)

local function AverageItemLevel()
    local total = 0
    for _, slot in ipairs(SLOTS) do
        local link = GetInventoryItemLink("player", slot)
        local level, equipLoc
        if link then
            level, equipLoc = select(4, GetItemInfo(link)), select(9, GetItemInfo(link))
        end
        if level then
            total = total + level
        end
        -- Nothing in the off hand behind a two-hander: the two-hander counts twice.
        if level and slot == 16 and equipLoc == "INVTYPE_2HWEAPON" and not GetInventoryItemLink("player", 17) then
            total = total + level
        end
    end
    return math.floor(total / #SLOTS + 0.5)
end

-- The two stat sections: a title to click, then six striped rows of Blizzard's stat frames.
local menu = CreateFrame("Frame", "EmberforgeStatPanelMenu", UIParent, "UIDropDownMenuTemplate")
local sections = {}

local function CategoryText(key)
    for _, category in ipairs(CATEGORIES) do
        if category.key == key then
            return category.text
        end
    end
    return key
end

local function StatRow(prefix, index, anchor)
    local ok, row = pcall(CreateFrame, "Frame", prefix .. index, panel, "StatFrameTemplate")
    if not ok then
        row = CreateFrame("Frame", prefix .. index, panel)
        local label = row:CreateFontString(prefix .. index .. "Label", "ARTWORK", "GameFontNormalSmall")
        label:SetPoint("LEFT")
        local stat = row:CreateFontString(prefix .. index .. "StatText", "ARTWORK", "GameFontHighlightSmall")
        stat:SetPoint("RIGHT")
        row:EnableMouse(true)
    end
    row:SetWidth(WIDTH - 40)
    row:SetHeight(ROW_HEIGHT)
    row:SetPoint("TOP", anchor, "BOTTOM", 0, index == 1 and -4 or 0)
    -- Every other row a faint stripe, as Cataclysm's; a little wider than the text.
    if index % 2 == 1 then
        local stripe = row:CreateTexture(nil, "BACKGROUND")
        stripe:SetTexture(1, 1, 1, 0.06)
        stripe:SetPoint("TOPLEFT", -4, 0)
        stripe:SetPoint("BOTTOMRIGHT", 4, 0)
    end
    return row
end

local function Choice(index)
    local db = EmberforgeResourcesDB
    return db and db.stats and db.stats[index] or DEFAULTS[index]
end

local function UpdateSection(index)
    local section = sections[index]
    local key = Choice(index)
    section.header.text:SetText(CategoryText(key))
    if UpdatePaperdollStats then
        UpdatePaperdollStats(section.prefix, key)
    end
end

local function UpdatePanel()
    if not panel:IsVisible() then
        return
    end
    itemLevel:SetText(AverageItemLevel())
    for index in ipairs(sections) do
        UpdateSection(index)
    end
end

local anchor = levelBar
for index = 1, 2 do
    local prefix = "EmberforgeStatPanel" .. (index == 1 and "Top" or "Bottom")
    local header = Header(panel, "", true)
    header:SetPoint("TOP", anchor, "BOTTOM", 0, -SECTION_GAP)
    local section = { header = header, prefix = prefix }
    anchor = header
    for row = 1, ROWS do
        anchor = StatRow(prefix, row, anchor)
    end
    header:SetScript("OnClick", function(self)
        local items = {}
        for _, category in ipairs(CATEGORIES) do
            items[#items + 1] = {
                text = category.text,
                checked = Choice(index) == category.key,
                func = function()
                    EmberforgeResourcesDB = EmberforgeResourcesDB or {}
                    EmberforgeResourcesDB.stats = EmberforgeResourcesDB.stats or {}
                    EmberforgeResourcesDB.stats[index] = category.key
                    UpdateSection(index)
                end,
            }
        end
        EasyMenu(items, menu, self, 0, 0, "MENU")
    end)
    sections[index] = section
end

-- The 3.3.5 stat area under the model (its dropdowns and two columns) gives way to the panel.
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
        UpdatePanel()
    end)
end
panel:SetScript("OnShow", function()
    HideOldStats()
    UpdatePanel()
end)
panel:RegisterEvent("PLAYER_EQUIPMENT_CHANGED")
panel:RegisterEvent("UNIT_INVENTORY_CHANGED")
panel:RegisterEvent("UNIT_STATS")
panel:RegisterEvent("UNIT_AURA")
panel:SetScript("OnEvent", function(_, event, unit)
    if not unit or unit == "player" then
        UpdatePanel()
    end
end)

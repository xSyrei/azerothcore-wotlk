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
local WIDTH, ROW_HEIGHT = 196, 15
-- The slots that count for the average (shirt and tabard do not); a two-hander counts for the off hand too.
local SLOTS = { 1, 2, 3, 15, 5, 9, 10, 6, 7, 8, 11, 12, 13, 14, 16, 17, 18 }

local GOLD = { 1, 0.82, 0 }

local panel = CreateFrame("Frame", "EmberforgeStatPanel", PaperDollFrame)
panel:SetWidth(WIDTH)
panel:SetHeight(392)
panel:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", -34, -14)
panel:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 24,
    insets = { left = 6, right = 6, top = 6, bottom = 6 },
})

-- A title bar: dark, gold-edged, its text centered.
local function Header(y, text)
    local header = CreateFrame("Button", nil, panel)
    header:SetWidth(WIDTH - 24)
    header:SetHeight(22)
    header:SetPoint("TOP", 0, y)
    header:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    header:SetBackdropColor(0.12, 0.09, 0.06, 1)
    header:SetBackdropBorderColor(0.86, 0.64, 0.32, 1)
    header.text = header:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    header.text:SetPoint("CENTER")
    header.text:SetTextColor(unpack(GOLD))
    header.text:SetText(text)
    return header
end

-- Item level.
Header(-14, "Item Level")
local itemLevel = panel:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
itemLevel:SetPoint("TOP", 0, -44)
itemLevel:SetTextColor(0.64, 0.21, 0.93)

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

-- The two stat sections: a title to click, then six rows of Blizzard's stat frames.
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
        row:SetHeight(13)
        local label = row:CreateFontString(prefix .. index .. "Label", "ARTWORK", "GameFontNormalSmall")
        label:SetPoint("LEFT")
        local stat = row:CreateFontString(prefix .. index .. "StatText", "ARTWORK", "GameFontHighlightSmall")
        stat:SetPoint("RIGHT")
        row:EnableMouse(true)
    end
    row:SetWidth(WIDTH - 36)
    row:SetPoint("TOP", anchor, "BOTTOM", 0, index == 1 and -6 or -2)
    return row
end

local function Choice(index)
    local db = EmberforgeResourcesDB
    return db and db.stats and db.stats[index] or DEFAULTS[index]
end

local function UpdateSection(index)
    local section = sections[index]
    local key = Choice(index)
    section.header.text:SetText(CategoryText(key) .. "  |cff999999v|r")
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

for index, y in ipairs({ -76, -226 }) do
    local prefix = "EmberforgeStatPanel" .. (index == 1 and "Top" or "Bottom")
    local header = Header(y, "")
    local section = { header = header, prefix = prefix }
    local anchor = header
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
    header:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
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

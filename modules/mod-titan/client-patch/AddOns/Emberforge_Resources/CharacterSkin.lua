-- Emberforge Resources: the Character tab as one window, Cataclysm style, for every class.
--
-- The Character tab's own art (two halves of a frame, and the stats column beside it) gives way to one frame around
-- both: a dark background under one border, a title bar across the top with the name in it and the close button at
-- its right end, a dark inset behind the model, a gold ring around the portrait, and a square frame behind every
-- gear slot, edged in the item's quality color. Only the Character tab: Reputation, Skills and the rest keep their
-- own look (what is moved here goes back when the tab closes). Hiding the stats column (StatPanel.lua) narrows the
-- window back.

local ART = "Interface\\AddOns\\Emberforge_Resources\\Art\\"
local EDGE = { 0.55, 0.45, 0.35 }
local SLOTS = { "Head", "Neck", "Shoulder", "Back", "Chest", "Shirt", "Tabard", "Wrist", "Hands", "Waist", "Legs",
    "Feet", "Finger0", "Finger1", "Trinket0", "Trinket1", "MainHand", "SecondaryHand", "Ranged", "Ammo" }

local skin = CreateFrame("Frame", "EmberforgeCharacterSkin", PaperDollFrame)
skin:SetFrameLevel(math.max(0, PaperDollFrame:GetFrameLevel() - 1))
skin:SetPoint("TOPLEFT", CharacterFrame, "TOPLEFT", 12, -12)
skin:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 24,
    insets = { left = 6, right = 6, top = 6, bottom = 6 },
})

-- Its right edge: the stats column's when it shows, else the old frame's.
local function Fit()
    skin:ClearAllPoints()
    skin:SetPoint("TOPLEFT", CharacterFrame, "TOPLEFT", 12, -12)
    if EmberforgeStatPanel and EmberforgeStatPanel:IsShown() then
        skin:SetPoint("BOTTOMRIGHT", EmberforgeStatPanel, "BOTTOMRIGHT", 8, -8)
    else
        skin:SetPoint("BOTTOMRIGHT", CharacterFrame, "BOTTOMRIGHT", -32, 76)
    end
end

-- The title bar, right of the portrait, across the whole window.
local title = CreateFrame("Frame", nil, skin)
title:SetHeight(24)
title:SetPoint("TOPLEFT", 58, -6)
title:SetPoint("TOPRIGHT", -30, -6)
title:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 12,
    insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
title:SetBackdropColor(0.08, 0.07, 0.06, 1)
title:SetBackdropBorderColor(EDGE[1], EDGE[2], EDGE[3], 1)

-- The name and level, in and under the title bar: the window's own would sit under the new frame, so they are
-- hidden while the tab shows and these show the same text.
local name = title:CreateFontString(nil, "ARTWORK", "GameFontNormal")
name:SetPoint("CENTER")
name:SetTextColor(1, 1, 1)
local level = skin:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
level:SetPoint("TOP", title, "BOTTOM", 0, -4)

-- The portrait, drawn again over the new frame, in a gold ring.
local portrait = skin:CreateTexture(nil, "ARTWORK")
portrait:SetWidth(60)
portrait:SetHeight(60)
local ring = skin:CreateTexture(nil, "OVERLAY")
ring:SetTexture(ART .. "Ring")
ring:SetWidth(76)
ring:SetHeight(76)

-- The model's inset.
local inset = CreateFrame("Frame", nil, skin)
inset:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 12,
    insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
inset:SetBackdropColor(0, 0, 0, 0.55)
inset:SetBackdropBorderColor(0.4, 0.34, 0.27, 1)

-- A square frame behind each gear slot, edged in its item's quality color.
local slotFrames = {}
for _, name in ipairs(SLOTS) do
    local button = _G["Character" .. name .. "Slot"]
    if button then
        local frame = CreateFrame("Frame", nil, skin)
        frame:SetPoint("TOPLEFT", button, "TOPLEFT", -3, 3)
        frame:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", 3, -3)
        frame:SetBackdrop({
            bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
            edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
            tile = true, tileSize = 16, edgeSize = 10,
            insets = { left = 2, right = 2, top = 2, bottom = 2 },
        })
        frame:SetBackdropColor(0, 0, 0, 0.8)
        frame.slot = button:GetID()
        slotFrames[#slotFrames + 1] = frame
    end
end

local function ColorSlots()
    for _, frame in ipairs(slotFrames) do
        local quality = frame.slot and GetInventoryItemQuality("player", frame.slot)
        local color = quality and quality > 1 and ITEM_QUALITY_COLORS[quality]
        if color then
            frame:SetBackdropBorderColor(color.r, color.g, color.b, 1)
        else
            frame:SetBackdropBorderColor(EDGE[1], EDGE[2], EDGE[3], 1)
        end
    end
end

-- The Character tab's own frame art: hidden while the tab shows (the portrait, slots and text are not art).
local hiddenArt = {}
local function HideArt()
    for _, region in ipairs({ PaperDollFrame:GetRegions() }) do
        if region:GetObjectType() == "Texture" and region:IsShown() then
            local file = region:GetTexture()
            if type(file) == "string" and file:lower():find("paperdollinfoframe", 1, true) then
                region:Hide()
                hiddenArt[region] = true
            end
        end
    end
end

local SetTexts

-- Moved while the tab shows, back where they were when it closes.
local moved = {}
local function Move(region, ...)
    if not region then
        return
    end
    if not moved[region] then
        local points = {}
        for i = 1, region:GetNumPoints() do
            points[i] = { region:GetPoint(i) }
        end
        moved[region] = points
    end
    region:ClearAllPoints()
    region:SetPoint(...)
end

SetTexts = function()
    name:SetText(CharacterNameText and CharacterNameText:GetText() or UnitName("player"))
    level:SetText(CharacterLevelText and CharacterLevelText:GetText() or "")
    local r, g, b = 1, 0.82, 0
    if CharacterLevelText then
        r, g, b = CharacterLevelText:GetTextColor()
    end
    level:SetTextColor(r, g, b)
end

local function Restore()
    for region, points in pairs(moved) do
        region:ClearAllPoints()
        for _, point in ipairs(points) do
            region:SetPoint(unpack(point))
        end
    end
    wipe(moved)
    for region in pairs(hiddenArt) do
        region:Show()
    end
    wipe(hiddenArt)
    for _, text in ipairs({ CharacterNameText, CharacterLevelText }) do
        if text then
            text:SetAlpha(1)
        end
    end
end

local function Apply()
    HideArt()
    Fit()
    Move(CharacterFrameCloseButton, "TOPRIGHT", skin, "TOPRIGHT", -2, -2)
    CharacterFrameCloseButton:SetFrameLevel(skin:GetFrameLevel() + 10)
    SetTexts()
    for _, text in ipairs({ CharacterNameText, CharacterLevelText }) do
        text:SetAlpha(0)
    end
    if CharacterFramePortrait then
        portrait:ClearAllPoints()
        portrait:SetPoint("CENTER", CharacterFramePortrait, "CENTER", 0, 0)
        SetPortraitTexture(portrait, "player")
        ring:ClearAllPoints()
        ring:SetPoint("CENTER", CharacterFramePortrait, "CENTER", 0, 0)
    end
    -- The inset: from the left slot column to the right one, the title bar to the weapons.
    if CharacterHeadSlot and CharacterTrinket1Slot then
        inset:ClearAllPoints()
        inset:SetPoint("TOPLEFT", CharacterHeadSlot, "TOPRIGHT", 6, 4)
        inset:SetPoint("BOTTOMRIGHT", CharacterTrinket1Slot, "BOTTOMLEFT", -6, -4)
    end
    ColorSlots()
end

PaperDollFrame:HookScript("OnShow", Apply)
PaperDollFrame:HookScript("OnHide", Restore)
if EmberforgeStatPanel then
    EmberforgeStatPanel:HookScript("OnShow", Fit)
    EmberforgeStatPanel:HookScript("OnHide", Fit)
end
if PaperDollFrame_SetLevel then
    hooksecurefunc("PaperDollFrame_SetLevel", SetTexts)
end
for _, event in ipairs({ "PLAYER_EQUIPMENT_CHANGED", "UNIT_INVENTORY_CHANGED", "UNIT_PORTRAIT_UPDATE",
    "PLAYER_LEVEL_UP" }) do
    skin:RegisterEvent(event)
end
skin:SetScript("OnEvent", function(_, event, unit)
    if event == "UNIT_PORTRAIT_UPDATE" then
        if unit == "player" then
            SetPortraitTexture(portrait, "player")
        end
    elseif not unit or unit == "player" then
        ColorSlots()
        SetTexts()
    end
end)

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
skin:SetPoint("TOPLEFT", CharacterFrame, "TOPLEFT", 12, -12)
-- Frames here sit over the slots and the model: they draw edges only. What fills (the background, the model's
-- inset) is drawn on the Character tab's own background layer, where its art was, under everything.
skin:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 16 })
skin:SetBackdropBorderColor(EDGE[1], EDGE[2], EDGE[3], 1)
local background = PaperDollFrame:CreateTexture(nil, "BACKGROUND")
background:SetTexture(1, 1, 1, 1)
background:SetGradientAlpha("VERTICAL", 0.04, 0.035, 0.03, 0.96, 0.1, 0.085, 0.07, 0.96)
background:SetPoint("TOPLEFT", skin, "TOPLEFT", 4, -4)
background:SetPoint("BOTTOMRIGHT", skin, "BOTTOMRIGHT", -4, 4)

-- Its right edge: the stats column's when it shows, else the old frame's.
local function Fit()
    skin:ClearAllPoints()
    skin:SetPoint("TOPLEFT", CharacterFrame, "TOPLEFT", 12, -12)
    if EmberforgeStatPanel and EmberforgeStatPanel:IsShown() then
        skin:SetPoint("BOTTOMRIGHT", EmberforgeStatPanel, "BOTTOMRIGHT", 12, -8)
    else
        skin:SetPoint("BOTTOMRIGHT", CharacterFrame, "BOTTOMRIGHT", -32, 64)
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

-- The portrait, drawn again over the new frame, in a gold ring.
local portrait = skin:CreateTexture(nil, "ARTWORK")
portrait:SetWidth(60)
portrait:SetHeight(60)
local ring = skin:CreateTexture(nil, "OVERLAY")
ring:SetTexture(ART .. "Ring")
ring:SetWidth(76)
ring:SetHeight(76)

-- The model's inset: its edge a frame, its fill on the background layer.
local inset = CreateFrame("Frame", nil, skin)
inset:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 12 })
inset:SetBackdropBorderColor(0.4, 0.34, 0.27, 1)
local insetFill = PaperDollFrame:CreateTexture(nil, "BORDER")
insetFill:SetTexture(0, 0, 0, 0.6)
insetFill:SetPoint("TOPLEFT", inset, "TOPLEFT", 3, -3)
insetFill:SetPoint("BOTTOMRIGHT", inset, "BOTTOMRIGHT", -3, 3)

-- A square frame behind each gear slot, edged in its item's quality color.
local slotFrames = {}
for _, name in ipairs(SLOTS) do
    local button = _G["Character" .. name .. "Slot"]
    if button then
        local frame = CreateFrame("Frame", nil, skin)
        frame:SetPoint("TOPLEFT", button, "TOPLEFT", -3, 3)
        frame:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", 3, -3)
        frame:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10 })
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
    name:SetText(UnitPVPName("player") or UnitName("player"))
    local format = PLAYER_LEVEL or "Level %s %s %s"
    level:SetText(format:format(UnitLevel("player"), UnitRace("player") or "", UnitClass("player") or ""))
    level:SetTextColor(1, 0.82, 0)
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
    for _, region in ipairs({ CharacterNameText, CharacterLevelText, CharacterFramePortrait }) do
        if region then
            region:SetAlpha(1)
        end
    end
end

-- The resistance boxes inside the model: Resistances is a section of the stats column instead.
local function HideResistances()
    for school = 1, 5 do
        local frame = _G["MagicResFrame" .. school]
        if frame and frame:IsShown() then
            frame:Hide()
            hiddenArt[frame] = true
        end
    end
end

local function Apply()
    HideArt()
    HideResistances()
    Fit()
    Move(CharacterFrameCloseButton, "TOPRIGHT", skin, "TOPRIGHT", -5, -5)
    CharacterFrameCloseButton:SetFrameLevel(skin:GetFrameLevel() + 10)
    SetTexts()
    for _, text in ipairs({ CharacterNameText, CharacterLevelText }) do
        text:SetAlpha(0)
    end
    -- Exactly over the window's own portrait, which hides while the tab shows.
    if CharacterFramePortrait then
        CharacterFramePortrait:SetAlpha(0)
        portrait:SetWidth(CharacterFramePortrait:GetWidth())
        portrait:SetHeight(CharacterFramePortrait:GetHeight())
        ring:SetWidth(CharacterFramePortrait:GetWidth() + 14)
        ring:SetHeight(CharacterFramePortrait:GetHeight() + 14)
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
    -- The level line over the model, clear of the stats column.
    level:ClearAllPoints()
    level:SetPoint("BOTTOM", inset, "TOP", 0, 4)
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

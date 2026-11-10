-- Emberforge Lighting: tells the Emberforge ReShade effects when the world is on screen. Only then, two 4x4 blocks of
-- near-black, (2, 1, 3) and (1, 3, 2) out of 255, sit in the screen's top-left corner, and the effects run only while
-- they find them: never on loading, login or character screens (no addons run there), the world map (the depth buffer
-- still holds the world behind it: occlusion "burned" the map) or a video. See reshade/Shaders/Emberforge.fxh.
local BLOCK = 4 -- screen pixels

-- On the world's frame, not UIParent: hiding the interface (Alt+Z) for a screenshot keeps the effects.
local marker = CreateFrame("Frame", "EmberforgeLightingMarker", WorldFrame)
marker:SetFrameStrata("TOOLTIP")
marker:SetFrameLevel(100)
marker:SetPoint("TOPLEFT", WorldFrame, "TOPLEFT", 0, 0)

local first = marker:CreateTexture(nil, "OVERLAY")
first:SetTexture(2 / 255, 1 / 255, 3 / 255, 1)
local second = marker:CreateTexture(nil, "OVERLAY")
second:SetTexture(1 / 255, 3 / 255, 2 / 255, 1)

-- Screen pixels to UI units: the UI is 768 units tall at scale 1, whatever the resolution.
local function Layout()
    local height = tonumber((GetCVar("gxResolution") or ""):match("%d+x(%d+)")) or 768
    local size = BLOCK * 768 / height / marker:GetEffectiveScale()
    marker:SetWidth(size * 2)
    marker:SetHeight(size)
    first:SetWidth(size)
    first:SetHeight(size)
    first:SetPoint("TOPLEFT", marker, "TOPLEFT", 0, 0)
    second:SetWidth(size)
    second:SetHeight(size)
    second:SetPoint("TOPLEFT", marker, "TOPLEFT", size, 0)
end

local function Covered()
    return (WorldMapFrame and WorldMapFrame:IsShown()) or (MovieFrame and MovieFrame:IsShown())
end

local function Update()
    Layout()
    if Covered() then
        marker:Hide()
    else
        marker:Show()
    end
end

local hooked = false
local events = CreateFrame("Frame")
events:RegisterEvent("PLAYER_LOGIN")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:RegisterEvent("UI_SCALE_CHANGED")
events:RegisterEvent("DISPLAY_SIZE_CHANGED")
events:SetScript("OnEvent", function()
    if not hooked then
        hooked = true
        for _, frame in ipairs({ WorldMapFrame, MovieFrame }) do
            frame:HookScript("OnShow", Update)
            frame:HookScript("OnHide", Update)
        end
    end
    Update()
end)

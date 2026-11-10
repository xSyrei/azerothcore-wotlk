-- Emberforge Lighting: while the world map covers the world, two 8x8 blocks (magenta, then green) in the screen's
-- top-left corner tell the Emberforge ReShade effects that read depth (occlusion, light sources, fog) to pause: the
-- depth buffer still holds the world behind the map, and they would shade the map. See reshade/Shaders/Emberforge.fxh.
local BLOCK = 8 -- screen pixels

local marker = CreateFrame("Frame", "EmberforgeLightingMarker", UIParent)
marker:SetFrameStrata("TOOLTIP")
marker:SetFrameLevel(100)
marker:SetPoint("TOPLEFT", UIParent, "TOPLEFT", 0, 0)
marker:Hide()

local magenta = marker:CreateTexture(nil, "OVERLAY")
magenta:SetTexture(1, 0, 1, 1)
local green = marker:CreateTexture(nil, "OVERLAY")
green:SetTexture(0, 1, 0, 1)

-- Screen pixels to UI units: the UI is 768 units tall at scale 1, whatever the resolution.
local function Layout()
    local height = tonumber((GetCVar("gxResolution") or ""):match("%d+x(%d+)")) or 768
    local pixel = 768 / height / UIParent:GetEffectiveScale()
    local size = BLOCK * pixel
    marker:SetWidth(size * 2)
    marker:SetHeight(size)
    magenta:SetWidth(size)
    magenta:SetHeight(size)
    magenta:SetPoint("TOPLEFT", marker, "TOPLEFT", 0, 0)
    green:SetWidth(size)
    green:SetHeight(size)
    green:SetPoint("TOPLEFT", marker, "TOPLEFT", size, 0)
end

local function Update()
    if WorldMapFrame and WorldMapFrame:IsShown() then
        Layout()
        marker:Show()
    else
        marker:Hide()
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("PLAYER_LOGIN")
events:RegisterEvent("UI_SCALE_CHANGED")
events:RegisterEvent("DISPLAY_SIZE_CHANGED")
events:SetScript("OnEvent", function(_, event)
    if event == "PLAYER_LOGIN" and WorldMapFrame then
        WorldMapFrame:HookScript("OnShow", Update)
        WorldMapFrame:HookScript("OnHide", Update)
    end
    Update()
end)

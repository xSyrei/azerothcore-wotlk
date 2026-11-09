-- Combo points for the Hero. The client keeps combo points only for a Rogue or Druid, so a Hero's never showed:
-- the server sends them (src/hero_resources.cpp, prefix EFCP, the count) and GetComboPoints answers with them, so
-- the stock combo frame by the target shows them. A finisher (ComboFinishers.lua, by name) looks unusable on the
-- action bars while there are none, as it does for a Rogue.

local PREFIX = "EFCP"
local heroPoints = 0
local BUTTON_BARS = {
    "ActionButton", "BonusActionButton", "MultiBarBottomLeftButton", "MultiBarBottomRightButton",
    "MultiBarRightButton", "MultiBarLeftButton",
}

local stockGetComboPoints = GetComboPoints
function GetComboPoints(unit, target)
    local points = stockGetComboPoints(unit, target)
    if points == 0 and heroPoints > 0 and (unit == nil or unit == "player" or UnitIsUnit(unit, "player"))
        and UnitExists(target or "target") then
        return heroPoints
    end
    return points
end

local function IsFinisher(action)
    local kind, id = GetActionInfo(action)
    if kind ~= "spell" or not EmberforgeComboFinishers then
        return false
    end
    local name = GetSpellInfo(id)
    return name and EmberforgeComboFinishers[name]
end

-- After the stock ActionButton_UpdateUsable: a finisher with no combo points looks unusable.
hooksecurefunc("ActionButton_UpdateUsable", function(button)
    if not button.action or heroPoints > 0 or not IsUsableAction(button.action) or not IsFinisher(button.action) then
        return
    end
    local name = button:GetName()
    local icon, normalTexture = _G[name .. "Icon"], _G[name .. "NormalTexture"]
    if icon then
        icon:SetVertexColor(0.4, 0.4, 0.4)
    end
    if normalTexture then
        normalTexture:SetVertexColor(1.0, 1.0, 1.0)
    end
end)

local function UpdateButtons()
    for _, bar in ipairs(BUTTON_BARS) do
        for i = 1, 12 do
            local button = _G[bar .. i]
            if button and button:IsVisible() and button.action and IsFinisher(button.action) then
                ActionButton_UpdateUsable(button)
            end
        end
    end
end

local function SetPoints(points)
    heroPoints = points
    if ComboFrame_Update then
        ComboFrame_Update()
    end
    UpdateButtons()
end

local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("PLAYER_TARGET_CHANGED")
frame:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            SetPoints(tonumber(message) or 0)
        end
    else
        UpdateButtons()
    end
end)

EmberforgeCombo = { SetPoints = SetPoints, IsFinisher = IsFinisher }

-- Rune spells on the action bars. The client patch leaves rune costs to the server (patch_spells.py,
-- drop_client_rune_costs): the client only recharges a real Death Knight's runes for casting. So the client thinks a
-- rune spell is always usable; this dims its button while its runes are not ready (as a spell short of mana: blue),
-- and lists its runes on the tooltip. The costs: RuneCosts.lua (EmberforgeRuneCosts, spell id = { blood, unholy,
-- frost }). A Death rune stands in for any rune.

local RUNE_NAMES = { "Blood", "Unholy", "Frost" }
local RUNE_COLORS = { "|cffff4040", "|cff40ff40", "|cff40c0ff" }
local DEATH = 4
local BUTTON_BARS = {
    "ActionButton", "BonusActionButton", "MultiBarBottomLeftButton", "MultiBarBottomRightButton",
    "MultiBarRightButton", "MultiBarLeftButton",
}

local function RuneCost(spellId)
    return spellId and EmberforgeRuneCosts and EmberforgeRuneCosts[spellId]
end

-- True if the ready runes pay the cost: each type its own runes first, then Death runes for what is left.
local function RunesReady(cost)
    local ready = { 0, 0, 0, [DEATH] = 0 }
    for rune = 1, 6 do
        local _, _, runeReady = GetRuneCooldown(rune)
        local runeType = GetRuneType(rune)
        if runeReady and runeType and ready[runeType] then
            ready[runeType] = ready[runeType] + 1
        end
    end
    local missing = 0
    for runeType = 1, 3 do
        missing = missing + math.max(0, cost[runeType] - ready[runeType])
    end
    return missing <= ready[DEATH]
end

local function ActionRuneCost(action)
    local kind, id = GetActionInfo(action)
    if kind == "spell" then
        return RuneCost(id)
    end
end

-- After the stock ActionButton_UpdateUsable: a usable rune spell whose runes are not ready looks short of power.
local function UpdateUsable(button)
    if not button.action then
        return
    end
    local cost = ActionRuneCost(button.action)
    if not cost or not IsUsableAction(button.action) or RunesReady(cost) then
        return
    end
    local name = button:GetName()
    local icon, normalTexture = _G[name .. "Icon"], _G[name .. "NormalTexture"]
    if icon then
        icon:SetVertexColor(0.5, 0.5, 1.0)
    end
    if normalTexture then
        normalTexture:SetVertexColor(0.5, 0.5, 1.0)
    end
end
hooksecurefunc("ActionButton_UpdateUsable", UpdateUsable)

-- The client sends no usable update when a rune comes back or is spent: every rune button is updated then.
local function UpdateAll()
    for _, bar in ipairs(BUTTON_BARS) do
        for i = 1, 12 do
            local button = _G[bar .. i]
            if button and button:IsVisible() and button.action and ActionRuneCost(button.action) then
                ActionButton_UpdateUsable(button)
            end
        end
    end
end

-- The tooltip's rune cost line, which the client no longer writes.
local function AddRuneLine(tooltip, spellId)
    local cost = RuneCost(spellId)
    if not cost then
        return
    end
    local parts = {}
    for runeType = 1, 3 do
        if cost[runeType] > 0 then
            parts[#parts + 1] = RUNE_COLORS[runeType] .. cost[runeType] .. " " .. RUNE_NAMES[runeType] .. "|r"
        end
    end
    if #parts > 0 then
        tooltip:AddLine(table.concat(parts, "  "), 1, 1, 1)
        tooltip:Show()
    end
end

GameTooltip:HookScript("OnTooltipSetSpell", function(tooltip)
    local _, _, spellId = tooltip:GetSpell()
    if type(spellId) == "number" then
        AddRuneLine(tooltip, spellId)
    end
end)

local frame = CreateFrame("Frame")
frame:RegisterEvent("RUNE_POWER_UPDATE")
frame:RegisterEvent("RUNE_TYPE_UPDATE")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:SetScript("OnEvent", UpdateAll)

EmberforgeRunes = { RunesReady = RunesReady, UpdateAll = UpdateAll }

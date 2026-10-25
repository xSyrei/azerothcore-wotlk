-- Emberforge Resources: extra resource bars for the classless Hero.
--
-- Heroes use every class resource. The player frame only shows mana, so this adds bars for
-- rage, energy and runic power under it, and turns on the Death Knight rune bar. A bar only
-- appears once you know an ability that uses it, or currently have some of that resource.

local HERO_CLASS = "PALADIN" -- class token of the hidden base class behind "Hero"

local POWER_RUNES = 5
local BARS = {
    { power = 1, token = "RAGE" },
    { power = 3, token = "ENERGY" },
    { power = 6, token = "RUNIC_POWER" },
}
local BAR_WIDTH, BAR_HEIGHT, BAR_GAP = 119, 10, 2

local _, playerClass = UnitClass("player")
if playerClass ~= HERO_CLASS then
    return
end

local known = {} -- power types used by abilities in the spellbook

local container = CreateFrame("Frame", "EmberforgeResourcesFrame", PlayerFrame)
container:SetPoint("TOPLEFT", PlayerFrameManaBar, "BOTTOMLEFT", 0, -6)
container:SetWidth(BAR_WIDTH)
container:SetHeight(1)

for _, info in ipairs(BARS) do
    local bar = CreateFrame("StatusBar", nil, container)
    bar:SetWidth(BAR_WIDTH)
    bar:SetHeight(BAR_HEIGHT)
    bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    local color = PowerBarColor[info.token]
    bar:SetStatusBarColor(color.r, color.g, color.b)

    bar.bg = bar:CreateTexture(nil, "BACKGROUND")
    bar.bg:SetAllPoints()
    bar.bg:SetTexture(0, 0, 0, 0.6)

    bar.text = bar:CreateFontString(nil, "OVERLAY", "TextStatusBarText")
    bar.text:SetPoint("CENTER", bar, "CENTER", 0, 0)

    info.bar = bar
end

local function ScanSpellbook()
    wipe(known)
    local _, _, offset, numSpells = GetSpellTabInfo(GetNumSpellTabs())
    for index = 1, offset + numSpells do
        local link = GetSpellLink(index, BOOKTYPE_SPELL)
        local spellId = link and tonumber(link:match("spell:(%d+)"))
        if spellId then
            local _, _, _, cost, _, powerType = GetSpellInfo(spellId)
            if powerType == POWER_RUNES or (cost and cost > 0) then
                known[powerType] = true
            end
        end
    end
end

local function Layout()
    local previous
    for _, info in ipairs(BARS) do
        local bar = info.bar
        local current = UnitPower("player", info.power)
        if known[info.power] or current > 0 then
            local max = UnitPowerMax("player", info.power)
            bar:SetMinMaxValues(0, max > 0 and max or 1)
            bar:SetValue(current)
            bar.text:SetText(current .. " / " .. max)
            bar:ClearAllPoints()
            if previous then
                bar:SetPoint("TOPLEFT", previous, "BOTTOMLEFT", 0, -BAR_GAP)
            else
                bar:SetPoint("TOPLEFT", container, "TOPLEFT", 0, 0)
            end
            bar:Show()
            previous = bar
        else
            bar:Hide()
        end
    end

    if known[POWER_RUNES] then
        RuneFrame:ClearAllPoints()
        RuneFrame:SetPoint("TOPLEFT", previous or container, "BOTTOMLEFT", -4, -4)
        RuneFrame:Show()
    else
        RuneFrame:Hide()
    end
end

local elapsedSinceUpdate = 0
container:SetScript("OnUpdate", function(_, elapsed)
    -- Energy ticks between events; refresh a few times a second.
    elapsedSinceUpdate = elapsedSinceUpdate + elapsed
    if elapsedSinceUpdate >= 0.1 then
        elapsedSinceUpdate = 0
        Layout()
    end
end)

container:SetScript("OnEvent", function(_, event)
    if event ~= "UNIT_DISPLAYPOWER" then
        ScanSpellbook()
    end
    Layout()
end)
container:RegisterEvent("PLAYER_ENTERING_WORLD")
container:RegisterEvent("SPELLS_CHANGED")
container:RegisterEvent("LEARNED_SPELL_IN_TAB")
container:RegisterEvent("UNIT_DISPLAYPOWER")

-- The Hero's resources on ElvUI. ElvUI hides Blizzard's player frame, where Emberforge_Resources.lua hangs the Hero's
-- Rage and Energy bars and the rune bar, and its own class bars know only real classes (the Hero is a Paladin to the
-- client): a Hero playing with ElvUI saw mana alone. Here, when ElvUI is loaded, the same bars in ElvUI's look:
-- Rage and Energy, then the six runes, under the player frame (on a mover: /moveui, "Hero Resources"), each shown as
-- Emberforge_Resources.lua shows its own (a known ability that uses it, or some of it now). ElvUI's target frame
-- combo bar, which shows only for a Rogue or a cat Druid, shows the Hero's combo points (Combo.lua's).

if not (IsAddOnLoaded and IsAddOnLoaded("ElvUI")) or not EmberforgeResources then
    return
end

local E = unpack(ElvUI)
local oUF = _G.ElvUF
local shared = EmberforgeResources
local BAR_HEIGHT, GAP = 12, 3
local RUNE_ORDER = { 1, 2, 5, 6, 3, 4 }              -- as the game's rune frame: blood, unholy, frost
local RUNE_COLORS = { { 1, 0, 0 }, { 0, 0.5, 0 }, { 0, 1, 1 }, { 0.8, 0.1, 1 } }

local holder, runes
local bars = {}

local function PowerColor(token)
    local c = oUF and oUF.colors and oUF.colors.power and oUF.colors.power[token]
    if c then
        return c[1] or c.r, c[2] or c.g, c[3] or c.b
    end
    c = PowerBarColor[token]
    return c.r, c.g, c.b
end

local function StatusBar(parent)
    local bar = CreateFrame("StatusBar", nil, parent)
    bar:SetStatusBarTexture(E.media.normTex)
    bar:CreateBackdrop("Default")
    bar.bg = bar:CreateTexture(nil, "BORDER")
    bar.bg:SetAllPoints()
    bar.bg:SetTexture(E.media.blankTex)
    return bar
end

local function Build()
    local player = _G.ElvUF_Player
    if holder or not player then
        return
    end
    holder = CreateFrame("Frame", "EmberforgeElvUIResources", E.UIParent)
    holder:SetWidth(player:GetWidth())
    holder:SetHeight(BAR_HEIGHT)
    holder:SetPoint("TOPLEFT", player, "BOTTOMLEFT", 0, -8)
    E:CreateMover(holder, "EmberforgeResourcesMover", "Hero Resources", nil, nil, nil, "ALL,SOLO")

    for _, info in ipairs(shared.BARS) do
        local bar = StatusBar(holder)
        local r, g, b = PowerColor(info.token)
        bar:SetStatusBarColor(r, g, b)
        bar.bg:SetVertexColor(r * 0.25, g * 0.25, b * 0.25)
        bar.text = bar:CreateFontString(nil, "OVERLAY")
        bar.text:FontTemplate()
        bar.text:SetPoint("CENTER", bar, "CENTER", 0, 0)
        bars[#bars + 1] = { info = info, bar = bar }
    end

    runes = CreateFrame("Frame", nil, holder)
    for i = 1, 6 do
        runes[i] = StatusBar(runes)
    end
end

-- Lays the shown bars out top to bottom and fills them (a few times a second: energy and runes tick between events).
local function Update()
    if not holder then
        return
    end
    local width = holder:GetWidth()
    local y = 0
    for _, entry in ipairs(bars) do
        local power, bar = entry.info.power, entry.bar
        local current = UnitPower("player", power)
        if shared.known[power] or current > 0 then
            local max = UnitPowerMax("player", power)
            bar:SetMinMaxValues(0, max > 0 and max or 1)
            bar:SetValue(current)
            bar.text:SetText(current)
            bar:ClearAllPoints()
            bar:SetPoint("TOPLEFT", holder, "TOPLEFT", 0, -y)
            bar:SetWidth(width)
            bar:SetHeight(BAR_HEIGHT)
            bar:Show()
            y = y + BAR_HEIGHT + GAP
        else
            bar:Hide()
        end
    end
    if shared.known[shared.POWER_RUNES] then
        runes:ClearAllPoints()
        runes:SetPoint("TOPLEFT", holder, "TOPLEFT", 0, -y)
        runes:SetWidth(width)
        runes:SetHeight(BAR_HEIGHT)
        local each = (width - GAP * 5) / 6
        local now = GetTime()
        for slot, rune in ipairs(RUNE_ORDER) do
            local bar = runes[slot]
            bar:ClearAllPoints()
            bar:SetPoint("TOPLEFT", runes, "TOPLEFT", (slot - 1) * (each + GAP), 0)
            bar:SetWidth(each)
            bar:SetHeight(BAR_HEIGHT)
            local color = (oUF and oUF.colors and oUF.colors.runes and oUF.colors.runes[GetRuneType(rune) or 1])
                or RUNE_COLORS[GetRuneType(rune) or 1]
            local r, g, b = color[1] or color.r, color[2] or color.g, color[3] or color.b
            bar:SetStatusBarColor(r, g, b)
            bar.bg:SetVertexColor(r * 0.25, g * 0.25, b * 0.25)
            local start, duration, ready = GetRuneCooldown(rune)
            bar:SetMinMaxValues(0, 1)
            if ready or not start or not duration or duration <= 0 then
                bar:SetValue(1)
            else
                bar:SetValue(math.min(1, math.max(0, (now - start) / duration)))
            end
        end
        runes:Show()
        y = y + BAR_HEIGHT + GAP
    else
        runes:Hide()
    end
    holder:SetHeight(math.max(1, y - GAP))
end

-- ElvUI's target combo bar for a Hero: its own update but with the Hero's points (GetComboPoints, Combo.lua's: ElvUI
-- kept the game's own, which knows none for a Paladin) and without its Rogue and Druid check.
local function HeroComboDisplay(frame, _, unit)
    if unit == "pet" then
        return
    end
    local element, db = frame.ComboPoints, frame.db
    if not element or not db or not db.combobar then
        return
    end
    local UF = E:GetModule("UnitFrames")
    local points = db.combobar.enable and GetComboPoints("player", "target") or 0
    if not db.combobar.enable or (points == 0 and db.combobar.autoHide) then
        element:Hide()
    else
        element:Show()
        for i = 1, MAX_COMBO_POINTS do
            if element[i] then
                if i <= points then
                    element[i]:Show()
                else
                    element[i]:Hide()
                end
                local r, g, b = element[i]:GetStatusBarColor()
                if element[i].bg then
                    element[i].bg:SetVertexColor(r * 0.35, g * 0.35, b * 0.35)
                end
            end
        end
    end
    UF.ToggleResourceBar(element)
end

local function HookCombo()
    local target = _G.ElvUF_Target
    if not target or not target.ComboPoints or target.ComboPoints.Override == HeroComboDisplay then
        return
    end
    target.ComboPoints.Override = HeroComboDisplay
    if EmberforgeCombo then
        EmberforgeCombo.OnChange = function()
            HeroComboDisplay(target, "ForceUpdate", "player")
        end
    end
    HeroComboDisplay(target, "ForceUpdate", "player")
end

local frame = CreateFrame("Frame")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:RegisterEvent("PLAYER_TARGET_CHANGED")
frame:SetScript("OnEvent", function(_, event)
    Build()
    HookCombo()
    if event == "PLAYER_TARGET_CHANGED" and _G.ElvUF_Target and _G.ElvUF_Target.ComboPoints then
        HeroComboDisplay(_G.ElvUF_Target, event, "player")
    end
    Update()
end)
local since = 0
frame:SetScript("OnUpdate", function(_, elapsed)
    since = since + elapsed
    if since >= 0.1 then
        since = 0
        Update()
    end
end)

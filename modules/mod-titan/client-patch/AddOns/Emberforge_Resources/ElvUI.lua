-- The Hero's resources on ElvUI. ElvUI hides Blizzard's player frame, where Emberforge_Resources.lua hangs the Hero's
-- Rage and Energy bars and the rune bar, and its own class bars know only real classes (the Hero is a Paladin to the
-- client): a Hero playing with ElvUI saw mana alone. Here, when ElvUI is loaded, the same bars in ElvUI's look:
-- Rage and Energy, then the six runes, under the player frame (on a mover: /moveui, "Hero Resources"), each shown as
-- Emberforge_Resources.lua shows its own (a known ability that uses it, or some of it now). ElvUI's target frame
-- combo bar, which shows only for a Rogue or a cat Druid, shows the Hero's combo points (Combo.lua's), and so do
-- ElvUI's nameplates. ElvUI's config (/ec) gets an Emberforge page: these bars' look, the target combo bar (detached,
-- it moves on its own mover) and nameplate combo points.

if not (IsAddOnLoaded and IsAddOnLoaded("ElvUI")) or not EmberforgeResources then
    return
end

local E = unpack(ElvUI)
local oUF = _G.ElvUF
local shared = EmberforgeResources

-- The Hero Resources bars' settings (per character, EmberforgeResourcesDB.elvui).
local DEFAULTS = { enable = true, width = 0, barHeight = 12, runeHeight = 12, spacing = 3, showText = true,
    textFormat = "current", fontSize = 11, rage = true, energy = true, runes = true }
local function DB()
    EmberforgeResourcesDB = EmberforgeResourcesDB or {}
    local db = EmberforgeResourcesDB.elvui or {}
    EmberforgeResourcesDB.elvui = db
    for key, value in pairs(DEFAULTS) do
        if db[key] == nil then
            db[key] = value
        end
    end
    return db
end
local SHOWN_BY = { [1] = "rage", [3] = "energy" }
local function Text(current, max, format)
    if format == "percent" then
        return max > 0 and string.format("%d%%", current * 100 / max) or "0%"
    elseif format == "current-max" then
        return current .. " / " .. max
    end
    return tostring(current)
end
local RUNE_ORDER = { 1, 2, 5, 6, 3, 4 }              -- as the game's rune frame: blood, unholy, frost
local RUNE_COLORS = { { 1, 0, 0 }, { 0, 0.5, 0 }, { 0, 1, 1 }, { 0.8, 0.1, 1 } }

local holder, runes
local UpdateNameplates                                 -- below: the nameplates' combo points
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
    holder:SetHeight(DB().barHeight)
    holder:SetPoint("TOPLEFT", player, "BOTTOMLEFT", 0, -8)
    E:CreateMover(holder, "EmberforgeResourcesMover", "Hero Resources", nil, nil, nil, "ALL,SOLO")

    for _, info in ipairs(shared.BARS) do
        local bar = StatusBar(holder)
        local r, g, b = PowerColor(info.token)
        bar:SetStatusBarColor(r, g, b)
        bar.bg:SetVertexColor(r * 0.25, g * 0.25, b * 0.25)
        bar.text = bar:CreateFontString(nil, "OVERLAY")
        bar.text:FontTemplate(nil, DB().fontSize)
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
    local db = DB()
    local player = _G.ElvUF_Player
    local width = db.width > 0 and db.width or (player and player:GetWidth() or 200)
    local BAR_HEIGHT, GAP = db.barHeight, db.spacing
    holder:SetWidth(width)
    if not db.enable then
        holder:Hide()
        return
    end
    holder:Show()
    local y = 0
    for _, entry in ipairs(bars) do
        local power, bar = entry.info.power, entry.bar
        local current = UnitPower("player", power)
        if db[SHOWN_BY[power]] and (shared.known[power] or current > 0) then
            local max = UnitPowerMax("player", power)
            bar:SetMinMaxValues(0, max > 0 and max or 1)
            bar:SetValue(current)
            bar.text:SetText(db.showText and Text(current, max, db.textFormat) or "")
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
    if db.runes and shared.known[shared.POWER_RUNES] then
        BAR_HEIGHT = db.runeHeight
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
            UpdateNameplates()
        end
    end
    HeroComboDisplay(target, "ForceUpdate", "player")
end

-- ElvUI's nameplates: the target's plate shows combo points from GetComboPoints, ElvUI's own copy, which knows none
-- for a Hero: its update reads the Hero's (Combo.lua's), and runs when they change.
local function HookNameplates()
    local NP = E:GetModule("NamePlates", true)
    if not NP or NP.EmberforgeHooked or not NP.Update_CPoints then
        return NP
    end
    NP.EmberforgeHooked = true
    function NP:Update_CPoints(plate)
        if plate.UnitType == "FRIENDLY_PLAYER" or plate.UnitType == "FRIENDLY_NPC" or not plate.CPoints then
            return
        end
        local db = self.db and self.db.units and self.db.units.TARGET and self.db.units.TARGET.comboPoints
        local points = db and db.enable and plate.isTarget and GetComboPoints("player", "target") or 0
        if points > 0 then
            plate.CPoints:Show()
            for i = 1, MAX_COMBO_POINTS do
                if plate.CPoints[i] then
                    if i <= points then
                        plate.CPoints[i]:Show()
                    else
                        plate.CPoints[i]:Hide()
                    end
                end
            end
        else
            plate.CPoints:Hide()
        end
    end
    return NP
end
function UpdateNameplates()
    local NP = HookNameplates()
    if NP and NP.VisiblePlates then
        for plate in pairs(NP.VisiblePlates) do
            NP:Update_CPoints(plate)
        end
    end
end

-- ElvUI's config (/ec): the Emberforge page.
local function Refresh()
    Update()
    local UF = E:GetModule("UnitFrames", true)
    if UF and UF.CreateAndUpdateUF then
        UF:CreateAndUpdateUF("target")
    end
    HookCombo()
    if _G.ElvUF_Target then
        HeroComboDisplay(_G.ElvUF_Target, "ForceUpdate", "player")
    end
    local NP = E:GetModule("NamePlates", true)
    if NP and NP.ConfigureAll then
        NP:ConfigureAll()
    end
    UpdateNameplates()
end
local function Own(key)
    return function() return DB()[key] end, function(_, value) DB()[key] = value; Refresh() end
end
local function Combo(key)
    return function() return E.db.unitframe.units.target.combobar[key] end,
        function(_, value) E.db.unitframe.units.target.combobar[key] = value; Refresh() end
end
local function Plate(key)
    return function() return E.db.nameplates.units.TARGET.comboPoints[key] end,
        function(_, value) E.db.nameplates.units.TARGET.comboPoints[key] = value; Refresh() end
end
local function Option(kind, name, order, getset, extra)
    local get, set = getset[1], getset[2]
    local option = { type = kind, name = name, order = order, get = get, set = set }
    for key, value in pairs(extra or {}) do
        option[key] = value
    end
    return option
end
local function InsertOptions()
    local function own(key) local g, s = Own(key) return { g, s } end
    local function combo(key) local g, s = Combo(key) return { g, s } end
    local function plate(key) local g, s = Plate(key) return { g, s } end
    E.Options.args.emberforge = {
        type = "group", name = "|cffff7d1fEmberforge|r", order = 6,
        args = {
            intro = { type = "description", order = 1,
                name = "The classless Hero's resources on ElvUI: Rage, Energy and runes under the player frame, " ..
                    "combo points on the target frame and nameplates." },
            move = { type = "execute", order = 2, name = "Move frames",
                desc = "ElvUI's movers: Hero Resources, and the Combobar when detached.",
                func = function() E:ToggleMoveMode() end },
            resources = {
                type = "group", inline = true, order = 10, name = "Hero Resources",
                args = {
                    enable = Option("toggle", "Show", 1, own("enable")),
                    rage = Option("toggle", "Rage", 2, own("rage"), { desc = "Once you know a Rage ability." }),
                    energy = Option("toggle", "Energy", 3, own("energy"), { desc = "Once you know an Energy ability." }),
                    runes = Option("toggle", "Runes", 4, own("runes"), { desc = "Once you know a rune ability." }),
                    width = Option("range", "Width", 5, own("width"),
                        { min = 0, max = 600, step = 1, desc = "0: as wide as the player frame." }),
                    barHeight = Option("range", "Bar height", 6, own("barHeight"), { min = 4, max = 40, step = 1 }),
                    runeHeight = Option("range", "Rune height", 7, own("runeHeight"), { min = 4, max = 40, step = 1 }),
                    spacing = Option("range", "Spacing", 8, own("spacing"), { min = 0, max = 20, step = 1 }),
                    showText = Option("toggle", "Show text", 9, own("showText")),
                    textFormat = Option("select", "Text", 10, own("textFormat"), { values = {
                        current = "Current", ["current-max"] = "Current / Max", percent = "Percent" } }),
                    fontSize = Option("range", "Font size", 11, {
                        function() return DB().fontSize end,
                        function(_, value)
                            DB().fontSize = value
                            for _, entry in ipairs(bars) do entry.bar.text:FontTemplate(nil, value) end
                            Refresh()
                        end }, { min = 6, max = 24, step = 1 }),
                },
            },
            combo = {
                type = "group", inline = true, order = 20, name = "Combo Points (target frame)",
                args = {
                    enable = Option("toggle", "Show", 1, combo("enable")),
                    autoHide = Option("toggle", "Hide at 0", 2, combo("autoHide")),
                    detachFromFrame = Option("toggle", "Detach from frame", 3, combo("detachFromFrame"),
                        { desc = "Its own mover (Combobar): Move frames to place it." }),
                    detachedWidth = Option("range", "Detached width", 4, combo("detachedWidth"),
                        { min = 15, max = 600, step = 1 }),
                    height = Option("range", "Height", 5, combo("height"), { min = 3, max = 30, step = 1 }),
                    spacing = Option("range", "Spacing", 6, combo("spacing"), { min = 0, max = 20, step = 1 }),
                    fill = Option("select", "Style", 7, combo("fill"),
                        { values = { fill = "Filled", spaced = "Spaced" } }),
                },
            },
            nameplates = {
                type = "group", inline = true, order = 30, name = "Combo Points (nameplates)",
                args = {
                    enable = Option("toggle", "Show on the target's nameplate", 1, plate("enable"), { width = "double" }),
                    width = Option("range", "Point width", 2, plate("width"), { min = 2, max = 30, step = 1 }),
                    height = Option("range", "Point height", 3, plate("height"), { min = 2, max = 30, step = 1 }),
                    spacing = Option("range", "Spacing", 4, plate("spacing"), { min = 0, max = 20, step = 1 }),
                    xOffset = Option("range", "X offset", 5, plate("xOffset"), { min = -100, max = 100, step = 1 }),
                    yOffset = Option("range", "Y offset", 6, plate("yOffset"), { min = -100, max = 100, step = 1 }),
                },
            },
        },
    }
end
local plugin = LibStub and LibStub("LibElvUIPlugin-1.0", true)
if plugin then
    plugin:RegisterPlugin("Emberforge_Resources", InsertOptions)
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
    UpdateNameplates()
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

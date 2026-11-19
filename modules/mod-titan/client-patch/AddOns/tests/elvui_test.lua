-- Emberforge_Resources' ElvUI module (ElvUI.lua) with a stand-in ElvUI: the Hero's Rage and Energy bars and runes
-- under ElvUI's player frame, and ElvUI's target combo bar showing the Hero's combo points.
local DIR = "../Emberforge_Resources/"
local failures = 0
local function check(ok, what)
    print((ok and "ok    " or "FAIL  ") .. what)
    if not ok then failures = failures + 1 end
end

local Widget = {}
Widget.__index = Widget
local function NewWidget(name)
    local w = setmetatable({ shown = true, points = {}, width = 200, height = 1, scripts = {}, events = {} }, Widget)
    if name then _G[name] = w end
    return w
end
function Widget:SetStatusBarTexture() end
function Widget:CreateBackdrop() end
function Widget:CreateTexture() return NewWidget() end
function Widget:CreateFontString() return NewWidget() end
function Widget:FontTemplate() end
function Widget:SetTexture() end
function Widget:SetAllPoints() end
function Widget:SetVertexColor() end
function Widget:SetStatusBarColor(r, g, b) self.color = { r, g, b } end
function Widget:GetStatusBarColor() return 1, 0.8, 0 end
function Widget:SetMinMaxValues(a, b) self.min, self.max = a, b end
function Widget:SetValue(v) self.value = v end
function Widget:SetText(t) self.text = t end
function Widget:SetWidth(w) self.width = w end
function Widget:SetHeight(h) self.height = h end
function Widget:GetWidth() return self.width end
function Widget:ClearAllPoints() self.points = {} end
function Widget:SetPoint(...) self.points[#self.points + 1] = { ... } end
function Widget:Show() self.shown = true end
function Widget:Hide() self.shown = false end
function Widget:IsShown() return self.shown end
function Widget:RegisterEvent(e) self.events[e] = true end
function Widget:SetScript(k, f) self.scripts[k] = f end
local frames = {}
function CreateFrame(_, name) local w = NewWidget(name); frames[#frames + 1] = w; return w end

local power = { [0] = 500, [1] = 0, [3] = 80 }
function UnitPower(_, p) return power[p] or 0 end
function UnitPowerMax(_, p) return p == 0 and 1000 or 100 end
function GetTime() return 100 end
function GetRuneType(i) return i <= 2 and 1 or i <= 4 and 3 or 2 end
function GetRuneCooldown(i) if i == 1 then return 95, 10, false end return 0, 0, true end
local heroPoints = 0
function GetComboPoints() return heroPoints end
MAX_COMBO_POINTS = 5
PowerBarColor = { RAGE = { r = 1, g = 0, b = 0 }, ENERGY = { r = 1, g = 1, b = 0 } }
function IsAddOnLoaded(name) return name == "ElvUI" end

local toggled = 0
local E = { media = { normTex = "t", blankTex = "b" }, UIParent = NewWidget() }
function E:CreateMover(frame, name) self.mover = name end
function E:GetModule() return { ToggleResourceBar = function() toggled = toggled + 1 end } end
ElvUI = { E }
ElvUF = { colors = { power = { RAGE = { 0.8, 0.1, 0.1 }, ENERGY = { 0.9, 0.9, 0.2 } }, runes = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 1 } } } }
NewWidget("ElvUF_Player").width = 240
local target = NewWidget("ElvUF_Target")
target.db = { combobar = { enable = true, autoHide = true } }
target.ComboPoints = NewWidget()
for i = 1, 5 do target.ComboPoints[i] = NewWidget() end
EmberforgeCombo = {}

local known = {}
EmberforgeResources = { known = known, POWER_RUNES = 5, BARS = { { power = 1, token = "RAGE" }, { power = 3, token = "ENERGY" } } }
local pluginCallback
function LibStub() return { RegisterPlugin = function(_, _, callback) pluginCallback = callback end } end
E.db = { unitframe = { units = { target = { combobar = { enable = true, detachFromFrame = false } } } },
    nameplates = { units = { TARGET = { comboPoints = { enable = false, yOffset = 0 } } } } }
function E:ToggleMoveMode() self.moving = true end
dofile(DIR .. "ElvUI.lua")
local listener
for _, f in ipairs(frames) do if f.events.PLAYER_ENTERING_WORLD then listener = f end end
local function fire(event) listener.scripts.OnEvent(listener, event) end

-- A Hero who knows no Rage ability, 80 Energy now: the Energy bar alone, no runes.
fire("PLAYER_ENTERING_WORLD")
local holder = _G.EmberforgeElvUIResources
check(holder and E.mover == "EmberforgeResourcesMover", "the bars sit on an ElvUI mover under the player frame")
check(holder.width == 240, "as wide as ElvUI's player frame")
local shown = {}
for _, f in ipairs(frames) do if f.min == 0 and f.max == 100 and f.shown then shown[#shown + 1] = f end end
check(#shown == 1 and shown[1].value == 80 and shown[1].text.text == "80", "Energy shown with its value, Rage hidden")
-- Rage known, runes known: Rage, Energy and the six runes; rune 1 half back
known[1], known[5] = true, true
listener.scripts.OnUpdate(listener, 0.2)
local runesShown, half = 0, false
for _, f in ipairs(frames) do
    if f.max == 1 and f.shown then
        runesShown = runesShown + 1
        if f.value == 0.5 then half = true end
    end
end
check(runesShown == 6 and half, "the six runes, one refilling halfway")
check(holder.height == 12 * 3 + 3 * 2, "three rows high: Rage, Energy, runes")

-- ElvUI's target combo bar: the Hero's points
check(target.ComboPoints.Override ~= nil and EmberforgeCombo.OnChange ~= nil, "the combo bar's update is the Hero's")
heroPoints = 3
EmberforgeCombo.OnChange(3)
local lit = 0
for i = 1, 5 do if target.ComboPoints[i].shown then lit = lit + 1 end end
check(target.ComboPoints.shown and lit == 3, "3 combo points lit")
heroPoints = 0
EmberforgeCombo.OnChange(0)
check(not target.ComboPoints.shown, "none: the bar hides (Auto Hide)")

-- The settings: no Energy, percent text, a fixed width, taller bars
EmberforgeResourcesDB.elvui.energy = false
EmberforgeResourcesDB.elvui.rage = true
power[1] = 25
EmberforgeResourcesDB.elvui.textFormat = "percent"
EmberforgeResourcesDB.elvui.width = 300
EmberforgeResourcesDB.elvui.barHeight = 20
listener.scripts.OnUpdate(listener, 0.2)
local rageBar
for _, f in ipairs(frames) do if f.shown and f.value == 25 then rageBar = f end end
check(rageBar and rageBar.text.text == "25%" and rageBar.width == 300 and rageBar.height == 20,
    "Rage as a percent, 300 wide, 20 high")
local energyShown = false
for _, f in ipairs(frames) do if f.shown and f.value == 80 then energyShown = true end end
check(not energyShown, "Energy turned off")
EmberforgeResourcesDB.elvui.enable = false
listener.scripts.OnUpdate(listener, 0.2)
check(not holder.shown, "the bars turned off")

-- ElvUI's config: the Emberforge page
E.Options = { args = {} }
check(pluginCallback ~= nil, "registered as an ElvUI plugin")
pluginCallback()
local page = E.Options.args.emberforge
check(page and page.args.resources and page.args.combo and page.args.nameplates, "the Emberforge page: three sections")
page.args.combo.args.detachFromFrame.set(nil, true)
check(E.db.unitframe.units.target.combobar.detachFromFrame == true, "Detach from frame sets ElvUI's combo bar")
page.args.nameplates.args.enable.set(nil, true)
check(E.db.nameplates.units.TARGET.comboPoints.enable == true, "nameplate combo points switched on")
page.args.resources.args.barHeight.set(nil, 16)
check(EmberforgeResourcesDB.elvui.barHeight == 16 and page.args.resources.args.barHeight.get() == 16,
    "bar height kept per character")
page.args.move.func()
check(E.moving, "Move frames opens ElvUI's movers")

print(failures == 0 and "All checks passed" or (failures .. " check(s) failed"))
os.exit(failures == 0 and 0 or 1)

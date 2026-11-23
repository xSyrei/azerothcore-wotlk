-- Offline test for Emberforge_Runes: loads the addon against a mocked WoW 3.3.5 API and plays the panel the way
-- the server drives it (see src/runes.cpp), stepping its animation. Run: lua5.1 runes_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Runes/"
local failures = 0
local function check(ok, what)
    print((ok and "ok    " or "FAIL  ") .. what)
    if not ok then
        failures = failures + 1
    end
end

-- Mock widgets: any capitalized method is a no-op unless defined below; state the test reads is recorded.
local Widget = {}
Widget.__index = function(self, key)
    local method = rawget(Widget, key)
    if method then
        return method
    end
    if type(key) == "string" and key:match("^[A-Z]") then
        return function() end
    end
end
local function NewWidget(name, parent)
    local widget = setmetatable({ shown = true, scripts = {}, text = "", width = 36, height = 36, name = name,
        parent = parent }, Widget)
    if name then
        _G[name] = widget
    end
    return widget
end
function Widget:GetName() return self.name end
function Widget:CreateTexture()
    local texture = NewWidget(nil, self)
    self.textures = self.textures or {}
    self.textures[#self.textures + 1] = texture
    return texture
end
function Widget:CreateFontString() return NewWidget(nil, self) end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:Show()
    local wasShown = self.shown
    self.shown = true
    if not wasShown and self.scripts.OnShow then
        self.scripts.OnShow(self)
    end
end
function Widget:Hide()
    local wasShown = self.shown
    self.shown = false
    if wasShown and self.scripts.OnHide then
        self.scripts.OnHide(self)
    end
end
function Widget:IsShown() return self.shown end
function Widget:SetText(text) self.text = text or "" end
function Widget:GetText() return self.text end
function Widget:GetStringWidth() return #self.text * 8 end
function Widget:SetTexture(texture) self.texture = texture end
function Widget:SetTextColor(r, g, b) self.color = { r, g, b } end
function Widget:SetDesaturated(on) self.desaturated = on end
function Widget:SetAlpha(alpha) self.alpha = alpha end
function Widget:GetWidth() return self.width end
function Widget:GetHeight() return self.height end
function Widget:SetFrameLevel(level) self.level = level end
function Widget:GetFrameLevel() return self.level or 1 end
function Widget:GetParent() return self.parent end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end

UIParent = NewWidget("UIParent")
UIParent.width, UIParent.height = 1600, 900
local frames = {}
function CreateFrame(_, name, parent)
    local widget = NewWidget(name, parent)
    frames[#frames + 1] = widget
    return widget
end
local sent = {}
UISpecialFrames, SlashCmdList = {}, {}
GameTooltip = NewWidget("GameTooltip")
-- The tooltip's aura methods and the hooks on them (only on tables: the action bar hooks stay unset here).
local tooltipLines = {}
function GameTooltip:SetUnitBuff() end
function GameTooltip:SetUnitDebuff() end
function GameTooltip:SetUnitAura() end
function GameTooltip:AddLine(text) tooltipLines[#tooltipLines + 1] = text end
function GameTooltip:HookScript(name, fn) self.scripts[name] = fn end
function GameTooltip:Show() end
local tooltipSpell
function GameTooltip:GetSpell() return tooltipSpell end
function hooksecurefunc(target, method, hook)
    if type(target) ~= "table" then
        return
    end
    local original = target[method]
    target[method] = function(...)
        original(...)
        hook(...)
    end
end
local auras = {}                                -- index -> spell id, the aura tooltip's
function UnitAura(_, index)
    if auras[index] then
        return "Aura", "", "", 0, nil, 0, 0, "player", nil, nil, auras[index]
    end
end
function SendAddonMessage(prefix, message) sent[#sent + 1] = { prefix, message } end
function UnitName() return "Tester" end
function UnitLevel() return 12 end
local SPELLS = { [26573] = "Consecration", [90730] = "Rune of the Sanctified Paragon",
    [90111] = "Throw Glaive", [90701] = "Fel Command", [90116] = "Metamorphosis",
    [686] = "Shadow Bolt", [29722] = "Incinerate", [688] = "Summon Imp", [697] = "Summon Voidwalker" }
function GetSpellInfo(id) return SPELLS[id] end
tinsert = table.insert
StaticPopupDialogs = {}
local popup
function StaticPopup_Show(name, a, b) popup = { name = name, a = a, b = b } return popup end
ACCEPT, CANCEL = "Accept", "Cancel"
-- The player's buffs (name -> stacks) and the action bars (button -> spell id).
local buffs, actions = {}, {}
function UnitBuff(_, name)
    if buffs[name] then
        return name, "", "", buffs[name]
    end
end
function GetActionInfo(slot)
    if actions[slot] then
        return "spell", 40 + slot, "spell", actions[slot]       -- 3.3.5: a spellbook index, the spell's id fourth
    end
end
for index = 1, 12 do
    local button = NewWidget("ActionButton" .. index)
    button.action = index
    button.GetName = function() return "ActionButton" .. index end
    _G["ActionButton" .. index .. "Icon"] = button:CreateTexture(nil, "BORDER")
end

-- The loader first: until the rest loads, /runes says why the panel is missing and asks for the state.
local chat = {}
DEFAULT_CHAT_FRAME = { AddMessage = function(_, text) chat[#chat + 1] = text end }
local handler = function() end
function geterrorhandler() return handler end
function seterrorhandler(fn) handler = fn end
dofile(DIR .. "Loader.lua")
handler("Interface\\AddOns\\Emberforge_Runes\\Emberforge_Runes.lua:12: attempt to index nil")
SlashCmdList["EMBERFORGERUNES"]()
check(chat[1] and chat[1]:find("did not load") and chat[2] and chat[2]:find("Emberforge_Runes.lua:12", 1, true)
    and sent[#sent][2] == "STATE", "before the panel loads, /runes reports its load error and asks for the state")
dofile(DIR .. "Catalog.lua")
-- The panel's mechanics are checked on every rune, retired ones too (their hiding is checked at the end).
local retired = {}
for _, rune in ipairs(EmberforgeRunesCatalog) do
    if rune.retired then
        retired[#retired + 1] = rune
        rune.retired = nil
    end
end
dofile(DIR .. "Art.lua")
dofile(DIR .. "Emberforge_Runes.lua")
local frame = EmberforgeLegendaryRunes
local view, debug = EmberforgeRunes_View, EmberforgeRunes_Debug
local function server(message) EmberforgeRunes_OnMessage(message) end
local function lastSent() return sent[#sent] and sent[#sent][2] end
local function run(seconds)
    for _ = 1, math.floor(seconds / 0.02 + 0.5) do
        debug.Step(0.02)
    end
end
frame:Hide()

-- The catalog and the art: only the runes the server builds (the first draft's placeholders are gone).
local function ById(id)
    for _, rune in ipairs(EmberforgeRunesCatalog) do
        if rune.id == id then
            return rune
        end
    end
end
check(#EmberforgeRunesCatalog == 28 and EmberforgeRunesCatalog[1].id == 21 and not ById(1) and not ById(20)
    and ById(22).name == "Rune of the Sanctified Paragon" and ById(23).name == "Rune of the Stormbound Conduit"
    and ById(24).name == "Rune of Echoing Thunder" and ById(25).name == "Rune of the Wellspring"
    and ById(26).name == "Rune of the Wandering Mist" and ById(34).name == "Rune of Arcane Mind"
    and ById(38).name == "Rune of Guiding Light" and ById(39).name == "Rune of Elune's Guardian"
    and ById(41).name == "Rune of Ursoc's Champion" and ById(42).name == "Rune of the Duelist"
    and ById(43).name == "Rune of the Stormreaver" and ById(44).name == "Rune of Ember's Wrath"
    and ById(45).name == "Rune of Whitefang" and ById(46).name == "Rune of Red Communion"
    and ById(47).name == "Rune of the Primal Worldwarden" and ById(48).name == "Rune of the Black Covenant",
    "the catalog holds only the runes the server builds")
for _, rune in ipairs(EmberforgeRunesCatalog) do
    check(EmberforgeRunesArt.runes[rune.id] and EmberforgeRunesArt.runes[rune.id].built, "built: " .. rune.name)
end
-- The five after them: each built, with its three slots, nine options and three builds.
for id, name in pairs({ [27] = "Rune of the Threefold Grave", [28] = "Rune of the Unbroken Onslaught",
    [29] = "Rune of the Dawnbound Crusader", [30] = "Rune of the Hollow Congregation",
    [31] = "Rune of the Shattered Sun", [32] = "Rune of the Spectral Blade", [33] = "Rune of the Wild Hunt" }) do
    local entry, rune = EmberforgeRunesArt.runes[id], ById(id)
    check(rune.name == name and entry.built and #entry.slots == 3 and #entry.texts == 9 and #rune.mods == 9
        and #rune.builds == 3, "built with its words: " .. name)
    for _, build in ipairs(rune.builds) do
        for _, mod in ipairs(build.mods) do
            local found = false
            for _, option in ipairs(rune.mods) do
                found = found or option.name == mod
            end
            check(found, name .. ": build " .. build.name .. " names a real modifier (" .. mod .. ")")
        end
    end
end
for _, id in ipairs({ 25, 26 }) do
    local entry = EmberforgeRunesArt.runes[id]
    check(entry.built and #entry.texts == 9 and #ById(id).mods == 9
        and ById(id).roles[1] == "HEALER", "a healer's rune with its words: rune " .. id)
end
local thunder = EmberforgeRunesArt.runes[24]
check(thunder.built and thunder.slots[3] == "Resonance" and #thunder.texts == 9
    and ById(24).roles[1] == "TANK", "Art.lua has Echoing Thunder's slots and words; a tank's rune")
local storm = EmberforgeRunesArt.runes[23]
check(storm.built and storm.slots[2] == "Lightning Shield" and #storm.texts == 9 and #ById(23).mods == 9,
    "Art.lua has the Stormbound Conduit's slots (Arcs, Lightning Shield, Surge) and its words")
local paragon = EmberforgeRunesArt.runes[22]
check(paragon.built and paragon.slots[1] == "Presence" and paragon.slots[3] == "Covenant" and #paragon.texts == 9,
    "Art.lua has the Sanctified Paragon's slots (Presence, Manifestation, Covenant) and its words")
local fel = EmberforgeRunesArt.runes[21]
check(fel.built and fel.asks[2] == "What your Chaos Bolt Volley does" and #fel.texts == 9,
    "Art.lua has Fel Commandment's questions and option words")

-- The panel's own tooltip and questions: the game's tooltip is never moved (moving it broke tooltips until a reload).
local tooltipParent = GameTooltip:GetParent()
local ask = debug.ask
local function Accept()
    check(ask:IsShown(), "a question is asked")
    ask.yes:Click()
end

-- Below the unlock level: nothing opens; /runes says when.
server("R,21,-")
server("S,8,10,1,0,0,0,0,-,0")
check(not frame:IsShown(), "below the unlock level the panel stays closed")
SlashCmdList.EMBERFORGERUNES()
check(frame:IsShown() and view.mode == "locked" and debug.locked.subtitle.text == "Legendary Runes unlock at level 10.",
    "/runes shows when the runes unlock")
check(lastSent() == "STATE", "opening asks the server for the state")
frame:Hide()

-- Unlocked, no rune: the panel opens by itself once, on Recommended for you.
server("S,10,10,1,0,0,0,0,21,1")
local offers = debug.offers
check(frame:IsShown() and view.mode == "offers" and offers.target == 1 and offers.title.text == "Recommended for you",
    "unlocked with no rune: Recommended for you")
check(offers.cards[1].hit.rune.id == 21 and offers.cards[1].name.text == "Fel Commandment"
    and not offers.cards[2].name.shown and offers.cards[1].preview.label.text == "Preview rune",
    "one medallion: the rune the Hero's abilities fit, its name without 'Rune of', and Preview rune")
check(offers.cards[1].role.label.text == "DAMAGE" and offers.cards[1].desc.text ~= "", "its role and what it does")
check(GameTooltip:GetParent() == tooltipParent, "the game's tooltip stays where it is")
frame:Hide()
server("S,10,10,1,0,0,0,0,21,1")
check(not frame:IsShown(), "it opens by itself only once a session")

-- Browse legendary runes: every rune in one list, searched and filtered.
EmberforgeRunes_Open()
offers.browse:Click()
local rows, browse = debug.rows, debug.browse
local function listed()
    local ids = {}
    for _, rune in ipairs(browse.list) do
        ids[#ids + 1] = rune.id
    end
    return table.concat(ids, ",")
end
check(view.mode == "browse" and debug.browser.target == 1 and #browse.list == 28 and rows[1].shown and rows[4].shown
    and debug.browser.count.text == "28 runes", "Browse lists every rune, four rows at a time")
check(rows[1].rune.id == 21 and rows[1].status.text == "Usable now" and rows[2].status.text == "Coming soon",
    "the usable rune comes first; each row says where it stands")
check(rows[1].choose.label.text == "Choose" and rows[1].choose.look == "primary" and rows[2].choose.look ~= "primary",
    "Choose is lit on a usable rune only")
debug.browser.scripts.OnMouseWheel(debug.browser, -1)
check(browse.offset == 1 and rows[1].rune == browse.list[2], "the mouse wheel scrolls the list")
debug.browser.scripts.OnMouseWheel(debug.browser, -30)
check(browse.offset == 24, "it stops at the end")
local search = debug.search
for query, expected in pairs({ ["warlock glaive"] = "21", ["soothing heal"] = "26", ["rogue"] = "32,42",
    ["kill command"] = "33", ["prayer of mending"] = "38", ["icicle"] = "35" }) do
    search.text = query
    search.scripts.OnTextChanged(search)
    check(listed() == expected, "the search finds '" .. query .. "'")
end
search.text = "lightning"
search.scripts.OnTextChanged(search)
check(browse.offset == 0 and rows[1].rune.id == 23 and not search.hint.shown, "the search finds a rune by its name")
search.text = "xyzzy"
search.scripts.OnTextChanged(search)
check(#browse.list == 0 and not rows[1].shown and debug.browser.empty.text:find("No rune matches"),
    "nothing found says so")
search.text = ""
search.scripts.OnTextChanged(search)
check(search.hint.shown and #browse.list == 28, "an empty search lists every rune again")
local chips = debug.chips
chips[3]:Click()
local healers = true
for _, rune in ipairs(browse.list) do
    local heals = false
    for _, role in ipairs(rune.roles or {}) do
        heals = heals or role == "HEALER"
    end
    healers = healers and heals
end
check(healers and #browse.list >= 3 and chips[3].on and not chips[1].on, "the Healing filter")
chips[1]:Click()
check(chips[1].on and #browse.list == 28, "All clears it")
debug.toggle:Click()
check(listed() == "21" and browse.usable, "Usable now: only the runes nothing is missing for")
debug.toggle:Click()
server("R,21,90111/688+697")
check(rows[1].status.text:find("Requires Throw Glaive"), "a rune missing abilities says what it requires")
rows[1].choose:Click()
check(ask.title.text == "Warning" and ask.text.text:find("Throw Glaive, and Summon Imp or Summon Voidwalker", 1, true),
    "a rune missing abilities warns, naming them, before it can be bound")
Accept()
check(lastSent() == "PICK,21,1,1", "binding it anyway says so to the server")
view.pending = nil
server("R,21,-")

-- Preview: the build as if bound, its picks tried here only; Back returns to the list as it was.
search.text = "fel"
search.scripts.OnTextChanged(search)
rows[1].preview:Click()
local build = debug.build
check(view.mode == "preview" and build.target == 1 and build.title.text == "Fel Commandment"
    and build.options[1].name.text == "Infernal Pact" and build.heading.text == "Choose your Invocation",
    "previewing shows its medallion, the first slot and its modifiers")
check(build.nodes[1].status.text == "Choosing" and build.nodes[2].lock.shown and build.nodes[3].lock.shown,
    "slot 1 is being chosen, the others locked")
local sentBefore = #sent
build.options[2]:Click()
check(build.picked.shown and build.pickedText.text == "Demonic Ascension selected" and build.options[2].check.shown,
    "a modifier clicked is selected")
check(build.right.label.text == "Confirm modifier" and build.right.enabled, "and can be confirmed")
build.right:Click()
check(#sent == sentBefore and view.picks[1] == 2 and view.active == 2 and not build.nodes[2].lock.shown,
    "confirmed in a preview: tried here, not sent; the next slot opens")
check(build.right.label.text == "Choose this rune", "with nothing pending, the preview offers to choose the rune")
build.left:Click()
check(view.mode == "browse" and not view.preview and browse.query == "fel" and rows[1].rune.id == 21,
    "Back to all runes returns to the list, its search kept")

-- Binding asks first; once the server binds it, its build.
rows[1].choose:Click()
check(ask.title.text == "Bind Fel Commandment?", "choosing asks to confirm the bind")
Accept()
check(lastSent() == "PICK,21,0,1", "accepting sends PICK,21 for the first rune")
server("N,21,1,3,686+29722")
server("S,10,10,1,21,0,0,0,21,0")
check(view.mode == "build" and build.target == 1 and view.active == 1, "bound: its build, the first slot to choose")
local options = build.options
check(options[1].name.text == "Infernal Pact" and options[1].text.text:find("twice as often")
    and options[1].ability.text == "Y O U R  D E M O N", "the modifiers in the mockup's words")
check(options[3].missing and options[3].ability.text:find("N E E D S"), "Dreadfire Discipline, missing an ability")
local before = #sent
options[3]:Click()
check(#sent == before and debug.chrome.note.text:find("needs Shadow Bolt or Incinerate"),
    "a modifier missing its ability says so and sends nothing")
options[1]:Click()
build.right:Click()
check(lastSent() == "MOD,1,1,1" and view.picks[1] == 1 and view.active == 2
    and build.nodes[1].status.text == "Infernal Pact", "Confirm modifier sends MOD,1,1; Manifestation is next")
server("S,10,10,1,21,1,0,0,21,0")
options[3]:Click()
build.right:Click()
server("S,10,10,1,21,1,3,0,21,0")
options[2]:Click()
build.right:Click()
server("S,10,10,1,21,1,3,2,21,0")
check(view.active == nil and build.heading.text == "The Legion's Master" and build.summary.shown
    and build.right.label.text == "Close", "the three picks of a doc build: complete, named")
check(build.ask.text == "Click a slot to change its modifier.", "the done build says a slot can be changed")

-- A done slot can be chosen again.
build.nodes[2]:Click()
check(view.active == 2 and options[3].check.shown and not build.right.enabled,
    "a done slot opens its modifiers again, its pick checked")
server("E,Modifiers can't be changed in combat.")
check(debug.chrome.note.text:find("in combat"), "a refusal shows in the note")

-- Reopened, bound: the build straight away.
frame:Hide()
EmberforgeRunes_Open()
check(view.mode == "build" and build.target == 1 and build.nodes[1].label.text == "I N V O C A T I O N",
    "reopened: the build, its slots named")
frame:Hide()

-- Another specialization bound to the Sanctified Paragon: its slots carry its own names.
server("R,22,-")
server("S,10,10,2,22,0,0,0,22,0")
EmberforgeRunes_Open()
check(view.mode == "build" and build.nodes[1].label.text == "P R E S E N C E"
    and build.nodes[3].label.text == "C O V E N A N T", "the Sanctified Paragon's slots: Presence, Covenant")
check(build.heading.text == "Choose your Presence" and options[1].name.text == "Boundless Sanctity",
    "choosing its Presence: Boundless Sanctity first")
frame:Hide()

-- The Stormbound Conduit.
server("R,23,-")
server("S,10,10,3,23,0,0,0,23,0")
EmberforgeRunes_Open()
check(build.nodes[2].label.text == "L I G H T N I N G  S H I E L D" and options[1].name.text == "Forked Tempest",
    "the Stormbound Conduit's slots and modifiers")
frame:Hide()
server("S,10,10,1,21,1,3,1,21,0")
check(GameTooltip:GetParent() == tooltipParent, "after all that, the game's tooltip is still where it was")

-- The glow: Throw Glaive on button 3 glows at 5 Fel Command, with the Felguard mark in Metamorphosis.
actions[3] = 90111
buffs["Fel Command"] = 4
local function fire(event, unit)
    for _, widget in ipairs(frames) do
        if widget.scripts.OnEvent then
            widget.scripts.OnEvent(widget, event, unit)
        end
    end
end
local function volleyShown(button)
    return _G[button:GetName() .. "Icon"].texture == "Interface\\Icons\\Ability_Warlock_ChaosBolt"
end
fire("UNIT_AURA", "player")
check(not volleyShown(ActionButton3), "4 stacks: no glow")
buffs["Fel Command"] = 5
fire("UNIT_AURA", "player")
check(volleyShown(ActionButton3) and not volleyShown(ActionButton4),
    "5 stacks: Throw Glaive's button (only it) shows Chaos Bolt Volley")
buffs["Fel Command"] = nil
fire("UNIT_AURA", "player")
check(not volleyShown(ActionButton3), "spent: the glow goes")

-- Consecration on button 4 shows the Sanctified Paragon's icon while that rune is bound (no glow).
local function iconShown(button, path)
    return _G[button:GetName() .. "Icon"].texture == path
end
local PARAGON_ICON = "Interface\\Icons\\Ability_Rune_SanctifiedParagon"
actions[4] = 26573
fire("UNIT_AURA", "player")
check(not iconShown(ActionButton4, PARAGON_ICON), "Consecration keeps its own icon without the rune")
server("S,10,5,1,22,0,0,0,-,0")
check(iconShown(ActionButton4, PARAGON_ICON) and not iconShown(ActionButton3, PARAGON_ICON),
    "with the Sanctified Paragon bound, Consecration's button shows the rune's icon")
server("S,10,5,1,21,1,3,1,21,0")
check(not iconShown(ActionButton4, PARAGON_ICON), "the rune gone, Consecration's own icon is back")
actions[4] = nil

-- The rune button: from the unlock level (5); unbound, its icon turns through the runes; bound, it settles.
local button = EmberforgeRunes_Button
server("S,4,5,1,0,0,0,0,-,0")
check(not button.shown, "below level 5 the rune button is hidden")
server("S,5,5,1,0,0,0,0,-,0")
check(button.shown and button.turning ~= nil and button.glow.on, "at level 5, unbound: shown, glowing, turning")
local first = button.turning.index
button.scripts.OnUpdate(button, 2.6)
button.scripts.OnUpdate(button, 0.7)
check(button.turning.index ~= first, "its icon turns to the next rune's")
server("S,10,5,1,21,1,3,1,21,0")
check(button.shown and button.turning == nil and not button.glow.on, "bound: it settles on the rune, no glow")

-- An error in the action bars' update (a stale spellbook slot as a level's spells change) never keeps the rune
-- button hidden; a level up asks for the state again, and once more 3 sec later.
local glowWas = EmberforgeRunes_UpdateGlow
EmberforgeRunes_UpdateGlow = function() error("Invalid spell slot") end
button:Hide()
server("S,11,5,1,21,1,3,1,21,0")
check(button.shown, "the rune button shows even when the bars' update fails")
EmberforgeRunes_UpdateGlow = glowWas
local before = #sent
fire("PLAYER_LEVEL_UP", "12")
check(#sent == before + 1 and lastSent() == "STATE", "a level up asks for the state")
EmberforgeRunes_Asking.scripts.OnUpdate(EmberforgeRunes_Asking, 3.1)
check(#sent == before + 2 and lastSent() == "STATE", "and once more when the level has settled")

-- A rune aura's amount (V): the server's, added to its tooltip; a stale one is replaced, others left alone.
server("V,90843,A,412")
auras[1], auras[2] = 90843, 26573
GameTooltip:SetUnitBuff("player", 1)
check(tooltipLines[#tooltipLines] == "Absorbs 412 damage.", "an absorb's tooltip shows the server's amount")
server("V,90841,D,57")
auras[3] = 90841
GameTooltip:SetUnitDebuff("target", 3)
check(tooltipLines[#tooltipLines] == "57 damage every second.", "a damage over time shows its tick")
local count = #tooltipLines
GameTooltip:SetUnitBuff("player", 2)
check(#tooltipLines == count, "another aura's tooltip is left alone")

-- The second Legendary Rune (Rune II): from level 30, its own bind and modifiers; Rune I and Rune II switch the
-- panel between them.
frame:Hide()
server("S,29,5,1,21,1,3,1,21.23,0,0,0,0,0,30")
EmberforgeRunes_Open()
local tabs = debug.chrome.runeTabs
check(view.mode == "build" and not tabs[1].shown, "below 30 there is one rune: no Rune I / Rune II")
frame:Hide()
server("S,30,5,1,21,1,3,1,21.23.24,1,0,0,0,0,30")
check(frame:IsShown() and view.slot == 2 and view.mode == "offers" and tabs[2].on and tabs[1].shown,
    "at 30 the panel opens on Rune II's suggestions")
local offered = {}
for _, card in ipairs(debug.offers.cards) do
    if card.name.shown then
        offered[#offered + 1] = card.hit.rune.id
    end
end
check(table.concat(offered, ",") == "23,24", "Rune II is not offered the rune Rune I holds")
debug.offers.cards[1].preview:Click()
debug.build.right:Click()
Accept()
check(lastSent() == "PICK,23,0,2", "binding Rune II says so to the server")
server("S,30,5,1,21,1,3,1,21.23.24,0,23,0,0,0,30")
check(view.slot == 2 and view.mode == "build", "Rune II bound: its build")
debug.build.options[2]:Click()
debug.build.right:Click()
check(lastSent() == "MOD,1,2,2", "Rune II's modifiers are its own")
check(EmberforgeRunes_Button.glow.on == false, "both runes bound: the rune button stops glowing")
tabs[1]:Click()
check(view.slot == 1 and view.mode == "build" and debug.build.title.text == "Fel Commandment", "Rune I again")
frame:Hide()
view.slot = 1

-- Retired runes (Fel Commandment, the Threefold Grave and the Unbroken Onslaught, being reworked) are not listed.
for _, rune in ipairs(retired) do
    rune.retired = true
end
server("S,10,5,1,0,0,0,0,24,0")
EmberforgeRunes_Open()
debug.offers.browse:Click()
local search = debug.search
search.text = ""
search.scripts.OnTextChanged(search)
local hidden = true
for _, rune in ipairs(debug.browse.list) do
    if rune.retired then
        hidden = false
    end
end
check(#retired == 3 and hidden and #debug.browse.list == 25 and debug.browser.count.text == "25 runes",
    "retired runes are not listed, nor counted")
frame:Hide()

-- A bound rune's line on the spells it reaches into: Flame Shock with Ember's Wrath, Frost Shock with Whitefang.
local function SpellTooltip(name)
    tooltipLines, tooltipSpell = {}, name
    GameTooltip.scripts.OnTooltipSetSpell(GameTooltip)
    return table.concat(tooltipLines, " | ")
end
server("S,40,10,1,44,1,1,1,-,0,45,1,1,1,30")
check(SpellTooltip("Flame Shock"):find("Rune of Ember's Wrath") and SpellTooltip("Flame Shock"):find("Blazing Strike"),
    "Flame Shock's tooltip tells what Ember's Wrath adds")
check(SpellTooltip("Frost Shock"):find("Rune of Whitefang") and SpellTooltip("Frost Shock"):find("Glacial Burst"),
    "Frost Shock's tooltip tells what Whitefang adds")
local claw = SpellTooltip("Claw")
check(claw:find("Blazing Strike") and claw:find("Frostfang Strike"), "Claw tells both runes' strikes")
check(SpellTooltip("Fireball") == "", "a spell no bound rune reaches into gets no line")
server("S,40,10,1,44,1,1,1,-,0,0,0,0,0,30")
check(SpellTooltip("Frost Shock") == "", "Whitefang unbound: Frost Shock gets no line")

-- Nothing usable: the panel opens on the search; a rune the slotted Skill Cards complete says so; Learn Available
-- (PLAN) on a rune whose abilities can be learned or slotted, its plan asked before anything changes (L), then APPLY.
frame:Hide()
SPELLS[2136] = "Fire Blast"
for _, rune in ipairs(EmberforgeRunesCatalog) do
    server("R," .. rune.id .. ",686,686,0")
end
server("R,22,686,-,0")
server("R,24,697,697,1")
server("S,5,5,1,0,0,0,0,-,0,0,0,0,0,30")
EmberforgeRunes_Open()
check(frame:IsShown() and view.mode == "browse", "no suggestions: the panel opens on the search")
local search = debug.search
local function RowOf(id)
    search.text = ById(id).name
    search.scripts.OnTextChanged(search)
    for _, row in ipairs(debug.rows) do
        if row.rune and row.rune.id == id then
            return row
        end
    end
end
local cardsRow = RowOf(22)
check(cardsRow and cardsRow.status.text == "Your cards bring Shadow Bolt", "a rune the slotted cards complete says so")
local learnRow = RowOf(24)
check(learnRow and learnRow.learn and learnRow.choose.label.text == "Learn Available" and learnRow.choose.enabled,
    "none usable: Learn Available where the abilities can be had")
learnRow.choose:Click()
check(lastSent() == "PLAN,24", "Learn Available asks the server for its plan")
server("L,24,686,697,2136,3:0:697")
local text = debug.ask.text.text
check(debug.ask.title.text == "Learn Available: Echoing Thunder"
    and text:find("Learn now: |cffffffffShadow Bolt", 1, true)
    and text:find("Slot its Skill Card: |cffffffffFire Blast|r (slot III)", 1, true)
    and text:find("This replaces:", 1, true) and text:find("Summon Voidwalker leaves your kit", 1, true)
    and text:find("Summon Voidwalker's Skill Card leaves slot III", 1, true),
    "the plan names what it learns, slots, and replaces")
Accept()
check(lastSent() == "APPLY,24", "proceeding applies it")
search.text = ""
search.scripts.OnTextChanged(search)
-- Bound before level 8: Unlearn; at 8 it is gone.
frame:Hide()
server("S,6,5,1,24,0,0,0,-,0,0,0,0,0,30")
EmberforgeRunes_Open()
local build = debug.build
check(build.left.shown and build.left.label.text == "Unlearn rune", "bound before level 8: Unlearn shows")
build.left:Click()
check(debug.ask.title.text == "Unlearn Echoing Thunder?", "Unlearn asks first")
Accept()
check(lastSent() == "UNBIND,1", "and unlearns it")
frame:Hide()
server("S,8,5,1,24,0,0,0,-,0,0,0,0,0,30")
EmberforgeRunes_Open()
check(not build.left.shown, "from level 8 it stays")
-- The frame's own update runs (it closes the panel when the UI comes back).
frame.scripts.OnUpdate(frame, 0.02)
check(frame:IsShown(), "the panel's update runs")
-- In combat it waits: it opens once the fight is over; a fight starting closes it.
frame:Hide()
local fighting = true
function UnitAffectingCombat() return fighting end
EmberforgeRunes_Open()
check(not frame:IsShown(), "in combat the panel waits")
fighting = false
frame.scripts.OnEvent(frame, "PLAYER_REGEN_ENABLED")
check(frame:IsShown(), "and opens when the fight is over")
fighting = true
frame.scripts.OnEvent(frame, "PLAYER_REGEN_DISABLED")
check(not frame:IsShown(), "a fight starting closes it")
fighting = false

print(failures == 0 and "All checks passed" or (failures .. " check(s) failed"))
os.exit(failures == 0 and 0 or 1)

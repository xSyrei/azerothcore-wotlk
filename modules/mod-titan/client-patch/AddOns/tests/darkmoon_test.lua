-- Offline test for Emberforge_Darkmoon: loads the addon against a mocked WoW 3.3.5 API and plays the shop and the
-- reveal the way the server drives them (see src/cards.cpp). Run: lua5.1 darkmoon_test.lua
local DIR = (arg and arg[1]) or "../Emberforge_Darkmoon/"
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
local function NewWidget(name)
    local widget = setmetatable({ shown = true, scripts = {}, text = "", width = 300, height = 20, name = name },
        Widget)
    if name then
        _G[name] = widget
    end
    return widget
end
function Widget:GetName() return self.name end
function Widget:GetFrameLevel() return 1 end
function Widget:CreateTexture() return NewWidget() end
function Widget:CreateFontString() return NewWidget() end
function Widget:SetScript(event, handler) self.scripts[event] = handler end
function Widget:HookScript(event, handler) self.scripts[event] = handler end
function Widget:Show()
    local wasShown = self.shown
    self.shown = true
    if not wasShown and self.scripts.OnShow then
        self.scripts.OnShow(self)
    end
end
function Widget:Hide() self.shown = false end
function Widget:IsShown() return self.shown end
function Widget:SetText(text) self.text = text end
function Widget:GetText() return self.text end
function Widget:SetTexture(texture) self.texture = texture end
function Widget:SetTexCoord(...) self.coords = { ... } end
function Widget:SetWidth(width) self.width = width end
function Widget:GetWidth() return self.width end
function Widget:SetHeight(height) self.height = height end
function Widget:GetHeight() return self.height end
function Widget:GetStringWidth() return #(self.text or "") * 6 end
function Widget:GetStringHeight() return 14 end
function Widget:Click() self.scripts.OnClick(self, "LeftButton") end
function Widget:SetAlpha(alpha) self.alpha = alpha end

UIParent = NewWidget("UIParent")
WorldFrame = NewWidget("WorldFrame")
local created = {}
function CreateFrame(_, name)
    local widget = NewWidget(name)
    created[#created + 1] = widget
    return widget
end
local sent = {}
UISpecialFrames, SlashCmdList = {}, {}
GameTooltip = NewWidget("GameTooltip")
ITEM_QUALITY_COLORS = setmetatable({}, { __index = function() return { hex = "|cffffffff" } end })
function SendAddonMessage(prefix, message) sent[#sent + 1] = { prefix, message } end
function UnitName() return "Tester" end
function SetPortraitToTexture() end
local items = {}
function GetItemCount(id) return items[id] or 0 end
function GetSpellInfo(id) return "Spell " .. id, "", "Interface\\Icons\\Spell_" .. id end
function GetItemIcon() return "Interface\\Icons\\INV_Misc_QuestionMark" end
function GetCursorPosition() return 0, 0 end
function wipe(t) for k in pairs(t) do t[k] = nil end return t end
tinsert = table.insert
function strsplit(sep, text, limit)
    local parts, start = {}, 1
    while true do
        if limit and #parts == limit - 1 then
            parts[#parts + 1] = text:sub(start)
            break
        end
        local i = text:find(sep, start, true)
        if not i then
            parts[#parts + 1] = text:sub(start)
            break
        end
        parts[#parts + 1] = text:sub(start, i - 1)
        start = i + 1
    end
    return unpack(parts)
end

StaticPopupDialogs = {}
local popup
function StaticPopup_Show(name, a, b) popup = { name = name, a = a, b = b } end
function StaticPopup_Hide() popup = nil end
ACCEPT, CANCEL = "Accept", "Cancel"
local xp, xpMax = 0, 400
function UnitXP() return xp end
function UnitXPMax() return xpMax end
function Widget:Enable() self.enabled = true end
function Widget:Disable() self.enabled = false end
function Widget:GetFrameLevel() return 1 end

StaticPopupDialogs = {}
local popup
function StaticPopup_Hide() popup = nil end
ACCEPT, CANCEL = "Accept", "Cancel"
function Widget:Enable() self.enabled = true end
function Widget:Disable() self.enabled = false end
function UnitXP() return 0 end
function UnitXPMax() return 1 end
function GetMoney() return 1000000 end
function PlaySound() end
function Widget:SetDesaturated() end
function Widget:EnableMouseWheel() end
function Widget:SetAutoFocus() end
function Widget:ClearFocus() end
function Widget:SetFrameLevel() end
local dialog = {}
function StaticPopup_Show(name, a, b) popup = { name = name, a = a, b = b }; return dialog end

dofile(DIR .. "Emberforge_Darkmoon.lua")
local frame = EmberforgeDarkmoon
local t = EmberforgeDarkmoon_State
local events
for _, widget in ipairs(created) do
    if widget.scripts.OnEvent then
        events = widget
    end
end
local function server(message)
    events.scripts.OnEvent(events, "CHAT_MSG_ADDON", "EFDM", message, "WHISPER", "Tester")
end
local function lastSent()
    return sent[#sent] and sent[#sent][2]
end

server("S,1500,1000,1250,20000,5,750000,13,10,50,500,5000,12000,30000")
server("E,3,21,24,100,2,0,0,4,2,8,60,4,8.10.20.30.40.50.60")
server("O")
check(frame:IsShown(), "O opens the window")
check(lastSent() == "LIST", "the first open asks for the catalog")
check(t.pages[1]:IsShown() and not t.pages[2]:IsShown(), "Specializations is the first page")
check(t.state.spark == 1500 and t.state.prices[0] == 1000 and t.state.prices[1] == 1250
    and t.state.prices[2] == 20000 and t.state.prices[3] == 20000, "the pack prices are read")
check(t.state.duplicates == 13 and t.state.goldLeft == 5 and t.state.cardPrices[3] == 30000, "the tallies are read")
check(t.eyes.count == 3 and t.eyes.claimed == 21 and t.eyes.level == 24 and not t.eyes.canBuy, "the Eyes are read")
check(t.eyes.ready == 3, "levels 22 to 24 are ready to claim")
check(t.chip:IsShown(), "the claim chip shows over the unclaimed levels")
check(t.claimButton.enabled and not t.buyEyes.enabled, "claiming is open, buying is not below 60")
t.claimButton:Click()
check(lastSent() == "CLAIM", "Claim asks the server")
server("E,6,24,24,100,2,0,0,4,2,8,60,4,8.10.20.30.40.50.60")
check(t.eyes.ready == 0 and not t.chip:IsShown() and not t.claimButton.enabled, "claimed: nothing left to claim")
server("E,6,24,60,100,2,1,0,4,2,8,60,4,8.10.20.30.40.50.60")
check(t.eyes.ready == 36 + 4 * 3, "at 60: every level from 25 owed, milestones four each")
check(t.buyEyes.enabled, "at 60 the Eyes can be bought")
t.buyEyes:Click()
check(lastSent() == "EYES", "Buy asks the server")

t.tabs[2]:Click()
check(t.pages[2]:IsShown(), "the Packs tab")
items[910010], items[910013] = 3, 1
events.scripts.OnEvent(events, "BAG_UPDATE")
check(t.packWidgets[0].open.enabled and t.packWidgets[0].all.enabled, "three Skill Packs: Open and Open all")
check(t.packWidgets[3].open.enabled and not t.packWidgets[3].all.enabled, "one Golden Talent Pack: Open only")
check(not t.packWidgets[1].open.enabled, "no Talent Packs: nothing to open")
t.packWidgets[0].all:Click()
check(lastSent() == "OPEN,0,3", "Open all opens every pack in the bags")
server("S,25000,1000,1250,20000,5,750000,13,10,50,500,5000,12000,30000")
t.packWidgets[2].buy:Click()
check(lastSent() == "BUY,2,0", "a Golden Skill Pack bought for Emberspark")

server("K0")
server("K,0.133.133.0.1.1.0;0.27243.27243.3.0.1.0;0.5.5.1.0.1.0")
server("K,1.2300.90200.1.2.5.1")
server("KE")
check(#t.catalog == 4, "the catalog arrives in parts")
server("U,0.27243.1.0")
local seed
for _, card in ipairs(t.catalog) do
    if card.id == 27243 then seed = card end
end
check(seed and seed.owned == 1, "an update marks a card owned")

server("R,0,0.133.133.0.0.40;0.27243.27243.3.1.0;0.5.5.1.1.0")
check(t.reveal.phase == "fly" and #t.reveal.cards == 3, "a pack opened flies in from its shelf")
local seen = {}
for _ = 1, 120 do
    t.stage.scripts.OnUpdate(t.stage, 0.05)
    seen[t.reveal.phase] = true
end
check(seen.charge and seen.rip and seen.burst and seen.turn, "it charges, tears, bursts, and the cards turn")
check(t.reveal.phase == "shown", "the cards deal out and turn over")
local cards = t.revealCards
check(cards[1].badge.text.text == "DUPLICATE" and cards[1].alpha == 0.6 and cards[2].badge.text.text == "NEW"
    and cards[2].alpha == 1, "a duplicate is tagged DUPLICATE and faded, a new card tagged NEW")
server("R,1,1.2300.90200.1.1.0")
check(#t.reveal.queue == 1 and t.reveal.phase == "dissolve", "a second pack: the cards on show burn away first")
t.stage.scripts.OnMouseDown(t.stage)
check(t.reveal.phase == "dissolve", "a click while they burn changes nothing")
for _ = 1, 40 do
    t.stage.scripts.OnUpdate(t.stage, 0.05)
end
check(t.reveal.phase ~= "idle" and #t.reveal.cards == 1 and #t.reveal.queue == 0, "then the waiting pack opens")
for _ = 1, 140 do
    t.stage.scripts.OnUpdate(t.stage, 0.05)
end
check(t.reveal.phase == "idle", "left alone, the cards burn away by themselves")
server("R,1,1.2300.90200.1.1.0")
t.stage.scripts.OnMouseDown(t.stage)
check(t.reveal.phase == "shown", "a click while it plays skips to the faces")

server("M,0,2")
server("R,0,0.133.133.0.0.40;0.27243.27243.3.1.0")
server("R,0,0.5.5.1.1.0;0.6.6.0.0.40")
server("ME")
check(t.reveal.phase == "mfly" and not t.results:IsShown(), "Open all: the stack flies in first")
seen = {}
for _ = 1, 100 do
    t.stage.scripts.OnUpdate(t.stage, 0.05)
    seen[t.reveal.phase] = true
end
check(seen.mcharge and seen.mrip and seen.mburst, "the vortex charges, the pack tears, the cards erupt")
check(t.results:IsShown(), "then the list of the new cards")
check(#t.mass().cards == 4, "every card of the packs is in the list")
local items = t.resultItems
check(items[1].badge.text.text == "NEW" and items[2].badge.text.text == "NEW" and items[3].badge.text.text
    == "DUPLICATE" and items[4]:IsShown() and items[4].alpha == 0.6,
    "the list: the new cards first, then the duplicates, faded")

t.tabs[3]:Click()
check(t.pages[3]:IsShown() and t.cells[1]:IsShown() and t.cells[1].entry.rarity == 3, "the Cards grid, rarest first")
t.cells[2]:Click()
check(t.modal:IsShown(), "a card not owned opens the buy dialog")

-- Card slots: Edit Cards, the two lists, one search over both, Golden, slotting and emptying, the levels the cards
-- come at, and the lock at level 8.
t.modal:Hide()
local function specs(message)
    events.scripts.OnEvent(events, "CHAT_MSG_ADDON", "EFSP", message, "WHISPER", "Tester")
end
local function sentSpec()
    for index = #sent, 1, -1 do
        if sent[index][1] == "EFSP" then
            return sent[index][2]
        end
    end
end
-- The catalog again, with unlock levels (a Skill Card's 9 at the earliest): Skill Cards at 9, 9, 20 and 30, a
-- Talent Card at 15.
server("K0")
server("K,0.133.133.0.1.1.0.9.4.4;0.27243.27243.3.1.1.0.9.2.32;0.5.5.1.1.1.0.20.0.1;0.6.6.0.0.1.0.30.2.1")
server("K,1.2300.90200.1.1.1.1.15")
server("KE")
specs("P,0,3,1")
specs("S,0,1,0,1,0,,Specialization I")
specs("C,0,0,0.0.0.27243;0.1.1.2300")
specs("S,1,1,0,0,0,,Specialization II")
specs("C,1,0,")
specs("PE")
t.tabs[1]:Click()
check(t.specSlots[1].icon:IsShown() and t.specSlots[1].icon.texture == "Interface\\Icons\\Spell_27243"
    and t.specSlots[1].golden:IsShown() and not t.specSlots[2].icon:IsShown(),
    "the spec's slots show its card and its Golden Card")
check(t.editCards.enabled and t.editCards.label.text == "Edit Cards", "Edit Cards, where the coming-soon line was")
t.editCards:Click()
local e = t.editor
check(t.pages[4]:IsShown() and not t.pages[1]:IsShown() and t.tabs[1].on, "Edit Cards opens the editor, under "
    .. "Specializations")
check(e.state.slot == 2, "the first empty slot is chosen")
local function rows(kind)
    local names = {}
    for _, row in ipairs(e.lists[kind].rows) do
        if row:IsShown() then
            names[#names + 1] = row.name.text
        end
    end
    return names
end
local skills, talents = rows(0), rows(1)
check(#skills == 3 and skills[1] == "Spell 27243" and #talents == 1 and talents[1] == "Spell 90200",
    "abilities and talents in their own lists: only the cards owned, rarest first")
check(e.lists[0].rows[1].note.text == "Slot I  -  Lv 9" and e.lists[0].rows[1].accent:IsShown(),
    "a slotted card says its slot and the level it comes at")
check(e.lists[0].rows[3].note.text == "Lv 9" and not e.lists[0].rows[3].accent:IsShown(),
    "the others their unlock level")
check(e.lists[0].rows[1].gems[4]:IsShown() and not e.lists[0].rows[3].gems[2]:IsShown(),
    "a gem for each step of rarity")
e.sort.level:Click()
check(e.state.sort == "level" and e.lists[0].rows[3].name.text == "Spell 5", "sorted by unlock level")
e.sort.rarity:Click()
e.search:SetText("133")
e.search.scripts.OnTextChanged(e.search)
check(#rows(0) == 1 and #rows(1) == 0 and e.lists[1].empty:IsShown(), "one search bar filters both lists")
-- Keywords: every word in the card's name or tooltip.
local descriptions = { [133] = "Hurls a fiery ball that causes 14 Fire damage.",
    [27243] = "Increases your spell haste by 5% and your Shadow damage by 10%.", [5] = "Stuns the target.",
    [90200] = "Increases your melee haste by 3%." }
wipe(t.keywords.cache)
t.keywords.describe = function(spell) return descriptions[spell] or "" end
local function find(query)
    e.search:SetText(query)
    e.search.scripts.OnTextChanged(e.search)
    return rows(0), rows(1)
end
-- (The test's cards are all named "Spell <id>": no query here uses the word.)
local found, foundTalents = find("% shadow")
check(#found == 1 and found[1] == "Spell 27243" and #foundTalents == 0, "a search finds words in the card's text")
found, foundTalents = find("haste")
check(#found == 1 and #foundTalents == 1, "one search over both lists")
found = find("fire damage")
check(#found == 1 and found[1] == "Spell 133", "the words need not be side by side")
found = find('"shadow damage"')
check(#found == 1 and found[1] == "Spell 27243" and #find('"fire ball"') == 0, "a quoted phrase is found exactly")
check(e.lists[0].count.text == "0 of 3" and e.lists[0].empty:IsShown(), "nothing found: the count and a line say so")
find("")
-- Filters: Role and School for Skill Cards, Rarity for both.
e.filters.role:Click()
check(e.menu:IsShown(), "a filter opens its menu")
local function pick(label)
    for _, item in ipairs(e.menu.items) do
        if item:IsShown() and item.label.text == label then
            item:Click()
            return
        end
    end
end
pick("Melee")
check(#rows(0) == 1 and rows(0)[1] == "Spell 27243" and #rows(1) == 1, "Role: the Skill Cards tagged for it")
check(e.filters.role.label.text == "Role:  Melee" and e.filters.role.on and e.clear.enabled,
    "the filter says what it is set to")
e.filters.role:Click()
pick("Utility")
check(#rows(0) == 1 and rows(0)[1] == "Spell 5", "Utility: the cards for every role")
e.clear:Click()
e.filters.school:Click()
pick("Fire")
check(#rows(0) == 1 and rows(0)[1] == "Spell 133", "School: the Skill Cards of it")
e.clear:Click()
e.filters.rarity:Click()
pick("Rare")
check(#rows(0) == 1 and rows(0)[1] == "Spell 5" and #rows(1) == 1, "Rarity: both lists")
e.clear:Click()
check(#rows(0) == 3 and not e.clear.enabled, "Clear shows everything again")
e.lists[0].rows[3]:Click()
check(sentSpec() == "SLOT,0,1,0,0,133", "a click slots the card in the chosen slot")
check(e.state.slot == 3, "then the next empty slot is chosen")
local at = t.slotting.Schedule(1)
check(at["0.27243"] == 9 and at["0.133"] == 11 and at["1.2300"] == 16,
    "two abilities unlocked at 9 come at 9 and 11; the talent on the first talent roll from 15")
e.lists[0].rows[2]:Click()
at = t.slotting.Schedule(1)
check(sentSpec() == "SLOT,0,2,0,0,5" and at["0.5"] == 21, "one unlocked at 20 comes on the ability roll at 21")
check(e.slots[1].when.text == "LV 9" and e.slots[2].when.text == "LV 11" and e.slots[1].goldenWhen.text == "LV 16"
    and e.slots[3].when.text == "LV 21" and e.slots[4].empty.text == "Empty",
    "each slot's small cards show the level they come at")
local before = #sent
e.slots[2]:Click()
check(#sent == before and t.specCards[1][2][0] == "0.133" and e.state.slot == 2, "a click on a slot chooses it")
e.slots[2].scripts.OnClick(e.slots[2], "RightButton")
check(sentSpec() == "SLOT,0,1,0,0,0" and t.specCards[1][2][0] == nil, "a right-click empties it")
e.tabs[1]:Click()
check(e.state.golden and #rows(0) == 0 and #rows(1) == 1, "Golden: the Golden Cards owned")
e.slots[1].scripts.OnClick(e.slots[1], "RightButton")
check(sentSpec() == "SLOT,0,0,1,0,0", "on Golden a right-click empties the slot's Golden Card")
e.lists[1].rows[1]:Click()
check(sentSpec() == "SLOT,0,0,1,1,2300", "a Golden Card goes in the slot's Golden place")

-- Level 8: the server locks them.
specs("P,0,3,1")
specs("C,0,1,1.0.0.133")
specs("C,1,0,")
specs("PE")
check(t.cardsLocked[1] and t.specCards[1][2][0] == "0.133" and t.specCards[1][1] == nil,
    "the server's slots replace the window's, locked")
before = #sent
e.tabs[0]:Click()
e.slots[2].scripts.OnClick(e.slots[2], "RightButton")
e.lists[0].rows[1]:Click()
check(#sent == before and t.specCards[1][2][0] == "0.133", "locked: clicks change nothing")
check(e.lists[0].rows[1].alpha == 0.5, "the cards not slotted are dimmed")
t.tabs[1]:Click()
check(t.pages[1]:IsShown() and not t.pages[4]:IsShown(), "Specializations again")
check(t.editCards.label.text == "View Cards", "a locked spec's button views its cards")

print(failures == 0 and "all passed" or (failures .. " failed"))
os.exit(failures == 0 and 0 or 1)

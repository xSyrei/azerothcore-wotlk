-- Emberforge Classless: every class's abilities and talent trees by spec, the Hero's own abilities
-- and talents, choosing the starting kit after a Titan Tome of Selection, and Wildcard roll reveals.
-- Open with N, the minimap button, /classless or /cl.
--
-- The server (mod-titan, src/classless.cpp) sends the data as addon whispers to the player, prefix
-- EFCL, and the window answers the same way:
--   to server     LIST, PICK,<spell id>, KEEP,<spell id>, REROLL,<spell id>
--   from server   B / T,<class>,<skill line>,<name> / A,<class>.<skill line>.<level>.<spell id>;...
--                 U,<class>,<tree order>,<tab id> / N,<tab>.<talent>.<row>.<col>.<rank1>+<rank2>...;... / E
--                 K0 / K,<spell id>,...   R0 / R,<talent id>:<rank>,...   S,<picks left>,<locked>,<lock level>
--                 G,<spell id>,<first rank id>,<can reroll>   O

local PREFIX = "EFCL"
local WIDTH, HEIGHT = 960, 620
local CELL_WIDTH, CELL_HEIGHT, COLUMNS, ROWS = 200, 44, 2, 9
local LIST_ROW_HEIGHT, LIST_ROWS = 26, 15
local TALENT_ROWS, TALENT_COLUMNS, TALENT_SIZE, TALENT_STEP_X, TALENT_STEP_Y = 11, 4, 30, 46, 40
local TREE_WIDTH, TREE_HEIGHT = 212, 452
local EMBER = "|cffff8a1e"
local YOUR_ABILITIES = 0

-- Server class ids, alphabetical like the class row in game. Names are spelled out because the
-- client's own class list calls the Paladin "Hero" on Emberforge.
local CLASSES = {
    { id = 6, token = "DEATHKNIGHT", name = "Death Knight" },
    { id = 11, token = "DRUID", name = "Druid" },
    { id = 3, token = "HUNTER", name = "Hunter" },
    { id = 8, token = "MAGE", name = "Mage" },
    { id = 2, token = "PALADIN", name = "Paladin" },
    { id = 5, token = "PRIEST", name = "Priest" },
    { id = 4, token = "ROGUE", name = "Rogue" },
    { id = 7, token = "SHAMAN", name = "Shaman" },
    { id = 9, token = "WARLOCK", name = "Warlock" },
    { id = 1, token = "WARRIOR", name = "Warrior" },
}
local CLASS_NAMES = {}
for _, class in ipairs(CLASSES) do
    CLASS_NAMES[class.id] = class.name
end

-- Each class's talent trees in the client's order: name and Interface\TalentFrame background.
local TREES = {
    [1] = { { "Arms", "WarriorArms" }, { "Fury", "WarriorFury" }, { "Protection", "WarriorProtection" } },
    [2] = { { "Holy", "PaladinHoly" }, { "Protection", "PaladinProtection" }, { "Retribution", "PaladinCombat" } },
    [3] = { { "Beast Mastery", "HunterBeastMastery" }, { "Marksmanship", "HunterMarksmanship" },
        { "Survival", "HunterSurvival" } },
    [4] = { { "Assassination", "RogueAssassination" }, { "Combat", "RogueCombat" }, { "Subtlety", "RogueSubtlety" } },
    [5] = { { "Discipline", "PriestDiscipline" }, { "Holy", "PriestHoly" }, { "Shadow", "PriestShadow" } },
    [6] = { { "Blood", "DeathKnightBlood" }, { "Frost", "DeathKnightFrost" }, { "Unholy", "DeathKnightUnholy" } },
    [7] = { { "Elemental", "ShamanElementalCombat" }, { "Enhancement", "ShamanEnhancement" },
        { "Restoration", "ShamanRestoration" } },
    [8] = { { "Arcane", "MageArcane" }, { "Fire", "MageFire" }, { "Frost", "MageFrost" } },
    [9] = { { "Affliction", "WarlockCurses" }, { "Demonology", "WarlockSummoning" },
        { "Destruction", "WarlockDestruction" } },
    [11] = { { "Balance", "DruidBalance" }, { "Feral Combat", "DruidFeralCombat" },
             { "Restoration", "DruidRestoration" } },
}
-- Skill lines whose name differs from their talent tree.
local SKILL_TO_TREE = { ["Elemental Combat"] = "Elemental", ["Shadow Magic"] = "Shadow" }

local data = {
    abilities = {},     -- spell id -> { id, class, skill, level }
    skillNames = {},    -- skill line -> name
    trees = {},         -- class -> order (1-3) -> tab id
    talents = {},       -- tab id -> { { id, row, col, ranks = { spell ids } }, ... }
    talentRanks = {},   -- talent id -> owned rank
    known = {},         -- spell id -> true
    picks = 0,
    locked = false,
    lockLevel = 9,
    loaded = false,
    loading = false,
}
local view = { class = nil, spec = nil, tree = 1, search = "", learnable = false }
local reveals = {}      -- queued roll reveals: { spell, firstRank, canReroll }
local frame, Refresh, ShowNextReveal

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function SpellName(id)
    return GetSpellInfo(id) or ("Spell " .. id)
end

local function SpellIcon(id)
    local _, _, icon = GetSpellInfo(id)
    return icon or "Interface\\Icons\\INV_Misc_QuestionMark"
end

local function Picking()
    return data.picks > 0 and not data.locked
end

local function CanPick(ability)
    return Picking() and not data.known[ability.id] and ability.level <= UnitLevel("player")
end

-- The talent tree an ability's skill line belongs to, by name ("Fire", "Feral Combat", ...).
local function TreeName(ability)
    local name = data.skillNames[ability.skill]
    return name and (SKILL_TO_TREE[name] or name)
end

local function ByLevelThenName(a, b)
    if a.level ~= b.level then
        return a.level < b.level
    end
    return SpellName(a.id) < SpellName(b.id)
end

-- What the ability grid shows for the current view, search and filter.
local function GridAbilities()
    local list, search, level = {}, view.search:lower(), UnitLevel("player")
    local treeName = view.spec and TREES[view.class] and TREES[view.class][view.spec][1]
    for id, ability in pairs(data.abilities) do
        local show
        if search ~= "" then
            show = SpellName(id):lower():find(search, 1, true) ~= nil
        elseif view.class == YOUR_ABILITIES then
            show = data.known[id]
        else
            show = ability.class == view.class and (not treeName or TreeName(ability) == treeName)
        end
        if show and view.learnable then
            show = not data.known[id] and ability.level <= level
        end
        if show then
            list[#list + 1] = ability
        end
    end
    table.sort(list, ByLevelThenName)
    return list
end

local function KnownAbilities()
    local list = {}
    for id in pairs(data.known) do
        if data.abilities[id] then
            list[#list + 1] = data.abilities[id]
        end
    end
    table.sort(list, function(a, b) return SpellName(a.id) < SpellName(b.id) end)
    return list
end

local function OwnedTalents()
    local list = {}
    for _, talents in pairs(data.talents) do
        for _, talent in ipairs(talents) do
            if (data.talentRanks[talent.id] or 0) > 0 then
                list[#list + 1] = talent
            end
        end
    end
    table.sort(list, function(a, b) return SpellName(a.ranks[1]) < SpellName(b.ranks[1]) end)
    return list
end

-- Messages from the server --------------------------------------------------------------------------

local function OnServerMessage(message)
    local kind = message:sub(1, 1)
    if message == "B" then
        wipe(data.abilities)
        wipe(data.skillNames)
        wipe(data.trees)
        wipe(data.talents)
    elseif kind == "T" then
        local _, _, skill, name = strsplit(",", message, 4)
        data.skillNames[tonumber(skill)] = name
    elseif kind == "A" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local class, skill, level, id = strsplit(".", entry)
            id = tonumber(id)
            data.abilities[id] = { id = id, class = tonumber(class), skill = tonumber(skill), level = tonumber(level) }
        end
    elseif kind == "U" then
        local _, class, order, tab = strsplit(",", message)
        class = tonumber(class)
        data.trees[class] = data.trees[class] or {}
        data.trees[class][tonumber(order) + 1] = tonumber(tab)
    elseif kind == "N" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local tab, id, row, col, ranks = strsplit(".", entry)
            local talent = { id = tonumber(id), row = tonumber(row), col = tonumber(col), ranks = {} }
            for rank in ranks:gmatch("%d+") do
                talent.ranks[#talent.ranks + 1] = tonumber(rank)
            end
            tab = tonumber(tab)
            data.talents[tab] = data.talents[tab] or {}
            tinsert(data.talents[tab], talent)
        end
    elseif message == "E" then
        data.loaded, data.loading = true, false
        Refresh()
    elseif message == "K0" then
        wipe(data.known)
    elseif kind == "K" then
        for id in message:sub(3):gmatch("%d+") do
            data.known[tonumber(id)] = true
        end
        Refresh()
    elseif message == "R0" then
        wipe(data.talentRanks)
        Refresh()
    elseif kind == "R" then
        for id, rank in message:sub(3):gmatch("(%d+):(%d+)") do
            data.talentRanks[tonumber(id)] = tonumber(rank)
        end
        Refresh()
    elseif kind == "S" then
        local _, picks, locked, lockLevel = strsplit(",", message)
        data.picks, data.locked, data.lockLevel = tonumber(picks) or 0, locked == "1", tonumber(lockLevel) or 9
        Refresh()
    elseif kind == "G" then
        local _, spell, firstRank, canReroll = strsplit(",", message)
        tinsert(reveals, { spell = tonumber(spell), firstRank = tonumber(firstRank), canReroll = canReroll == "1" })
        ShowNextReveal()
    elseif message == "O" then
        frame:Show()
    end
end

-- Window --------------------------------------------------------------------------------------------

local function Backdrop(target)
    target:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true, tileSize = 32, edgeSize = 32,
        insets = { left = 11, right = 12, top = 12, bottom = 11 },
    })
    target:SetBackdropColor(0.03, 0.03, 0.05, 0.96)
end

frame = CreateFrame("Frame", "EmberforgeClassless", UIParent)
frame:SetWidth(WIDTH)
frame:SetHeight(HEIGHT)
frame:SetPoint("CENTER")
frame:SetFrameStrata("HIGH")
frame:SetToplevel(true)
frame:EnableMouse(true)
frame:SetMovable(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
Backdrop(frame)
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeClassless")     -- Escape closes it

local title = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
title:SetPoint("TOP", 0, -16)
title:SetText(EMBER .. "Classless|r")

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -6, -6)

local status = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
status:SetPoint("BOTTOM", 60, 22)

local function ResetScrolls()
    FauxScrollFrame_SetOffset(EmberforgeClasslessGridScroll, 0)
end

-- Class row: your abilities, then the ten classes.
local classButtons = {}
local function ClassButton(index, id, icon, coords, label)
    local button = CreateFrame("Button", nil, frame)
    button:SetWidth(42)
    button:SetHeight(42)
    button:SetPoint("TOPLEFT", 78 + (index - 1) * 76, -44)
    button.id = id
    local texture = button:CreateTexture(nil, "ARTWORK")
    texture:SetAllPoints()
    texture:SetTexture(icon)
    if coords then
        texture:SetTexCoord(unpack(coords))
    end
    button:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight", "ADD")
    local glow = button:CreateTexture(nil, "OVERLAY")
    glow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    glow:SetBlendMode("ADD")
    glow:SetVertexColor(1, 0.55, 0.12)
    glow:SetWidth(76)
    glow:SetHeight(76)
    glow:SetPoint("CENTER")
    button.glow = glow
    local text = button:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    text:SetPoint("TOP", button, "BOTTOM", 0, -3)
    text:SetText(label)
    button.label = text
    button:SetScript("OnClick", function(self)
        view.class, view.spec, view.tree = self.id, nil, 1
        ResetScrolls()
        Refresh()
    end)
    classButtons[#classButtons + 1] = button
end

ClassButton(1, YOUR_ABILITIES, "Interface\\Icons\\INV_Misc_Book_11", nil, "Your Build")
for index, class in ipairs(CLASSES) do
    ClassButton(index + 1, class.id, "Interface\\TargetingFrame\\UI-Classes-Circles",
                CLASS_ICON_TCOORDS[class.token], class.name)
end

-- Spec buttons: All, then the class's three trees (abilities and talent tree follow the spec).
local specButtons = {}
for index = 1, 4 do
    local button = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    button:SetWidth(100)
    button:SetHeight(22)
    button:SetPoint("TOPLEFT", 24 + (index - 1) * 104, -114)
    button.spec = index - 1                         -- 0: All
    button:SetScript("OnClick", function(self)
        view.spec = self.spec > 0 and self.spec or nil
        if view.spec then
            view.tree = view.spec
        end
        ResetScrolls()
        Refresh()
    end)
    specButtons[index] = button
end

-- Search every class's abilities.
local search = CreateFrame("EditBox", "EmberforgeClasslessSearch", frame, "InputBoxTemplate")
search:SetWidth(196)
search:SetHeight(20)
search:SetPoint("TOPLEFT", 704, -114)
search:SetAutoFocus(false)
search:SetScript("OnTextChanged", function(self)
    view.search = self:GetText() or ""
    ResetScrolls()
    Refresh()
end)
search:SetScript("OnEscapePressed", search.ClearFocus)
local searchHint = frame:CreateFontString(nil, "ARTWORK", "GameFontDisableSmall")
searchHint:SetPoint("BOTTOMLEFT", search, "TOPLEFT", -4, 2)
searchHint:SetText("Search all abilities")

-- Ability grid.
local cells = {}
local function OnCellEnter(self)
    if not self.ability then
        return
    end
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("spell:" .. self.ability.id)
    GameTooltip:AddLine((CLASS_NAMES[self.ability.class] or "") .. " - level " .. self.ability.level, 0.7, 0.7, 0.7)
    if CanPick(self.ability) then
        GameTooltip:AddLine("Click to choose this ability.", 0.3, 1, 0.3)
    end
    GameTooltip:Show()
end

local function OnCellClick(self)
    local ability = self.ability
    if not ability then
        return
    end
    if IsModifiedClick("CHATLINK") then
        ChatEdit_InsertLink(GetSpellLink(ability.id))
    elseif CanPick(ability) then
        StaticPopup_Show("EMBERFORGE_CLASSLESS_PICK", SpellName(ability.id), nil, ability.id)
    end
end

for index = 1, COLUMNS * ROWS do
    local cell = CreateFrame("Button", nil, frame)
    cell:SetWidth(CELL_WIDTH)
    cell:SetHeight(CELL_HEIGHT - 4)
    local column, row = (index - 1) % COLUMNS, math.floor((index - 1) / COLUMNS)
    cell:SetPoint("TOPLEFT", 24 + column * (CELL_WIDTH + 4), -148 - row * CELL_HEIGHT)
    cell:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    cell.icon = cell:CreateTexture(nil, "ARTWORK")
    cell.icon:SetWidth(36)
    cell.icon:SetHeight(36)
    cell.icon:SetPoint("LEFT", 2, 0)
    cell.name = cell:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    cell.name:SetPoint("TOPLEFT", cell.icon, "TOPRIGHT", 6, -2)
    cell.name:SetWidth(CELL_WIDTH - 66)
    cell.name:SetJustifyH("LEFT")
    cell.sub = cell:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    cell.sub:SetPoint("BOTTOMLEFT", cell.icon, "BOTTOMRIGHT", 6, 2)
    cell.check = cell:CreateTexture(nil, "OVERLAY")
    cell.check:SetTexture("Interface\\Buttons\\UI-CheckBox-Check")
    cell.check:SetWidth(22)
    cell.check:SetHeight(22)
    cell.check:SetPoint("RIGHT", -4, 0)
    cell:SetScript("OnEnter", OnCellEnter)
    cell:SetScript("OnLeave", function() GameTooltip:Hide() end)
    cell:SetScript("OnClick", OnCellClick)
    cells[index] = cell
end

local gridScroll = CreateFrame("ScrollFrame", "EmberforgeClasslessGridScroll", frame, "FauxScrollFrameTemplate")
gridScroll:SetPoint("TOPLEFT", 24, -148)
gridScroll:SetWidth(COLUMNS * (CELL_WIDTH + 4))
gridScroll:SetHeight(ROWS * CELL_HEIGHT)
gridScroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, CELL_HEIGHT, Refresh)
end)

local empty = frame:CreateFontString(nil, "ARTWORK", "GameFontDisable")
empty:SetPoint("TOPLEFT", 30, -160)

local learnable = CreateFrame("CheckButton", "EmberforgeClasslessLearnable", frame, "UICheckButtonTemplate")
learnable:SetWidth(24)
learnable:SetHeight(24)
learnable:SetPoint("BOTTOMLEFT", 22, 16)
EmberforgeClasslessLearnableText:SetText("Learnable now only")
learnable:SetScript("OnClick", function(self)
    view.learnable = self:GetChecked() and true or false
    ResetScrolls()
    Refresh()
end)

-- Talent tree panel: the selected spec's tree on its Blizzard background, or your talents.
local tree = CreateFrame("Frame", nil, frame)
tree:SetWidth(TREE_WIDTH)
tree:SetHeight(TREE_HEIGHT)
tree:SetPoint("TOPLEFT", 452, -148)
local backgrounds = {}
for index, part in ipairs({ { "TopLeft", 0.8, 2 / 3 }, { "TopRight", 0.2, 2 / 3 }, { "BottomLeft", 0.8, 1 / 3 },
    { "BottomRight", 0.2, 1 / 3 } }) do
    local texture = tree:CreateTexture(nil, "BACKGROUND")
    texture:SetWidth(TREE_WIDTH * part[2])
    texture:SetHeight(TREE_HEIGHT * part[3])
    texture.part = part[1]
    backgrounds[index] = texture
end
backgrounds[1]:SetPoint("TOPLEFT")
backgrounds[2]:SetPoint("TOPLEFT", backgrounds[1], "TOPRIGHT")
backgrounds[3]:SetPoint("TOPLEFT", backgrounds[1], "BOTTOMLEFT")
backgrounds[4]:SetPoint("TOPLEFT", backgrounds[1], "BOTTOMRIGHT")
local treeTitle = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
treeTitle:SetPoint("BOTTOM", tree, "TOP", 0, 4)

local talentButtons = {}
local function OnTalentEnter(self)
    local talent = self.talent
    if not talent then
        return
    end
    local rank = data.talentRanks[talent.id] or 0
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("spell:" .. talent.ranks[math.max(rank, 1)])
    GameTooltip:AddLine("Rank " .. rank .. "/" .. #talent.ranks, 1, 0.82, 0)
    if rank < #talent.ranks then
        GameTooltip:AddLine("Wildcard grants talents at random from level 10.", 0.6, 0.6, 0.6)
    end
    GameTooltip:Show()
end

for row = 1, TALENT_ROWS do
    for column = 1, TALENT_COLUMNS do
        local button = CreateFrame("Button", nil, tree)
        button:SetWidth(TALENT_SIZE)
        button:SetHeight(TALENT_SIZE)
        button:SetPoint("TOPLEFT", 18 + (column - 1) * TALENT_STEP_X, -12 - (row - 1) * TALENT_STEP_Y)
        button.icon = button:CreateTexture(nil, "ARTWORK")
        button.icon:SetAllPoints()
        button.border = button:CreateTexture(nil, "OVERLAY")
        button.border:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        button.border:SetBlendMode("ADD")
        button.border:SetWidth(TALENT_SIZE * 1.9)
        button.border:SetHeight(TALENT_SIZE * 1.9)
        button.border:SetPoint("CENTER")
        button.rank = button:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        button.rank:SetPoint("BOTTOMRIGHT", 6, -4)
        button:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
        button:SetScript("OnEnter", OnTalentEnter)
        button:SetScript("OnLeave", function() GameTooltip:Hide() end)
        button:SetScript("OnClick", function(self)
            if self.talent and IsModifiedClick("CHATLINK") then
                ChatEdit_InsertLink(GetSpellLink(self.talent.ranks[1]))
            end
        end)
        talentButtons[(row - 1) * TALENT_COLUMNS + column] = button
    end
end

-- Your talents (Your Build view), listed in the tree panel.
local talentRows = {}
for index = 1, 15 do
    local row = CreateFrame("Button", nil, tree)
    row:SetWidth(TREE_WIDTH - 8)
    row:SetHeight(26)
    row:SetPoint("TOPLEFT", 4, -4 - (index - 1) * 29)
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(22)
    row.icon:SetHeight(22)
    row.icon:SetPoint("LEFT", 2, 0)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 6, 0)
    row.name:SetWidth(TREE_WIDTH - 44)
    row.name:SetJustifyH("LEFT")
    row:SetScript("OnEnter", OnTalentEnter)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    talentRows[index] = row
end

-- Your abilities.
local listTitle = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
listTitle:SetPoint("TOPLEFT", 690, -148)

local listRows = {}
for index = 1, LIST_ROWS do
    local row = CreateFrame("Button", nil, frame)
    row:SetWidth(230)
    row:SetHeight(LIST_ROW_HEIGHT - 2)
    row:SetPoint("TOPLEFT", 686, -168 - (index - 1) * LIST_ROW_HEIGHT)
    row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(22)
    row.icon:SetHeight(22)
    row.icon:SetPoint("LEFT", 2, 0)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 6, 0)
    row.name:SetWidth(196)
    row.name:SetJustifyH("LEFT")
    row:SetScript("OnEnter", OnCellEnter)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    row:SetScript("OnClick", OnCellClick)
    listRows[index] = row
end

local listScroll = CreateFrame("ScrollFrame", "EmberforgeClasslessListScroll", frame, "FauxScrollFrameTemplate")
listScroll:SetPoint("TOPLEFT", 686, -168)
listScroll:SetWidth(230)
listScroll:SetHeight(LIST_ROWS * LIST_ROW_HEIGHT)
listScroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, LIST_ROW_HEIGHT, Refresh)
end)

StaticPopupDialogs["EMBERFORGE_CLASSLESS_PICK"] = {
    text = "Choose %s for your starting abilities?",
    button1 = ACCEPT,
    button2 = CANCEL,
    OnAccept = function(self, spellId)
        Send("PICK," .. spellId)
    end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
}

-- Refresh -------------------------------------------------------------------------------------------

local function RefreshClassRow()
    for _, button in ipairs(classButtons) do
        if button.id == view.class and view.search == "" then
            button.glow:Show()
            button.label:SetTextColor(1, 0.55, 0.12)
        else
            button.glow:Hide()
            button.label:SetTextColor(0.85, 0.85, 0.85)
        end
    end
end

local function RefreshSpecs()
    local trees = TREES[view.class]
    for index, button in ipairs(specButtons) do
        if view.class == YOUR_ABILITIES or not trees or view.search ~= "" then
            button:Hide()
        else
            button:SetText(index > 1 and trees[index - 1][1] or ALL)
            if button.spec == (view.spec or 0) then
                button:LockHighlight()
            else
                button:UnlockHighlight()
            end
            button:Show()
        end
    end
end

local function RefreshGrid()
    local list = GridAbilities()
    FauxScrollFrame_Update(EmberforgeClasslessGridScroll, math.ceil(#list / COLUMNS), ROWS, CELL_HEIGHT)
    local offset = FauxScrollFrame_GetOffset(EmberforgeClasslessGridScroll) * COLUMNS
    local level = UnitLevel("player")
    for index, cell in ipairs(cells) do
        local ability = list[offset + index]
        cell.ability = ability
        if ability then
            cell.icon:SetTexture(SpellIcon(ability.id))
            cell.name:SetText(SpellName(ability.id))
            local known = data.known[ability.id]
            if view.class == YOUR_ABILITIES and view.search == "" then
                cell.sub:SetText(CLASS_NAMES[ability.class] or "")
                cell.sub:SetTextColor(0.7, 0.7, 0.7)
            elseif known then
                cell.sub:SetText("Known")
                cell.sub:SetTextColor(0.3, 1, 0.3)
            elseif ability.level > level then
                cell.sub:SetText("Level " .. ability.level)
                cell.sub:SetTextColor(1, 0.25, 0.25)
            elseif CanPick(ability) then
                cell.sub:SetText("Level " .. ability.level .. " - click to choose")
                cell.sub:SetTextColor(1, 0.55, 0.12)
            else
                cell.sub:SetText("Level " .. ability.level)
                cell.sub:SetTextColor(1, 0.82, 0)
            end
            if known then
                cell.check:Show()
            else
                cell.check:Hide()
            end
            if ability.level > level and not known then
                cell.icon:SetVertexColor(0.45, 0.45, 0.45)
            else
                cell.icon:SetVertexColor(1, 1, 1)
            end
            cell:Show()
        else
            cell:Hide()
        end
    end

    if not data.loaded then
        empty:SetText("Loading abilities...")
    elseif #list == 0 then
        empty:SetText(view.search ~= "" and "No ability matches your search."
            or view.class == YOUR_ABILITIES and "You know no abilities yet." or "No abilities here.")
    else
        empty:SetText("")
    end
end

local function RefreshTree()
    for _, button in ipairs(talentButtons) do
        button.talent = nil
        button:Hide()
    end
    for _, row in ipairs(talentRows) do
        row.talent = nil
        row:Hide()
    end

    if view.class == YOUR_ABILITIES or view.search ~= "" then
        for _, texture in ipairs(backgrounds) do
            texture:SetTexture(0.05, 0.05, 0.07, 0.9)
        end
        local owned = OwnedTalents()
        local points = 0
        for _, talent in ipairs(owned) do
            points = points + data.talentRanks[talent.id]
        end
        treeTitle:SetText("Your Talents (" .. points .. ")")
        for index, row in ipairs(talentRows) do
            local talent = owned[index]
            if talent then
                row.talent = talent
                row.icon:SetTexture(SpellIcon(talent.ranks[1]))
                local text = SpellName(talent.ranks[1]) .. "  " .. data.talentRanks[talent.id] .. "/" .. #talent.ranks
                if index == #talentRows and #owned > index then
                    text = "+" .. (#owned - index + 1) .. " more"
                end
                row.name:SetText(text)
                row:Show()
            end
        end
        return
    end

    local trees = TREES[view.class]
    local tab = data.trees[view.class] and data.trees[view.class][view.tree]
    local info = trees and trees[view.tree]
    for _, texture in ipairs(backgrounds) do
        texture:SetTexture(info and ("Interface\\TalentFrame\\" .. info[2] .. "-" .. texture.part) or nil)
    end
    local points = 0
    for _, talent in ipairs(tab and data.talents[tab] or {}) do
        local rank = data.talentRanks[talent.id] or 0
        points = points + rank
        local button = talentButtons[talent.row * TALENT_COLUMNS + talent.col + 1]
        if button then
            button.talent = talent
            button.icon:SetTexture(SpellIcon(talent.ranks[math.max(rank, 1)]))
            button.icon:SetDesaturated(rank == 0)
            button.rank:SetText(rank .. "/" .. #talent.ranks)
            if rank > 0 then
                button.border:SetVertexColor(rank == #talent.ranks and 1 or 0.3, 0.85, 0.2)
                button.border:Show()
                button.rank:SetTextColor(0.3, 1, 0.3)
            else
                button.border:Hide()
                button.rank:SetTextColor(0.6, 0.6, 0.6)
            end
            button:Show()
        end
    end
    treeTitle:SetText(info and (info[1] .. " Talents (" .. points .. ")") or "")
end

local function RefreshList()
    local list = KnownAbilities()
    listTitle:SetText("Your Abilities (" .. #list .. ")")
    FauxScrollFrame_Update(EmberforgeClasslessListScroll, #list, LIST_ROWS, LIST_ROW_HEIGHT)
    local offset = FauxScrollFrame_GetOffset(EmberforgeClasslessListScroll)
    for index, row in ipairs(listRows) do
        local ability = list[offset + index]
        row.ability = ability
        if ability then
            row.icon:SetTexture(SpellIcon(ability.id))
            row.name:SetText(SpellName(ability.id))
            row:Show()
        else
            row:Hide()
        end
    end
end

local function RefreshStatus()
    if Picking() then
        status:SetText(EMBER .. "Titan Tome of Selection:|r choose " .. data.picks .. " more "
            .. (data.picks == 1 and "ability" or "abilities")
            .. " (your abilities lock at level " .. data.lockLevel .. ")")
    elseif not data.locked then
        status:SetText("|cffaaaaaaReroll with the Titan Tome of Randomization, or choose with the Titan Tome of "
            .. "Selection, until level "
            .. data.lockLevel .. ".|r")
    else
        status:SetText("")
    end
end

function Refresh()
    if not frame:IsShown() then
        return
    end
    if view.class == nil then
        view.class = next(data.known) and not Picking() and YOUR_ABILITIES or CLASSES[1].id
    end
    RefreshClassRow()
    RefreshSpecs()
    RefreshGrid()
    RefreshTree()
    RefreshList()
    RefreshStatus()
end

frame:SetScript("OnShow", function()
    if not data.loaded and not data.loading then
        data.loading = true
        Send("LIST")
    end
    Refresh()
end)

function EmberforgeClassless_Toggle()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
    end
end

-- Roll reveal: a quick shuffle of icons, then the rolled ability with its tooltip, Keep or Reroll.

local reveal = CreateFrame("Frame", "EmberforgeClasslessReveal", UIParent)
reveal:SetWidth(320)
reveal:SetHeight(150)
reveal:SetPoint("TOP", 0, -150)
reveal:SetFrameStrata("DIALOG")
reveal:EnableMouse(true)
Backdrop(reveal)
reveal:Hide()

local revealTitle = reveal:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
revealTitle:SetPoint("TOP", 0, -16)
local revealIcon = reveal:CreateTexture(nil, "ARTWORK")
revealIcon:SetWidth(56)
revealIcon:SetHeight(56)
revealIcon:SetPoint("TOPLEFT", 22, -42)
local revealName = reveal:CreateFontString(nil, "ARTWORK", "GameFontHighlightLarge")
revealName:SetPoint("TOPLEFT", revealIcon, "TOPRIGHT", 12, -6)
revealName:SetWidth(200)
revealName:SetJustifyH("LEFT")
local revealNote = reveal:CreateFontString(nil, "ARTWORK", "GameFontDisableSmall")
revealNote:SetPoint("TOPLEFT", revealName, "BOTTOMLEFT", 0, -4)

local keep = CreateFrame("Button", nil, reveal, "UIPanelButtonTemplate")
keep:SetWidth(110)
keep:SetHeight(24)
keep:SetPoint("BOTTOMLEFT", 40, 18)
local reroll = CreateFrame("Button", nil, reveal, "UIPanelButtonTemplate")
reroll:SetWidth(110)
reroll:SetHeight(24)
reroll:SetPoint("BOTTOMRIGHT", -40, 18)
reroll:SetText("Reroll")

local SHUFFLE_TIME, SHUFFLE_STEP = 0.8, 0.07
local function Shuffle(self, elapsed)
    self.shuffle = self.shuffle - elapsed
    self.step = self.step - elapsed
    if self.shuffle <= 0 then
        self:SetScript("OnUpdate", nil)
        local current = reveals[1]
        revealIcon:SetTexture(SpellIcon(current.spell))
        revealName:SetText(SpellName(current.spell))
        revealNote:SetText(current.canReroll and "Keep it, or reroll once for another." or "")
        keep:Enable()
        if current.canReroll then
            reroll:Enable()
        end
        GameTooltip:SetOwner(reveal, "ANCHOR_NONE")
        GameTooltip:SetPoint("TOPLEFT", reveal, "TOPRIGHT", 0, 0)
        GameTooltip:SetHyperlink("spell:" .. current.spell)
        GameTooltip:Show()
    elseif self.step <= 0 then
        self.step = SHUFFLE_STEP
        local ids = {}
        for id in pairs(data.abilities) do
            ids[#ids + 1] = id
        end
        revealIcon:SetTexture(#ids > 0 and SpellIcon(ids[math.random(#ids)]) or "Interface\\Icons\\INV_Misc_Dice_01")
    end
end

function ShowNextReveal()
    local current = reveals[1]
    if not current then
        reveal:Hide()
        return
    end
    if reveal:IsShown() and reveal.current == current then
        return
    end
    reveal.current = current
    revealTitle:SetText(EMBER .. "Wildcard Roll|r")
    revealName:SetText("Rolling...")
    revealNote:SetText("")
    keep:SetText(current.canReroll and "Keep" or OKAY)
    keep:Disable()
    reroll:Disable()
    if current.canReroll then
        reroll:Show()
        keep:ClearAllPoints()
        keep:SetPoint("BOTTOMLEFT", 40, 18)
    else
        reroll:Hide()
        keep:ClearAllPoints()
        keep:SetPoint("BOTTOM", 0, 18)
    end
    reveal.shuffle, reveal.step = SHUFFLE_TIME, 0
    reveal:SetScript("OnUpdate", Shuffle)
    reveal:Show()
end

local function Answer(message)
    local current = tremove(reveals, 1)
    if GameTooltip:IsOwned(reveal) then
        GameTooltip:Hide()
    end
    if message and current and current.canReroll then
        Send(message .. "," .. current.firstRank)
    end
    reveal.current = nil
    ShowNextReveal()
end
keep:SetScript("OnClick", function() Answer("KEEP") end)
reroll:SetScript("OnClick", function() Answer("REROLL") end)

-- Minimap button: drag it around the minimap; the position is kept per character.

local minimapButton = CreateFrame("Button", "EmberforgeClasslessMinimapButton", Minimap)
minimapButton:SetWidth(32)
minimapButton:SetHeight(32)
minimapButton:SetFrameStrata("MEDIUM")
minimapButton:SetFrameLevel(8)
minimapButton:RegisterForDrag("LeftButton")
minimapButton:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")
local minimapIcon = minimapButton:CreateTexture(nil, "BACKGROUND")
minimapIcon:SetTexture("Interface\\Icons\\INV_Misc_Book_11")
minimapIcon:SetWidth(20)
minimapIcon:SetHeight(20)
minimapIcon:SetPoint("CENTER")
local minimapBorder = minimapButton:CreateTexture(nil, "OVERLAY")
minimapBorder:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
minimapBorder:SetWidth(54)
minimapBorder:SetHeight(54)
minimapBorder:SetPoint("TOPLEFT")

local function PlaceMinimapButton(angle)
    minimapButton:ClearAllPoints()
    minimapButton:SetPoint("CENTER", Minimap, "CENTER", 80 * math.cos(angle), 80 * math.sin(angle))
end

local function MinimapDrag()
    local x, y = GetCursorPosition()
    local scale = Minimap:GetEffectiveScale()
    local cx, cy = Minimap:GetCenter()
    local angle = math.atan2(y / scale - cy, x / scale - cx)
    EmberforgeClasslessDB.minimapAngle = angle
    PlaceMinimapButton(angle)
end

minimapButton:SetScript("OnDragStart", function(self) self:SetScript("OnUpdate", MinimapDrag) end)
minimapButton:SetScript("OnDragStop", function(self) self:SetScript("OnUpdate", nil) end)
minimapButton:SetScript("OnClick", EmberforgeClassless_Toggle)
minimapButton:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine(EMBER .. "Classless|r")
    GameTooltip:AddLine("Click: open the Classless window (N)", 1, 1, 1)
    GameTooltip:AddLine("Drag: move this button", 0.7, 0.7, 0.7)
    GameTooltip:Show()
end)
minimapButton:SetScript("OnLeave", function() GameTooltip:Hide() end)

-- Key binding: N opens the window (Heroes have no talent window to open with it). Set once per
-- character, so a player who rebinds it keeps their choice.
BINDING_HEADER_EMBERFORGE = "Project Emberforge"
BINDING_NAME_EMBERFORGE_CLASSLESS_TOGGLE = "Classless window"

local function OnLogin()
    EmberforgeClasslessDB = EmberforgeClasslessDB or {}
    PlaceMinimapButton(EmberforgeClasslessDB.minimapAngle or math.rad(200))
    if not EmberforgeClasslessDB.boundKey and not GetBindingKey("EMBERFORGE_CLASSLESS_TOGGLE") then
        SetBinding("N", "EMBERFORGE_CLASSLESS_TOGGLE")
        SaveBindings(GetCurrentBindingSet())
    end
    EmberforgeClasslessDB.boundKey = true
end

-- Events and slash command ----------------------------------------------------------------------------

frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("PLAYER_LEVEL_UP")
frame:RegisterEvent("PLAYER_LOGIN")
frame:SetScript("OnEvent", function(self, event, prefix, message, channel, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            OnServerMessage(message)
        end
    elseif event == "PLAYER_LOGIN" then
        OnLogin()
    else
        Refresh()
    end
end)

SLASH_EMBERFORGECLASSLESS1 = "/classless"
SLASH_EMBERFORGECLASSLESS2 = "/cl"
SlashCmdList["EMBERFORGECLASSLESS"] = EmberforgeClassless_Toggle

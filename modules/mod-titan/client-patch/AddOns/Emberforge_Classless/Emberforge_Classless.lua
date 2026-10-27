-- Emberforge Classless: every class's abilities by spec, the Hero's own abilities, and choosing the
-- starting kit after reading the Titan Tome of Selection. Open with /classless (or /cl).
--
-- The server (mod-titan, src/classless.cpp) sends the data as addon whispers to the player, prefix
-- EFCL, and the window asks for it the same way:
--   to server     LIST, PICK,<spell id>
--   from server   B / T,<class>,<skill line>,<name> / A,<class>.<skill line>.<level>.<spell id>;... / E
--                 K0 / K,<spell id>,...   S,<picks left>,<locked>,<lock level>   O

local PREFIX = "EFCL"
local WIDTH, HEIGHT = 880, 580
local CELL_WIDTH, CELL_HEIGHT, COLUMNS, ROWS = 196, 44, 3, 8
local LIST_ROW_HEIGHT, LIST_ROWS = 26, 13
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

local data = {
    abilities = {},     -- spell id -> { id, class, skill, level }
    tabs = {},          -- class -> { { skill, name }, ... }
    known = {},         -- spell id -> true
    picks = 0,
    locked = false,
    lockLevel = 9,
    loaded = false,
    loading = false,
}
local view = { class = nil, skill = nil, search = "" }
local frame, Refresh

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

local function ByLevelThenName(a, b)
    if a.level ~= b.level then
        return a.level < b.level
    end
    return SpellName(a.id) < SpellName(b.id)
end

-- What the grid shows for the current view.
local function GridAbilities()
    local list = {}
    for id, ability in pairs(data.abilities) do
        if view.class == YOUR_ABILITIES then
            if data.known[id] then
                list[#list + 1] = ability
            end
        elseif ability.class == view.class and (not view.skill or ability.skill == view.skill) then
            list[#list + 1] = ability
        end
    end
    table.sort(list, ByLevelThenName)
    return list
end

-- The right-hand list: the abilities you know, filtered by the search box.
local function KnownAbilities()
    local list, search = {}, view.search:lower()
    for id in pairs(data.known) do
        local ability = data.abilities[id]
        if ability and (search == "" or SpellName(id):lower():find(search, 1, true)) then
            list[#list + 1] = ability
        end
    end
    table.sort(list, function(a, b) return SpellName(a.id) < SpellName(b.id) end)
    return list
end

-- Messages from the server --------------------------------------------------------------------------

local function OnServerMessage(message)
    local kind = message:sub(1, 1)
    if message == "B" then
        wipe(data.abilities)
        wipe(data.tabs)
    elseif kind == "T" then
        local _, class, skill, name = strsplit(",", message, 4)
        class = tonumber(class)
        data.tabs[class] = data.tabs[class] or {}
        tinsert(data.tabs[class], { skill = tonumber(skill), name = name })
    elseif kind == "A" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local class, skill, level, id = strsplit(".", entry)
            id = tonumber(id)
            data.abilities[id] = { id = id, class = tonumber(class), skill = tonumber(skill), level = tonumber(level) }
        end
    elseif message == "E" then
        for _, tabs in pairs(data.tabs) do
            table.sort(tabs, function(a, b) return a.name < b.name end)
        end
        data.loaded, data.loading = true, false
        Refresh()
    elseif message == "K0" then
        wipe(data.known)
    elseif kind == "K" then
        for id in message:sub(3):gmatch("%d+") do
            data.known[tonumber(id)] = true
        end
        Refresh()
    elseif kind == "S" then
        local _, picks, locked, lockLevel = strsplit(",", message)
        data.picks, data.locked, data.lockLevel = tonumber(picks) or 0, locked == "1", tonumber(lockLevel) or 9
        Refresh()
    elseif message == "O" then
        frame:Show()
    end
end

-- Window --------------------------------------------------------------------------------------------

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
frame:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 },
})
frame:SetBackdropColor(0.03, 0.03, 0.05, 0.96)
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeClassless")     -- Escape closes it

local title = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
title:SetPoint("TOP", 0, -16)
title:SetText(EMBER .. "Classless|r")

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -6, -6)

local status = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
status:SetPoint("BOTTOM", 0, 20)

-- Class row: your abilities, then the ten classes.
local classButtons = {}
local function ClassButton(index, id, icon, coords, label)
    local button = CreateFrame("Button", nil, frame)
    button:SetWidth(42)
    button:SetHeight(42)
    button:SetPoint("TOPLEFT", 34 + (index - 1) * 74, -44)
    button.id = id
    local texture = button:CreateTexture(nil, "ARTWORK")
    texture:SetAllPoints()
    texture:SetTexture(icon)
    if coords then
        texture:SetTexCoord(unpack(coords))
    end
    button.icon = texture
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
        view.class, view.skill = self.id, nil
        FauxScrollFrame_SetOffset(EmberforgeClasslessGridScroll, 0)
        Refresh()
    end)
    classButtons[#classButtons + 1] = button
end

ClassButton(1, YOUR_ABILITIES, "Interface\\Icons\\INV_Misc_Book_11", nil, "Abilities")
for index, class in ipairs(CLASSES) do
    ClassButton(index + 1, class.id, "Interface\\TargetingFrame\\UI-Classes-Circles", CLASS_ICON_TCOORDS[class.token], class.name)
end

-- Spec tabs for the selected class: All, then its skill lines.
local tabButtons = {}
for index = 1, 5 do
    local tab = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    tab:SetWidth(120)
    tab:SetHeight(22)
    tab:SetPoint("TOPLEFT", 24 + (index - 1) * 124, -116)
    tab:SetScript("OnClick", function(self)
        view.skill = self.skill
        FauxScrollFrame_SetOffset(EmberforgeClasslessGridScroll, 0)
        Refresh()
    end)
    tabButtons[index] = tab
end

-- Ability grid.
local cells = {}
local function OnCellEnter(self)
    if not self.ability then
        return
    end
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("spell:" .. self.ability.id)
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

-- Your abilities, with a search box.
local listTitle = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
listTitle:SetPoint("TOPLEFT", 648, -116)

local search = CreateFrame("EditBox", "EmberforgeClasslessSearch", frame, "InputBoxTemplate")
search:SetWidth(196)
search:SetHeight(20)
search:SetPoint("TOPLEFT", 654, -138)
search:SetAutoFocus(false)
search:SetScript("OnTextChanged", function(self)
    view.search = self:GetText() or ""
    FauxScrollFrame_SetOffset(EmberforgeClasslessListScroll, 0)
    Refresh()
end)
search:SetScript("OnEscapePressed", search.ClearFocus)

local listRows = {}
for index = 1, LIST_ROWS do
    local row = CreateFrame("Button", nil, frame)
    row:SetWidth(204)
    row:SetHeight(LIST_ROW_HEIGHT - 2)
    row:SetPoint("TOPLEFT", 648, -164 - (index - 1) * LIST_ROW_HEIGHT)
    row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(22)
    row.icon:SetHeight(22)
    row.icon:SetPoint("LEFT", 2, 0)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 6, 0)
    row.name:SetWidth(170)
    row.name:SetJustifyH("LEFT")
    row:SetScript("OnEnter", OnCellEnter)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    row:SetScript("OnClick", OnCellClick)
    listRows[index] = row
end

local listScroll = CreateFrame("ScrollFrame", "EmberforgeClasslessListScroll", frame, "FauxScrollFrameTemplate")
listScroll:SetPoint("TOPLEFT", 648, -164)
listScroll:SetWidth(204)
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
        local selected = button.id == view.class
        if selected then
            button.glow:Show()
            button.label:SetTextColor(1, 0.55, 0.12)
        else
            button.glow:Hide()
            button.label:SetTextColor(0.85, 0.85, 0.85)
        end
    end
end

local function RefreshTabs()
    local tabs = view.class ~= YOUR_ABILITIES and data.tabs[view.class] or {}
    for index, tab in ipairs(tabButtons) do
        if view.class == YOUR_ABILITIES or (index > 1 and not tabs[index - 1]) then
            tab:Hide()
        else
            tab.skill = index > 1 and tabs[index - 1].skill or nil
            tab:SetText(index > 1 and tabs[index - 1].name or ALL)
            if tab.skill == view.skill then
                tab:LockHighlight()
            else
                tab:UnlockHighlight()
            end
            tab:Show()
        end
    end
end

local function RefreshGrid()
    local list = GridAbilities()
    local totalRows = math.ceil(#list / COLUMNS)
    FauxScrollFrame_Update(EmberforgeClasslessGridScroll, totalRows, ROWS, CELL_HEIGHT)
    local offset = FauxScrollFrame_GetOffset(EmberforgeClasslessGridScroll) * COLUMNS
    local level = UnitLevel("player")
    for index, cell in ipairs(cells) do
        local ability = list[offset + index]
        cell.ability = ability
        if ability then
            cell.icon:SetTexture(SpellIcon(ability.id))
            cell.name:SetText(SpellName(ability.id))
            local known = data.known[ability.id]
            if view.class == YOUR_ABILITIES then
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
                cell.icon:SetVertexColor(1, 1, 1)
                cell.check:Show()
            else
                cell.check:Hide()
                if ability.level > level then
                    cell.icon:SetVertexColor(0.45, 0.45, 0.45)
                else
                    cell.icon:SetVertexColor(1, 1, 1)
                end
            end
            cell:Show()
        else
            cell:Hide()
        end
    end

    if not data.loaded then
        empty:SetText("Loading abilities...")
    elseif #list == 0 then
        empty:SetText(view.class == YOUR_ABILITIES and "You know no abilities yet." or "No abilities here.")
    else
        empty:SetText("")
    end
end

local function RefreshList()
    local list = KnownAbilities()
    local count = 0
    for _ in pairs(data.known) do
        count = count + 1
    end
    listTitle:SetText("Your Abilities (" .. count .. ")")
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
            .. (data.picks == 1 and "ability" or "abilities") .. " (your abilities lock at level " .. data.lockLevel .. ")")
    elseif not data.locked then
        status:SetText("|cffaaaaaaReroll with the Titan Tome of Randomization, or build your own with the Titan Tome of Selection, until level "
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
    RefreshTabs()
    RefreshGrid()
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

-- Events and slash command ----------------------------------------------------------------------------

frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("PLAYER_LEVEL_UP")
frame:SetScript("OnEvent", function(self, event, prefix, message, channel, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            OnServerMessage(message)
        end
    else
        Refresh()
    end
end)

SLASH_EMBERFORGECLASSLESS1 = "/classless"
SLASH_EMBERFORGECLASSLESS2 = "/cl"
SlashCmdList["EMBERFORGECLASSLESS"] = function()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
    end
end

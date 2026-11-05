-- Emberforge Darkmoon: Silas Darkmoon's shop (opened by talking to him) and the card reveal.
-- Packs: Skill, Talent and Golden Card Packs for Emberspark (Skill Packs for gold too, a few a week), and how close the
-- account is to its next free pack. Cards: every Skill and Talent Card, what the account owns, any one bought outright.
-- Opening a pack from the bags plays the reveal: its five cards turn over one by one.
--
-- The server (mod-titan, src/cards.cpp) talks in addon whispers to the player, prefix EFDM:
--   from server   S,<Emberspark>,<Skill Pack>,<Talent Pack>,<card price>,<gold packs left>,<gold price, copper>,
--                   <Golden Pack>,<duplicates>,<a pack every>,<a Golden Pack every>,<rarity price factors x4>
--                 K0 / K,<kind>.<id>.<spell>.<rarity>.<owned>.<max>.<golden owned>;... / KE   the catalog
--                 U,<kind>.<id>.<owned>.<golden owned>;...   owned changes
--                 R,<pack>,<kind>.<id>.<spell>.<rarity>.<new 0|1>.<refund>;...   a pack opened: the reveal
--                 B,<kind>.<id>.<spell>.<rarity>   a card bought      O   open the window
--   to server     LIST, BUY,<pack 0 skill|1 talent|2 golden>,<0 Emberspark|1 gold>, CARD,<kind>,<id>
-- Kinds: 0 Skill Card, 1 Talent Card. Rarities: 0 Common, 1 Rare, 2 Epic, 3 Legendary.

local PREFIX = "EFDM"
local ART = "Interface\\AddOns\\Emberforge_Classless\\Art\\"
local SPARK_ICON = "Interface\\Icons\\INV_Emberforge_Emberspark"
local WIDTH, HEIGHT = 900, 560
local GOLD = { 1, 0.82, 0 }
local SUBTEXT = { 0.78, 0.66, 0.9 }
local WINDOW_FILL, WINDOW_EDGE = { 0.04, 0.026, 0.06, 0.97 }, { 0.62, 0.45, 0.85, 1 }
local PANEL_FILL, PANEL_EDGE = { 0.02, 0.012, 0.03, 0.85 }, { 0.42, 0.3, 0.58, 1 }
local BUTTON_FILL, BUTTON_EDGE = { 0.22, 0.09, 0.3, 0.96 }, { 0.82, 0.62, 1, 1 }
local OFF_FILL, OFF_EDGE = { 0.08, 0.07, 0.08, 0.96 }, { 0.38, 0.35, 0.4, 1 }
local RARITY_NAMES = { [0] = "Common", "Rare", "Epic", "Legendary" }
local RARITY_COLORS = { [0] = { 0.92, 0.92, 0.92 }, { 0, 0.44, 0.87 }, { 0.64, 0.21, 0.93 }, { 1, 0.5, 0 } }
local GOLDEN_COLOR = { 1, 0.84, 0.25 }
local PACKS = {
    { kind = 0, name = "Skill Card Pack", icon = "Interface\\Icons\\INV_Misc_Ticket_Tarot_Stack_01",
        text = "Five Skill Cards: abilities for your account's collection." },
    { kind = 1, name = "Talent Card Pack", icon = "Interface\\Icons\\INV_Misc_Ticket_Tarot_Furies",
        text = "Five Talent Cards: a rank of a talent each." },
    { kind = 2, name = "Golden Card Pack", icon = "Interface\\Icons\\INV_Inscription_TarotGreatness",
        text = "Five Golden Cards: one more of a card, past your slots." },
}

local state = { spark = 0, prices = { 0, 0, 0 }, cardPrice = 100, goldLeft = 0, goldPrice = 750000, duplicates = 0,
    packEvery = 10, goldenEvery = 50, factors = { [0] = 1, 2, 4, 8 } }
local catalog, byKey, catalogLoaded, asked = {}, {}, false, false
local view = { tab = 1, kind = 0, rarity = -1, missing = false, search = "", offset = 0 }

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function Thousands(value)
    local text = tostring(value)
    while true do
        local done
        text, done = text:gsub("^(%d+)(%d%d%d)", "%1,%2")
        if done == 0 then
            return text
        end
    end
end

local function CardName(card)
    local name = GetSpellInfo(card.spell) or ("Spell " .. card.spell)
    return name
end

local function CardIcon(card)
    local _, _, icon = GetSpellInfo(card.spell)
    return icon or "Interface\\Icons\\INV_Misc_QuestionMark"
end

local function CardPrice(card)
    return state.cardPrice * (state.factors[card.rarity] or 1)
end

-- Rounded boxes, as the Classless window draws them (Art\FillCorner, LineCorner, LineEdge).
local FLIPS = { { 0, 1, 0, 1 }, { 1, 0, 0, 1 }, { 0, 1, 1, 0 }, { 1, 0, 1, 0 } }
local CORNERS = { "TOPLEFT", "TOPRIGHT", "BOTTOMLEFT", "BOTTOMRIGHT" }

local function BoxColor(target, fill, edge)
    for _, piece in ipairs(target.boxFill) do
        if piece.solid then
            piece:SetTexture(unpack(fill))
        else
            piece:SetVertexColor(unpack(fill))
        end
    end
    for _, piece in ipairs(target.boxEdge) do
        piece:SetVertexColor(unpack(edge))
    end
end

local function Box(target, fill, edge, small)
    local size, suffix = small and 8 or 16, small and "Small" or ""
    target.boxFill, target.boxEdge = {}, {}
    local function Piece(list, layer, file, flip)
        local texture = target:CreateTexture(nil, layer)
        if file then
            texture:SetTexture(ART .. file .. suffix)
            texture:SetTexCoord(unpack(flip or FLIPS[1]))
        else
            texture.solid = true
        end
        list[#list + 1] = texture
        return texture
    end
    for index, corner in ipairs(CORNERS) do
        for _, piece in ipairs({ Piece(target.boxFill, "BACKGROUND", "FillCorner", FLIPS[index]),
            Piece(target.boxEdge, "BORDER", "LineCorner", FLIPS[index]) }) do
            piece:SetWidth(size)
            piece:SetHeight(size)
            piece:SetPoint(corner)
        end
    end
    local middle = Piece(target.boxFill, "BACKGROUND")
    middle:SetPoint("TOPLEFT", size, -0.5)
    middle:SetPoint("BOTTOMRIGHT", -size, 0.5)
    local left = Piece(target.boxFill, "BACKGROUND")
    left:SetPoint("TOPLEFT", 0.5, -size)
    left:SetPoint("BOTTOMRIGHT", target, "BOTTOMLEFT", size, size)
    local right = Piece(target.boxFill, "BACKGROUND")
    right:SetPoint("TOPLEFT", target, "TOPRIGHT", -size, -size)
    right:SetPoint("BOTTOMRIGHT", -0.5, size)
    local top = Piece(target.boxEdge, "BORDER", "LineEdge")
    top:SetPoint("TOPLEFT", size, 0)
    top:SetPoint("BOTTOMRIGHT", target, "TOPRIGHT", -size, -size)
    local bottom = Piece(target.boxEdge, "BORDER", "LineEdge", FLIPS[3])
    bottom:SetPoint("TOPLEFT", target, "BOTTOMLEFT", size, size)
    bottom:SetPoint("BOTTOMRIGHT", -size, 0)
    local leftEdge = Piece(target.boxEdge, "BORDER", "LineEdgeV")
    leftEdge:SetPoint("TOPLEFT", 0, -size)
    leftEdge:SetPoint("BOTTOMRIGHT", target, "BOTTOMLEFT", size, size)
    local rightEdge = Piece(target.boxEdge, "BORDER", "LineEdgeV", FLIPS[2])
    rightEdge:SetPoint("TOPLEFT", target, "TOPRIGHT", -size, -size)
    rightEdge:SetPoint("BOTTOMRIGHT", 0, size)
    BoxColor(target, fill, edge)
end

local function Text(parent, size, color, font)
    local text = parent:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    text:SetFont(font or "Fonts\\FRIZQT__.TTF", size)
    text:SetShadowOffset(1, -1)
    text:SetTextColor(unpack(color or { 1, 1, 1 }))
    return text
end

local function Panel(parent, x, y, width, height, fill, edge)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetWidth(width)
    panel:SetHeight(height)
    panel:SetPoint("TOPLEFT", x, y)
    Box(panel, fill or PANEL_FILL, edge or PANEL_EDGE)
    return panel
end


local function Text(parent, size, color, font)
    local text = parent:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    text:SetFont(font or "Fonts\\FRIZQT__.TTF", size)
    text:SetShadowOffset(1, -1)
    text:SetTextColor(unpack(color or { 1, 1, 1 }))
    return text
end

local function Panel(parent, x, y, width, height, fill, edge)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetWidth(width)
    panel:SetHeight(height)
    panel:SetPoint("TOPLEFT", x, y)
    Box(panel, fill or PANEL_FILL, edge or PANEL_EDGE)
    return panel
end

local function Button(parent, width, height, label, onClick)
    local button = CreateFrame("Button", nil, parent)
    button:SetWidth(width)
    button:SetHeight(height)
    Box(button, BUTTON_FILL, BUTTON_EDGE, true)
    button:SetHighlightTexture(ART .. "HeaderGlow", "ADD")
    button.label = Text(button, 12, GOLD)
    button.label:SetPoint("CENTER", 0, 0)
    button.label:SetText(label)
    button:SetScript("OnClick", onClick)
    function button:SetOn(on)
        if on then
            BoxColor(self, BUTTON_FILL, BUTTON_EDGE)
            self.label:SetTextColor(unpack(GOLD))
            self:Enable()
        else
            BoxColor(self, OFF_FILL, OFF_EDGE)
            self.label:SetTextColor(0.5, 0.5, 0.5)
            self:Disable()
        end
    end
    return button
end

-- Window -----------------------------------------------------------------------------------------------------------

local frame = CreateFrame("Frame", "EmberforgeDarkmoon", UIParent)
frame:SetWidth(WIDTH)
frame:SetHeight(HEIGHT)
frame:SetPoint("CENTER")
frame:SetFrameStrata("HIGH")
frame:SetToplevel(true)
frame:EnableMouse(true)
frame:SetMovable(true)
frame:SetClampedToScreen(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
Box(frame, WINDOW_FILL, WINDOW_EDGE)
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeDarkmoon")

local headerGlow = frame:CreateTexture(nil, "BORDER")
headerGlow:SetTexture(ART .. "HeaderGlow")
headerGlow:SetBlendMode("ADD")
headerGlow:SetVertexColor(0.7, 0.45, 1)
headerGlow:SetWidth(600)
headerGlow:SetHeight(130)
headerGlow:SetPoint("TOP", 0, -1)
local title = Text(frame, 22, GOLD, "Fonts\\MORPHEUS.TTF")
title:SetPoint("TOP", 0, -14)
title:SetText("Darkmoon Cards")
local divider = frame:CreateTexture(nil, "ARTWORK")
divider:SetTexture(ART .. "Divider")
divider:SetWidth(380)
divider:SetHeight(16)
divider:SetPoint("TOP", 0, -36)
local close = CreateFrame("Button", nil, frame)
close:SetWidth(28)
close:SetHeight(28)
close:SetPoint("TOPRIGHT", -12, -12)
close:SetNormalTexture(ART .. "Close")
close:SetHighlightTexture(ART .. "RingHover", "ADD")
close:SetScript("OnClick", function() frame:Hide() end)

local balance = CreateFrame("Frame", nil, frame)
balance:SetWidth(150)
balance:SetHeight(28)
balance:SetPoint("TOPLEFT", 16, -14)
Box(balance, PANEL_FILL, PANEL_EDGE, true)
local balanceIcon = balance:CreateTexture(nil, "ARTWORK")
balanceIcon:SetTexture(SPARK_ICON)
balanceIcon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
balanceIcon:SetWidth(20)
balanceIcon:SetHeight(20)
balanceIcon:SetPoint("LEFT", 5, 0)
local balanceText = Text(balance, 13)
balanceText:SetPoint("LEFT", balanceIcon, "RIGHT", 6, 0)

-- Tabs.
local pages, tabs = {}, {}
local Refresh
local function ShowTab(index)
    view.tab = index
    for i, page in ipairs(pages) do
        if i == index then
            page:Show()
        else
            page:Hide()
        end
    end
    for i, tab in ipairs(tabs) do
        BoxColor(tab, i == index and BUTTON_FILL or PANEL_FILL, i == index and BUTTON_EDGE or PANEL_EDGE)
    end
    if index == 2 and not catalogLoaded and not asked then
        asked = true
        Send("LIST")
    end
    Refresh()
end
for index, name in ipairs({ "Packs", "Cards", "Specializations" }) do
    local tab = Button(frame, 150, 28, name, function() ShowTab(index) end)
    tab:SetPoint("TOPLEFT", 20 + (index - 1) * 158, -60)
    tabs[index] = tab
    local page = CreateFrame("Frame", nil, frame)
    page:SetPoint("TOPLEFT", 20, -96)
    page:SetPoint("BOTTOMRIGHT", -20, 20)
    pages[index] = page
end

-- Packs page.
local packRows = {}
for index, pack in ipairs(PACKS) do
    local panel = Panel(pages[1], 0, -(index - 1) * 106, WIDTH - 40, 98)
    local icon = panel:CreateTexture(nil, "ARTWORK")
    icon:SetTexture(pack.icon)
    icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    icon:SetWidth(60)
    icon:SetHeight(60)
    icon:SetPoint("LEFT", 18, 0)
    local name = Text(panel, 17, index == 3 and GOLDEN_COLOR or GOLD, "Fonts\\MORPHEUS.TTF")
    name:SetPoint("TOPLEFT", icon, "TOPRIGHT", 14, -4)
    name:SetText(pack.name)
    local text = Text(panel, 12, { 0.9, 0.86, 0.95 })
    text:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 0, -6)
    text:SetText(pack.text)
    local note = Text(panel, 11, SUBTEXT)
    note:SetPoint("TOPLEFT", text, "BOTTOMLEFT", 0, -6)
    local spark = Button(panel, 170, 30, "", function() Send("BUY," .. pack.kind .. ",0") end)
    spark:SetPoint("RIGHT", -16, 0)
    local row = { spark = spark, note = note }
    if pack.kind == 0 then
        row.gold = Button(panel, 170, 30, "", function() Send("BUY,0,1") end)
        row.gold:SetPoint("RIGHT", spark, "LEFT", -10, 0)
    end
    packRows[index] = row
end
local tally = Panel(pages[1], 0, -318, WIDTH - 40, 100)
local function TallyBar(y, label)
    local name = Text(tally, 12, SUBTEXT)
    name:SetPoint("TOPLEFT", 18, y)
    name:SetText(label)
    local bar = CreateFrame("StatusBar", nil, tally)
    bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    bar:SetWidth(WIDTH - 260)
    bar:SetHeight(10)
    bar:SetPoint("TOPLEFT", 200, y - 2)
    bar:SetMinMaxValues(0, 1)
    local back = bar:CreateTexture(nil, "BACKGROUND")
    back:SetTexture(0, 0, 0, 0.7)
    back:SetAllPoints()
    local value = Text(tally, 12)
    value:SetPoint("LEFT", bar, "RIGHT", 10, 0)
    return bar, value
end
local packBar, packValue = TallyBar(-18, "Next free pack")
packBar:SetStatusBarColor(0.64, 0.21, 0.93)
local goldenBar, goldenValue = TallyBar(-44, "Next Golden Pack")
goldenBar:SetStatusBarColor(1, 0.75, 0.2)
local ownedText = Text(tally, 12, { 0.9, 0.86, 0.95 })
ownedText:SetPoint("TOPLEFT", 18, -72)

-- Cards page: filters on top, a grid of cards (the mouse wheel pages it).
local COLUMNS, ROWS, CELL_W, CELL_H = 5, 6, 166, 52
local filters = CreateFrame("Frame", nil, pages[2])
filters:SetPoint("TOPLEFT")
filters:SetWidth(WIDTH - 40)
filters:SetHeight(30)
local kindButtons, rarityButtons = {}, {}
for index, label in ipairs({ "Skill Cards", "Talent Cards" }) do
    local button = Button(filters, 110, 26, label, function() view.kind = index - 1; view.offset = 0; Refresh() end)
    button:SetPoint("TOPLEFT", (index - 1) * 116, 0)
    kindButtons[index] = button
end
for index, label in ipairs({ "All", "Common", "Rare", "Epic", "Legendary" }) do
    local button = Button(filters, 74, 26, label, function() view.rarity = index - 2; view.offset = 0; Refresh() end)
    button:SetPoint("TOPLEFT", 246 + (index - 1) * 78, 0)
    rarityButtons[index] = button
end
local missingButton = Button(filters, 110, 26, "Not owned", function()
    view.missing = not view.missing
    view.offset = 0
    Refresh()
end)
missingButton:SetPoint("TOPLEFT", 646, 0)
local search = CreateFrame("EditBox", "EmberforgeDarkmoonSearch", filters, "InputBoxTemplate")
search:SetWidth(96)
search:SetHeight(20)
search:SetPoint("TOPRIGHT", -4, -3)
search:SetAutoFocus(false)
search:SetScript("OnTextChanged", function(self)
    view.search = (self:GetText() or ""):lower()
    view.offset = 0
    Refresh()
end)
search:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)

local grid = CreateFrame("Frame", nil, pages[2])
grid:SetPoint("TOPLEFT", 0, -38)
grid:SetPoint("BOTTOMRIGHT", 0, 22)
grid:EnableMouseWheel(true)
local gridStatus = Text(pages[2], 11, SUBTEXT)
gridStatus:SetPoint("BOTTOMLEFT", 2, 2)
local cells = {}
local shown = {}

StaticPopupDialogs["EMBERFORGE_DARKMOON_CARD"] = {
    text = "Buy %s for %s Emberspark?",
    button1 = ACCEPT,
    button2 = CANCEL,
    OnAccept = function(self) Send("CARD," .. self.data.kind .. "," .. self.data.id) end,
    timeout = 0,
    hideOnEscape = true,
}

local function OnCellEnter(self)
    local card = self.card
    if not card then
        return
    end
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("spell:" .. card.spell)
    local color = RARITY_COLORS[card.rarity]
    GameTooltip:AddLine(RARITY_NAMES[card.rarity] .. (card.kind == 0 and " Skill Card" or " Talent Card"),
        color[1], color[2], color[3])
    if card.kind == 1 then
        GameTooltip:AddLine("Ranks owned: " .. card.owned .. " / " .. card.max, 1, 1, 1)
    end
    if card.golden > 0 then
        GameTooltip:AddLine("Golden copy owned", unpack(GOLDEN_COLOR))
    end
    if card.owned < card.max then
        GameTooltip:AddLine("Click to buy for " .. Thousands(CardPrice(card)) .. " Emberspark.", 0.8, 0.7, 1)
    end
    GameTooltip:Show()
end

local function OnCellClick(self)
    local card = self.card
    if card and card.owned < card.max then
        local dialog = StaticPopup_Show("EMBERFORGE_DARKMOON_CARD", CardName(card), Thousands(CardPrice(card)))
        if dialog then
            dialog.data = { kind = card.kind, id = card.id }
        end
    end
end

for index = 1, COLUMNS * ROWS do
    local cell = CreateFrame("Button", nil, grid)
    cell:SetWidth(CELL_W)
    cell:SetHeight(CELL_H)
    local col, row = (index - 1) % COLUMNS, math.floor((index - 1) / COLUMNS)
    cell:SetPoint("TOPLEFT", col * (CELL_W + 6), -row * (CELL_H + 6))
    Box(cell, PANEL_FILL, PANEL_EDGE, true)
    cell.icon = cell:CreateTexture(nil, "ARTWORK")
    cell.icon:SetWidth(36)
    cell.icon:SetHeight(36)
    cell.icon:SetPoint("LEFT", 8, 0)
    cell.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    cell.name = Text(cell, 11)
    cell.name:SetPoint("TOPLEFT", cell.icon, "TOPRIGHT", 6, 0)
    cell.name:SetWidth(CELL_W - 58)
    cell.name:SetHeight(26)
    cell.name:SetJustifyH("LEFT")
    cell.name:SetJustifyV("TOP")
    cell.owned = Text(cell, 10, SUBTEXT)
    cell.owned:SetPoint("BOTTOMLEFT", cell.icon, "BOTTOMRIGHT", 6, 0)
    cell:SetScript("OnEnter", OnCellEnter)
    cell:SetScript("OnLeave", function() GameTooltip:Hide() end)
    cell:SetScript("OnClick", OnCellClick)
    cells[index] = cell
end
grid:SetScript("OnMouseWheel", function(_, delta)
    local most = math.max(0, math.ceil(#shown / COLUMNS) - ROWS)
    view.offset = math.max(0, math.min(most, view.offset - delta))
    Refresh()
end)

-- Specializations page: next.
local soon = Text(pages[3], 15, SUBTEXT, "Fonts\\MORPHEUS.TTF")
soon:SetPoint("CENTER", 0, 20)
soon:SetText("Specializations are coming next")
local soonText = Text(pages[3], 12, { 0.9, 0.86, 0.95 })
soonText:SetPoint("TOP", soon, "BOTTOM", 0, -10)
soonText:SetWidth(560)
soonText:SetText("Loadouts of your cards that your level-up rolls follow: slot a card, and the next roll at its level "
    .. "gives it to you. Collect your cards now; they are your account's forever.")

local function Matches(card)
    if card.kind ~= view.kind then
        return false
    end
    if view.rarity >= 0 and card.rarity ~= view.rarity then
        return false
    end
    if view.missing and card.owned >= card.max then
        return false
    end
    if view.search ~= "" and not CardName(card):lower():find(view.search, 1, true) then
        return false
    end
    return true
end

local function RefreshCards()
    wipe(shown)
    for _, card in ipairs(catalog) do
        if Matches(card) then
            shown[#shown + 1] = card
        end
    end
    table.sort(shown, function(a, b)
        if a.rarity ~= b.rarity then
            return a.rarity > b.rarity
        end
        return CardName(a) < CardName(b)
    end)
    for index, cell in ipairs(cells) do
        local card = shown[view.offset * COLUMNS + index]
        cell.card = card
        if card then
            local color = RARITY_COLORS[card.rarity]
            local owned = card.owned >= card.max
            cell.icon:SetTexture(CardIcon(card))
            cell.icon:SetDesaturated(card.owned == 0)
            cell.name:SetText(CardName(card))
            cell.name:SetTextColor(color[1], color[2], color[3])
            BoxColor(cell, PANEL_FILL, card.golden > 0 and { GOLDEN_COLOR[1], GOLDEN_COLOR[2], GOLDEN_COLOR[3], 1 }
                or { color[1] * 0.8, color[2] * 0.8, color[3] * 0.8, owned and 1 or 0.45 })
            if card.kind == 1 then
                cell.owned:SetText(card.owned .. " / " .. card.max .. " ranks")
            else
                cell.owned:SetText(owned and "Owned" or (Thousands(CardPrice(card)) .. " Emberspark"))
            end
            cell:Show()
        else
            cell:Hide()
        end
    end
    for index, button in ipairs(kindButtons) do
        BoxColor(button, index - 1 == view.kind and BUTTON_FILL or PANEL_FILL,
            index - 1 == view.kind and BUTTON_EDGE or PANEL_EDGE)
    end
    for index, button in ipairs(rarityButtons) do
        BoxColor(button, index - 2 == view.rarity and BUTTON_FILL or PANEL_FILL,
            index - 2 == view.rarity and BUTTON_EDGE or PANEL_EDGE)
    end
    BoxColor(missingButton, view.missing and BUTTON_FILL or PANEL_FILL, view.missing and BUTTON_EDGE or PANEL_EDGE)
    if not catalogLoaded then
        gridStatus:SetText("Loading your collection...")
    else
        gridStatus:SetText(#shown .. " cards. Scroll for more; click a card you lack to buy it.")
    end
end

function Refresh()
    balanceText:SetText(Thousands(state.spark))
    for index, row in ipairs(packRows) do
        row.spark.label:SetText(Thousands(state.prices[index]) .. " Emberspark")
        row.spark:SetOn(state.spark >= state.prices[index])
        if row.gold then
            row.gold.label:SetText(math.floor(state.goldPrice / 10000) .. " gold")
            row.gold:SetOn(state.goldLeft > 0 and GetMoney() >= state.goldPrice)
            row.note:SetText("Prices climb with each purchase and start over when you prestige. With gold: "
                .. state.goldLeft .. " left this week.")
        elseif index == 2 then
            row.note:SetText("Prices climb with each purchase and start over when you prestige.")
        else
            row.note:SetText("Also every " .. state.goldenEvery .. " duplicates.")
        end
    end
    local intoPack, intoGolden = state.duplicates % state.packEvery, state.duplicates % state.goldenEvery
    packBar:SetValue(intoPack / state.packEvery)
    packValue:SetText(intoPack .. " / " .. state.packEvery)
    goldenBar:SetValue(intoGolden / state.goldenEvery)
    goldenValue:SetText(intoGolden .. " / " .. state.goldenEvery)
    local skills, skillsOwned, ranks, ranksOwned = 0, 0, 0, 0
    for _, card in ipairs(catalog) do
        if card.kind == 0 then
            skills, skillsOwned = skills + 1, skillsOwned + (card.owned > 0 and 1 or 0)
        else
            ranks, ranksOwned = ranks + card.max, ranksOwned + card.owned
        end
    end
    ownedText:SetText(catalogLoaded and ("Your collection: " .. skillsOwned .. " / " .. skills .. " Skill Cards, "
        .. ranksOwned .. " / " .. ranks .. " talent ranks. Duplicates pay Emberspark back, more for rarer cards.")
        or "Duplicates pay Emberspark back, more for rarer cards.")
    if view.tab == 2 then
        RefreshCards()
    end
end

-- The reveal: the pack's cards, face down, turn over one by one; a new card glows, a duplicate shows its Emberspark.
local reveal = CreateFrame("Frame", "EmberforgeDarkmoonReveal", UIParent)
reveal:SetAllPoints(UIParent)
reveal:SetFrameStrata("DIALOG")
reveal:EnableMouse(true)
reveal:Hide()
local shade = reveal:CreateTexture(nil, "BACKGROUND")
shade:SetAllPoints()
shade:SetTexture(0, 0, 0, 0.55)
local revealTitle = Text(reveal, 26, GOLD, "Fonts\\MORPHEUS.TTF")
revealTitle:SetPoint("CENTER", 0, 170)
local revealHint = Text(reveal, 12, SUBTEXT)
revealHint:SetPoint("CENTER", 0, -180)
revealHint:SetText("Click anywhere to continue")
local revealCards = {}
for index = 1, 5 do
    local card = CreateFrame("Frame", nil, reveal)
    card:SetWidth(170)
    card:SetHeight(240)
    card:SetPoint("CENTER", (index - 3) * 186, 0)
    Box(card, { 0.05, 0.03, 0.08, 0.97 }, { 0.6, 0.45, 0.8, 1 })
    card.glow = card:CreateTexture(nil, "BACKGROUND")
    card.glow:SetTexture(ART .. "Rays")
    card.glow:SetBlendMode("ADD")
    card.glow:SetWidth(360)
    card.glow:SetHeight(360)
    card.glow:SetPoint("CENTER")
    card.back = card:CreateTexture(nil, "ARTWORK")
    card.back:SetTexture("Interface\\Icons\\INV_Misc_Ticket_Tarot_Stack_01")
    card.back:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    card.back:SetWidth(90)
    card.back:SetHeight(90)
    card.back:SetPoint("CENTER")
    card.icon = card:CreateTexture(nil, "ARTWORK")
    card.icon:SetWidth(72)
    card.icon:SetHeight(72)
    card.icon:SetPoint("TOP", 0, -30)
    card.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    card.name = Text(card, 14, GOLD, "Fonts\\MORPHEUS.TTF")
    card.name:SetPoint("TOP", card.icon, "BOTTOM", 0, -12)
    card.name:SetWidth(150)
    card.rarity = Text(card, 11)
    card.rarity:SetPoint("TOP", card.name, "BOTTOM", 0, -6)
    card.result = Text(card, 12, { 0.9, 0.86, 0.95 })
    card.result:SetPoint("BOTTOM", 0, 18)
    card.result:SetWidth(150)
    revealCards[index] = card
end
local revealing, revealTime = {}, 0
local function ShowFace(card, entry, golden)
    local color = golden and GOLDEN_COLOR or RARITY_COLORS[entry.rarity]
    card.back:Hide()
    card.icon:SetTexture(CardIcon(entry))
    card.icon:Show()
    card.name:SetText(CardName(entry))
    card.name:SetTextColor(color[1], color[2], color[3])
    card.rarity:SetText((golden and "Golden " or "") .. RARITY_NAMES[entry.rarity]
        .. (entry.kind == 0 and " Skill Card" or " Talent Card"))
    card.rarity:SetTextColor(color[1], color[2], color[3])
    card.result:SetText(entry.new and "|cff40ff40New!|r" or ("Duplicate: +" .. Thousands(entry.refund)
        .. " Emberspark"))
    BoxColor(card, { 0.05, 0.03, 0.08, 0.97 }, { color[1], color[2], color[3], 1 })
    card.glow:SetVertexColor(color[1], color[2], color[3])
    card.glow:Show()
end
reveal:SetScript("OnUpdate", function(_, delta)
    revealTime = revealTime + delta
    for index, card in ipairs(revealCards) do
        local entry = revealing.cards and revealing.cards[index]
        if entry and not card.turned and revealTime > 0.5 + index * 0.6 then
            card.turned = true
            ShowFace(card, entry, revealing.golden)
            PlaySound(entry.new and "LEVELUPSOUND" or "igMainMenuOptionCheckBoxOn")
        end
        if card.turned then
            card.glow:SetAlpha(0.35 + 0.25 * math.sin(revealTime * 2 + index))
        end
    end
end)
reveal:SetScript("OnMouseDown", function()
    if revealTime > 0.5 + #(revealing.cards or {}) * 0.6 then
        reveal:Hide()
    end
end)

local function Reveal(pack, cards)
    revealing = { cards = cards, golden = pack == 2 }
    revealTime = 0
    revealTitle:SetText(PACKS[pack + 1] and PACKS[pack + 1].name or "Card Pack")
    revealTitle:SetTextColor(unpack(pack == 2 and GOLDEN_COLOR or GOLD))
    for index, card in ipairs(revealCards) do
        card.turned = false
        card.icon:Hide()
        card.glow:Hide()
        card.name:SetText("")
        card.rarity:SetText("")
        card.result:SetText("")
        card.back:Show()
        BoxColor(card, { 0.05, 0.03, 0.08, 0.97 }, { 0.6, 0.45, 0.8, 1 })
        if cards[index] then
            card:Show()
        else
            card:Hide()
        end
    end
    reveal:Show()
end

-- Server messages --------------------------------------------------------------------------------------------------

local function Split(text, sep)
    local parts = {}
    for part in (text .. sep):gmatch("(.-)" .. sep:gsub("%p", "%%%0")) do
        parts[#parts + 1] = part
    end
    return parts
end

local function Numbers(text)
    local numbers = {}
    for index, part in ipairs(Split(text, ".")) do
        numbers[index] = tonumber(part) or 0
    end
    return numbers
end

local function OnServerMessage(message)
    local kind = message:match("^([^,]+)")
    local body = message:match("^[^,]+,(.*)$") or ""
    if kind == "S" then
        local f = Split(body, ",")
        state.spark = tonumber(f[1]) or 0
        state.prices = { tonumber(f[2]) or 0, tonumber(f[3]) or 0, tonumber(f[7]) or 0 }
        state.cardPrice, state.goldLeft, state.goldPrice = tonumber(f[4]) or 0, tonumber(f[5]) or 0, tonumber(f[6]) or 0
        state.duplicates, state.packEvery, state.goldenEvery = tonumber(f[8]) or 0, tonumber(f[9]) or 10,
            tonumber(f[10]) or 50
        state.factors = { [0] = tonumber(f[11]) or 1, tonumber(f[12]) or 2, tonumber(f[13]) or 4, tonumber(f[14]) or 8 }
        Refresh()
    elseif kind == "K0" then
        wipe(catalog)
        wipe(byKey)
        catalogLoaded = false
    elseif kind == "K" then
        for _, item in ipairs(Split(body, ";")) do
            local n = Numbers(item)
            local card = { kind = n[1], id = n[2], spell = n[3], rarity = n[4], owned = n[5], max = n[6], golden = n[7] }
            catalog[#catalog + 1] = card
            byKey[card.kind .. "." .. card.id] = card
        end
    elseif kind == "KE" then
        catalogLoaded = true
        Refresh()
    elseif kind == "U" then
        for _, item in ipairs(Split(body, ";")) do
            local n = Numbers(item)
            local card = byKey[n[1] .. "." .. n[2]]
            if card then
                card.owned, card.golden = n[3], n[4]
            end
        end
        Refresh()
    elseif kind == "R" then
        local pack, rest = body:match("^(%d+),(.*)$")
        local cards = {}
        for _, item in ipairs(Split(rest or "", ";")) do
            local n = Numbers(item)
            cards[#cards + 1] = { kind = n[1], id = n[2], spell = n[3], rarity = n[4], new = n[5] == 1, refund = n[6] }
        end
        Reveal(tonumber(pack) or 0, cards)
    elseif kind == "B" then
        local n = Numbers(body)
        Reveal(n[1], { { kind = n[1], id = n[2], spell = n[3], rarity = n[4], new = true, refund = 0 } })
    elseif kind == "O" then
        frame:Show()
        ShowTab(view.tab)
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_MONEY")
events:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            OnServerMessage(message)
        end
    elseif frame:IsShown() then
        Refresh()
    end
end)

EmberforgeDarkmoon_State = { state = state, catalog = catalog, view = view, reveal = reveal }     -- for tests

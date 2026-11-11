-- Emberforge Runes: the Legendary Rune panel. From level 10 a Hero binds one rune to each specialization (only a
-- prestige frees it again) and picks its three modifiers (free, out of combat). The panel suggests up to three runes
-- the Hero's abilities can use; Browse all lists every rune (one missing an ability greyed, naming it; one the server
-- does not build yet: Coming soon). Also: Throw Glaive's glow at 5 Fel Command (Rune of Fel Commandment).
-- The look is the Classless window's (its Art folder); the runes' texts are Catalog.lua.
--
-- The server (mod-titan, src/runes.cpp) talks in addon whispers to the player, prefix EFRN:
--   from server   S,<level>,<unlock level>,<spec>,<rune>,<m1>,<m2>,<m3>,<offers a.b.c or ->,<prompt 0|1>
--                 R,<rune>,<missing>   (a rune the server builds; missing: groups by '/', one of by '+'; '-' none)
--                 N,<rune>,<slot>,<choice>,<missing>   (a modifier's own needs)
--                 E,<text>   O (open the panel)
--   to server     STATE   PICK,<rune>   MOD,<slot 1-3>,<choice 1-3>

local PREFIX = "EFRN"
local ART = "Interface\\AddOns\\Emberforge_Classless\\Art\\"
local WIDTH, HEIGHT = 900, 560
local GOLD = { 1, 0.82, 0 }
local SUBTEXT = { 0.8, 0.72, 0.9 }
local FEL = { 0.55, 1, 0.35 }
local GREY = { 0.55, 0.55, 0.55 }
local WINDOW_FILL, WINDOW_EDGE = { 0.03, 0.02, 0.05, 0.97 }, { 0.72, 0.55, 1, 1 }
local PANEL_FILL, PANEL_EDGE = { 0.02, 0.012, 0.035, 0.88 }, { 0.45, 0.33, 0.66, 1 }
local PICK_FILL, PICK_EDGE = { 0.12, 0.05, 0.22, 0.95 }, { 0.93, 0.85, 1, 1 }
local OFF_FILL, OFF_EDGE = { 0.06, 0.06, 0.06, 0.9 }, { 0.3, 0.3, 0.3, 1 }
local BUTTON_FILL, BUTTON_EDGE = { 0.18, 0.08, 0.3, 0.96 }, { 0.82, 0.66, 1, 1 }
local SLOT_NAMES = { "Invocation", "Manifestation", "Adaptation" }
local FEL_COMMANDMENT = 21

local catalog = EmberforgeRunesCatalog or {}
local state = { level = UnitLevel and UnitLevel("player") or 1, unlock = 10, spec = 1, rune = 0, mods = { 0, 0, 0 },
    offers = {}, prompt = false, built = {}, needs = {}, error = nil, loaded = false }
local view = { page = "offers", browseOffset = 0, prompted = {} }

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function Split(text, sep)
    local parts = {}
    for part in (text .. sep):gmatch("(.-)%" .. sep) do
        parts[#parts + 1] = part
    end
    return parts
end

-- "90111/688+697" -> "Throw Glaive, and Summon Imp or Summon Voidwalker"
local function MissingText(missing)
    if not missing or missing == "-" or missing == "" then
        return nil
    end
    local groups = {}
    for _, group in ipairs(Split(missing, "/")) do
        local names = {}
        for _, id in ipairs(Split(group, "+")) do
            names[#names + 1] = GetSpellInfo(tonumber(id)) or ("spell " .. id)
        end
        groups[#groups + 1] = table.concat(names, " or ")
    end
    return table.concat(groups, ", and ")
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

local function Box(target, fill, edge)
    local size = 16
    target.boxFill, target.boxEdge = {}, {}
    local function Piece(list, layer, file, flip)
        local texture = target:CreateTexture(nil, layer)
        if file then
            texture:SetTexture(ART .. file)
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

local function Text(parent, size, color, font, width)
    local text = parent:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    text:SetFont(font or "Fonts\\FRIZQT__.TTF", size)
    text:SetShadowOffset(1, -1)
    text:SetTextColor(unpack(color or { 1, 1, 1 }))
    if width then
        text:SetWidth(width)
        text:SetJustifyH("LEFT")
        text:SetJustifyV("TOP")
    end
    return text
end

local function Icon(parent, size)
    local icon = parent:CreateTexture(nil, "ARTWORK")
    icon:SetWidth(size)
    icon:SetHeight(size)
    icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    return icon
end

local function Button(parent, width, height, label, onClick)
    local button = CreateFrame("Button", nil, parent)
    button:SetWidth(width)
    button:SetHeight(height)
    Box(button, BUTTON_FILL, BUTTON_EDGE)
    button.label = Text(button, 13, GOLD)
    button.label:SetPoint("CENTER")
    button.label:SetText(label)
    button:SetHighlightTexture(ART .. "RingHover", "ADD")
    button:SetScript("OnClick", onClick)
    return button
end

-- Window ---------------------------------------------------------------------------------------------------------

local frame = CreateFrame("Frame", "EmberforgeRunes", UIParent)
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
tinsert(UISpecialFrames, "EmberforgeRunes")         -- Escape closes it

local title = Text(frame, 24, GOLD, "Fonts\\MORPHEUS.TTF")
title:SetPoint("TOP", 0, -16)
local subtitle = Text(frame, 13, SUBTEXT)
subtitle:SetPoint("TOP", title, "BOTTOM", 0, -6)

local close = CreateFrame("Button", nil, frame)
close:SetWidth(28)
close:SetHeight(28)
close:SetPoint("TOPRIGHT", -12, -12)
close:SetNormalTexture(ART .. "Close")
close:SetHighlightTexture(ART .. "RingHover", "ADD")
close:SetScript("OnClick", function() frame:Hide() end)

local footer = Text(frame, 13, SUBTEXT)
footer:SetPoint("BOTTOM", 0, 18)

local Refresh

-- Binding a rune is for good (until a prestige): asked first.
StaticPopupDialogs["EMBERFORGE_RUNE_BIND"] = {
    text = "Bind %s to this specialization?\n\nOnly a prestige can change it. Its modifiers stay free to change.",
    button1 = ACCEPT or "Accept",
    button2 = CANCEL or "Cancel",
    OnAccept = function(_, data) Send("PICK," .. data) end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
}

local function AskBind(rune)
    local dialog = StaticPopup_Show("EMBERFORGE_RUNE_BIND", rune.name)
    if dialog then
        dialog.data = rune.id
    end
end

-- A rune's standing for this Hero: "ok", "missing" (with what), or "soon" (the server does not build it yet).
local function Standing(rune)
    if not state.built[rune.id] then
        return "soon"
    end
    local missing = MissingText(state.built[rune.id])
    return missing and "missing" or "ok", missing
end

-- Offers: three cards ------------------------------------------------------------------------------------------------

local offers = CreateFrame("Frame", nil, frame)
offers:SetAllPoints()
local cards = {}
for index = 1, 3 do
    local card = CreateFrame("Frame", nil, offers)
    card:SetWidth(270)
    card:SetHeight(330)
    card:SetPoint("TOPLEFT", 26 + (index - 1) * 288, -78)
    Box(card, PANEL_FILL, PANEL_EDGE)
    card.icon = Icon(card, 56)
    card.icon:SetPoint("TOP", 0, -18)
    card.name = Text(card, 15, GOLD, nil, 240)
    card.name:SetJustifyH("CENTER")
    card.name:SetPoint("TOP", card.icon, "BOTTOM", 0, -10)
    card.role = Text(card, 11, SUBTEXT, nil, 240)
    card.role:SetJustifyH("CENTER")
    card.role:SetPoint("TOP", card.name, "BOTTOM", 0, -4)
    card.core = Text(card, 12, { 0.92, 0.9, 0.96 }, nil, 238)
    card.core:SetPoint("TOPLEFT", 16, -132)
    card.uses = Text(card, 11, FEL, nil, 238)
    card.uses:SetPoint("BOTTOMLEFT", 16, 58)
    card.choose = Button(card, 140, 30, "Choose", function(self) AskBind(self:GetParent().rune) end)
    card.choose:SetPoint("BOTTOM", 0, 16)
    cards[index] = card
end
local noOffers = Text(offers, 14, SUBTEXT, nil, 600)
noOffers:SetJustifyH("CENTER")
noOffers:SetPoint("TOP", 0, -200)
local browseHint = Text(offers, 13, SUBTEXT)
browseHint:SetPoint("BOTTOM", 0, 92)
browseHint:SetText("Not seeing what you're looking for?")
local browseButton = Button(offers, 220, 32, "Browse all runes", function()
    view.page = "browse"
    Refresh()
end)
browseButton:SetPoint("BOTTOM", 0, 52)

-- Browse all: every rune ---------------------------------------------------------------------------------------------

local browse = CreateFrame("Frame", nil, frame)
browse:SetAllPoints()
local BROWSE_ROWS = 9
local rows = {}
for index = 1, BROWSE_ROWS do
    local row = CreateFrame("Button", nil, browse)
    row:SetWidth(WIDTH - 60)
    row:SetHeight(44)
    row:SetPoint("TOPLEFT", 30, -74 - (index - 1) * 48)
    Box(row, PANEL_FILL, PANEL_EDGE)
    row.icon = Icon(row, 34)
    row.icon:SetPoint("LEFT", 6, 0)
    row.name = Text(row, 14, GOLD)
    row.name:SetPoint("TOPLEFT", row.icon, "TOPRIGHT", 10, -2)
    row.detail = Text(row, 11, SUBTEXT, nil, 560)
    row.detail:SetPoint("TOPLEFT", row.name, "BOTTOMLEFT", 0, -3)
    row.status = Text(row, 12, FEL)
    row.status:SetPoint("RIGHT", -14, 0)
    row:SetHighlightTexture(ART .. "RingHover", "ADD")
    row:SetScript("OnClick", function(self)
        if self.rune and Standing(self.rune) == "ok" and state.rune == 0 then
            AskBind(self.rune)
        end
    end)
    row:SetScript("OnEnter", function(self)
        if not self.rune then
            return
        end
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:AddLine(self.rune.name, 1, 0.82, 0)
        GameTooltip:AddLine(self.rune.core, 1, 1, 1, true)
        GameTooltip:AddLine(self.rune.loop, 0.8, 0.72, 0.9, true)
        GameTooltip:Show()
    end)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    rows[index] = row
end
browse:EnableMouseWheel(true)
browse:SetScript("OnMouseWheel", function(_, delta)
    view.browseOffset = math.max(0, math.min(#catalog - BROWSE_ROWS, view.browseOffset - delta))
    Refresh()
end)
local back = Button(browse, 120, 28, "Back", function()
    view.page = "offers"
    Refresh()
end)
back:SetPoint("BOTTOMLEFT", 28, 14)

-- Build: the bound rune and its modifiers ------------------------------------------------------------------------

local build = CreateFrame("Frame", nil, frame)
build:SetAllPoints()
local runeIcon = Icon(build, 48)
runeIcon:SetPoint("TOPLEFT", 30, -74)
local runeCore = Text(build, 12, { 0.92, 0.9, 0.96 }, nil, 780)
runeCore:SetPoint("TOPLEFT", runeIcon, "TOPRIGHT", 14, 0)
local columns = {}
for slot = 1, 3 do
    local column = CreateFrame("Frame", nil, build)
    column:SetWidth(270)
    column:SetHeight(360)
    column:SetPoint("TOPLEFT", 26 + (slot - 1) * 288, -140)
    column.title = Text(column, 15, GOLD)
    column.title:SetPoint("TOP", 0, 0)
    column.title:SetText(SLOT_NAMES[slot])
    column.hint = Text(column, 11, SUBTEXT, nil, 260)
    column.hint:SetJustifyH("CENTER")
    column.hint:SetPoint("TOP", column.title, "BOTTOM", 0, -3)
    column.options = {}
    for choice = 1, 3 do
        local option = CreateFrame("Button", nil, column)
        option:SetWidth(270)
        option:SetHeight(96)
        option:SetPoint("TOP", 0, -40 - (choice - 1) * 104)
        Box(option, PANEL_FILL, PANEL_EDGE)
        option.name = Text(option, 13, GOLD, nil, 246)
        option.name:SetPoint("TOPLEFT", 12, -10)
        option.text = Text(option, 11, { 0.9, 0.88, 0.95 }, nil, 246)
        option.text:SetPoint("TOPLEFT", option.name, "BOTTOMLEFT", 0, -4)
        option.slot, option.choice = slot, choice
        option:SetHighlightTexture(ART .. "RingHover", "ADD")
        option:SetScript("OnClick", function(self)
            if self.enabled and state.mods[self.slot] ~= self.choice then
                Send("MOD," .. self.slot .. "," .. self.choice)
            end
        end)
        column.options[choice] = option
    end
    columns[slot] = column
end

-- Locked: below the unlock level.
local locked = Text(frame, 15, SUBTEXT, nil, 600)
locked:SetJustifyH("CENTER")
locked:SetPoint("CENTER")

local function RuneById(id)
    for _, rune in ipairs(catalog) do
        if rune.id == id then
            return rune
        end
    end
end

-- The build the three picks make, by the doc's builds, if any.
local function BuildName(rune)
    for _, build in ipairs(rune.builds or {}) do
        local match = true
        for slot = 1, 3 do
            local mod = rune.mods[(slot - 1) * 3 + state.mods[slot]]
            if state.mods[slot] == 0 or not mod or mod.name ~= build.mods[slot] then
                match = false
            end
        end
        if match then
            return build.name
        end
    end
end

local function RefreshOffers()
    title:SetText("Legendary Runic Enhancements")
    subtitle:SetText("Choices based on your current abilities")
    local shown = 0
    for index, card in ipairs(cards) do
        local rune = RuneById(state.offers[index])
        card.rune = rune
        if rune then
            shown = shown + 1
            card.icon:SetTexture("Interface\\Icons\\" .. rune.icon)
            card.name:SetText(rune.name)
            card.role:SetText(rune.role)
            card.core:SetText(rune.core)
            card.uses:SetText("Built from " .. rune.builtFrom .. "\nSpent with " .. rune.spentWith)
            card:Show()
        else
            card:Hide()
        end
    end
    noOffers:SetText(shown == 0 and "None of the runes fits your abilities yet. Browse all to see what each one "
        .. "needs." or "")
end

local function RefreshBrowse()
    title:SetText("Every Legendary Rune")
    subtitle:SetText(state.rune == 0 and "Choose one for this specialization" or "Your specialization's rune is bound")
    for index, row in ipairs(rows) do
        local rune = catalog[view.browseOffset + index]
        row.rune = rune
        if rune then
            local standing, missing = Standing(rune)
            row.icon:SetTexture("Interface\\Icons\\" .. rune.icon)
            row.icon:SetDesaturated(standing ~= "ok")
            row.name:SetText(rune.name)
            row.name:SetTextColor(unpack(standing == "ok" and GOLD or GREY))
            row.detail:SetText(rune.role .. "  -  " .. rune.builtFrom .. " into " .. rune.spentWith)
            if standing == "soon" then
                row.status:SetText("Coming soon")
                row.status:SetTextColor(unpack(GREY))
            elseif standing == "missing" then
                row.status:SetText("Needs " .. missing)
                row.status:SetTextColor(1, 0.45, 0.4)
            else
                row.status:SetText(state.rune == rune.id and "Bound" or (state.rune == 0 and "Choose" or ""))
                row.status:SetTextColor(unpack(FEL))
            end
            BoxColor(row, standing == "ok" and PANEL_FILL or OFF_FILL, standing == "ok" and PANEL_EDGE or OFF_EDGE)
            row:Show()
        else
            row:Hide()
        end
    end
end

local HINTS = { "How you build %s", "What your empowered %s does", "Utility, or a new tradeoff" }

local function RefreshBuild()
    local rune = RuneById(state.rune)
    if not rune then
        return
    end
    title:SetText(rune.name)
    subtitle:SetText("Bound to specialization " .. state.spec .. " until your next prestige")
    runeIcon:SetTexture("Interface\\Icons\\" .. rune.icon)
    runeCore:SetText(rune.core)
    for slot, column in ipairs(columns) do
        column.hint:SetText(HINTS[slot]:format(slot == 1 and rune.resource or rune.spentWith))
        for choice, option in ipairs(column.options) do
            local mod = rune.mods[(slot - 1) * 3 + choice]
            local missing = MissingText(state.needs[rune.id .. "." .. slot .. "." .. choice])
            option.enabled = not missing
            option.name:SetText(mod and mod.name or "")
            option.text:SetText((mod and mod.text or "") .. (missing and ("\n|cffff7766Needs " .. missing .. "|r")
                or ""))
            local picked = state.mods[slot] == choice
            BoxColor(option, picked and PICK_FILL or (missing and OFF_FILL or PANEL_FILL),
                picked and PICK_EDGE or (missing and OFF_EDGE or PANEL_EDGE))
            option.name:SetTextColor(unpack(missing and GREY or (picked and { 1, 1, 1 } or GOLD)))
        end
    end
end

function Refresh()
    if not frame:IsShown() then
        return
    end
    offers:Hide()
    browse:Hide()
    build:Hide()
    locked:SetText("")
    if state.level < state.unlock then
        title:SetText("Legendary Runes")
        subtitle:SetText("")
        locked:SetText("Legendary Runes unlock at level " .. state.unlock .. ".")
        footer:SetText("")
        return
    end
    if view.page == "browse" then
        browse:Show()
        RefreshBrowse()
    elseif state.rune ~= 0 then
        build:Show()
        RefreshBuild()
    else
        offers:Show()
        RefreshOffers()
    end
    if state.error then
        footer:SetText("|cffff7766" .. state.error .. "|r")
    elseif state.rune ~= 0 and view.page ~= "browse" then
        local rune = RuneById(state.rune)
        local done = state.mods[1] > 0 and state.mods[2] > 0 and state.mods[3] > 0
        local name = done and rune and BuildName(rune)
        footer:SetText(done and ("Your rune is complete" .. (name and (": " .. name) or "") .. ".")
            or ("Choose your " .. SLOT_NAMES[state.mods[1] == 0 and 1 or (state.mods[2] == 0 and 2 or 3)]
                .. ". Modifiers change freely out of combat."))
    else
        footer:SetText("")
    end
end

frame:SetScript("OnShow", function()
    state.error = nil
    Send("STATE")
    Refresh()
end)

local function Open()
    if state.rune ~= 0 then
        view.page = "build"
    elseif view.page ~= "browse" then
        view.page = "offers"
    end
    frame:Show()
    Refresh()
end

SLASH_EMBERFORGERUNES1 = "/runes"
SLASH_EMBERFORGERUNES2 = "/rune"
SlashCmdList["EMBERFORGERUNES"] = function()
    if frame:IsShown() then
        frame:Hide()
    else
        Open()
    end
end

-- Messages ---------------------------------------------------------------------------------------------------------

local function OnMessage(message)
    local parts = Split(message, ",")
    local kind = parts[1]
    if kind == "S" then
        state.level, state.unlock, state.spec = tonumber(parts[2]) or 1, tonumber(parts[3]) or 10, tonumber(parts[4])
            or 1
        state.rune = tonumber(parts[5]) or 0
        state.mods = { tonumber(parts[6]) or 0, tonumber(parts[7]) or 0, tonumber(parts[8]) or 0 }
        state.offers = {}
        if parts[9] and parts[9] ~= "-" then
            for _, id in ipairs(Split(parts[9], ".")) do
                state.offers[#state.offers + 1] = tonumber(id)
            end
        end
        state.prompt = parts[10] == "1"
        state.loaded = true
        if state.rune ~= 0 and view.page == "offers" then
            view.page = "build"
        end
        -- Unlocked and unbound: the panel opens by itself, once a session for each specialization.
        if state.prompt and not view.prompted[state.spec] then
            view.prompted[state.spec] = true
            Open()
        end
        Refresh()
    elseif kind == "R" then
        state.built[tonumber(parts[2]) or 0] = parts[3] or "-"
    elseif kind == "N" then
        state.needs[(parts[2] or "") .. "." .. (parts[3] or "") .. "." .. (parts[4] or "")] = parts[5] or "-"
    elseif kind == "E" then
        state.error = message:sub(3)
        Refresh()
    elseif kind == "O" then
        Open()
    end
end

-- Throw Glaive's glow at 5 Fel Command (brighter, with a purple rim, in Metamorphosis): the client has no proc glow,
-- so a pulsing border is drawn over each action button holding Throw Glaive, with Chaos Bolt's icon over it.
local glow = { buttons = {}, phase = 0 }
local BAR_BUTTONS = { "ActionButton", "MultiBarBottomLeftButton", "MultiBarBottomRightButton", "MultiBarRightButton",
    "MultiBarLeftButton", "BonusActionButton" }
local THROW_GLAIVE, FEL_COMMAND = 90111, 90701

local function Stacks()
    local name = GetSpellInfo(FEL_COMMAND)
    if not name then
        return 0
    end
    local _, _, _, count = UnitBuff("player", name)
    return count or 0
end

local function InMetamorphosis()
    local name = GetSpellInfo(90116)
    return name and UnitBuff("player", name) ~= nil
end

local function Overlay(button)
    local overlay = glow.buttons[button]
    if overlay then
        return overlay
    end
    overlay = {}
    overlay.border = button:CreateTexture(nil, "OVERLAY")
    overlay.border:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    overlay.border:SetBlendMode("ADD")
    overlay.border:SetWidth(button:GetWidth() * 1.9)
    overlay.border:SetHeight(button:GetHeight() * 1.9)
    overlay.border:SetPoint("CENTER")
    overlay.icon = button:CreateTexture(nil, "OVERLAY")
    overlay.icon:SetTexture("Interface\\Icons\\Ability_Warlock_ChaosBolt")
    overlay.icon:SetAllPoints()
    overlay.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    overlay.felguard = button:CreateTexture(nil, "OVERLAY")
    overlay.felguard:SetTexture("Interface\\Icons\\Spell_Shadow_SummonFelGuard")
    overlay.felguard:SetWidth(14)
    overlay.felguard:SetHeight(14)
    overlay.felguard:SetPoint("BOTTOMRIGHT", -1, 1)
    glow.buttons[button] = overlay
    return overlay
end

local function UpdateGlow()
    local armed = Stacks() >= 5
    local meta = armed and InMetamorphosis()
    local glaive = GetSpellInfo(THROW_GLAIVE)
    for _, bar in ipairs(BAR_BUTTONS) do
        for index = 1, 12 do
            local button = _G[bar .. index]
            if button and button.action then
                local kind, id = GetActionInfo(button.action)
                local holds = kind == "spell" and id and GetSpellInfo(id) == glaive
                if holds and armed then
                    local overlay = Overlay(button)
                    overlay.border:SetVertexColor(meta and 0.75 or 0.35, meta and 0.45 or 1, meta and 1 or 0.3)
                    overlay.border:Show()
                    overlay.icon:Show()
                    if meta then
                        overlay.felguard:Show()
                    else
                        overlay.felguard:Hide()
                    end
                elseif glow.buttons[button] then
                    for _, texture in pairs(glow.buttons[button]) do
                        texture:Hide()
                    end
                end
            end
        end
    end
    glow.armed = armed
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:RegisterEvent("UNIT_AURA")
events:RegisterEvent("ACTIONBAR_SLOT_CHANGED")
events:RegisterEvent("ACTIONBAR_PAGE_CHANGED")
events:RegisterEvent("PLAYER_LEVEL_UP")
events:SetScript("OnEvent", function(_, event, prefix, message, channel, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and channel == "WHISPER" and sender == UnitName("player") then
            OnMessage(message)
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        Send("STATE")
        UpdateGlow()
    elseif event == "PLAYER_LEVEL_UP" then
        Send("STATE")
    elseif event == "UNIT_AURA" then
        if prefix == "player" then
            UpdateGlow()
        end
    else
        UpdateGlow()
    end
end)
-- The glow pulses.
events:SetScript("OnUpdate", function(_, elapsed)
    if not glow.armed then
        return
    end
    glow.phase = (glow.phase + elapsed * 3) % (2 * math.pi)
    local alpha = 0.55 + 0.45 * math.sin(glow.phase)
    for _, overlay in pairs(glow.buttons) do
        overlay.border:SetAlpha(alpha)
    end
end)

-- For tests and other addons.
EmberforgeRunes_OnMessage = OnMessage
EmberforgeRunes_State = state
EmberforgeRunes_View = view
EmberforgeRunes_Open = Open

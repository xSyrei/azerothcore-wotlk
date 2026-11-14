-- The Halo: the Runes of Enhancement window (src/rune_tiers.cpp, addon whispers with prefix EFRT).
--
-- The Legendary Rune in the centre (clicked: its own panel, Emberforge_Runes.lua), 5 Epic sockets on the inner ring,
-- 10 Rare sockets on the outer one; the Uncommons by ability on the right, under the selected rune's words. An
-- offer (every 5 levels) opens it on a choice of 3; a full set of sockets asks which one to replace. The rune button
-- and /runes open it. The catalog: Tiers.lua (tools/rune_tiers/generate.py).

local ART = "Interface\\AddOns\\Emberforge_Runes\\Art\\"
local FONTS = "Interface\\AddOns\\Emberforge_Runes\\Fonts\\"
local CINZEL, SANS = FONTS .. "Cinzel-Bold.ttf", FONTS .. "AlegreyaSans-Regular.ttf"
local SANS_MEDIUM = FONTS .. "AlegreyaSans-Medium.ttf"
local PREFIX = "EFRT"
local tiers = EmberforgeRuneTiers or { prefixes = {}, effects = {}, runes = {}, abilities = {} }

local W, H = 1100, 680
local C = 300                                           -- the wheel's centre, in its 600 x 600 square
local EPIC_R, RARE_R = 124, 214
local SIZE = { epic = 52, rare = 38 }
local LEGEND = 112
local ROWS = 9                                          -- Uncommon rows shown at once

local COLOR = {
    legend = { 1, 0.70, 0.36 }, epic = { 0.77, 0.48, 1 }, rare = { 0.44, 0.69, 1 }, uncommon = { 0.42, 0.94, 0.42 },
    dormant = { 0.55, 0.57, 0.62 }, gold = { 0.91, 0.76, 0.48 }, ink = { 0.95, 0.91, 0.83 },
    muted = { 0.62, 0.59, 0.53 }, faint = { 0.40, 0.38, 0.44 },
}
local LABEL = { epic = "EPIC", rare = "RARE", uncommon = "UNCOMMON" }

local state = { loaded = false, level = 1, sockets = { epic = 5, rare = 10 }, slots = 10, owned = {}, offers = {} }
local staging
local view = { sel = { kind = "legend" }, offer = 1, replace = nil, scroll = 0 }

local function Send(message)
    if SendAddonMessage and UnitName then
        SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
    end
end

local function Split(text, sep)
    local out = {}
    for piece in (text .. sep):gmatch("(.-)" .. sep:gsub("%p", "%%%0")) do
        out[#out + 1] = piece
    end
    return out
end

-- A rune's words from the catalog: rarity ("epic", "rare", "uncommon"), home ability, name, effect.
local function Info(id)
    local rune = tiers.runes[id]
    if rune then
        local ability = tiers.abilities[rune[2]]
        return { id = id, rarity = rune[1] == 3 and "epic" or "rare", ability = rune[2], name = rune[3],
            text = rune[4], abilityName = ability and ability[1] or (GetSpellInfo and GetSpellInfo(rune[2])) or "",
            spec = rune[6] }
    end
    if id and id >= 1000000 then
        local spell, prefix = math.floor((id - 1000000) / 8), (id - 1000000) % 8
        local ability = tiers.abilities[spell]
        if ability and tiers.prefixes[prefix + 1] then
            return { id = id, rarity = "uncommon", ability = spell, prefix = prefix,
                name = tiers.prefixes[prefix + 1] .. " " .. ability[1], abilityName = ability[1],
                text = ability[1] .. ": " .. tiers.effects[prefix + 1] .. "." }
        end
    end
end

-- Widgets -----------------------------------------------------------------------------------------------------------

local frame = CreateFrame("Frame", "EmberforgeHalo", UIParent)
frame:SetAllPoints(UIParent)
frame:SetFrameStrata("DIALOG")
frame:EnableMouse(true)
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeHalo")             -- Escape closes it

local stage = CreateFrame("Frame", nil, frame)
stage:SetWidth(W)
stage:SetHeight(H)
stage:SetPoint("CENTER")
local function Fit()
    local scale = math.min(UIParent:GetWidth() / W, UIParent:GetHeight() / H, 1.25) * 0.94
    if scale > 0 then
        stage:SetScale(scale)
    end
end
frame:SetScript("OnShow", Fit)

local function Tex(parent, file, size, layer)
    local texture = parent:CreateTexture(nil, layer or "ARTWORK")
    if file then
        texture:SetTexture(ART .. file)
    end
    if size then
        texture:SetWidth(size)
        texture:SetHeight(size)
    end
    return texture
end

local function Text(parent, font, size, color, justify, width)
    local text = parent:CreateFontString(nil, "OVERLAY")
    text:SetFont(font, size)
    text:SetShadowColor(0, 0, 0, 1)
    text:SetShadowOffset(1, -1)
    text:SetTextColor(color[1], color[2], color[3])
    text:SetJustifyH(justify or "LEFT")
    if width then
        text:SetWidth(width)
    end
    return text
end

local function Tint(texture, color, alpha)
    texture:SetVertexColor(color[1], color[2], color[3])
    texture:SetAlpha(alpha or 1)
end

local function Pill(parent, label, width, color)
    local button = CreateFrame("Button", nil, parent)
    button:SetWidth(width)
    button:SetHeight(32)
    button.bg = button:CreateTexture(nil, "BACKGROUND")
    button.bg:SetAllPoints()
    button.bg:SetTexture(0.08, 0.06, 0.04, 0.9)
    button.edge = Tex(button, "HaloSocketDash", nil, "BORDER")
    button.edge:Hide()
    button.text = Text(button, CINZEL, 12, color or COLOR.gold, "CENTER", width)
    button.text:SetPoint("CENTER")
    button.text:SetText(label)
    button.hl = button:CreateTexture(nil, "HIGHLIGHT")
    button.hl:SetAllPoints()
    button.hl:SetTexture(1, 0.8, 0.4, 0.12)
    return button
end

-- The backdrop: near-black, a warm light behind the wheel, gold corner brackets.
local bg = stage:CreateTexture(nil, "BACKGROUND")
bg:SetAllPoints()
bg:SetTexture(0.035, 0.03, 0.05, 0.96)
local warm = Tex(stage, "Soft", 760, "BORDER")
warm:SetPoint("CENTER", stage, "TOPLEFT", 20 + C, -(60 + C))
warm:SetBlendMode("ADD")
Tint(warm, { 0.55, 0.32, 0.12 }, 0.35)
for i, spot in ipairs({ { "TOPLEFT", 14, -14, 0, 1, 0, 1 }, { "TOPRIGHT", -14, -14, 1, 0, 0, 1 },
    { "BOTTOMLEFT", 14, 14, 0, 1, 1, 0 }, { "BOTTOMRIGHT", -14, 14, 1, 0, 1, 0 } }) do
    local corner = Tex(stage, "HaloCorner", 24, "BORDER")
    corner:SetPoint(spot[1], stage, spot[1], spot[2], spot[3])
    corner:SetTexCoord(spot[4], spot[5], spot[6], spot[7])
    Tint(corner, COLOR.gold, 0.7)
end

local close = CreateFrame("Button", nil, stage)
close:SetWidth(32)
close:SetHeight(32)
close:SetPoint("TOPRIGHT", -26, -22)
close.text = Text(close, CINZEL, 18, COLOR.gold, "CENTER", 32)
close.text:SetPoint("CENTER")
close.text:SetText("X")
close:SetScript("OnClick", function() frame:Hide() end)

-- The wheel ---------------------------------------------------------------------------------------------------------

local wheel = CreateFrame("Frame", nil, stage)
wheel:SetWidth(600)
wheel:SetHeight(600)
wheel:SetPoint("TOPLEFT", 20, -60)
local function At(texture, size, x, y)
    texture:SetWidth(size)
    texture:SetHeight(size)
    texture:SetPoint("CENTER", wheel, "TOPLEFT", x, -y)
end
for _, ring in ipairs({ { "HaloTicks", 2 * (RARE_R + 40), COLOR.gold, 0.16 },
    { "HaloRing", 2 * (RARE_R + 30), COLOR.gold, 0.14 }, { "HaloRing", 2 * RARE_R, COLOR.rare, 0.28 },
    { "HaloRing", 2 * EPIC_R, COLOR.epic, 0.34 } }) do
    local texture = Tex(wheel, ring[1], nil, "BACKGROUND")
    At(texture, ring[2], C, C)
    Tint(texture, ring[3], ring[4])
end

local legend = CreateFrame("Button", nil, wheel)
legend:SetWidth(LEGEND)
legend:SetHeight(LEGEND)
legend:SetPoint("CENTER", wheel, "TOPLEFT", C, -C)
legend.icon = legend:CreateTexture(nil, "ARTWORK")
legend.icon:SetPoint("CENTER")
legend.icon:SetWidth(LEGEND - 6)
legend.icon:SetHeight(LEGEND - 6)
legend.gold = Tex(legend, "HaloGold", LEGEND + 6, "OVERLAY")
legend.gold:SetPoint("CENTER")
legend.segments = Tex(wheel, "HaloSegments", LEGEND + 40, "BORDER")
legend.segments:SetPoint("CENTER", legend, "CENTER")
legend.sel = Tex(wheel, "HaloRing", LEGEND + 22, "BORDER")
legend.sel:SetPoint("CENTER", legend, "CENTER")
Tint(legend.sel, COLOR.gold, 0.8)
legend.title = Text(wheel, CINZEL, 11, COLOR.legend, "CENTER", 220)
legend.title:SetPoint("TOP", legend, "BOTTOM", 0, -24)
legend.name = Text(wheel, CINZEL, 14, COLOR.ink, "CENTER", 220)
legend.name:SetPoint("TOP", legend.title, "BOTTOM", 0, -3)

local ringLabels = {
    rare = Text(wheel, CINZEL, 11, COLOR.rare, "CENTER", 200),
    epic = Text(wheel, CINZEL, 11, COLOR.epic, "CENTER", 200),
}
ringLabels.rare:SetPoint("CENTER", wheel, "TOPLEFT", C, -(C - RARE_R))
ringLabels.epic:SetPoint("CENTER", wheel, "TOPLEFT", C, -(C + EPIC_R))

local sockets = { epic = {}, rare = {} }
local Refresh, Pick                                  -- below
for kind, count in pairs({ epic = 5, rare = 10 }) do
    local radius, size = kind == "epic" and EPIC_R or RARE_R, SIZE[kind]
    local offset = kind == "epic" and 0 or 18
    for i = 1, count do
        local angle = math.rad(-90 + offset + (i - 1) * 360 / count)
        local x, y = C + radius * math.cos(angle), C + radius * math.sin(angle)
        local socket = CreateFrame("Button", nil, wheel)
        socket:SetWidth(size)
        socket:SetHeight(size)
        socket:SetPoint("CENTER", wheel, "TOPLEFT", x, -y)
        socket.kind, socket.index = kind, i
        socket.glow = Tex(socket, "Soft", size * 1.9, "BACKGROUND")
        socket.glow:SetPoint("CENTER")
        socket.glow:SetBlendMode("ADD")
        socket.fill = Tex(socket, "HaloFill", size, "BORDER")
        socket.fill:SetPoint("CENTER")
        socket.ring = Tex(socket, "HaloSocketRing", size, "ARTWORK")
        socket.ring:SetPoint("CENTER")
        socket.dash = Tex(socket, "HaloSocketDash", size, "ARTWORK")
        socket.dash:SetPoint("CENTER")
        socket.glyph = Tex(socket, nil, size * 0.5, "OVERLAY")
        socket.glyph:SetPoint("CENTER")
        socket.plus = Tex(socket, "HaloPlus", size * 0.28, "OVERLAY")
        socket.plus:SetPoint("CENTER")
        socket.sel = Tex(socket, "HaloRing", size + 14, "OVERLAY")
        socket.sel:SetPoint("CENTER")
        Tint(socket.sel, COLOR.gold, 0.9)
        socket:SetScript("OnClick", function(self)
            if view.replace and view.replace.rarity == self.kind then
                Pick(view.replace.choice, self.index)
                return
            end
            view.sel = { kind = self.kind, index = self.index }
            Refresh()
        end)
        socket:SetScript("OnEnter", function(self)
            local info = self.rune and Info(self.rune)
            if not info or not GameTooltip.SetOwner then
                return
            end
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:AddLine(info.name, COLOR[info.rarity][1], COLOR[info.rarity][2], COLOR[info.rarity][3])
            GameTooltip:AddLine(info.text, 1, 1, 1, true)
            GameTooltip:Show()
        end)
        socket:SetScript("OnLeave", function() GameTooltip:Hide() end)
        sockets[kind][i] = socket
    end
end

-- The words -----------------------------------------------------------------------------------------------------------

local panel = CreateFrame("Frame", nil, stage)
panel:SetWidth(410)
panel:SetHeight(600)
panel:SetPoint("TOPLEFT", 660, -48)
local header = Text(panel, CINZEL, 12, COLOR.muted)
header:SetPoint("TOPLEFT")
header:SetText("RUNES")
local offersButton = Pill(panel, "", 190, COLOR.legend)
offersButton:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -40, 6)

local detail = {}
detail.rarity = Text(panel, CINZEL, 11, COLOR.gold, "LEFT", 400)
detail.rarity:SetPoint("TOPLEFT", 0, -46)
detail.name = Text(panel, CINZEL, 22, COLOR.ink, "LEFT", 400)
detail.name:SetPoint("TOPLEFT", detail.rarity, "BOTTOMLEFT", 0, -6)
detail.ability = Text(panel, SANS, 14, COLOR.muted, "LEFT", 400)
detail.ability:SetPoint("TOPLEFT", detail.name, "BOTTOMLEFT", 0, -4)
detail.text = Text(panel, SANS, 15, { 0.80, 0.76, 0.68 }, "LEFT", 390)
detail.text:SetPoint("TOPLEFT", detail.ability, "BOTTOMLEFT", 0, -8)
detail.note = Text(panel, SANS, 14, COLOR.dormant, "LEFT", 390)
detail.note:SetPoint("TOPLEFT", detail.text, "BOTTOMLEFT", 0, -8)
detail.action = Pill(panel, "", 170)
detail.action:SetPoint("TOPLEFT", panel, "TOPLEFT", 0, -238)

local counts = Text(panel, CINZEL, 13, COLOR.ink, "LEFT", 410)
counts:SetPoint("TOPLEFT", 0, -290)
local listTitle = Text(panel, CINZEL, 11, COLOR.uncommon, "LEFT", 410)
listTitle:SetPoint("TOPLEFT", 0, -324)

local list = CreateFrame("Frame", nil, panel)
list:SetWidth(410)
list:SetHeight(ROWS * 28)
list:SetPoint("TOPLEFT", 0, -344)
list:EnableMouseWheel(true)
local rows = {}
for i = 1, ROWS do
    local row = CreateFrame("Button", nil, list)
    row:SetWidth(400)
    row:SetHeight(26)
    row:SetPoint("TOPLEFT", 0, -(i - 1) * 28)
    row.name = Text(row, SANS_MEDIUM, 14, COLOR.ink, "LEFT", 150)
    row.name:SetPoint("LEFT", 0, 0)
    row.tags = Text(row, SANS, 13, COLOR.uncommon, "RIGHT", 250)
    row.tags:SetPoint("RIGHT", 0, 0)
    row.hl = row:CreateTexture(nil, "HIGHLIGHT")
    row.hl:SetAllPoints()
    row.hl:SetTexture(1, 0.85, 0.5, 0.08)
    row:SetScript("OnClick", function(self)
        if self.ability then
            view.sel = { kind = "ability", ability = self.ability }
            Refresh()
        end
    end)
    rows[i] = row
end
list:SetScript("OnMouseWheel", function(_, delta)
    view.scroll = view.scroll - delta
    Refresh()
end)

-- The offer ---------------------------------------------------------------------------------------------------------

local offer = CreateFrame("Frame", nil, stage)
offer:SetAllPoints()
offer:SetFrameLevel(stage:GetFrameLevel() + 20)
offer:EnableMouse(true)
offer:Hide()
offer.bg = offer:CreateTexture(nil, "BACKGROUND")
offer.bg:SetAllPoints()
offer.bg:SetTexture(0.02, 0.02, 0.03, 0.94)
offer.eyebrow = Text(offer, CINZEL, 12, COLOR.muted, "CENTER", 600)
offer.eyebrow:SetPoint("TOP", 0, -110)
offer.title = Text(offer, CINZEL, 32, COLOR.ink, "CENTER", 600)
offer.title:SetPoint("TOP", offer.eyebrow, "BOTTOM", 0, -8)
offer.title:SetText("Choose a Rune")
offer.cards = {}
for i = 1, 3 do
    local card = CreateFrame("Button", nil, offer)
    card:SetWidth(250)
    card:SetHeight(320)
    card:SetPoint("TOP", offer, "TOP", (i - 2) * 280, -200)
    card.bg = card:CreateTexture(nil, "BACKGROUND")
    card.bg:SetAllPoints()
    card.bg:SetTexture(1, 1, 1, 0.03)
    card.edge = Tex(card, "HaloRing", 96, "BORDER")
    card.edge:SetPoint("TOP", 0, -26)
    card.fill = Tex(card, "HaloFill", 90, "BORDER")
    card.fill:SetPoint("CENTER", card.edge, "CENTER")
    card.glow = Tex(card, "Soft", 170, "BACKGROUND")
    card.glow:SetPoint("CENTER", card.edge, "CENTER")
    card.glow:SetBlendMode("ADD")
    card.gem = Tex(card, nil, 58, "ARTWORK")
    card.gem:SetPoint("CENTER", card.edge, "CENTER")
    card.rarity = Text(card, CINZEL, 11, COLOR.gold, "CENTER", 230)
    card.rarity:SetPoint("TOP", card.edge, "BOTTOM", 0, -14)
    card.name = Text(card, CINZEL, 18, COLOR.ink, "CENTER", 230)
    card.name:SetPoint("TOP", card.rarity, "BOTTOM", 0, -6)
    card.ability = Text(card, SANS, 13, COLOR.muted, "CENTER", 230)
    card.ability:SetPoint("TOP", card.name, "BOTTOM", 0, -4)
    card.text = Text(card, SANS, 14, { 0.80, 0.76, 0.68 }, "CENTER", 220)
    card.text:SetPoint("TOP", card.ability, "BOTTOM", 0, -10)
    card.hl = card:CreateTexture(nil, "HIGHLIGHT")
    card.hl:SetAllPoints()
    card.hl:SetTexture(1, 0.85, 0.5, 0.06)
    card.index = i
    card:SetScript("OnClick", function(self) Pick(self.index) end)
    offer.cards[i] = card
end
offer.prev = Pill(offer, "<", 44)
offer.prev:SetPoint("TOP", offer, "TOP", -170, -560)
offer.next = Pill(offer, ">", 44)
offer.next:SetPoint("TOP", offer, "TOP", 170, -560)
offer.later = Pill(offer, "DECIDE LATER", 200, COLOR.muted)
offer.later:SetPoint("TOP", offer, "TOP", 0, -560)
offer.later:SetScript("OnClick", function()
    offer:Hide()
    view.replace = nil
    Refresh()
end)
offer.prev:SetScript("OnClick", function()
    view.offer = view.offer - 1
    Refresh()
end)
offer.next:SetScript("OnClick", function()
    view.offer = view.offer + 1
    Refresh()
end)

local banner = Text(stage, CINZEL, 15, COLOR.legend, "CENTER", 600)
banner:SetPoint("TOP", stage, "TOP", -150, -26)
local note = Text(stage, SANS, 14, { 1, 0.55, 0.45 }, "CENTER", 600)
note:SetPoint("BOTTOM", stage, "BOTTOM", -150, 26)

-- Drawing -------------------------------------------------------------------------------------------------------------

local GEM = { epic = "HaloGemHex", rare = "HaloGemDiamond", uncommon = "HaloGemDiamond" }

-- The held runes by kind: socketed[kind][socket] = rune; uncommons[ability] = { prefix -> rune }.
local function Held()
    local socketed, uncommons = { epic = {}, rare = {} }, {}
    for rune, held in pairs(state.owned) do
        local info = Info(rune)
        if info and info.rarity == "uncommon" then
            uncommons[info.ability] = uncommons[info.ability] or {}
            uncommons[info.ability][info.prefix] = rune
        elseif info and socketed[info.rarity] then
            socketed[info.rarity][held.socket] = rune
        end
    end
    return socketed, uncommons
end

local function DrawSocket(socket, rune)
    socket.rune = rune
    local held = rune and state.owned[rune]
    local live = held and not held.dormant
    local color = not rune and COLOR[socket.kind] or live and COLOR[socket.kind] or COLOR.dormant
    local selected = view.sel.kind == socket.kind and view.sel.index == socket.index
    local replacing = view.replace and view.replace.rarity == socket.kind
    Tint(socket.ring, color, rune and 1 or 0)
    Tint(socket.dash, color, rune and 0 or 0.45)
    Tint(socket.plus, color, rune and 0 or 0.55)
    Tint(socket.glow, color, live and 0.22 or 0)
    socket.glyph:SetTexture(ART .. "HaloGlyph" .. ((rune or socket.index) % 10 + 1))
    Tint(socket.glyph, color, rune and 1 or 0)
    socket.sel:SetAlpha((selected or replacing) and 0.9 or 0)
end

local function RuneCount(socketed, kind)
    local n = 0
    for _ in pairs(socketed[kind]) do
        n = n + 1
    end
    return n
end

local function ShowDetail(rarityText, color, name, ability, text, noteText, action, actionHandler)
    detail.rarity:SetText(rarityText)
    detail.rarity:SetTextColor(color[1], color[2], color[3])
    detail.name:SetText(name)
    detail.ability:SetText(ability or "")
    detail.text:SetText(text or "")
    detail.note:SetText(noteText or "")
    detail.action.text:SetText(action or "")
    detail.action.handler = actionHandler
    if action then
        detail.action:Show()
    else
        detail.action:Hide()
    end
end

local function Release(rune, ability)
    if ability then
        Send("RELA," .. ability)
    else
        Send("REL," .. rune)
    end
    view.sel = { kind = "legend" }
end
detail.action:SetScript("OnClick", function(self)
    if self.handler then
        self.handler()
    end
end)

if StaticPopupDialogs then
    StaticPopupDialogs.EMBERFORGE_RUNE_RELEASE = {
        text = "Release %s?\nIt is gone for good (releasing is free).",
        button1 = ACCEPT or "Accept", button2 = CANCEL or "Cancel",
        OnAccept = function(_, data) Release(data.rune, data.ability) end,
        timeout = 0, whileDead = true, hideOnEscape = true,
    }
end

local function AskRelease(label, rune, ability)
    if StaticPopup_Show then
        local popup = StaticPopup_Show("EMBERFORGE_RUNE_RELEASE", label)
        if popup then
            popup.data = { rune = rune, ability = ability }
        end
    end
end

-- The Legendary in the centre: the bound rune of Emberforge_Runes.lua, if any.
local function LegendaryRune()
    local legendaryState = EmberforgeRunes_State
    if not legendaryState or not legendaryState.rune or legendaryState.rune == 0 then
        return nil
    end
    for _, rune in ipairs(EmberforgeRunesCatalog or {}) do
        if rune.id == legendaryState.rune then
            return rune
        end
    end
end

function Refresh()
    if not frame:IsShown() then
        return
    end
    local socketed, uncommons = Held()
    local legendary = LegendaryRune()
    if legendary and legendary.icon and SetPortraitToTexture then
        SetPortraitToTexture(legend.icon, "Interface\\Icons\\" .. legendary.icon)
    else
        legend.icon:SetTexture(ART .. "HaloFill")
    end
    legend.title:SetText("LEGENDARY")
    legend.name:SetText(legendary and legendary.name:gsub("^Rune of t?h?e? ?", "") or "Choose your rune")
    legend.segments:SetAlpha(legendary and 1 or 0.25)
    legend.sel:SetAlpha(view.sel.kind == "legend" and 0.9 or 0)

    for kind, list in pairs(sockets) do
        for i, socket in ipairs(list) do
            DrawSocket(socket, socketed[kind][i])
        end
    end
    ringLabels.rare:SetText(("RARE  %d / %d"):format(RuneCount(socketed, "rare"), state.sockets.rare))
    ringLabels.epic:SetText(("EPIC  %d / %d"):format(RuneCount(socketed, "epic"), state.sockets.epic))

    -- The selected rune's words.
    local sel = view.sel
    if sel.kind == "legend" then
        ShowDetail("LEGENDARY", COLOR.legend, legendary and legendary.name or "No Legendary Rune yet",
            legendary and legendary.role or "", legendary and "Click the medallion to see and change its build."
            or "Click the medallion to choose your Legendary Rune.", nil, nil)
    elseif sel.kind == "ability" then
        local ability = tiers.abilities[sel.ability]
        local held = uncommons[sel.ability] or {}
        local names = {}
        for p = 0, #tiers.prefixes - 1 do
            if held[p] then
                names[#names + 1] = tiers.prefixes[p + 1] .. " (" .. tiers.effects[p + 1] .. ")"
            end
        end
        local anyRune = next(held) and held[next(held)]
        local dormant = anyRune and state.owned[anyRune] and state.owned[anyRune].dormant
        ShowDetail("UNCOMMON", dormant and COLOR.dormant or COLOR.uncommon, ability and ability[1] or "",
            ("%d Uncommon rune%s"):format(#names, #names == 1 and "" or "s"), table.concat(names, "\n"),
            dormant and "Dormant: you no longer know this ability. They wake if you learn it again." or nil,
            "RELEASE ALL", function() AskRelease((ability and ability[1] or "") .. "'s Uncommon runes", nil,
                sel.ability) end)
    else
        local rune = socketed[sel.kind] and socketed[sel.kind][sel.index]
        local info = rune and Info(rune)
        if info then
            local dormant = state.owned[rune].dormant
            ShowDetail(LABEL[sel.kind] .. (dormant and "  ·  DORMANT" or ""), dormant and COLOR.dormant
                or COLOR[sel.kind], info.name, info.abilityName .. (info.spec and ("  ·  " .. info.spec) or ""),
                info.text, dormant and ("Dormant: you no longer know " .. info.abilityName
                .. ". It wakes if you learn it again.") or nil, "RELEASE", function() AskRelease(info.name, rune) end)
        else
            ShowDetail(LABEL[sel.kind] .. "  ·  EMPTY", COLOR[sel.kind], "An empty socket",
                "Filled from a rune offer, every five levels", sel.kind == "epic"
                and "Epic Runes are the big upgrades: each one changes how an ability plays."
                or "Rare Runes are smaller upgrades and interactions between your abilities.", nil, nil)
        end
    end

    -- The counts and the Uncommons by ability.
    local abilities, total = {}, 0
    for ability, held in pairs(uncommons) do
        abilities[#abilities + 1] = ability
        for _ in pairs(held) do
            total = total + 1
        end
    end
    table.sort(abilities, function(a, b)
        return (tiers.abilities[a] and tiers.abilities[a][1] or "") < (tiers.abilities[b] and tiers.abilities[b][1]
            or "")
    end)
    counts:SetText(("|cffc47bffEpic %d/%d|r    |cff6fb0ffRare %d/%d|r    |cff6cf06aUncommon %d|r")
        :format(RuneCount(socketed, "epic"), state.sockets.epic, RuneCount(socketed, "rare"), state.sockets.rare,
        total))
    listTitle:SetText(("UNCOMMON RUNES  ·  %d / %d ABILITIES"):format(#abilities, state.slots))
    view.scroll = math.max(0, math.min(view.scroll, #abilities - ROWS))
    for i, row in ipairs(rows) do
        local ability = abilities[i + view.scroll]
        row.ability = ability
        if ability then
            local data, held = tiers.abilities[ability], uncommons[ability]
            local anyRune = held[next(held)]
            local dormant = state.owned[anyRune] and state.owned[anyRune].dormant
            local tags = {}
            for p = 0, #tiers.prefixes - 1 do
                if data and data[3] and math.floor(data[3] / 2 ^ p) % 2 == 1 then
                    tags[#tags + 1] = (held[p] and (dormant and "|cff8d909c" or "|cff6cf06a") or "|cff4a4552")
                        .. tiers.prefixes[p + 1] .. "|r"
                end
            end
            row.name:SetText(data and data[1] or "")
            row.name:SetTextColor(unpack(dormant and COLOR.dormant or COLOR.ink))
            row.tags:SetText(table.concat(tags, "  "))
            row:Show()
        else
            row:Hide()
        end
    end
    if #abilities == 0 then
        rows[1].name:SetText("None yet: they come from rune offers.")
        rows[1].name:SetTextColor(unpack(COLOR.muted))
        rows[1].tags:SetText("")
        rows[1]:Show()
    end

    -- Offers.
    offersButton.text:SetText(("%d RUNE OFFER%s"):format(#state.offers, #state.offers == 1 and "" or "S"))
    if #state.offers > 0 then
        offersButton:Show()
    else
        offersButton:Hide()
        offer:Hide()
    end
    view.offer = math.max(1, math.min(view.offer, #state.offers))
    local current = state.offers[view.offer]
    if current and offer:IsShown() then
        offer.eyebrow:SetText(("LEVEL %d  ·  OFFER %d OF %d"):format(current.level, view.offer, #state.offers))
        for i, card in ipairs(offer.cards) do
            local info = Info(current.runes[i])
            card.rune = current.runes[i]
            if info then
                local color = COLOR[info.rarity]
                card.gem:SetTexture(ART .. GEM[info.rarity])
                Tint(card.gem, color)
                Tint(card.edge, color, 0.9)
                Tint(card.glow, color, 0.25)
                card.rarity:SetText(LABEL[info.rarity])
                card.rarity:SetTextColor(color[1], color[2], color[3])
                card.name:SetText(info.name)
                card.ability:SetText(info.abilityName)
                card.text:SetText(info.text)
                card:Show()
            else
                card:Hide()
            end
        end
        if #state.offers > 1 then
            offer.prev:Show()
            offer.next:Show()
        else
            offer.prev:Hide()
            offer.next:Hide()
        end
    end
    if view.replace then
        banner:SetText("Choose a socket to replace for " .. view.replace.name .. "  (the offer keeps if you close)")
    else
        banner:SetText("")
    end
end

-- Taking a rune: an Epic or Rare with its sockets full asks which one to replace first.
function Pick(choice, socket)
    local current = state.offers[view.offer]
    if not current then
        return
    end
    if socket then
        Send(("PICK,%d,%d,%d"):format(current.seq, choice, socket))
        view.replace = nil
        view.sel = { kind = Info(current.runes[choice]).rarity, index = socket }
        Refresh()
        return
    end
    local info = Info(current.runes[choice])
    if not info then
        return
    end
    if info.rarity ~= "uncommon" then
        local socketed = Held()
        if RuneCount(socketed, info.rarity) >= state.sockets[info.rarity] then
            view.replace = { rarity = info.rarity, choice = choice, name = info.name }
            offer:Hide()
            Refresh()
            return
        end
    end
    Send(("PICK,%d,%d"):format(current.seq, choice))
    note:SetText("")
    if info.rarity == "uncommon" then
        view.sel = { kind = "ability", ability = info.ability }
    end
end

offersButton:SetScript("OnClick", function()
    view.replace = nil
    offer:Show()
    Refresh()
end)

legend:SetScript("OnClick", function()
    frame:Hide()
    if EmberforgeRunes_Open then
        EmberforgeRunes_Open()
    end
end)
legend:SetScript("OnEnter", function()
    view.sel = { kind = "legend" }
    Refresh()
end)

-- Closing lets go of a socket choice (the offer keeps).
frame:SetScript("OnHide", function()
    view.replace = nil
    offer:Hide()
end)

-- Messages ----------------------------------------------------------------------------------------------------------

local function Open(showOffer)
    if EmberforgeLegendaryRunes and EmberforgeLegendaryRunes:IsShown() then
        EmberforgeLegendaryRunes:Hide()
    end
    frame:Show()
    Fit()
    if showOffer and #state.offers > 0 then
        offer:Show()
    end
    if not state.loaded then
        Send("STATE")
    end
    Refresh()
end

local function OnMessage(message)
    local parts = Split(message, ",")
    local kind = parts[1]
    if kind == "S" then
        staging = { level = tonumber(parts[2]) or 1, sockets = { epic = tonumber(parts[3]) or 5,
            rare = tonumber(parts[4]) or 10 }, slots = tonumber(parts[5]) or 10, owned = {}, offers = {} }
    elseif kind == "T" and staging then
        for _, item in ipairs(Split(parts[2] or "", ".")) do
            local fields = Split(item, ":")
            local rune = tonumber(fields[1])
            if rune then
                staging.owned[rune] = { socket = tonumber(fields[2]) or 0, dormant = fields[3] == "1" }
            end
        end
    elseif kind == "Q" and staging then
        local runes = {}
        for i, id in ipairs(Split(parts[4] or "", ".")) do
            runes[i] = tonumber(id) or 0
        end
        staging.offers[#staging.offers + 1] = { seq = tonumber(parts[2]) or 0, level = tonumber(parts[3]) or 0,
            runes = runes }
    elseif kind == "Z" and staging then
        staging.loaded = true
        state = staging
        staging = nil
        Refresh()
    elseif kind == "O" then
        Open(true)
    elseif kind == "E" then
        note:SetText(message:sub(3))
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:SetScript("OnEvent", function(_, event, prefix, message, channel, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and channel == "WHISPER" and sender == UnitName("player") then
            OnMessage(message)
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        Send("STATE")
    end
end)

-- The rune button and /runes open the Halo; the Legendary's own panel is a click on the medallion away.
local function Toggle()
    if frame:IsShown() then
        frame:Hide()
    else
        Open(#state.offers > 0)
    end
end
local runeButton = EmberforgeRunes_Button
if runeButton and runeButton.GetScript then
    local legendaryClick = runeButton:GetScript("OnClick")
    runeButton:SetScript("OnClick", function(self, button)
        local dragging = self.dragged and GetTime and GetTime() - self.dragged < 0.2
        local resetting = button == "RightButton" and IsShiftKeyDown and IsShiftKeyDown()
        if (dragging or resetting) and legendaryClick then
            return legendaryClick(self, button)
        end
        Toggle()
    end)
end
if SlashCmdList then
    SlashCmdList["EMBERFORGERUNES"] = Toggle
end

-- For tests.
EmberforgeHalo_OnMessage = OnMessage
EmberforgeHalo_State = function() return state end
EmberforgeHalo_View = view
EmberforgeHalo_Debug = { frame = frame, legend = legend, sockets = sockets, detail = detail, rows = rows,
    list = list, offer = offer, offersButton = offersButton, banner = banner, note = note, counts = counts,
    listTitle = listTitle, ringLabels = ringLabels, Info = Info, Open = Open }

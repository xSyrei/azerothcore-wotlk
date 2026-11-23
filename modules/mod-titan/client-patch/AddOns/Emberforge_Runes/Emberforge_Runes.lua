-- Emberforge Runes: the Legendary Rune panel, as the redesign's three mockups draw it: no window, one 16:9 stage over
-- the dimmed game world (1600 x 900 points, scaled to the screen); the rest of the UI is hidden while it is open.
--
-- Recommended for you (from level 1; it opens by itself from 5 while no rune is bound): up to three runes the server
-- suggests for the Hero's abilities and slotted Skill Cards, each a gold-ringed medallion with its name, roles, what
-- it does and Preview rune; Browse all runes. With nothing to suggest it opens on Browse legendary runes: a search
-- (names, abilities, keywords), All / Damage / Healing / Tank, Usable now, the runes in a list (Preview, Choose; while
-- none is usable, Learn Available). The build: the rune's medallion with its three slots about it (a slot opens once
-- the one before has its modifier), "Choose your <slot>" and its three modifiers; a choice is confirmed with Confirm
-- modifier. A preview tries picks here only (Choose this rune binds it, its picks sent along); a bound rune's picks
-- go to the server, and before level 8 it can be unlearned. Its questions and tooltip are the panel's own.
-- The pieces are Art/Ui*.tga (build_rune_ui.py); the runes' words are Catalog.lua and Art.lua (build_rune_art.py).
-- Also here: the changed abilities' icons (Consecration with the Sanctified Paragon; Throw Glaive at 5 Fel Command,
-- glowing) and the rune button.
--
-- The server (mod-titan, src/runes.cpp) talks in addon whispers to the player, prefix EFRN:
--   from server   S,<level>,<unlock level>,<spec>,<rune>,<m1>,<m2>,<m3>,<offers a.b.c or ->,<prompt 0|1>
--                 R,<rune>,<missing>,<missing with cards>,<learn available 0|1>   (a rune the server builds; missing:
--                   groups by '/', one of by '+'; '-' none; with cards: what the slotted Skill Cards won't bring)
--                 L,<rune>,<learn>,<replaced>,<slot>,<card replaced>   Learn Available's plan (asked: PLAN)
--                 N,<rune>,<slot>,<choice>,<missing>   (a modifier's own needs)
--                 E,<text>   O (open the panel)
--                 V,<spell>,<kind>,<amount>   (a rune aura's amount: A absorb, H healing a second, D damage a
--                 second, S slow %; the client only knows its Spell.dbc base, so the tooltips add it)
--   to server     STATE   PICK,<rune>[,1]   MOD,<slot 1-3>,<choice 1-3>   UNBIND,<rune slot> (before level 8)
--                 PLAN,<rune> / APPLY,<rune> (Learn Available: what it would do / do it)
--                 (PICK's 1: the Hero was warned of the abilities it lacks and binds it anyway)

local PREFIX = "EFRN"
local ART = "Interface\\AddOns\\Emberforge_Runes\\Art\\"
local FONTS = "Interface\\AddOns\\Emberforge_Runes\\Fonts\\"
local CINZEL, CINZEL_BLACK = FONTS .. "Cinzel-Bold.ttf", FONTS .. "Cinzel-Black.ttf"
local SANS, SANS_MEDIUM = FONTS .. "AlegreyaSans-Regular.ttf", FONTS .. "AlegreyaSans-Medium.ttf"

local W, H = 1600, 900                                  -- the stage, in points
local SLOT_NAMES = { "Invocation", "Manifestation", "Adaptation" }      -- a rune's own (Art.lua) win
local GOLDEN = { 1, 0.886, 0.659 }                      -- #ffe2a8
local EMBER = { 1, 0.702, 0.361 }                       -- #ffb35c

local catalog = EmberforgeRunesCatalog or {}
local art = EmberforgeRunesArt or { runes = {}, labels = {}, card = { u = 1, v = 1 },
    option = { u = 1, v = 1, margin = 24 }, pill = { u = 1, v = 1, margin = 20, width = 106 }, num = { span = 80 },
    arcane = { rgb = { 0.73, 0.55, 1 }, hot = { 0.92, 0.84, 1 }, ink = { 0.95, 0.9, 1 }, muted = { 0.8, 0.72, 0.9 } } }

-- Two runes a specialization (slots: the first from base, the second from second); rune, mods, unlock and offers
-- are the slot the panel shows (view.slot), copied from slots by UseSlot.
local state = { level = UnitLevel and UnitLevel("player") or 1, unlock = 1, spec = 1, rune = 0, mods = { 0, 0, 0 },
    offers = {}, prompt = false, built = {}, needs = {}, error = nil, loaded = false, base = 1, second = 30,
    allOffers = {}, slots = { { rune = 0, mods = { 0, 0, 0 } }, { rune = 0, mods = { 0, 0, 0 } } } }
-- mode: locked, offers, browse or build; phase: the mockup's (intro, offer, erupt, condense, split, build).
local view = { mode = nil, phase = nil, page = 1, prompted = {}, now = 0, t0 = 0, picks = { 0, 0, 0 }, slot = 1 }

local MAX_OFFERS = 3

-- The panel shows rune slot n: its rune, modifiers, unlock level and suggestions (the other slot's rune left out).
local function UseSlot(n)
    view.slot = n
    local slot = state.slots[n]
    state.rune = slot.rune
    state.mods = { slot.mods[1], slot.mods[2], slot.mods[3] }
    state.unlock = n == 2 and state.second or state.base
    local other = state.slots[n == 1 and 2 or 1].rune
    state.offers = {}
    for _, id in ipairs(state.allOffers) do
        if id ~= other and #state.offers < MAX_OFFERS then
            state.offers[#state.offers + 1] = id
        end
    end
end

-- Either slot's rune (the button, the glows).
local function Bound(id)
    return state.slots[1].rune == id or state.slots[2].rune == id and state.level >= state.second
end

-- A spell's tooltip with a bound rune that reaches into it (the rune's spells: Fire Blast, Flame Shock and Claw for
-- Ember's Wrath): the rune's name and what it adds, in the rune's gold.
local function AddRuneNotes(tooltip)
    local name = tooltip:GetSpell()
    if not name then
        return
    end
    local added = false
    for _, rune in ipairs(catalog) do
        local note = rune.spells and rune.spells[name]
        if note and Bound(rune.id) then
            tooltip:AddLine(rune.name, EMBER[1], EMBER[2], EMBER[3], true)
            tooltip:AddLine(note, GOLDEN[1], GOLDEN[2], GOLDEN[3], true)
            added = true
        end
    end
    if added then
        tooltip:Show()
    end
end
if GameTooltip and GameTooltip.HookScript then
    GameTooltip:HookScript("OnTooltipSetSpell", AddRuneNotes)
end

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

-- The rune the build view shows: the one previewed, else the bound one.
local function BuildRune()
    return view.preview or state.rune
end

-- The roles a rune serves, as the LFG role icons name them (its own list, else its role's words).
local ROLE_ORDER = { "DAMAGER", "HEALER", "TANK" }
local function Roles(rune)
    if rune.roles then
        return rune.roles
    end
    local role = rune.role or ""
    if role:find("^Tank") then
        return { "TANK" }
    elseif role:find("^Healer") then
        return { "HEALER" }
    elseif role:find("^Any") then
        return ROLE_ORDER
    end
    return { "DAMAGER" }
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

local function RuneById(id)
    for _, rune in ipairs(catalog) do
        if rune.id == id then
            return rune
        end
    end
end

-- A rune's standing for this Hero: "ok", "missing" (with what), or "soon" (the server does not build it yet).
-- "cards": the abilities it lacks come with the specialization's slotted Skill Cards.
local function Standing(rune)
    if not state.built[rune.id] then
        return "soon"
    end
    local missing = MissingText(state.built[rune.id])
    if missing and state.withCards and not MissingText(state.withCards[rune.id]) then
        return "cards", missing
    end
    return missing and "missing" or "ok", missing
end

-- The look -----------------------------------------------------------------------------------------------------------
-- The redesign's three mockups ("Recommended for you", "Browse legendary runes", the rune's build): the dimmed world,
-- cream Cinzel titles over a spaced gold eyebrow, gold-ringed round medallions, violet pills, a dark rounded list.
-- The pieces are Art/Ui*.tga (client-patch/build_rune_ui.py), most of them white and tinted here.

local CREAM = { 1, 0.95, 0.88 }
local MUTED = { 0.86, 0.82, 0.9 }
local GOLD = { 0.96, 0.8, 0.42 }
local VIOLET = { 0.74, 0.56, 1 }
local VIOLET_DEEP = { 0.2, 0.1, 0.32 }
local GREEN = { 0.55, 0.95, 0.45 }
local AMBER = { 1, 0.7, 0.3 }
local RED = { 1, 0.45, 0.4 }
local BLUE = { 0.55, 0.75, 1 }
local SANS_ITALIC = FONTS .. "AlegreyaSans-Italic.ttf"
local SERIF = FONTS .. "Marcellus-Regular.ttf"          -- the mockups' titles, names and buttons (OFL)

local function Sound(name)
    if PlaySound then
        PlaySound(name)
    end
end

local function clamp(v, a, b)
    a, b = a or 0, b or 1
    return math.max(a, math.min(b, v))
end

-- A rune's name as the mockups write it ("Red Communion", not "Rune of Red Communion").
local function ShortName(rune)
    return ((rune.name or ""):gsub("^Rune of the ", ""):gsub("^Rune of ", ""))
end

-- Its words without the mockup's **bold** marks.
local function Plain(text)
    return ((text or ""):gsub("%*%*", ""))
end

-- A rune's colours (Art.lua), for its medallion's glow.
local function Theme(runeId)
    local entry = runeId and art.runes[runeId] or nil
    return entry and entry.rgb or VIOLET
end

local ROLE_WORDS = { DAMAGER = "Damage", HEALER = "Healing", TANK = "Tank" }

-- The stage -----------------------------------------------------------------------------------------------------------

-- On the world frame, not the UI's: the panel hides the rest of the UI while it is open (UIParent hidden), as the
-- game's own Alt+Z does; it takes the UI's scale, so it looks as it would on it. It has its own tooltip and its own
-- questions: nothing of the UI's is moved (moving the game's tooltip left it broken until a /reload).
local frame = CreateFrame("Frame", "EmberforgeLegendaryRunes", WorldFrame or UIParent)
frame:SetAllPoints(UIParent)
frame:SetFrameStrata("HIGH")
frame:EnableMouse(false)                             -- clicks off the panel's buttons reach the world
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeLegendaryRunes")

local dim = frame:CreateTexture(nil, "BACKGROUND")
dim:SetTexture(ART .. "UiVignette")
dim:SetAllPoints(frame)

local stage = CreateFrame("Frame", nil, frame)
stage:SetWidth(W)
stage:SetHeight(H)
stage:SetPoint("CENTER")

local function Fit()
    if frame:GetParent() ~= UIParent and UIParent.GetScale then
        frame:SetScale(UIParent:GetScale() or 1)
    end
    local width, height = UIParent:GetWidth(), UIParent:GetHeight()
    local scale = math.min(width / W, height / H)
    if scale and scale > 0 then
        stage:SetScale(scale)
    end
end

-- A region's centre at a stage point (from the top left, down positive), and its size.
local function At(region, x, y, width, height)
    region:ClearAllPoints()
    region:SetPoint("CENTER", stage, "TOPLEFT", x, -y)
    if width then
        region:SetWidth(width)
        region:SetHeight(height or width)
    end
end

local function Tex(parent, layer, file, blend)
    local texture = parent:CreateTexture(nil, layer or "ARTWORK")
    if file then
        texture:SetTexture(ART .. file)
    end
    if blend then
        texture:SetBlendMode(blend)
    end
    return texture
end

local function Text(parent, font, size, color, justify, width)
    local text = parent:CreateFontString(nil, "OVERLAY")
    text:SetFont(font, size)
    text:SetShadowOffset(1, -1)
    text:SetShadowColor(0, 0, 0, 0.85)
    text:SetTextColor(color[1], color[2], color[3])
    text:SetJustifyH(justify or "CENTER")
    if width then
        text:SetWidth(width)
    end
    return text
end

-- Spaced capitals, as the mockups' eyebrows and slot names.
local function Spaced(text)
    return (text:upper():gsub("(.)", "%1 "):gsub(" $", ""):gsub("   ", "  "))
end

local function RoundIcon(texture, icon)
    local path = icon and icon:find("\\") and icon or ("Interface\\Icons\\" .. (icon or "INV_Misc_Rune_01"))
    if SetPortraitToTexture then
        SetPortraitToTexture(texture, path)
    else
        texture:SetTexture(path)
    end
end

-- Views: each a frame over the stage that fades in and out.
local fades = {}
local function View(parent)
    local view = CreateFrame("Frame", nil, parent or stage)
    view:SetAllPoints(stage)
    view.alpha, view.target = 0, 0
    view:SetAlpha(0)
    view:Hide()
    fades[#fades + 1] = view
    return view
end
local function Fade(view, on, instant)
    view.target = on and 1 or 0
    if instant then
        view.alpha = view.target
    end
    if on then
        view:Show()
    elseif view.alpha <= 0 then
        view:Hide()
    end
    view:SetAlpha(view.alpha)
end
local function StepFades(dt)
    for _, view in ipairs(fades) do
        if view.alpha ~= view.target then
            local step = dt / 0.25
            view.alpha = view.target > view.alpha and math.min(1, view.alpha + step) or math.max(0, view.alpha - step)
            view:SetAlpha(view.alpha)
            if view.alpha <= 0 then
                view:Hide()
            end
        end
    end
end

-- Three slices of a capsule texture (its ends round at any width).
local function Capsule(parent, file, layer)
    local pieces = {}
    for index = 1, 3 do
        pieces[index] = Tex(parent, layer or "BACKGROUND", file)
    end
    pieces[1]:SetTexCoord(0, 0.125, 0, 1)
    pieces[2]:SetTexCoord(0.125, 0.875, 0, 1)
    pieces[3]:SetTexCoord(0.875, 1, 0, 1)
    function pieces:Place(width, height)
        local cap = height / 2
        self[1]:ClearAllPoints()
        self[1]:SetPoint("LEFT", parent, "LEFT", 0, 0)
        self[1]:SetWidth(cap)
        self[1]:SetHeight(height)
        self[3]:ClearAllPoints()
        self[3]:SetPoint("RIGHT", parent, "RIGHT", 0, 0)
        self[3]:SetWidth(cap)
        self[3]:SetHeight(height)
        self[2]:ClearAllPoints()
        self[2]:SetPoint("LEFT", self[1], "RIGHT", 0, 0)
        self[2]:SetPoint("RIGHT", self[3], "LEFT", 0, 0)
        self[2]:SetHeight(height)
    end
    function pieces:Color(r, g, b, a)
        for index = 1, 3 do
            self[index]:SetVertexColor(r, g, b, a or 1)
        end
    end
    return pieces
end

-- The pill button, in the mockups' looks: primary (filled violet, glowing), outline (violet edge), gold (gold
-- edge), navy (the build's: dark blue, gold edge), off (grey: can't be pressed).
local PILL_LOOKS = {
    primary = { fill = { 0.27, 0.13, 0.45, 0.96 }, edge = { 0.8, 0.6, 1 }, ink = { 1, 1, 1 }, glow = 0.45 },
    outline = { fill = { 0.11, 0.06, 0.18, 0.88 }, edge = { 0.72, 0.52, 0.98 }, ink = { 0.96, 0.92, 1 } },
    gold = { fill = { 0.06, 0.04, 0.03, 0.7 }, edge = { 0.95, 0.8, 0.38 }, ink = CREAM },
    navy = { fill = { 0.05, 0.07, 0.17, 0.92 }, edge = { 0.9, 0.76, 0.38 }, ink = CREAM },
    off = { fill = { 0.13, 0.13, 0.15, 0.75 }, edge = { 0.42, 0.42, 0.45 }, ink = { 0.58, 0.58, 0.6 } },
}
local function Pill(parent, width, height, text, look, onClick)
    local pill = CreateFrame("Button", nil, parent)
    pill:SetWidth(width)
    pill:SetHeight(height)
    pill.glow = Tex(pill, "BACKGROUND", "UiPillGlow", "ADD")
    pill.glow:SetPoint("CENTER")
    pill.glow:SetWidth(width + 60)
    pill.glow:SetHeight(height * 2)
    pill.fill = Capsule(pill, "UiPillFill", "BORDER")
    pill.edge = Capsule(pill, "UiPillEdge", "ARTWORK")
    pill.fill:Place(width, height)
    pill.edge:Place(width, height)
    pill.label = Text(pill, SERIF, math.floor(height * 0.36), CREAM)
    pill.label:SetPoint("CENTER", 0, 1)
    pill.label:SetText(text)
    pill.enabled = true
    function pill:Look(name)
        self.look = name
        local spec = PILL_LOOKS[self.enabled and name or "off"]
        local hot = self.hot and self.enabled
        self.fill:Color(spec.fill[1], spec.fill[2], spec.fill[3], spec.fill[4])
        local k = hot and 1.15 or 1
        self.edge:Color(math.min(1, spec.edge[1] * k), math.min(1, spec.edge[2] * k), math.min(1, spec.edge[3] * k))
        self.label:SetTextColor(spec.ink[1], spec.ink[2], spec.ink[3])
        local glow = (spec.glow or 0) + (hot and 0.3 or 0)
        self.glow:SetVertexColor(spec.edge[1], spec.edge[2], spec.edge[3])
        self.glow:SetAlpha(glow)
    end
    function pill:SetLabel(label)
        self.label:SetText(label)
    end
    function pill:SetEnabled(on)
        self.enabled = on and true or false
        self:Look(self.look)
    end
    function pill:SetOn(on)
        self.on = on
        self:Look(on and "primary" or self.base)
    end
    function pill:Resize(newWidth)
        self:SetWidth(newWidth)
        self.glow:SetWidth(newWidth + 60)
    end
    pill.base = look or "outline"
    pill:SetScript("OnEnter", function(self)
        self.hot = true
        self:Look(self.look)
        if self.tip then
            self.tip(self)
        end
    end)
    pill:SetScript("OnLeave", function(self)
        self.hot = false
        self:Look(self.look)
        EmberforgeRunesTip:Hide()
    end)
    pill:SetScript("OnClick", function(self, button)
        if self.enabled and onClick then
            Sound("igMainMenuOptionCheckBoxOn")
            onClick(self, button)
        end
    end)
    pill:Look(pill.base)
    return pill
end

-- The panel's own tooltip (over the panel; the game's stays where it is).
local tip = CreateFrame("GameTooltip", "EmberforgeRunesTip", frame, "GameTooltipTemplate")
tip:SetFrameStrata("TOOLTIP")
local function Tip(owner, title, lines)
    tip:SetOwner(owner, "ANCHOR_RIGHT")
    tip:AddLine(title, GOLDEN[1], GOLDEN[2], GOLDEN[3])
    for _, line in ipairs(lines or {}) do
        tip:AddLine(line[1], line[2] or 0.85, line[3] or 0.82, line[4] or 0.9, true)
    end
    tip:Show()
end

-- The medallion: the rune's round icon in the gold ring, its colour glowing behind.
local function Medal(parent, size)
    local medal = CreateFrame("Frame", nil, parent)
    medal:SetWidth(size)
    medal:SetHeight(size)
    medal.glow = Tex(medal, "BACKGROUND", "Soft", "ADD")
    medal.glow:SetPoint("CENTER")
    medal.glow:SetWidth(size * 1.7)
    medal.glow:SetHeight(size * 1.7)
    medal.icon = Tex(medal, "ARTWORK")
    medal.icon:SetPoint("CENTER")
    medal.icon:SetWidth(size * 0.81)
    medal.icon:SetHeight(size * 0.81)
    medal.ring = Tex(medal, "OVERLAY", "UiRing")
    medal.ring:SetAllPoints()
    function medal:SetRune(rune)
        self.rune = rune
        RoundIcon(self.icon, rune and rune.icon)
        local rgb = Theme(rune and rune.id)
        self.glow:SetVertexColor(rgb[1], rgb[2], rgb[3])
        self.glow:SetAlpha(0.32)
    end
    return medal
end

-- The eyebrow: spaced gold capitals between two gold lines fading outward.
local function Eyebrow(parent, y, label)
    local eyebrow = Text(parent, CINZEL, 17, GOLD)
    At(eyebrow, W / 2, y)
    eyebrow:SetText(Spaced(label))
    local width = (eyebrow:GetStringWidth() or 300) / 2 + 26
    for side = -1, 1, 2 do
        local line = Tex(parent, "ARTWORK", "UiLineFade")
        At(line, W / 2 + side * (width + 40), y, 80, 3)
        line:SetVertexColor(GOLD[1], GOLD[2], GOLD[3], 0.9)
        if side < 0 then
            line:SetTexCoord(0, 1, 0, 1)
        else
            line:SetTexCoord(1, 0, 0, 1)
        end
    end
    return eyebrow
end

-- The role pill under a card's name: the LFG role icon and the role's word, in a violet edge.
local function RoleCoords(role)
    if GetTexCoordsForRole then
        return GetTexCoordsForRole(role)
    end
    if role == "TANK" then
        return 0, 19 / 64, 22 / 64, 41 / 64
    elseif role == "HEALER" then
        return 20 / 64, 39 / 64, 1 / 64, 20 / 64
    end
    return 20 / 64, 39 / 64, 22 / 64, 41 / 64
end
local function RolePill(parent)
    local pill = CreateFrame("Frame", nil, parent)
    pill:SetHeight(40)
    pill.edge = Capsule(pill, "UiPillEdge", "ARTWORK")
    pill.fill = Capsule(pill, "UiPillFill", "BACKGROUND")
    pill.icons = {}
    for index = 1, 3 do
        local icon = pill:CreateTexture(nil, "OVERLAY")
        icon:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-ROLES")
        icon:SetWidth(22)
        icon:SetHeight(22)
        pill.icons[index] = icon
    end
    pill.label = Text(pill, CINZEL, 15, CREAM)
    function pill:SetRoles(roles)
        local words = {}
        for index, icon in ipairs(self.icons) do
            if roles[index] then
                icon:SetTexCoord(RoleCoords(roles[index]))
                icon:Show()
                words[#words + 1] = (ROLE_WORDS[roles[index]] or ""):upper()
            else
                icon:Hide()
            end
        end
        self.label:SetText(table.concat(words, " / "))
        local width = #roles * 24 + (self.label:GetStringWidth() or 60) + 36
        self:SetWidth(width)
        self.edge:Place(width, 40)
        self.fill:Place(width, 40)
        self.edge:Color(VIOLET[1], VIOLET[2], VIOLET[3], 0.9)
        self.fill:Color(0.08, 0.05, 0.12, 0.75)
        for index, icon in ipairs(self.icons) do
            icon:ClearAllPoints()
            icon:SetPoint("LEFT", self, "LEFT", 14 + (index - 1) * 24, 0)
        end
        self.label:ClearAllPoints()
        self.label:SetPoint("LEFT", self, "LEFT", 18 + #roles * 24, 0)
    end
    return pill
end

-- A rounded box, cut nine ways (Art/UiBoxFill, UiBoxEdge: 64 px, 16 px corners).
local function Box(parent, file, corner, layer)
    local pieces = {}
    local cuts = { { 0, 0.25 }, { 0.25, 0.75 }, { 0.75, 1 } }
    for row = 1, 3 do
        for column = 1, 3 do
            local piece = Tex(parent, layer or "BACKGROUND", file)
            piece:SetTexCoord(cuts[column][1], cuts[column][2], cuts[row][1], cuts[row][2])
            local h = ({ "LEFT", "LEFT", "RIGHT" })[column]
            local v = ({ "TOP", "TOP", "BOTTOM" })[row]
            local x, y = ({ 0, corner, 0 })[column], ({ 0, -corner, 0 })[row]
            piece:SetPoint(v .. h, parent, v .. h, x, y)
            if column == 2 then
                piece:SetPoint(v .. "RIGHT", parent, v .. "RIGHT", -corner, y)
            else
                piece:SetWidth(corner)
            end
            if row == 2 then
                piece:SetPoint("BOTTOM" .. h, parent, "BOTTOM" .. h, column == 2 and corner or x, corner)
            else
                piece:SetHeight(corner)
            end
            pieces[#pieces + 1] = piece
        end
    end
    function pieces:Color(r, g, b, a)
        for _, piece in ipairs(self) do
            piece:SetVertexColor(r, g, b, a or 1)
        end
    end
    return pieces
end

-- Common to every view: the close button, the note (a refusal, a hint), Rune I and Rune II.
local chrome = View()
chrome.close = CreateFrame("Button", nil, chrome)
At(chrome.close, 1548, 52, 44, 44)
chrome.close.x = Tex(chrome.close, "ARTWORK", "UiX")
chrome.close.x:SetAllPoints()
chrome.close.x:SetVertexColor(GOLD[1], GOLD[2], GOLD[3])
chrome.close:SetScript("OnClick", function() frame:Hide() end)
chrome.close:SetScript("OnEnter", function(self) self.x:SetVertexColor(1, 0.95, 0.75) end)
chrome.close:SetScript("OnLeave", function(self) self.x:SetVertexColor(GOLD[1], GOLD[2], GOLD[3]) end)
chrome.note = Text(chrome, SANS_MEDIUM, 19, AMBER, "CENTER", 1100)
At(chrome.note, W / 2, 20)                          -- above everything: no column or row can cover it
chrome.runeTabs = {}

local function Note(text)
    chrome.note:SetText(text or "")
    chrome.note.until_ = view.now + 3.5
    chrome.note:SetAlpha(1)
end

-- Questions (a bind, a missing ability, Learn Available's plan, an unlearn), the panel's own: a dark box over it.
local ask = CreateFrame("Frame", nil, frame)
ask:SetAllPoints(frame)
ask:SetFrameStrata("DIALOG")
ask:EnableMouse(true)
ask:Hide()
do
    local shade = ask:CreateTexture(nil, "BACKGROUND")
    shade:SetTexture(0, 0, 0, 0.55)
    shade:SetAllPoints()
end
ask.box = CreateFrame("Frame", nil, ask)
ask.box:SetScale(1)
ask.boxFill = Box(ask.box, "UiBoxFill", 16, "BACKGROUND")
ask.boxEdge = Box(ask.box, "UiBoxEdge", 16, "BORDER")
ask.boxFill:Color(0.07, 0.05, 0.1, 0.97)
ask.boxEdge:Color(GOLD[1], GOLD[2], GOLD[3], 0.85)
ask.title = Text(ask.box, SERIF, 26, CREAM, "CENTER", 600)
ask.title:SetPoint("TOP", 0, -28)
ask.text = Text(ask.box, SANS, 21, MUTED, "CENTER", 600)
ask.text:SetPoint("TOP", ask.title, "BOTTOM", 0, -16)
ask.yes = Pill(ask.box, 230, 50, "Accept", "primary", function()
    ask:Hide()
    if ask.onYes then
        ask.onYes()
    end
end)
ask.no = Pill(ask.box, 200, 50, "Cancel", "outline", function() ask:Hide() end)

-- Asks: a title, words (may hold colour codes), the yes button's label, and what yes does.
local function Ask(title, text, yes, onYes, warning)
    ask.box:SetParent(ask)
    ask.title:SetText(title)
    ask.title:SetTextColor(unpack(warning and RED or CREAM))
    ask.text:SetText(text)
    ask.yes:SetLabel(yes or "Accept")
    ask.onYes = onYes
    local height = 28 + (ask.title:GetStringHeight() or 30) + 16 + (ask.text:GetStringHeight() or 60) + 40 + 50 + 30
    ask.box:ClearAllPoints()
    ask.box:SetPoint("CENTER", stage, "CENTER", 0, 30)
    ask.box:SetWidth(680)
    ask.box:SetHeight(height)
    ask.box:SetScale(stage:GetScale() or 1)
    ask.yes:ClearAllPoints()
    ask.yes:SetPoint("BOTTOMRIGHT", ask.box, "BOTTOM", -10, 28)
    ask.no:ClearAllPoints()
    ask.no:SetPoint("BOTTOMLEFT", ask.box, "BOTTOM", 10, 28)
    ask:Show()
end

-- The flow's names, set below.
local Refresh, Enter, ShowBuild, Fill, AskBind, OpenPreview

-- Recommended for you -------------------------------------------------------------------------------------------------

local offers = View()
offers.eyebrow = Eyebrow(offers, 72, "Legendary Runes")
offers.title = Text(offers, SERIF, 52, CREAM)
At(offers.title, W / 2, 132)
offers.subtitle = Text(offers, SANS, 23, MUTED)
At(offers.subtitle, W / 2, 186)
offers.cards = {}
for index = 1, MAX_OFFERS do
    local card = {}
    card.medal = Medal(offers, 220)
    card.hit = CreateFrame("Button", nil, offers)
    card.hit:SetWidth(220)
    card.hit:SetHeight(220)
    card.hit:SetScript("OnClick", function(self)
        if self.rune then
            OpenPreview(self.rune.id)
        end
    end)
    card.hit:SetScript("OnEnter", function(self)
        self.card.hover = true
        self.card.medal.glow:SetAlpha(0.6)
    end)
    card.hit:SetScript("OnLeave", function(self) self.card.hover = false end)
    card.hit.card = card
    card.name = Text(offers, SERIF, 34, CREAM)
    card.role = RolePill(offers)
    card.desc = Text(offers, SANS, 23, MUTED, "CENTER", 370)
    card.status = Text(offers, SANS_MEDIUM, 18, AMBER, "CENTER", 360)
    card.preview = Pill(offers, 320, 54, "Preview rune", "outline", function(self)
        if self.rune then
            OpenPreview(self.rune.id)
        end
    end)
    offers.cards[index] = card
end
offers.browse = Pill(offers, 370, 60, "Browse all runes", "gold", function()
    view.mode = "browse"
    Enter()
end)
At(offers.browse, W / 2, 846)
offers.empty = Text(offers, SANS, 22, MUTED, "CENTER", 800)
At(offers.empty, W / 2, 420)
offers.empty:SetText("None of the runes fits your abilities yet. Browse all to see what each one needs.")

-- The runes suggested to the slot shown.
local function Suggested()
    local list = {}
    for _, id in ipairs(state.offers) do
        local rune = RuneById(id)
        if rune and not rune.retired then
            list[#list + 1] = rune
        end
    end
    return list
end

local OFFER_X = { { 800 }, { 560, 1040 }, { 370, 800, 1230 } }
local function ShowOffers()
    local list = Suggested()
    offers.title:SetText(view.slot == 2 and "Your second rune" or "Recommended for you")
    offers.subtitle:SetText("Based on your abilities and the Skill Cards you have slotted.")
    local layout = OFFER_X[#list] or {}
    for index, card in ipairs(offers.cards) do
        local rune = list[index]
        local x = layout[index]
        local shown = rune ~= nil
        for _, region in ipairs({ card.medal, card.hit, card.name, card.role, card.desc, card.status, card.preview }) do
            if shown then
                region:Show()
            else
                region:Hide()
            end
        end
        if rune then
            card.hit.rune, card.preview.rune = rune, rune
            card.medal:SetRune(rune)
            At(card.medal, x, 346)
            At(card.hit, x, 346)
            card.name:SetText(ShortName(rune))
            At(card.name, x, 486)
            card.role:SetRoles(Roles(rune))
            At(card.role, x, 542)
            card.desc:SetText(rune.loop or "")
            At(card.desc, x, 612)
            local standing, missing = Standing(rune)
            card.status:SetText(standing == "cards" and ("Your cards bring " .. missing) or "")
            At(card.status, x, 664)
            At(card.preview, x, 720)
        end
    end
    if #list == 0 then
        offers.empty:Show()
    else
        offers.empty:Hide()
    end
    Fade(offers, true)
end

-- Browse legendary runes ----------------------------------------------------------------------------------------------

local browser = View()
local browse = { query = "", role = nil, usable = false, offset = 0, list = {} }
local ROWS, ROW_H, LIST_TOP = 4, 134, 262
browser.eyebrow = Eyebrow(browser, 54, "Legendary Runes")
browser.title = Text(browser, SERIF, 40, CREAM)
At(browser.title, W / 2, 98)
browser.title:SetText("Browse legendary runes")
do
    local diamond = Tex(browser, "ARTWORK", "UiDiamond")
    At(diamond, W / 2, 134, 22, 22)
    diamond:SetVertexColor(GOLD[1], GOLD[2], GOLD[3])
    for side = -1, 1, 2 do
        local line = Tex(browser, "ARTWORK", "UiLineFade")
        At(line, W / 2 + side * 84, 134, 140, 3)
        line:SetVertexColor(GOLD[1], GOLD[2], GOLD[3], 0.9)
        line:SetTexCoord(side < 0 and 0 or 1, side < 0 and 1 or 0, 0, 1)
    end
end

-- The search: a violet capsule, its lens, its hint.
local search = CreateFrame("EditBox", nil, browser)
At(search, 392, 192, 600, 52)
search:SetAutoFocus(false)
search:SetFont(SANS, 22)
search:SetTextInsets(58, 20, 0, 0)
search:SetTextColor(CREAM[1], CREAM[2], CREAM[3])
search.fill = Capsule(search, "UiPillFill", "BACKGROUND")
search.edge = Capsule(search, "UiPillEdge", "BORDER")
search.fill:Place(600, 52)
search.edge:Place(600, 52)
search.fill:Color(0.07, 0.05, 0.1, 0.85)
search.edge:Color(VIOLET[1], VIOLET[2], VIOLET[3], 0.85)
search.lens = Tex(search, "ARTWORK", "UiLens")
search.lens:SetPoint("LEFT", 22, 0)
search.lens:SetWidth(24)
search.lens:SetHeight(24)
search.lens:SetVertexColor(CREAM[1], CREAM[2], CREAM[3])
search.hint = Text(search, SANS, 22, { 0.7, 0.66, 0.74 }, "LEFT")
search.hint:SetPoint("LEFT", 58, 0)
search.hint:SetText("Search by name, ability, or keyword")
search:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
search:SetScript("OnEnterPressed", function(self) self:ClearFocus() end)

for _, x in ipairs({ 726, 1300 }) do
    local bar = Tex(browser, "ARTWORK")
    bar:SetTexture(1, 1, 1)
    bar:SetVertexColor(GOLD[1], GOLD[2], GOLD[3], 0.8)
    At(bar, x, 192, 2, 46)
end

-- The role chips (one at a time: All clears it) and the Usable now switch.
local chips = {}
for index, chip in ipairs({ { "All", nil, 806, 100 }, { "Damage", "DAMAGER", 934, 132 },
    { "Healing", "HEALER", 1078, 134 }, { "Tank", "TANK", 1212, 112 } }) do
    local pill = Pill(browser, chip[4], 50, chip[1], "outline", function()
        browse.role = chip[2]
        browse.offset = 0
        Fill()
    end)
    pill.label:SetFont(SANS_MEDIUM, 21)
    pill.role = chip[2]
    At(pill, chip[3], 192)
    chips[index] = pill
end
local toggle = CreateFrame("Button", nil, browser)
At(toggle, 1430, 192, 250, 50)
toggle.track = Capsule(toggle, "UiPillFill", "BACKGROUND")
toggle.trackEdge = Capsule(toggle, "UiPillEdge", "BORDER")
toggle.knob = Tex(toggle, "ARTWORK", "UiDisc")
toggle.knob:SetWidth(28)
toggle.knob:SetHeight(28)
toggle.label = Text(toggle, SANS, 22, CREAM, "LEFT")
toggle.label:SetPoint("LEFT", toggle, "LEFT", 96, 0)
toggle.label:SetText("Usable now")
do
    -- The track is the switch's left 72 points.
    local holder = CreateFrame("Frame", nil, toggle)
    holder:SetPoint("LEFT", toggle, "LEFT", 6, 0)
    holder:SetWidth(72)
    holder:SetHeight(38)
    toggle.track = Capsule(holder, "UiPillFill", "BACKGROUND")
    toggle.trackEdge = Capsule(holder, "UiPillEdge", "BORDER")
    toggle.track:Place(72, 38)
    toggle.trackEdge:Place(72, 38)
    toggle.holder = holder
end
toggle:SetScript("OnClick", function()
    browse.usable = not browse.usable
    browse.offset = 0
    Sound("igMainMenuOptionCheckBoxOn")
    Fill()
end)
local function DrawToggle()
    local on = browse.usable
    toggle.track:Color(on and VIOLET_DEEP[1] * 2 or 0.12, on and VIOLET_DEEP[2] * 2 or 0.12,
        on and VIOLET_DEEP[3] * 2 or 0.14, 0.95)
    toggle.trackEdge:Color(on and VIOLET[1] or 0.5, on and VIOLET[2] or 0.5, on and VIOLET[3] or 0.55, 0.9)
    toggle.knob:ClearAllPoints()
    toggle.knob:SetPoint("CENTER", toggle.holder, "LEFT", on and 52 or 20, 0)
    toggle.knob:SetVertexColor(on and 1 or 0.75, on and 0.95 or 0.75, on and 1 or 0.78)
end

-- The list: a dark rounded panel, four rows, a scroll bar.
local listPanel = CreateFrame("Frame", nil, browser)
At(listPanel, 805, LIST_TOP + ROWS * ROW_H / 2 - 4, 1480, ROWS * ROW_H + 16)
Box(listPanel, "UiBoxFill", 16, "BACKGROUND"):Color(0.07, 0.05, 0.1, 0.84)
Box(listPanel, "UiBoxEdge", 16, "BORDER"):Color(0.5, 0.38, 0.66, 0.55)
listPanel:EnableMouseWheel(true)
local rows = {}
for index = 1, ROWS do
    local y = LIST_TOP + (index - 1) * ROW_H + ROW_H / 2
    local row = CreateFrame("Button", nil, browser)
    At(row, 805, y, 1440, ROW_H - 6)
    row.hover = Tex(row, "BACKGROUND")
    row.hover:SetTexture(1, 1, 1)
    row.hover:SetAllPoints()
    row.hover:SetVertexColor(VIOLET[1], VIOLET[2], VIOLET[3], 0)
    if index < ROWS then
        row.rule = Tex(browser, "ARTWORK", "UiLine")
        At(row.rule, 805, y + ROW_H / 2, 1400, 2)
        row.rule:SetVertexColor(0.6, 0.5, 0.75, 0.35)
    end
    row.medal = Medal(row, 112)
    At(row.medal, 172, y)
    -- Its name, roles, and what it does in up to two lines (never cut off: smaller words when longer).
    row.name = Text(row, SERIF, 30, CREAM, "LEFT", 580)
    row.name:SetPoint("TOPLEFT", stage, "TOPLEFT", 250, -(y - 58))
    row.role = Text(row, SANS_MEDIUM, 20, VIOLET, "LEFT", 580)
    row.role:SetPoint("TOPLEFT", stage, "TOPLEFT", 252, -(y - 22))
    row.desc = Text(row, SANS, 20, CREAM, "LEFT", 584)
    row.desc:SetPoint("TOPLEFT", stage, "TOPLEFT", 252, -(y + 2))
    row.bar = Tex(row, "ARTWORK")
    row.bar:SetTexture(1, 1, 1)
    row.bar:SetVertexColor(GOLD[1], GOLD[2], GOLD[3], 0.6)
    At(row.bar, 850, y, 2, 80)
    row.status = Text(row, SANS_MEDIUM, 23, GREEN, "LEFT", 250)
    row.status:SetPoint("LEFT", stage, "TOPLEFT", 882, -y)
    row.preview = Pill(row, 168, 54, "Preview", "outline", function(self)
        local rune = self:GetParent().rune
        if rune then
            OpenPreview(rune.id)
        end
    end)
    At(row.preview, 1218, y)
    row.choose = Pill(row, 180, 54, "Choose", "primary", function(self)
        local parent = self:GetParent()
        if parent.rune and parent.learn then
            Send("PLAN," .. parent.rune.id)
        elseif parent.rune then
            AskBind(parent.rune)
        end
    end)
    At(row.choose, 1418, y)
    row:SetScript("OnEnter", function(self) self.hover:SetVertexColor(VIOLET[1], VIOLET[2], VIOLET[3], 0.06) end)
    row:SetScript("OnLeave", function(self) self.hover:SetVertexColor(VIOLET[1], VIOLET[2], VIOLET[3], 0) end)
    row:SetScript("OnClick", function(self)
        if self.rune then
            OpenPreview(self.rune.id)
        end
    end)
    row:Hide()
    rows[index] = row
end
local track = Tex(browser, "ARTWORK")
track:SetTexture(1, 1, 1)
track:SetVertexColor(1, 1, 1, 0.08)
At(track, 1532, LIST_TOP + ROWS * ROW_H / 2 - 4, 6, ROWS * ROW_H - 30)
local thumb = Tex(browser, "OVERLAY")
thumb:SetTexture(1, 1, 1)
thumb:SetVertexColor(VIOLET[1], VIOLET[2], VIOLET[3], 0.9)
thumb:SetWidth(6)
browser.back = Pill(browser, 310, 52, "Back to suggestions", "gold", function()
    view.mode = "offers"
    Enter()
end)
At(browser.back, 250, 846)
browser.count = Text(browser, SANS, 24, CREAM, "RIGHT", 300)
browser.count:SetPoint("RIGHT", stage, "TOPLEFT", 1530, -846)
browser.empty = Text(browser, SANS, 22, MUTED, "CENTER", 900)
At(browser.empty, 805, 500)
browser.track, browser.thumb = track, thumb

-- Every word of a rune the search can find, lower case (made once).
local SEARCH_ROLES = { DAMAGER = "damage dps damager", HEALER = "healer healing heal", TANK = "tank tanking" }
local haystacks = {}
local function Haystack(rune)
    if haystacks[rune.id] then
        return haystacks[rune.id]
    end
    local words = { rune.name, rune.role, rune.builtFrom, rune.spentWith, rune.resource, rune.core, rune.loop,
        rune.keywords }
    for _, mod in ipairs(rune.mods or {}) do
        words[#words + 1] = mod.name
        words[#words + 1] = mod.text
    end
    local entry = art.runes[rune.id] or {}
    for _, list in ipairs({ entry.abilities or {}, entry.texts or {}, entry.slots or {} }) do
        for _, word in ipairs(list) do
            words[#words + 1] = word
        end
    end
    for _, role in ipairs(Roles(rune)) do
        words[#words + 1] = SEARCH_ROLES[role]
    end
    local text = {}
    for _, word in ipairs(words) do
        if type(word) == "string" then
            text[#text + 1] = word:lower()
        end
    end
    haystacks[rune.id] = table.concat(text, " | "):gsub("%*%*", "")
    return haystacks[rune.id]
end

local function HasRole(rune, wanted)
    for _, role in ipairs(Roles(rune)) do
        if role == wanted then
            return true
        end
    end
    return false
end

-- The runes the search and filters let through: those whose name matches first, then the usable ones, then the
-- catalog's order.
local function Filtered()
    local query = browse.query:lower()
    local found = {}
    for order, rune in ipairs(catalog) do
        local match = true
        for word in query:gmatch("%S+") do
            if not Haystack(rune):find(word, 1, true) then
                match = false
                break
            end
        end
        if match and not rune.retired and (not browse.role or HasRole(rune, browse.role))
            and (not browse.usable or Standing(rune) == "ok") then
            local named = query ~= "" and rune.name:lower():find(query, 1, true) and 0 or 1
            found[#found + 1] = { rune = rune, rank = named * 1000 + (Standing(rune) == "ok" and 0 or 100) + order }
        end
    end
    table.sort(found, function(a, b) return a.rank < b.rank end)
    local list = {}
    for _, item in ipairs(found) do
        list[#list + 1] = item.rune
    end
    return list
end

-- Why a rune cannot be bound now (nil: it can).
local function BindRefusal(rune)
    if state.slots[view.slot == 1 and 2 or 1].rune == rune.id then
        return "This rune is already your other Legendary Rune."
    end
    if state.rune ~= 0 then
        return state.rune == rune.id and "This rune is bound to your specialization."
            or "Your specialization's rune is bound."
    end
    if Standing(rune) == "soon" then
        return ShortName(rune) .. " is coming soon."
    end
end

-- No rune usable now (Learn Available is offered then).
function EmberforgeRunes_NoneUsable()
    for _, rune in ipairs(catalog) do
        if not rune.retired and Standing(rune) == "ok" then
            return false
        end
    end
    return true
end

local function FillRow(row, rune)
    row.rune = rune
    if not rune then
        row:Hide()
        return
    end
    row.medal:SetRune(rune)
    row.name:SetText(ShortName(rune))
    local words = {}
    for _, role in ipairs(Roles(rune)) do
        words[#words + 1] = ROLE_WORDS[role]
    end
    row.role:SetText(table.concat(words, " / "))
    row.desc:SetFont(SANS, 20)
    row.desc:SetText(rune.loop or "")
    if (row.desc:GetStringHeight() or 0) > 48 then
        row.desc:SetFont(SANS, 17)
    end
    local standing, missing = Standing(rune)
    local status, color
    if state.rune == rune.id then
        status, color = "Bound to this specialization", GOLD
    elseif state.slots[view.slot == 1 and 2 or 1].rune == rune.id then
        status, color = "Your other Legendary Rune", GOLD
    elseif standing == "cards" then
        status, color = "Your cards bring " .. missing, AMBER
    elseif standing == "missing" then
        status, color = "Requires " .. missing, AMBER
    elseif standing == "soon" then
        status, color = "Coming soon", { 0.7, 0.7, 0.7 }
    else
        status, color = "Usable now", GREEN
    end
    row.status:SetText(status)
    row.status:SetTextColor(color[1], color[2], color[3])
    -- None usable: a rune whose abilities can be learned or slotted offers Learn Available instead of Choose.
    row.learn = EmberforgeRunes_NoneUsable() and state.learnable and state.learnable[rune.id] and state.rune == 0
    row.choose:SetLabel(row.learn and "Learn Available" or "Choose")
    row.choose:Resize(row.learn and 210 or 180)
    local usable = row.learn or (BindRefusal(rune) == nil and standing == "ok")
    row.choose:SetEnabled(row.learn or BindRefusal(rune) == nil)
    row.choose:Look(usable and "primary" or "off")      -- grey, as the mockup, but it still asks (a warning)
    row:Show()
end

Fill = function()
    browse.list = Filtered()
    local most = math.max(0, #browse.list - ROWS)
    browse.offset = math.max(0, math.min(browse.offset, most))
    for index, row in ipairs(rows) do
        FillRow(row, browse.list[browse.offset + index])
    end
    for _, chip in ipairs(chips) do
        chip:SetOn(chip.role == browse.role)
    end
    DrawToggle()
    local total = 0
    for _, rune in ipairs(catalog) do
        if not rune.retired then
            total = total + 1
        end
    end
    browser.count:SetText(#browse.list == total and (total .. " runes") or (#browse.list .. " of " .. total
        .. " runes"))
    browser.empty:SetText(#browse.list == 0 and "No rune matches your search." or "")
    local span = ROWS * ROW_H - 30
    if most > 0 then
        local size = math.max(40, span * ROWS / #browse.list)
        thumb:SetHeight(size)
        thumb:ClearAllPoints()
        thumb:SetPoint("TOP", track, "TOP", 0, -(span - size) * browse.offset / most)
        thumb:Show()
        track:Show()
    else
        thumb:Hide()
        track:Hide()
    end
end

listPanel:SetScript("OnMouseWheel", function(_, delta)
    browse.offset = browse.offset - delta
    Fill()
end)
browser:EnableMouseWheel(true)
browser:SetScript("OnMouseWheel", function(_, delta)
    browse.offset = browse.offset - delta
    Fill()
end)
search:SetScript("OnTextChanged", function(self)
    browse.query = self:GetText() or ""
    browse.offset = 0
    if browse.query == "" then
        self.hint:Show()
    else
        self.hint:Hide()
    end
    Fill()
end)

local function ShowBrowser()
    Fill()
    Fade(browser, true)
end

-- The rune's build ----------------------------------------------------------------------------------------------------
-- Its medallion, the three slots about it (1 left, 2 right, 3 below, a line to each), "Choose your <slot>" and the
-- slot's three modifiers; a choice is confirmed with Confirm modifier. A slot opens once the one before has its
-- modifier; a done build can change any.

local build = View()
local NODES = { { 498, 258 }, { 1102, 258 }, { 800, 410 } }
local MEDAL_X, MEDAL_Y, MEDAL_SIZE = 800, 280, 188
build.eyebrow = Eyebrow(build, 52, "Legendary Rune")
build.title = Text(build, SERIF, 46, CREAM)
At(build.title, W / 2, 104)
build.subtitle = Text(build, SANS, 21, MUTED, "CENTER", 1150)
At(build.subtitle, W / 2, 158)
build.medal = Medal(build, MEDAL_SIZE)
At(build.medal, MEDAL_X, MEDAL_Y)
-- The lines from each slot to the medallion: dots along the way (the client draws no lines).
build.lines = {}
for slot, node in ipairs(NODES) do
    local dots = {}
    local dx, dy = MEDAL_X - node[1], MEDAL_Y - node[2]
    local length = math.sqrt(dx * dx + dy * dy)
    local from, to = 36, length - MEDAL_SIZE / 2 - 4
    local count = math.floor((to - from) / 3)
    for index = 0, count do
        local k = (from + (to - from) * index / count) / length
        local dot = Tex(build, "BORDER")
        dot:SetTexture(1, 1, 1)
        At(dot, node[1] + dx * k, node[2] + dy * k, 2, 2)
        dots[#dots + 1] = dot
    end
    build.lines[slot] = dots
end
build.nodes = {}
for slot, node in ipairs(NODES) do
    local button = CreateFrame("Button", nil, build)
    At(button, node[1], node[2], 66, 66)
    button.fill = Tex(button, "BACKGROUND", "UiDisc")
    button.fill:SetAllPoints()
    button.fill:SetVertexColor(0.04, 0.04, 0.08, 0.9)
    button.glow = Tex(button, "BACKGROUND", "Soft", "ADD")
    button.glow:SetPoint("CENTER")
    button.glow:SetWidth(130)
    button.glow:SetHeight(130)
    button.ring = Tex(button, "BORDER", "UiRingThin")
    button.ring:SetAllPoints()
    button.number = Text(button, SERIF, 32, CREAM)
    button.number:SetPoint("CENTER", 0, 1)
    button.number:SetText(slot)
    button.label = Text(build, CINZEL, 17, GOLD)
    At(button.label, node[1], node[2] + 54)
    button.status = Text(build, SANS_ITALIC, 22, CREAM, "CENTER", 300)
    At(button.status, node[1], node[2] + 82)
    button.lock = Tex(build, "ARTWORK")
    button.lock:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-LOCK")
    At(button.lock, node[1], node[2] + 82, 22, 22)
    button.slot = slot
    button:SetScript("OnClick", function(self)
        if self.open then
            view.active, view.selected = self.slot, nil
            ShowBuild()
        else
            Note("Choose the slot before it first.")
        end
    end)
    build.nodes[slot] = button
end
build.heading = Text(build, SERIF, 40, CREAM)
At(build.heading, W / 2, 550)
build.ask = Text(build, SANS, 21, MUTED, "CENTER", 1100)
At(build.ask, W / 2, 594)
build.options = {}
-- The modifiers' columns hang from OPTIONS_TOP and grow down to their longest words (down to OPTIONS_BOTTOM, above the
-- buttons; past that their words get smaller): long words are never cut off or run past the lines.
local OPTIONS_TOP, OPTIONS_BOTTOM, OPTION_HEAD = 628, 830, 84
for index, x in ipairs({ 340, 800, 1260 }) do
    local option = CreateFrame("Button", nil, build)
    option:SetPoint("TOP", stage, "TOPLEFT", x, -OPTIONS_TOP)
    option:SetWidth(440)
    option:SetHeight(160)
    option.top = Tex(option, "ARTWORK", "UiLine")
    option.top:SetPoint("TOP", option, "TOP", 0, 2)
    option.top:SetWidth(440)
    option.top:SetHeight(3)
    option.bottom = Tex(option, "ARTWORK", "UiLine")
    option.bottom:SetPoint("BOTTOM", option, "BOTTOM", 0, -2)
    option.bottom:SetWidth(440)
    option.bottom:SetHeight(3)
    option.glow = Tex(option, "BACKGROUND", "Soft", "ADD")
    option.glow:SetPoint("TOP", option, "TOP", 0, 20)
    option.glow:SetWidth(380)
    option.glow:SetHeight(40)
    option.glow:SetVertexColor(GOLD[1], GOLD[2], GOLD[3])
    option.radio = Tex(option, "ARTWORK", "UiRingThin")
    option.radio:SetPoint("TOPLEFT", 14, -24)
    option.radio:SetWidth(32)
    option.radio:SetHeight(32)
    option.check = Tex(option, "OVERLAY", "UiCheck")
    option.check:SetPoint("CENTER", option.radio, "CENTER", 1, 0)
    option.check:SetWidth(20)
    option.check:SetHeight(20)
    option.check:SetVertexColor(GOLD[1], GOLD[2], GOLD[3])
    option.name = Text(option, SERIF, 27, CREAM, "LEFT", 370)
    option.name:SetPoint("TOPLEFT", 64, -24)
    option.ability = Text(option, SANS_MEDIUM, 16, BLUE, "LEFT", 370)
    option.ability:SetPoint("TOPLEFT", 66, -60)
    option.text = Text(option, SANS, 20, MUTED, "LEFT", 364)
    option.text:SetPoint("TOPLEFT", 66, -OPTION_HEAD)
    option.text:SetJustifyV("TOP")
    option.choice = index
    option:SetScript("OnEnter", function(self)
        if not self.picked and not self.missing then
            self.glow:SetAlpha(0.25)
        end
    end)
    option:SetScript("OnLeave", function(self)
        self.glow:SetAlpha(self.selected and 0.55 or 0)
    end)
    build.options[index] = option
end
build.summary = Text(build, SANS, 22, MUTED, "CENTER", 1200)
At(build.summary, W / 2, 672)
build.left = Pill(build, 280, 52, "Back to all runes", "navy")
At(build.left, 270, 860)
build.right = Pill(build, 280, 52, "Confirm modifier", "navy")
At(build.right, 1330, 860)
build.picked = CreateFrame("Frame", nil, build)
At(build.picked, W / 2, 860, 600, 40)
build.pickedCheck = Tex(build.picked, "ARTWORK", "UiCheck")
build.pickedCheck:SetWidth(24)
build.pickedCheck:SetHeight(24)
build.pickedCheck:SetVertexColor(GOLD[1], GOLD[2], GOLD[3])
build.pickedText = Text(build.picked, SANS, 22, CREAM)
build.pickedText:SetPoint("CENTER", 14, 0)
build.pickedCheck:SetPoint("RIGHT", build.pickedText, "LEFT", -10, 0)

-- The rune the build shows: the one previewed, else the bound one.
local function SlotNames(runeId)
    local entry = art.runes[runeId] or {}
    local rune = RuneById(runeId) or {}
    return entry.slots or rune.slots or SLOT_NAMES
end

local function FirstEmpty()
    for slot = 1, 3 do
        if (view.picks[slot] or 0) == 0 then
            return slot
        end
    end
end

-- The named build the three picks make (the doc's), else "Your own build".
local function BuildName(rune)
    for _, named in ipairs(rune.builds or {}) do
        local match = true
        for slot = 1, 3 do
            local mod = rune.mods[(slot - 1) * 3 + view.picks[slot]]
            match = match and mod and mod.name == named.mods[slot]
        end
        if match then
            return named.name
        end
    end
    return "Your own build"
end

-- A slot opens once every slot before it has its modifier (a done build opens all).
local function SlotOpen(slot)
    for before = 1, slot - 1 do
        if (view.picks[before] or 0) == 0 then
            return false
        end
    end
    return true
end

local function DrawNodes(runeId)
    local names = SlotNames(runeId)
    local rune = RuneById(runeId)
    for slot, node in ipairs(build.nodes) do
        local pick = view.picks[slot] or 0
        local active = view.active == slot
        node.open = SlotOpen(slot)
        node.label:SetText(Spaced(names[slot] or SLOT_NAMES[slot]))
        local mod = pick > 0 and rune and rune.mods[(slot - 1) * 3 + pick]
        if active then
            node.status:SetText("Choosing")
            node.status:SetFont(SANS_ITALIC, 22)
            node.status:SetTextColor(0.85, 0.9, 1)
        elseif mod then
            node.status:SetText(mod.name)
            node.status:SetFont(SANS_MEDIUM, 19)
            node.status:SetTextColor(CREAM[1], CREAM[2], CREAM[3])
        else
            node.status:SetText("")
        end
        if node.open or pick > 0 then
            node.lock:Hide()
        else
            node.lock:Show()
        end
        local ring = active and { 1, 1, 1 } or pick > 0 and GOLD or node.open and { 0.8, 0.8, 0.85 }
            or { 0.45, 0.45, 0.5 }
        node.ring:SetVertexColor(ring[1], ring[2], ring[3])
        node.number:SetTextColor(ring[1], ring[2], ring[3])
        node.glow:SetVertexColor(0.6, 0.75, 1)
        node.glow:SetAlpha(active and 0.5 or 0)
        node.label:SetTextColor(GOLD[1], GOLD[2], GOLD[3], (node.open or pick > 0) and 1 or 0.6)
        for _, dot in ipairs(build.lines[slot]) do
            dot:SetVertexColor(active and 0.7 or 0.6, active and 0.82 or 0.6, active and 1 or 0.66,
                active and 0.85 or 0.4)
        end
    end
end

-- The options of the active slot, the one selected (not confirmed yet) lit.
local function DrawOptions(runeId)
    local rune = RuneById(runeId)
    local entry = art.runes[runeId] or {}
    local slot = view.active
    local names = SlotNames(runeId)
    build.heading:SetText("Choose your " .. (names[slot] or SLOT_NAMES[slot]))
    build.ask:SetText((entry.asks and entry.asks[slot] and (entry.asks[slot] .. ".  ") or "")
        .. "Select one modifier for this slot.")
    for index, option in ipairs(build.options) do
        local number = (slot - 1) * 3 + index
        local mod = rune and rune.mods[number]
        local missing = MissingText(state.needs[runeId .. "." .. slot .. "." .. index])
        option.slot, option.missing = slot, missing
        option.picked = view.picks[slot] == index
        option.selected = view.selected == index or (not view.selected and option.picked)
        option.name:SetText(mod and mod.name or "")
        option.ability:SetText(missing and Spaced("Needs " .. missing) or Spaced(entry.abilities
            and entry.abilities[number] or ""))
        option.ability:SetTextColor(unpack(missing and RED or BLUE))
        option.text:SetText(entry.texts and entry.texts[number] or (mod and mod.text) or "")
        local line = option.selected and GOLD or { 0.7, 0.7, 0.75 }
        option.top:SetVertexColor(line[1], line[2], line[3], option.selected and 1 or 0.5)
        option.bottom:SetVertexColor(line[1], line[2], line[3], option.selected and 1 or 0.5)
        option.glow:SetAlpha(option.selected and 0.55 or 0)
        option.radio:SetVertexColor(unpack(option.selected and GOLD or { 0.8, 0.8, 0.85 }))
        if option.selected then
            option.check:Show()
        else
            option.check:Hide()
        end
        option.name:SetTextColor(unpack(missing and { 0.6, 0.58, 0.62 } or CREAM))
        option:Show()
    end
    -- Every column as tall as the longest words, smaller words when they would not fit.
    local room = OPTIONS_BOTTOM - OPTIONS_TOP - OPTION_HEAD - 14
    local tallest = 0
    for _, size in ipairs({ 20, 18, 17, 16 }) do
        tallest = 0
        for _, option in ipairs(build.options) do
            option.text:SetFont(SANS, size)
            tallest = math.max(tallest, option.text:GetStringHeight() or 0)
        end
        build.textSize = size
        if tallest <= room then
            break
        end
    end
    for _, option in ipairs(build.options) do
        option:SetHeight(math.max(160, OPTION_HEAD + tallest + 14))
    end
    build.summary:Hide()
end

local CHECK = "\226\156\147"
ShowBuild = function()
    local runeId = view.preview or state.rune
    local rune = RuneById(runeId)
    if not rune then
        return
    end
    if view.active and not SlotOpen(view.active) then
        view.active = nil
    end
    if not view.active and not view.done then
        view.active = FirstEmpty()
    end
    build.title:SetText(ShortName(rune))
    build.subtitle:SetText(Plain((art.runes[runeId] or {}).core or rune.loop or ""))
    build.medal:SetRune(rune)
    DrawNodes(runeId)
    if view.active then
        DrawOptions(runeId)
    else
        for _, option in ipairs(build.options) do
            option:Hide()
        end
        build.heading:SetText(BuildName(rune))
        build.ask:SetText("Click a slot to change its modifier.")
        local names, picks = SlotNames(runeId), {}
        for slot = 1, 3 do
            local mod = rune.mods[(slot - 1) * 3 + (view.picks[slot] or 0)]
            picks[#picks + 1] = (names[slot] or SLOT_NAMES[slot]) .. ": |cfffff2e0" .. (mod and mod.name or "-")
                .. "|r"
        end
        build.summary:SetText(table.concat(picks, "     "))
        build.summary:Show()
    end
    -- The bottom: Back (a preview) or Unlearn (bound, before level 8); what is selected; Confirm or Choose.
    if view.preview then
        build.left:SetLabel("Back to all runes")
        build.left:Show()
    elseif state.rune ~= 0 and state.level < 8 then
        build.left:SetLabel("Unlearn rune")
        build.left:Show()
    else
        build.left:Hide()
    end
    local selected = view.active and view.selected
    local mod = selected and rune.mods[(view.active - 1) * 3 + selected]
    if mod then
        build.pickedText:SetText(mod.name .. " selected")
        build.picked:Show()
    else
        build.picked:Hide()
    end
    local pending = selected and selected ~= view.picks[view.active]
    if view.preview and not pending then
        build.right:SetLabel("Choose this rune")
        build.right:SetEnabled(BindRefusal(rune) == nil)
    elseif view.active then
        build.right:SetLabel("Confirm modifier")
        build.right:SetEnabled(pending and true or false)
    else
        build.right:SetLabel("Close")
        build.right:SetEnabled(true)
    end
    Fade(build, true)
end

-- A modifier clicked: selected (Confirm modifier takes it).
for _, option in ipairs(build.options) do
    option:SetScript("OnClick", function(self)
        if self.missing then
            Note(self.name:GetText() .. " needs " .. self.missing .. ".")
            return
        end
        Sound("igMainMenuOptionCheckBoxOn")
        view.selected = self.choice
        ShowBuild()
    end)
end

-- Confirmed: the preview keeps it here; a bound rune's goes to the server. Then the next empty slot.
local function Confirm()
    local slot, choice = view.active, view.selected
    if not slot or not choice then
        return
    end
    local firstBuild = not view.preview and FirstEmpty() == slot and not (EmberforgeRunesDB
        and EmberforgeRunesDB.builtOnce)
    if not view.preview then
        Send("MOD," .. slot .. "," .. choice .. "," .. view.slot)
    end
    view.picks[slot] = choice
    view.selected = nil
    view.active = FirstEmpty()
    view.done = view.active == nil
    ShowBuild()
    -- The first build finished: a moment to see it complete, then on to the runes (the Halo).
    if firstBuild and view.done then
        if EmberforgeRunesDB then
            EmberforgeRunesDB.builtOnce = true
        end
        view.haloAt = view.now + 2.6
    end
end

-- A rune's build as if bound: its picks tried here only.
OpenPreview = function(runeId)
    view.previewFrom = view.mode == "browse" and "browse" or "offers"
    view.preview = runeId
    view.mode = "preview"
    view.picks, view.active, view.selected, view.done = { 0, 0, 0 }, nil, nil, false
    Fade(offers, false)
    Fade(browser, false)
    ShowBuild()
end

-- The locked slot (Rune II below its level) ---------------------------------------------------------------------------

local locked = View()
locked.eyebrow = Eyebrow(locked, 300, "Legendary Runes")
locked.title = Text(locked, SERIF, 46, CREAM)
At(locked.title, W / 2, 360)
locked.subtitle = Text(locked, SANS, 23, MUTED)
At(locked.subtitle, W / 2, 414)

-- Rune I and Rune II --------------------------------------------------------------------------------------------------

local function SwitchSlot(n)
    if view.slot == n then
        return
    end
    UseSlot(n)
    view.mode, view.preview = nil, nil
    Enter()
end
for n = 1, 2 do
    local tab = Pill(chrome, 140, 42, n == 1 and "Rune I" or "Rune II", "outline", function() SwitchSlot(n) end)
    tab.label:SetFont(SERIF, 17)
    At(tab, 110 + (n - 1) * 152, 52)
    tab.tip = function(self)
        local rune = RuneById(state.slots[n].rune)
        Tip(self, n == 1 and "Rune I" or "Rune II", { { rune and rune.name or "None yet: choose one." } })
    end
    chrome.runeTabs[n] = tab
end

-- The flow ------------------------------------------------------------------------------------------------------------

local function HideViews()
    for _, each in ipairs({ offers, browser, build, locked }) do
        Fade(each, false)
    end
end

-- The panel as the state has it: locked, offers, browse, or the build (bound or previewed).
function Enter()
    HideViews()
    Fade(chrome, true)
    local both = state.level >= state.second or state.slots[2].rune ~= 0
    for n, tab in ipairs(chrome.runeTabs) do
        tab:SetOn(view.slot == n)
        if both then
            tab:Show()
        else
            tab:Hide()
        end
    end
    if state.level < state.unlock then
        view.mode = "locked"
        locked.title:SetText(view.slot == 2 and "Rune II" or "Legendary Runes")
        locked.subtitle:SetText(view.slot == 2 and ("Your second Legendary Rune unlocks at level " .. state.unlock
            .. ".") or ("Legendary Runes unlock at level " .. state.unlock .. "."))
        Fade(locked, true)
        return
    end
    if view.mode == "preview" and view.preview then
        ShowBuild()
        return
    end
    view.preview = nil
    if state.rune ~= 0 and view.mode ~= "browse" then
        view.mode = "build"
        view.picks = { state.mods[1], state.mods[2], state.mods[3] }
        view.selected, view.active = nil, nil
        view.done = FirstEmpty() == nil
        ShowBuild()
        return
    end
    if view.mode ~= "browse" then
        view.mode = "offers"
        ShowOffers()
    else
        ShowBrowser()
    end
end

-- A new state from the server, while the panel is open.
function Refresh()
    if not frame:IsShown() then
        return
    end
    -- Bound as asked: the build, with the picks tried in the preview sent along.
    if view.pending and state.rune == view.pending then
        local picks = view.preview and view.picks
        view.pending, view.preview = nil, nil
        view.mode = "build"
        if picks then
            for slot = 1, 3 do
                if picks[slot] and picks[slot] > 0 then
                    Send("MOD," .. slot .. "," .. picks[slot] .. "," .. view.slot)
                end
            end
        end
        Enter()
        return
    end
    if view.mode == "preview" then
        ShowBuild()
    elseif view.mode == "browse" then
        if state.rune ~= 0 then
            view.mode = "build"
            Enter()
        else
            Fill()
        end
    elseif view.mode == "build" then
        if state.rune == 0 then
            view.mode = nil
            Enter()
        else
            -- The server's picks (a refusal puts one back); the slot being chosen stays.
            view.picks = { state.mods[1], state.mods[2], state.mods[3] }
            ShowBuild()
        end
    else
        Enter()
    end
end

build.left:SetScript("OnClick", function(self)
    if not self.enabled then
        return
    end
    Sound("igMainMenuOptionCheckBoxOn")
    if view.preview then
        view.preview = nil
        view.mode = view.previewFrom or "offers"
        Enter()
    elseif state.rune ~= 0 then
        local rune = RuneById(state.rune)
        Ask("Unlearn " .. ShortName(rune) .. "?", "Its modifiers go with it. You can choose any rune again until "
            .. "level 8.", "Unlearn", function() Send("UNBIND," .. view.slot) end)
    end
end)
build.right:SetScript("OnClick", function(self)
    if not self.enabled then
        return
    end
    Sound("igMainMenuOptionCheckBoxOn")
    local pending = view.active and view.selected and view.selected ~= view.picks[view.active]
    if pending then
        Confirm()
    elseif view.preview then
        AskBind(RuneById(view.preview))
    elseif not view.active then
        frame:Hide()
    end
end)

-- Binding: asked first (a rune missing abilities warns, naming them); the build opens when the server binds it.
AskBind = function(rune)
    if not rune then
        return
    end
    local why = BindRefusal(rune)
    if why then
        Note(why)
        return
    end
    local standing, missing = Standing(rune)
    local slot = view.slot
    local function Bind(anyway)
        view.pending = rune.id
        Send("PICK," .. rune.id .. "," .. (anyway and 1 or 0) .. "," .. slot)
    end
    if standing == "missing" or standing == "cards" then
        Ask("Warning", "You will not be able to use " .. ShortName(rune) .. ": you don't have " .. missing .. ".\n\n"
            .. "It stays bound to this specialization (until level 8 you can unlearn it), whether you learn them or "
            .. "not.\n\nBind it anyway?", "Bind anyway", function() Bind(true) end, true)
    else
        Ask("Bind " .. ShortName(rune) .. "?", "Until level 8 you can unlearn it in this panel; after that only a "
            .. "prestige can change it. Its modifiers stay free to change.", "Bind", function() Bind(false) end)
    end
end

-- Learn Available's plan (L), asked before anything changes: what is learned, what is slotted, and, in red, what
-- each replaces.
local SLOT_NUMERALS = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII" }
function EmberforgeRunes_AskLearn(parts)
    local rune = RuneById(tonumber(parts[2]) or 0)
    if not rune then
        return
    end
    local function Name(id)
        return GetSpellInfo(tonumber(id) or 0) or ("spell " .. tostring(id))
    end
    local lines, warnings = {}, {}
    local replaced = Split(parts[4] or "-", ".")
    if parts[3] and parts[3] ~= "-" then
        for index, id in ipairs(Split(parts[3], ".")) do
            lines[#lines + 1] = "Learn now: |cffffffff" .. Name(id) .. "|r"
            if replaced[index] and replaced[index] ~= "0" then
                warnings[#warnings + 1] = Name(replaced[index]) .. " leaves your kit"
            end
        end
    end
    if parts[5] and parts[5] ~= "-" then
        local places = Split(parts[6] or "-", ".")
        for index, id in ipairs(Split(parts[5], ".")) do
            local slot, kind, old = (places[index] or ""):match("^(%d+):(%d*):?(%d*)$")
            local numeral = SLOT_NUMERALS[tonumber(slot) or 0] or "?"
            lines[#lines + 1] = "Slot its Skill Card: |cffffffff" .. Name(id) .. "|r (slot " .. numeral .. ")"
            if old and old ~= "" then
                warnings[#warnings + 1] = (kind == "0" and (Name(old) .. "'s Skill Card") or "A Talent Card")
                    .. " leaves slot " .. numeral
            end
        end
    end
    local text = table.concat(lines, "\n")
    if #warnings > 0 then
        text = text .. "\n\n|cffff3b30This replaces:|r\n" .. table.concat(warnings, "\n")
    end
    view.asking = text
    Ask("Learn Available: " .. ShortName(rune), text, "Proceed", function() Send("APPLY," .. rune.id) end)
end

-- One frame: the fades, the note, the glows' breath, the Halo after a first build.
local function Step(dt)
    view.now = view.now + dt
    StepFades(dt)
    if chrome.note.until_ and view.now > chrome.note.until_ then
        local k = clamp((view.now - chrome.note.until_) / 0.6)
        chrome.note:SetAlpha(1 - k)
        if k >= 1 then
            chrome.note.until_ = nil
            chrome.note:SetText("")
        end
    end
    local breath = 0.3 + 0.08 * math.sin(view.now * 2)
    build.medal.glow:SetAlpha(breath + 0.12)
    for _, card in ipairs(offers.cards) do
        if not card.hover then
            card.medal.glow:SetAlpha(breath)
        end
    end
    if view.haloAt and view.now >= view.haloAt then
        view.haloAt = nil
        if frame:IsShown() and view.mode == "build" then
            frame:Hide()
            if EmberforgeHalo_Open then
                EmberforgeHalo_Open()
            end
        end
    end
end

-- The rest of the UI hidden while the panel is open (UIParent, and with it everything but this panel; Escape or Alt+Z
-- shows it again, and that closes the panel). Out of combat only: asked for in combat, it opens once the fight is
-- over; a fight starting closes it.
local alone = { hid = false, openAfterCombat = false }
function alone.Hide()
    if WorldFrame and frame:GetParent() == WorldFrame and UIParent:IsShown() then
        UIParent:Hide()
        alone.hid = true
    end
end
function alone.Show()
    if alone.hid then
        alone.hid = false
        if not UIParent:IsShown() then
            UIParent:Show()
        end
    end
end
function alone.InCombat()
    return (UnitAffectingCombat and UnitAffectingCombat("player")) or (InCombatLockdown and InCombatLockdown())
end

frame:SetScript("OnUpdate", function(_, elapsed)
    if alone.hid and UIParent:IsShown() then
        frame:Hide()
        return
    end
    Step(math.min(0.05, elapsed))
end)

frame:SetScript("OnShow", function()
    Fit()
    alone.Hide()
    state.error = nil
    Send("STATE")
end)
frame:SetScript("OnHide", function()
    alone.Show()
    tip:Hide()
    ask:Hide()
    for _, each in ipairs(fades) do
        Fade(each, false, true)
    end
    view.pending, view.preview, view.previewFrom, view.haloAt = nil, nil, nil, nil
end)

local function Open()
    if frame:IsShown() then
        return
    end
    if alone.InCombat() then
        alone.openAfterCombat = true
        if DEFAULT_CHAT_FRAME then
            DEFAULT_CHAT_FRAME:AddMessage("|cffa335eeLegendary Runes:|r the rune panel opens once you leave combat.")
        end
        return
    end
    -- Nothing to suggest: straight to the search.
    view.mode = (state.rune == 0 and #state.offers == 0 and state.level >= state.unlock) and "browse" or nil
    frame:Show()
    Enter()
end

frame:RegisterEvent("DISPLAY_SIZE_CHANGED")
frame:RegisterEvent("UI_SCALE_CHANGED")
frame:RegisterEvent("PLAYER_REGEN_DISABLED")
frame:RegisterEvent("PLAYER_REGEN_ENABLED")
frame:SetScript("OnEvent", function(_, event)
    if event == "PLAYER_REGEN_DISABLED" then
        if frame:IsShown() then
            frame:Hide()
            alone.openAfterCombat = true
        end
    elseif event == "PLAYER_REGEN_ENABLED" then
        if alone.openAfterCombat then
            alone.openAfterCombat = false
            Open()
        end
    else
        Fit()
    end
end)

SLASH_EMBERFORGERUNES1 = "/runes"
SLASH_EMBERFORGERUNES2 = "/rune"
SlashCmdList["EMBERFORGERUNES"] = function()
    if not state.loaded then
        Send("STATE")
        DEFAULT_CHAT_FRAME:AddMessage("|cffa335eeLegendary Runes:|r asking the server for your rune; try again in a "
            .. "moment.")
        return
    end
    if frame:IsShown() then
        frame:Hide()
    else
        Open()
    end
end

-- Messages ---------------------------------------------------------------------------------------------------------

local auraAmounts = {}                                  -- spell id -> { kind, amount } (V)

local function OnMessage(message)
    local parts = Split(message, ",")
    local kind = parts[1]
    if kind == "S" then
        state.level, state.base, state.spec = tonumber(parts[2]) or 1, tonumber(parts[3]) or 10, tonumber(parts[4])
            or 1
        state.second = tonumber(parts[15]) or 30
        state.slots[1] = { rune = tonumber(parts[5]) or 0,
            mods = { tonumber(parts[6]) or 0, tonumber(parts[7]) or 0, tonumber(parts[8]) or 0 } }
        state.slots[2] = { rune = tonumber(parts[11]) or 0,
            mods = { tonumber(parts[12]) or 0, tonumber(parts[13]) or 0, tonumber(parts[14]) or 0 } }
        state.allOffers = {}
        if parts[9] and parts[9] ~= "-" then
            for _, id in ipairs(Split(parts[9], ".")) do
                state.allOffers[#state.allOffers + 1] = tonumber(id)
            end
        end
        state.prompt = parts[10] == "1"
        -- An open, empty slot asked for: the panel shows it (the first first).
        local open
        for n = 1, 2 do
            if not open and state.slots[n].rune == 0 and state.level >= (n == 2 and state.second or state.base) then
                open = n
            end
        end
        local key = state.spec .. ":" .. (open or 0)
        if state.prompt and open and not view.prompted[key] and not frame:IsShown() then
            view.slot = open
        end
        UseSlot(view.slot)
        state.loaded = true
        if EmberforgeRunes_UpdateRuneButton then
            EmberforgeRunes_UpdateRuneButton()  -- first: nothing below may keep the button hidden
        end
        if EmberforgeRunes_UpdateGlow then
            pcall(EmberforgeRunes_UpdateGlow)   -- the changed abilities' icons follow the bound rune
        end
        -- Unlocked and unbound: the panel opens by itself, once a session for each specialization.
        if state.prompt and open and not view.prompted[key] then
            view.prompted[key] = true
            Open()
        end
        Refresh()
    elseif kind == "R" then
        local id = tonumber(parts[2]) or 0
        state.built[id] = parts[3] or "-"
        state.withCards = state.withCards or {}
        state.learnable = state.learnable or {}
        state.withCards[id] = parts[4] or parts[3] or "-"
        state.learnable[id] = parts[5] == "1"
        if frame:IsShown() and view.mode == "browse" then
            Fill()
        end
    elseif kind == "N" then
        state.needs[(parts[2] or "") .. "." .. (parts[3] or "") .. "." .. (parts[4] or "")] = parts[5] or "-"
    elseif kind == "E" then
        state.error = message:sub(3)
        if frame:IsShown() then
            Note(state.error)
            view.pending = nil
            if view.mode == "build" then
                view.picks = { state.mods[1], state.mods[2], state.mods[3] }
                ShowBuild()
            end
        end
    elseif kind == "L" then
        EmberforgeRunes_AskLearn(parts)
    elseif kind == "O" then
        Open()
    elseif kind == "V" then
        local spell, amount = tonumber(parts[2]), tonumber(parts[4])
        if spell and amount then
            auraAmounts[spell] = { kind = parts[3], amount = amount }
        end
    end
end

-- The rune auras' tooltips: their real amounts (V), as the server set them.
local AMOUNT_LINES = {
    A = "Absorbs %d damage.",
    H = "Heals %d every second.",
    D = "%d damage every second.",
    S = "Movement speed reduced by %d%%.",
}

local function AddAuraAmount(tooltip, unit, index, filter)
    local spellId = select(11, UnitAura(unit, index, filter))
    local known = spellId and auraAmounts[spellId]
    if not known or not AMOUNT_LINES[known.kind] then
        return
    end
    tooltip:AddLine(string.format(AMOUNT_LINES[known.kind], known.amount), 1, 0.82, 0)
    tooltip:Show()
end

if hooksecurefunc and GameTooltip then
    hooksecurefunc(GameTooltip, "SetUnitAura", AddAuraAmount)
    hooksecurefunc(GameTooltip, "SetUnitBuff", function(tooltip, unit, index, filter)
        AddAuraAmount(tooltip, unit, index, filter or "HELPFUL")
    end)
    hooksecurefunc(GameTooltip, "SetUnitDebuff", function(tooltip, unit, index, filter)
        AddAuraAmount(tooltip, unit, index, filter or "HARMFUL")
    end)
end

-- Abilities a rune changes show the rune's look on the action bars: Consecration (any rank) the Sanctified
-- Paragon's icon while that rune is bound; Throw Glaive Chaos Bolt's icon at 5 Fel Command, with a pulsing border
-- (brighter, a purple rim in Metamorphosis): the client has no proc glow, so both are drawn over
-- each action button holding the ability. In the spellbook, Consecration shows the Paragon's icon too.
local glow = { buttons = {}, phase = 0 }
local BAR_BUTTONS = { "ActionButton", "MultiBarBottomLeftButton", "MultiBarBottomRightButton", "MultiBarRightButton",
    "MultiBarLeftButton", "BonusActionButton" }
local THROW_GLAIVE, FEL_COMMAND, METAMORPHOSIS = 90111, 90701, 90116
local CONSECRATION, SANCTIFIED_PARAGON = 26573, 22
local CHAOS_BOLT_ICON = "Interface\\Icons\\Ability_Warlock_ChaosBolt"
local PARAGON_ICON = "Interface\\Icons\\Ability_Rune_SanctifiedParagon"

local function Buff(spellId)
    local name = GetSpellInfo(spellId)
    if not name then
        return nil
    end
    local found, _, _, count = UnitBuff("player", name)
    return found and (count or 0) or nil
end

local function Stacks()
    return Buff(FEL_COMMAND) or 0
end

-- A spellbook slot's spell name, or nil: GetSpellName raises an error on a slot past the book's end (the spellbook
-- updates its empty buttons too).
local function BookSpellName(slot, bookType)
    if not GetSpellName or not slot then
        return nil
    end
    local ok, name = pcall(GetSpellName, slot, bookType or BOOKTYPE_SPELL)
    return ok and name or nil
end

-- The spell an action slot holds, by name (3.3.5's GetActionInfo gives a spellbook index, the spell's id fourth).
local function ActionSpell(slot)
    local kind, id, subType, spellId = GetActionInfo(slot)
    if kind ~= "spell" or not id then
        return nil
    end
    if spellId then
        return (GetSpellInfo(spellId))
    end
    return BookSpellName(id, subType) or (GetSpellInfo(id))
end

-- What an ability looks like now: (icon, glow) while a rune changes it, else nil.
local function Changed(spellName, state)
    if not spellName then
        return nil
    end
    if spellName == state.glaive and state.armed then
        return CHAOS_BOLT_ICON, true
    end
    if spellName == state.consecration and state.paragon then
        return PARAGON_ICON, false
    end
    return nil
end

local function State()
    return { glaive = GetSpellInfo(THROW_GLAIVE), consecration = GetSpellInfo(CONSECRATION),
             armed = Stacks() >= 5, meta = Buff(METAMORPHOSIS) ~= nil,
             paragon = Bound(SANCTIFIED_PARAGON) and (state.level or 0) >= (state.base or 5) }
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
    overlay.felguard = button:CreateTexture(nil, "OVERLAY")
    overlay.felguard:SetTexture("Interface\\Icons\\Spell_Shadow_SummonFelGuard")
    overlay.felguard:SetWidth(14)
    overlay.felguard:SetHeight(14)
    overlay.felguard:SetPoint("BOTTOMRIGHT", -1, 1)
    glow.buttons[button] = overlay
    return overlay
end

-- The button's own icon swapped (so it keeps the icon's place inside the border, the cooldown sweep over it and the
-- out-of-range and unusable tints), and put back from the action once the rune lets go.
local function SetIcon(button, path)
    local icon = _G[(button:GetName() or "") .. "Icon"]
    if not icon then
        return
    end
    if path then
        icon:SetTexture(path)
        button.emberforgeIcon = path
    elseif button.emberforgeIcon then
        button.emberforgeIcon = nil
        icon:SetTexture(GetActionTexture and GetActionTexture(button.action) or nil)
    end
end

local function UpdateButton(button, state)
    local icon, glowing = Changed(ActionSpell(button.action), state)
    SetIcon(button, icon)
    if icon and glowing then
        local overlay, meta = Overlay(button), state.meta
        overlay.border:SetVertexColor(meta and 0.75 or 0.35, meta and 0.45 or 1, meta and 1 or 0.3)
        overlay.border:Show()
        overlay.felguard:Hide()
        return true
    end
    if glow.buttons[button] then
        for _, texture in pairs(glow.buttons[button]) do
            texture:Hide()
        end
    end
    return false
end

local function UpdateGlow()
    local state = State()
    local pulsing = false
    for _, bar in ipairs(BAR_BUTTONS) do
        for index = 1, 12 do
            local button = _G[bar .. index]
            if button and button.action and UpdateButton(button, state) then
                pulsing = true
            end
        end
    end
    glow.armed = pulsing
end
-- Blizzard's own update sets the icon back from the action: the swap follows it.
if hooksecurefunc and ActionButton_Update then
    hooksecurefunc("ActionButton_Update", function(button)
        if button.action then
            UpdateButton(button, State())
        end
    end)
end
EmberforgeRunes_UpdateGlow = UpdateGlow

-- The spellbook: Consecration's icon the Paragon's while that rune is bound. Only the icon texture is touched, never
-- the spellbook's own update (SpellBookFrame_Update): run from here on every aura change, it rebuilt the open book's
-- buttons over and over (learning, swapping runes) and tainted them, so they could no longer be hovered or clicked.
local function BookIcon(button, state)
    local slot = SpellBook_GetSpellID and SpellBook_GetSpellID(button:GetID())
    local icon = _G[button:GetName() .. "IconTexture"]
    if not slot or not icon or (SpellBookFrame and SpellBookFrame.bookType ~= BOOKTYPE_SPELL) then
        return
    end
    local name = BookSpellName(slot, BOOKTYPE_SPELL)
    if name and name == state.consecration then
        local texture = state.paragon and PARAGON_ICON or (GetSpellTexture and GetSpellTexture(slot, BOOKTYPE_SPELL))
        if texture then
            icon:SetTexture(texture)
        end
    end
end

if SpellButton_UpdateButton and hooksecurefunc then
    hooksecurefunc("SpellButton_UpdateButton", function(button)
        BookIcon(button, State())
    end)
end

-- The open book's icons again when the Paragon comes or goes (not on every aura change).
local bookParagon
local function UpdateBookIcons()
    local state = State()
    if state.paragon == bookParagon then
        return
    end
    bookParagon = state.paragon
    if not (SpellBookFrame and SpellBookFrame:IsShown()) then
        return
    end
    for index = 1, SPELLS_PER_PAGE or 12 do
        local button = _G["SpellButton" .. index]
        if button and button:IsShown() then
            BookIcon(button, state)
        end
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:RegisterEvent("UNIT_AURA")
events:RegisterEvent("SPELLS_CHANGED")
events:RegisterEvent("UPDATE_BONUS_ACTIONBAR")
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
        EmberforgeRunes_Asking.again = 3       -- and once more when the level's rolls have settled
    elseif event == "UNIT_AURA" then
        if prefix == "player" then
            UpdateGlow()
            UpdateBookIcons()
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

-- The rune button: the active specialization's rune in the gold ring (Art/RuneRing; its icon made round by
-- SetPortraitToTexture), left of the minimap and clear of the buffs, from the unlock level (5). A click opens the
-- panel. Until the specialization has a rune it glows, and its icon turns through every rune's (each 2.5 sec, a
-- crossfade), settling on the rune once one is bound. Dragging it moves it anywhere (kept per character); shift-right-click
-- puts it back.
EmberforgeRunesDB = EmberforgeRunesDB or {}
local RUNE_BUTTON_SIZE = 58
local runeButton = CreateFrame("Button", "EmberforgeRuneButton", UIParent)
runeButton:SetWidth(RUNE_BUTTON_SIZE)
runeButton:SetHeight(RUNE_BUTTON_SIZE)
runeButton:SetFrameStrata("MEDIUM")
runeButton:SetMovable(true)
runeButton:SetClampedToScreen(true)
runeButton:RegisterForClicks("LeftButtonUp", "RightButtonUp")
runeButton:RegisterForDrag("LeftButton")
runeButton.icon = runeButton:CreateTexture(nil, "BACKGROUND")
runeButton.icon:SetPoint("CENTER")
runeButton.icon:SetWidth(RUNE_BUTTON_SIZE * 0.76)
runeButton.icon:SetHeight(RUNE_BUTTON_SIZE * 0.76)
-- The next icon, fading in over the last while they turn.
runeButton.nextIcon = runeButton:CreateTexture(nil, "BACKGROUND")
runeButton.nextIcon:SetPoint("CENTER")
runeButton.nextIcon:SetWidth(RUNE_BUTTON_SIZE * 0.76)
runeButton.nextIcon:SetHeight(RUNE_BUTTON_SIZE * 0.76)
runeButton.nextIcon:SetAlpha(0)
runeButton.glow = runeButton:CreateTexture(nil, "BORDER")
runeButton.glow:SetTexture(ART .. "Soft")
runeButton.glow:SetBlendMode("ADD")
runeButton.glow:SetPoint("CENTER")
runeButton.glow:SetWidth(RUNE_BUTTON_SIZE * 1.6)
runeButton.glow:SetHeight(RUNE_BUTTON_SIZE * 1.6)
runeButton.glow:SetVertexColor(1, 0.75, 0.3)
runeButton.glow:Hide()
runeButton.ring = runeButton:CreateTexture(nil, "ARTWORK")
runeButton.ring:SetTexture(ART .. "RuneRing")
runeButton.ring:SetAllPoints()
runeButton.shine = runeButton:CreateTexture(nil, "HIGHLIGHT")
runeButton.shine:SetTexture(ART .. "Soft")
runeButton.shine:SetBlendMode("ADD")
runeButton.shine:SetPoint("CENTER")
runeButton.shine:SetWidth(RUNE_BUTTON_SIZE * 0.8)
runeButton.shine:SetHeight(RUNE_BUTTON_SIZE * 0.8)
runeButton.shine:SetAlpha(0.35)

local function PlaceRuneButton()
    runeButton:ClearAllPoints()
    local spot = EmberforgeRunesDB.button
    if spot then
        runeButton:SetPoint("CENTER", UIParent, "BOTTOMLEFT", spot[1], spot[2])
    else
        -- Left of the minimap, level with its lower half: clear of its ring and its own buttons, under the buffs.
        runeButton:SetPoint("RIGHT", MinimapCluster or UIParent, "BOTTOMLEFT", -16, 46)
    end
end

local function RoundIcon(texture, icon)
    local path = "Interface\\Icons\\" .. (icon or "INV_Misc_Rune_01")
    if SetPortraitToTexture then
        SetPortraitToTexture(texture, path)
    else
        texture:SetTexture(path)
    end
end

-- The runes the unbound button turns through: every one with an icon, in the catalog's order.
local cycle = {}
for _, rune in ipairs(catalog) do
    if rune.icon and not rune.retired then
        cycle[#cycle + 1] = rune.icon
    end
end
local CYCLE_SECONDS, FADE_SECONDS = 2.5, 0.6

-- Shown from the unlock level; its icon the bound rune's, else turning (OnUpdate).
local function UpdateRuneButton()
    if not state.loaded or state.level < state.base then
        runeButton:Hide()
        return
    end
    runeButton:Show()
    local first = state.slots[1].rune
    local rune = first ~= 0 and RuneById(first)
    if rune then
        RoundIcon(runeButton.icon, rune.icon)
        runeButton.icon:SetAlpha(1)
        runeButton.nextIcon:SetAlpha(0)
        runeButton.turning = nil
    elseif not runeButton.turning and #cycle > 0 then
        runeButton.turning = { index = math.random(#cycle), t = 0 }
        RoundIcon(runeButton.icon, cycle[runeButton.turning.index])
        runeButton.icon:SetAlpha(1)
    end
    -- Glowing while a slot that is open has no rune.
    runeButton.glow.on = first == 0 or state.level >= state.second and state.slots[2].rune == 0
    runeButton.glow:SetShown(runeButton.glow.on)
end
if not runeButton.glow.SetShown then
    runeButton.glow.SetShown = function(self, on)
        if on then
            self:Show()
        else
            self:Hide()
        end
    end
end

runeButton:SetScript("OnClick", function(self, button)
    -- The release that ends a drag is not a click.
    if self.dragged and GetTime() - self.dragged < 0.2 then
        return
    end
    if button == "RightButton" and IsShiftKeyDown() then
        EmberforgeRunesDB.button = nil
        PlaceRuneButton()
        return
    end
    if frame:IsShown() then
        frame:Hide()
    else
        Open()
    end
end)
runeButton:SetScript("OnDragStart", function(self)
    self.moving = true
    GameTooltip:Hide()
    self:StartMoving()
end)
runeButton:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    self.moving = nil
    self.dragged = GetTime()
    local x, y = self:GetCenter()
    EmberforgeRunesDB.button = { x, y }
end)
runeButton:SetScript("OnEnter", function(self)
    if self.moving then
        return
    end
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine("Legendary Runes", GOLDEN[1], GOLDEN[2], GOLDEN[3])
    if state.level < state.base then
        GameTooltip:AddLine("Unlocks at level " .. state.base .. ".", 0.8, 0.8, 0.8, true)
    else
        for n = 1, 2 do
            local id = state.slots[n].rune
            local rune = id ~= 0 and RuneById(id)
            local label = n == 1 and "Rune I: " or "Rune II: "
            if n == 2 and state.level < state.second then
                GameTooltip:AddLine(label .. "unlocks at level " .. state.second .. ".", 0.8, 0.8, 0.8, true)
            elseif rune then
                GameTooltip:AddLine(label .. rune.name, 1, 1, 1, true)
            else
                GameTooltip:AddLine(label .. "none yet: choose one.", 1, 0.82, 0.4, true)
            end
        end
        GameTooltip:AddLine("Click to see your runes or change their modifiers.", 0.8, 0.8, 0.8, true)
    end
    GameTooltip:AddLine("Drag to move it anywhere; shift-right-click to put it back.", 0.6, 0.6, 0.6, true)
    GameTooltip:Show()
end)
runeButton:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)
runeButton:SetScript("OnUpdate", function(self, elapsed)
    if self.glow.on then
        self.pulse = ((self.pulse or 0) + elapsed * 2.5) % (2 * math.pi)
        self.glow:SetAlpha(0.45 + 0.35 * math.sin(self.pulse))
    end
    -- Unbound: the next rune's icon fades in over the last, then takes its place.
    local turn = self.turning
    if not turn then
        return
    end
    turn.t = turn.t + elapsed
    if turn.t >= CYCLE_SECONDS and not turn.fading then
        turn.fading = 0
        turn.next = turn.index % #cycle + 1
        RoundIcon(self.nextIcon, cycle[turn.next])
    end
    if turn.fading then
        turn.fading = turn.fading + elapsed
        local k = math.min(1, turn.fading / FADE_SECONDS)
        self.nextIcon:SetAlpha(k)
        self.icon:SetAlpha(1 - k)
        if k >= 1 then
            RoundIcon(self.icon, cycle[turn.next])
            self.icon:SetAlpha(1)
            self.nextIcon:SetAlpha(0)
            turn.index, turn.t, turn.fading = turn.next, 0, nil
        end
    end
end)
runeButton:RegisterEvent("PLAYER_LOGIN")
runeButton:RegisterEvent("PLAYER_LEVEL_UP")
runeButton:SetScript("OnEvent", function(_, event, level)
    if event == "PLAYER_LEVEL_UP" then
        state.level = tonumber(level) or state.level
    end
    EmberforgeRunesDB = EmberforgeRunesDB or {}
    PlaceRuneButton()
    UpdateRuneButton()
end)
PlaceRuneButton()
UpdateRuneButton()
EmberforgeRunes_UpdateRuneButton = UpdateRuneButton

-- Until the server's state arrives, asked again every 3 sec (a request sent while loading can be lost); after a
-- minute without one, the chat says so.
local asking = CreateFrame("Frame")
asking.wait, asking.tries = 3, 0
asking:SetScript("OnUpdate", function(self, elapsed)
    if self.again then
        self.again = self.again - elapsed
        if self.again <= 0 then
            self.again = nil
            Send("STATE")
        end
    end
    if state.loaded then
        return
    end
    self.wait = self.wait - elapsed
    if self.wait > 0 then
        return
    end
    self.wait, self.tries = 3, self.tries + 1
    Send("STATE")
    if self.tries == 20 and DEFAULT_CHAT_FRAME then
        DEFAULT_CHAT_FRAME:AddMessage("|cffa335eeLegendary Runes:|r the server has not answered the rune panel. "
            .. "Please report this.")
    end
end)
EmberforgeRunes_Asking = asking

-- The button follows the server's state.
local RunesOnMessage = OnMessage
OnMessage = function(message)
    RunesOnMessage(message)
    UpdateRuneButton()
end

-- For tests and other addons.
EmberforgeRunes_Button = runeButton
EmberforgeRunes_OnMessage = OnMessage
EmberforgeRunes_State = state
EmberforgeRunes_View = view
EmberforgeRunes_Open = Open
EmberforgeRunes_Debug = { offers = offers, browser = browser, build = build, locked = locked, chrome = chrome,
    rows = rows, chips = chips, toggle = toggle, search = search, browse = browse, ask = ask, tip = tip,
    Step = Step, SwitchSlot = SwitchSlot, Note = Note }

-- Loaded to the end (Loader.lua's /runes stands aside).
EmberforgeRunes_Loaded = true

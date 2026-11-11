-- Emberforge Runes: the Legendary Rune panel, as the "Legendary Rune Panel" mockup draws it: no window, one 16:9
-- stage over the game world (1600 x 900 points, scaled to the screen), its artwork, light and particles.
--
-- Unbound (level 10 on): "Legendary Runic Enhancements", up to three rune cards the server suggests for the Hero's
-- abilities, and Browse all (every rune, three a page). Choosing one asks first (a rune is bound until a prestige),
-- then the chosen card erupts, the others burn away, it condenses into the rune's medallion, the medallion breaks
-- into its three slots (Invocation, Manifestation, Adaptation), and the three choices for each follow. Bound: the
-- medallion and its slots straight away; a done slot can be chosen again (free, out of combat).
-- The art is baked from the mockup's own drawing code (build_rune_art.py: Art/, Art.lua); the runes' words are
-- Catalog.lua. Also here: Throw Glaive's glow at 5 Fel Command (Rune of Fel Commandment).
--
-- The server (mod-titan, src/runes.cpp) talks in addon whispers to the player, prefix EFRN:
--   from server   S,<level>,<unlock level>,<spec>,<rune>,<m1>,<m2>,<m3>,<offers a.b.c or ->,<prompt 0|1>
--                 R,<rune>,<missing>   (a rune the server builds; missing: groups by '/', one of by '+'; '-' none)
--                 N,<rune>,<slot>,<choice>,<missing>   (a modifier's own needs)
--                 E,<text>   O (open the panel)
--   to server     STATE   PICK,<rune>   MOD,<slot 1-3>,<choice 1-3>

local PREFIX = "EFRN"
local ART = "Interface\\AddOns\\Emberforge_Runes\\Art\\"
local FONTS = "Interface\\AddOns\\Emberforge_Runes\\Fonts\\"
local CINZEL, CINZEL_BLACK = FONTS .. "Cinzel-Bold.ttf", FONTS .. "Cinzel-Black.ttf"
local SANS, SANS_MEDIUM = FONTS .. "AlegreyaSans-Regular.ttf", FONTS .. "AlegreyaSans-Medium.ttf"

local W, H = 1600, 900                                  -- the stage, in points
local CW, CH = 300, 440                                 -- a card
local CARD_Y = 450
local LAYOUTS = { { 800 }, { 605, 995 }, { 410, 800, 1190 } }
local CENTER_X, CENTER_Y = 800, 300                     -- the medallion
local MED_R = 94
local SEG_SPAN = 260                                    -- the medallion's pieces span this square
-- The slots about the medallion: Invocation (left), Manifestation (right), Adaptation (below).
local SLOT_POS = { { 530, 300, "l" }, { 1070, 300, "r" }, { 800, 452, "b" } }
local SLOT_NAMES = { "Invocation", "Manifestation", "Adaptation" }
local GOLDEN = { 1, 0.886, 0.659 }                      -- #ffe2a8
local EMBER = { 1, 0.702, 0.361 }                       -- #ffb35c

local catalog = EmberforgeRunesCatalog or {}
local art = EmberforgeRunesArt or { runes = {}, labels = {}, card = { u = 1, v = 1 },
    option = { u = 1, v = 1, margin = 24 }, pill = { u = 1, v = 1, margin = 20, width = 106 }, num = { span = 80 },
    arcane = { rgb = { 0.73, 0.55, 1 }, hot = { 0.92, 0.84, 1 }, ink = { 0.95, 0.9, 1 }, muted = { 0.8, 0.72, 0.9 } } }

local state = { level = UnitLevel and UnitLevel("player") or 1, unlock = 10, spec = 1, rune = 0, mods = { 0, 0, 0 },
    offers = {}, prompt = false, built = {}, needs = {}, error = nil, loaded = false }
-- mode: locked, offers, browse or build; phase: the mockup's (intro, offer, erupt, condense, split, build).
local view = { mode = nil, phase = nil, page = 1, prompted = {}, now = 0, t0 = 0, picks = { 0, 0, 0 } }

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

local function RuneById(id)
    for _, rune in ipairs(catalog) do
        if rune.id == id then
            return rune
        end
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

-- Easing, as the mockup's ---------------------------------------------------------------------------------------

local function clamp(v, a, b)
    a, b = a or 0, b or 1
    return math.max(a, math.min(b, v))
end
local function lerp(a, b, t) return a + (b - a) * t end
local function easeOut(t) return 1 - (1 - t) ^ 3 end
local function easeInOut(t)
    if t < 0.5 then
        return 4 * t * t * t
    end
    return 1 - (-2 * t + 2) ^ 3 / 2
end
local function backOut(t) return 1 + 2.6 * (t - 1) ^ 3 + 1.6 * (t - 1) ^ 2 end
local function rand(a, b) return a + math.random() * (b - a) end
local function seg(t, a, b) return clamp((t - a) / (b - a)) end
local TAU = 2 * math.pi

-- Colours: the theme in play (Arcane until a rune is chosen, as the mockup's) -----------------------------------

local function Theme(runeId)
    local entry = art.runes[runeId]
    if not entry or runeId == 10 or not runeId then
        local arcane = art.runes[10] or {}
        return { key = "Arcane", rgb = art.arcane.rgb, hot = art.arcane.hot, ink = art.arcane.ink,
            muted = art.arcane.muted, hue = arcane.hue or art.arcane.rgb, inner = arcane.inner or art.arcane.hot }
    end
    return { key = entry.built and tostring(runeId) or nil, rgb = entry.rgb, hot = entry.hot, ink = entry.ink,
        muted = entry.muted, hue = entry.hue, inner = entry.inner }
end
local theme = Theme(nil)

local function Hex(rgb)
    return string.format("%02x%02x%02x", rgb[1] * 255, rgb[2] * 255, rgb[3] * 255)
end

-- A line in the theme's muted colour, its **words** in its ink (the mockup's <b>).
local function Ink(text)
    local ink, muted = Hex(theme.ink), Hex(theme.muted)
    return "|cff" .. muted .. text:gsub("%*%*(.-)%*%*", "|r|cff" .. ink .. "%1|r|cff" .. muted) .. "|r"
end

-- The stage ------------------------------------------------------------------------------------------------------

local frame = CreateFrame("Frame", "EmberforgeLegendaryRunes", UIParent)
frame:SetAllPoints(UIParent)
frame:SetFrameStrata("HIGH")                         -- under the bind question (DIALOG)
frame:EnableMouse(true)
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeLegendaryRunes")    -- Escape closes it

local dim = frame:CreateTexture(nil, "BACKGROUND")      -- the world, darkened toward the edges
dim:SetAllPoints()
dim:SetTexture(ART .. "Dim")

local stage = CreateFrame("Frame", nil, frame)
stage:SetWidth(W)
stage:SetHeight(H)
stage:SetPoint("CENTER")

local function Fit()
    local width, height = UIParent:GetWidth(), UIParent:GetHeight()
    local scale = math.min(width / W, height / H)
    if scale and scale > 0 then
        stage:SetScale(scale)
    end
end

-- Back to front: the rays and glows under the cards, the cards, the medallion, the light (sparks, shockwaves, the
-- flash), and the words and buttons.
local layers = {}
for index, name in ipairs({ "under", "cards", "medal", "fx", "ui" }) do
    local layer = CreateFrame("Frame", nil, stage)
    layer:SetAllPoints()
    layer:SetFrameLevel(stage:GetFrameLevel() + index * 4)
    layers[name] = layer
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

-- A region's centre at a stage point, and its size.
local function At(region, x, y, width, height)
    region:ClearAllPoints()
    region:SetPoint("CENTER", stage, "TOPLEFT", x, -y)
    if width then
        region:SetWidth(math.max(0.01, width))
        region:SetHeight(math.max(0.01, height or width))
    end
end

-- A square texture turned by angle (radians, clockwise on screen), by its texture coordinates.
local function Rotate(texture, angle)
    local c, s = math.cos(-angle), math.sin(-angle)
    local function corner(dx, dy)
        return 0.5 + dx * c - dy * s, 0.5 + dx * s + dy * c
    end
    local ulx, uly = corner(-0.5, -0.5)
    local llx, lly = corner(-0.5, 0.5)
    local urx, ury = corner(0.5, -0.5)
    local lrx, lry = corner(0.5, 0.5)
    texture:SetTexCoord(ulx, uly, llx, lly, urx, ury, lrx, lry)
end

local function Text(parent, font, size, color, justify, width)
    local text = parent:CreateFontString(nil, "OVERLAY")
    text:SetFont(font, size)
    text:SetShadowColor(0, 0, 0, 1)
    text:SetShadowOffset(0, -1)
    text:SetTextColor(color[1], color[2], color[3], color[4] or 1)
    text:SetJustifyH(justify or "CENTER")
    if width then
        text:SetWidth(width)
    end
    return text
end

-- One of the mockup's baked lines of lettering (Art.lua has its size and the share of its texture it fills).
local function Label(parent, name)
    local spec = art.labels[name] or { w = 100, h = 20, u = 1, v = 1 }
    local texture = Tex(parent, "OVERLAY", name)
    texture:SetTexCoord(0, spec.u, 0, spec.v)
    texture:SetWidth(spec.w)
    texture:SetHeight(spec.h)
    return texture
end

-- The mockup's .ui blocks: they fade in and rise 10 points (0.6 sec), and fade out sinking.
local groups = {}
local function PlaceGroup(group)
    group:ClearAllPoints()
    group:SetPoint("CENTER", stage, "TOPLEFT", group.x, -(group.y + 10 * (1 - group.alpha)))
end
local function Group(x, y, width, height, kind)
    local group = CreateFrame(kind or "Frame", nil, layers.ui)
    group:SetWidth(width)
    group:SetHeight(height)
    group.x, group.y, group.alpha, group.target = x, y, 0, 0
    group:SetAlpha(0)
    group:Hide()
    PlaceGroup(group)
    groups[#groups + 1] = group
    return group
end
local function Fade(group, on, instant)
    group.target = on and 1 or 0
    if instant then
        group.alpha = group.target
    end
    if on then
        group:Show()
    elseif group.alpha <= 0 then
        group:Hide()
    end
    group:SetAlpha(group.alpha)
    PlaceGroup(group)
end
-- A child at an offset from its group's centre (points, down positive).
local function In(region, group, dx, dy, width, height)
    region:ClearAllPoints()
    region:SetPoint("CENTER", group, "CENTER", dx, -dy)
    if width then
        region:SetWidth(width)
        region:SetHeight(height or width)
    end
end

-- The pill button: its ends and a stretched middle, its words in Cinzel.
local function PillArt(pill)
    local spec = art.pill
    local file = (pill.hot and "PillHot" or "Pill") .. (theme.key or "Arcane")
    local cap = spec.margin + 23
    local width = pill.width + 2 * spec.margin
    local u = function(points) return points / spec.width * spec.u end
    pill.left:SetTexture(ART .. file)
    pill.middle:SetTexture(ART .. file)
    pill.right:SetTexture(ART .. file)
    pill.left:SetTexCoord(0, u(cap), 0, spec.v)
    pill.middle:SetTexCoord(u(cap), u(spec.width - cap), 0, spec.v)
    pill.right:SetTexCoord(u(spec.width - cap), spec.u, 0, spec.v)
    pill.middle:SetWidth(math.max(0.01, width - 2 * cap))
    local color = pill.hot and { 1, 1, 1 } or theme.hot
    pill.label:SetTextColor(color[1], color[2], color[3])
end
local function PillText(pill, text)
    pill.label:SetText(text)
    pill.width = math.floor((pill.label:GetStringWidth() or 100) + 52)
    pill:SetWidth(pill.width)
    local height = 46 + 2 * art.pill.margin
    for _, piece in ipairs({ pill.left, pill.middle, pill.right }) do
        piece:SetHeight(height)
    end
    pill.left:SetWidth(art.pill.margin + 23)
    pill.right:SetWidth(art.pill.margin + 23)
    PillArt(pill)
end
local function Pill(parent, text, onClick)
    local pill = CreateFrame("Button", nil, parent)
    pill:SetHeight(46)
    pill.left = Tex(pill, "BACKGROUND")
    pill.middle = Tex(pill, "BACKGROUND")
    pill.right = Tex(pill, "BACKGROUND")
    pill.left:SetPoint("RIGHT", pill, "LEFT", 23, 0)
    pill.middle:SetPoint("LEFT", pill.left, "RIGHT", 0, 0)
    pill.right:SetPoint("LEFT", pill.middle, "RIGHT", 0, 0)
    pill.label = Text(pill, CINZEL, 16, theme.hot)
    pill.label:SetPoint("CENTER", 0, 0)
    pill:SetScript("OnEnter", function(self)
        self.hot = true
        PillArt(self)
    end)
    pill:SetScript("OnLeave", function(self)
        self.hot = false
        PillArt(self)
    end)
    pill:SetScript("OnClick", onClick)
    PillText(pill, text)
    return pill
end

-- Particles: the mockup's glow sprites, drawn adding light ---------------------------------------------------------

local particles, pool = {}, {}
local function SpriteOf(kind)
    -- kind: "acc" or "hot" (the theme's), or a fixed sprite's name.
    if kind == "acc" or kind == "hot" then
        if theme.key then
            return (kind == "hot" and "SpriteHot" or "Sprite") .. theme.key
        end
        return "Soft", (kind == "hot" and theme.hot or theme.rgb)
    end
    return kind
end
local function Emit(p)
    local texture = table.remove(pool) or Tex(layers.fx, "ARTWORK", nil, "ADD")
    local file, tint = SpriteOf(p.kind or "acc")
    texture:SetTexture(ART .. file)
    if p.tint or tint then
        local color = p.tint or tint
        texture:SetVertexColor(color[1], color[2], color[3])
    else
        texture:SetVertexColor(1, 1, 1)
    end
    texture:Show()
    p.texture = texture
    p.age, p.life = 0, p.life or 1
    p.drag, p.grav, p.size, p.alpha = p.drag or 0.985, p.grav or 0, p.size or 6, p.alpha or 0.9
    p.vx, p.vy = p.vx or 0, p.vy or 0
    particles[#particles + 1] = p
end
local function Ambient(dt)
    if math.random() < dt * 22 then
        local ember = math.random() >= 0.7
        Emit({ x = rand(0, W), y = H + 10, vx = rand(-10, 10), vy = rand(-40, -16), life = rand(5, 9),
            size = rand(2, 5), kind = ember and "SpriteGold" or "acc", alpha = ember and 0.5 or 0.55, drag = 1,
            mote = true })
    end
end
local function StepParticles(dt)
    local index = 1
    while index <= #particles do
        local p = particles[index]
        p.age = p.age + dt
        if p.age >= p.life then
            p.texture:Hide()
            pool[#pool + 1] = p.texture
            table.remove(particles, index)
        else
            if p.homing then
                local dx, dy = p.tx - p.x, p.ty - p.y
                local d = math.sqrt(dx * dx + dy * dy)
                if d < 1 then
                    d = 1
                end
                local pull = 2600 * dt
                p.vx, p.vy = p.vx + dx / d * pull, p.vy + dy / d * pull
                if p.swirl then
                    p.vx, p.vy = p.vx - dy / d * 300 * dt, p.vy + dx / d * 300 * dt
                end
                p.vx, p.vy = p.vx * 0.9, p.vy * 0.9
                if d < 14 then
                    p.age = p.life
                end
            end
            p.vy = p.vy + p.grav * dt
            p.vx, p.vy = p.vx * p.drag, p.vy * p.drag
            p.x, p.y = p.x + p.vx * dt, p.y + p.vy * dt
            local k = 1 - p.age / p.life
            local alpha = p.mote and math.sin(math.pi * p.age / p.life) or k
            local size = p.size * (p.mote and 1 or 0.6 + 0.6 * k) * 3
            if p.streak then
                size = size * 1.4
            end
            p.texture:SetAlpha(clamp(alpha) * p.alpha)
            At(p.texture, p.x, p.y, size, size)
            index = index + 1
        end
    end
end
local function ClearParticles()
    for _, p in ipairs(particles) do
        p.texture:Hide()
        pool[#pool + 1] = p.texture
    end
    particles = {}
end

-- Shockwaves and the flash.
local shocks, shockPool = {}, {}
local function Shock(x, y, max, color, delay)
    local wave = table.remove(shockPool)
    if not wave then
        wave = { band = Tex(layers.fx, "BORDER", "ShockBand", "ADD"), line = Tex(layers.fx, "BORDER", "ShockLine",
            "ADD") }
    end
    wave.x, wave.y, wave.max, wave.t = x, y, max, -(delay or 0)
    wave.band:SetVertexColor(color[1], color[2], color[3])
    shocks[#shocks + 1] = wave
end
local function StepShocks(dt)
    local index = 1
    while index <= #shocks do
        local wave = shocks[index]
        wave.t = wave.t + dt
        local k = wave.t / 0.8
        if k >= 1 then
            wave.band:Hide()
            wave.line:Hide()
            shockPool[#shockPool + 1] = wave
            table.remove(shocks, index)
        else
            if k >= 0 then
                local r = wave.max * easeOut(k)
                local a = (1 - k) * (1 - k)
                At(wave.band, wave.x, wave.y, 2 * r, 2 * r)
                At(wave.line, wave.x, wave.y, 2 * r, 2 * r)
                wave.band:SetAlpha(a * 0.32)
                wave.line:SetAlpha(a * 0.45)
                wave.band:Show()
                wave.line:Show()
            end
            index = index + 1
        end
    end
end
local flash = { level = 0, texture = Tex(layers.fx, "OVERLAY", nil, "ADD") }
flash.texture:SetTexture(1, 1, 1)
flash.texture:SetPoint("TOPLEFT", frame, "TOPLEFT")
flash.texture:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT")
flash.texture:Hide()

-- Rays: the eruption's, and the medallion's.
local rays = Tex(layers.under, "ARTWORK", "Rays", "ADD")
rays:Hide()
local function DrawRays(x, y, radius, alpha, spin)
    At(rays, x, y, 2 * radius, 2 * radius)
    Rotate(rays, spin)
    rays:SetVertexColor(theme.rgb[1], theme.rgb[2], theme.rgb[3])
    rays:SetAlpha(clamp(alpha))
    rays:Show()
end

-- Words ------------------------------------------------------------------------------------------------------------

local head = Group(800, 114, 1100, 110)
head.title = Label(head, "Title")
In(head.title, head, 0, -24)
head.subtitle = Text(head, SANS, 20, art.arcane.muted)
In(head.subtitle, head, 0, 24)

local note = Group(800, 718, 1100, 30)
note.text = Text(note, SANS, 17, GOLDEN)
In(note.text, note, 0, 0)
local function Note(text)
    note.text:SetText(text)
    note.until_ = view.now + 2.6
    Fade(note, true)
end

local Refresh, Enter, StartIntro

-- Unbound: "Not seeing what you're looking for?" and Browse all; browsing, the pages.
local browseAsk = Group(800, 805, 700, 80)
browseAsk.text = Text(browseAsk, SANS, 18, art.arcane.muted)
In(browseAsk.text, browseAsk, 0, -22)
browseAsk.pill = Pill(browseAsk, "Browse all and choose one manually", function()
    view.mode, view.page = "browse", 1
    Enter()
end)
In(browseAsk.pill, browseAsk, 0, 22)

local pager = Group(800, 805, 900, 80)
pager.text = Text(pager, SANS, 18, art.arcane.muted)
In(pager.text, pager, 0, -22)
local function Pages()
    return math.max(1, math.ceil(#catalog / 3))
end
pager.previous = Pill(pager, "Previous", function()
    view.page = view.page > 1 and view.page - 1 or Pages()
    StartIntro()
end)
pager.back = Pill(pager, "Back to your suggestions", function()
    view.mode = "offers"
    Enter()
end)
pager.next = Pill(pager, "Next", function()
    view.page = view.page < Pages() and view.page + 1 or 1
    StartIntro()
end)
local function LayPager()
    local gap = 16
    local total = pager.previous.width + pager.back.width + pager.next.width + 2 * gap
    local x = -total / 2
    for _, pill in ipairs({ pager.previous, pager.back, pager.next }) do
        In(pill, pager, x + pill.width / 2, 22)
        x = x + pill.width + gap
    end
end
LayPager()

local empty = Group(800, 450, 900, 60)
empty.text = Text(empty, SANS, 20, art.arcane.muted, "CENTER", 700)
In(empty.text, empty, 0, 0)
empty.text:SetText("None of the runes fits your abilities yet. Browse all to see what each one needs.")

-- The close button, where the mockup has its Replay.
local closer = Group(1500, 856, 200, 46)
closer.pill = Pill(closer, "Close", function() frame:Hide() end)
closer.pill.label:SetFont(CINZEL, 14)
PillText(closer.pill, "Close")
In(closer.pill, closer, 0, 0)
closer.x = W - 26 - closer.pill.width / 2

-- Bound: the rune's name and its line, above the medallion.
local buildHead = Group(800, 120, 1000, 160)
buildHead.eyebrow = Label(buildHead, "Eyebrow")
In(buildHead.eyebrow, buildHead, 0, -67)
buildHead.name = Text(buildHead, CINZEL_BLACK, 40, { 1, 1, 1 })
In(buildHead.name, buildHead, 0, -27)
buildHead.glow = Tex(buildHead, "BACKGROUND", "Soft", "ADD")
In(buildHead.glow, buildHead, 0, -27, 760, 120)
buildHead.core = Text(buildHead, SANS, 17, { 1, 1, 1 }, "CENTER", 620)
buildHead.core:SetPoint("TOP", buildHead, "CENTER", 0, -8)
buildHead.core:SetSpacing(7)

local function SetBuildHead(runeId)
    local rune = RuneById(runeId)
    local entry = art.runes[runeId] or {}
    buildHead.name:SetText(rune and rune.name or "")
    buildHead.name:SetTextColor(theme.ink[1], theme.ink[2], theme.ink[3])
    buildHead.glow:SetVertexColor(theme.rgb[1], theme.rgb[2], theme.rgb[3])
    buildHead.glow:SetAlpha(0.45)
    buildHead.core:SetText(Ink(entry.core or (rune and rune.core) or ""))
end

-- The slots: a number disc on the medallion's side, the slot's name and its pick beside it.
local slots = {}
for index, place in ipairs(SLOT_POS) do
    local x, y, side = place[1], place[2], place[3]
    local slot = Group(x, y, 54, 54, "Button")
    slot.index, slot.side = index, side
    slot.glow = Tex(slot, "BACKGROUND", "Soft", "ADD")
    In(slot.glow, slot, 0, 0, 110, 110)
    slot.disc = Tex(slot, "BORDER")
    In(slot.disc, slot, 0, 0, art.num.span, art.num.span)
    slot.numeral = Text(slot, CINZEL_BLACK, 18, { 1, 1, 1 })
    In(slot.numeral, slot, 0, 0)
    slot.numeral:SetText(({ "I", "II", "III" })[index])
    slot.small = Label(slot, "Slot" .. index)
    slot.pick = Text(slot, CINZEL, 20, { 1, 1, 1 }, side == "l" and "RIGHT" or "LEFT", side == "b" and 230 or 260)
    local pad = 6                                       -- the baked label's margin
    if side == "l" then
        slot.small:SetPoint("RIGHT", slot, "CENTER", -51 + pad, 12.5)
        slot.pick:SetPoint("RIGHT", slot, "CENTER", -51, -8)
    else
        slot.small:SetPoint("LEFT", slot, "CENTER", 51 - pad, 12.5)
        slot.pick:SetPoint("LEFT", slot, "CENTER", 51, -8)
    end
    -- The whole slot answers the mouse (its disc and its words).
    slot:SetHitRectInsets(side == "l" and -246 or 0, side == "l" and 0 or -246, -8, -8)
    slots[index] = slot
end

-- Choosing: "Choose your ...", the slot's question, and its three options.
local choose = Group(800, 640, 1200, 240)
choose.labels = {}
for index = 1, 3 do
    choose.labels[index] = Label(choose, "Choose" .. index)
    In(choose.labels[index], choose, 0, -82)
end
choose.ask = Text(choose, SANS, 17, art.arcane.muted)
In(choose.ask, choose, 0, -47)

local options = {}
local OPTION_W, OPTION_H = 340, 172
for index = 1, 3 do
    local option = CreateFrame("Button", nil, choose)
    option:SetWidth(OPTION_W)
    option:SetHeight(OPTION_H)
    option.index, option.dx, option.lift, option.shown = index, (index - 2) * 366, 0, 0
    option.bg = Tex(option, "BACKGROUND")
    local margin = art.option.margin
    option.bg:SetPoint("TOPLEFT", -margin, margin)
    option.bg:SetPoint("BOTTOMRIGHT", margin, -margin)
    option.bg:SetTexCoord(0, art.option.u, 0, art.option.v)
    option.icon = Tex(option, "ARTWORK")
    option.icon:SetWidth(44)
    option.icon:SetHeight(44)
    option.icon:SetPoint("TOPLEFT", 22, -20)
    option.name = Text(option, CINZEL, 19, { 1, 1, 1 }, "LEFT", 240)
    option.name:SetPoint("LEFT", option, "TOPLEFT", 78, -42)
    option.ability = Text(option, SANS_MEDIUM, 13, { 1, 1, 1 }, "LEFT", 296)
    option.ability:SetPoint("TOPLEFT", 22, -72)
    option.text = Text(option, SANS, 16, { 1, 1, 1 }, "LEFT", 296)
    option.text:SetPoint("TOPLEFT", 22, -96)
    option.text:SetSpacing(5)
    option:SetScript("OnEnter", function(self)
        self.hover = true
    end)
    option:SetScript("OnLeave", function(self)
        self.hover = false
    end)
    options[index] = option
end

-- Done: the build the three picks make.
local done = Group(800, 603, 1000, 110)
done.eyebrow = Label(done, "DoneEyebrow")
In(done.eyebrow, done, 0, -34)
done.name = Text(done, CINZEL, 30, GOLDEN)
In(done.name, done, 0, 0)
done.list = Text(done, SANS, 18, { 1, 1, 1 })
In(done.list, done, 0, 40)

-- The medallion -------------------------------------------------------------------------------------------------

local medal = { pieces = {} }
medal.halo = Tex(layers.medal, "BACKGROUND", "Soft", "ADD")
medal.ring = Tex(layers.medal, "BORDER", "RingGold")
medal.dash = Tex(layers.medal, "BORDER", "RingDash", "ADD")
for q = 1, 3 do
    medal.pieces[q] = { glow = Tex(layers.medal, "BORDER", "SegGlow", "ADD"), gold = Tex(layers.medal, "ARTWORK",
        "SegGold"), line = Tex(layers.medal, "OVERLAY", "SegLine") }
end
medal.core = Tex(layers.medal, "ARTWORK")
medal.sparks = {}
for k = 1, 6 do
    medal.sparks[k] = Tex(layers.medal, "OVERLAY", nil, "ADD")
end
local function HideMedal()
    for _, texture in ipairs({ medal.halo, medal.ring, medal.dash, medal.core }) do
        texture:Hide()
    end
    for _, piece in ipairs(medal.pieces) do
        piece.glow:Hide()
        piece.gold:Hide()
        piece.line:Hide()
    end
    for _, spark in ipairs(medal.sparks) do
        spark:Hide()
    end
end
HideMedal()

local function DrawMedal()
    local m = view.medal
    if not m then
        HideMedal()
        return
    end
    local now, x, y = view.now, m.x, m.y
    At(medal.halo, x, y, MED_R * 5.2, MED_R * 5.2)
    medal.halo:SetVertexColor(theme.rgb[1], theme.rgb[2], theme.rgb[3])
    medal.halo:SetAlpha(clamp(0.35 + 0.12 * math.sin(now * 2) + (m.charge or 0) * 0.06))
    medal.halo:Show()
    for q, place in ipairs(SLOT_POS) do
        local sx, sy = place[1], place[2]
        local mid = math.atan2(sy - y, sx - x)
        local k = easeInOut(clamp(m.split * 1.15 - (q - 1) * 0.05))
        local ox, oy, scale = (sx - x) * k, (sy - y) * k, 1 - 0.58 * k
        local lit = q <= (m.charge or 0)
        local piece = medal.pieces[q]
        for _, texture in ipairs({ piece.glow, piece.gold, piece.line }) do
            At(texture, x + ox, y + oy, SEG_SPAN * scale)
            Rotate(texture, mid)
            texture:Show()
        end
        local glow = lit and theme.inner or theme.hue
        piece.glow:SetVertexColor(glow[1], glow[2], glow[3])
        piece.glow:SetAlpha(0.85)
        if lit then
            piece.line:SetVertexColor(1, 1, 1)
        else
            piece.line:SetVertexColor(theme.hue[1], theme.hue[2], theme.hue[3])
        end
    end
    if m.split > 0.5 then
        local alpha = (m.split - 0.5) * 2
        At(medal.ring, x, y, SEG_SPAN)
        medal.ring:SetAlpha(alpha)
        medal.ring:Show()
        At(medal.dash, x, y, SEG_SPAN)
        Rotate(medal.dash, now * 0.3)
        medal.dash:SetVertexColor(theme.rgb[1], theme.rgb[2], theme.rgb[3])
        medal.dash:SetAlpha(alpha * 0.8)
        medal.dash:Show()
    else
        medal.ring:Hide()
        medal.dash:Hide()
    end
    At(medal.core, x, y, SEG_SPAN)
    medal.core:Show()
    for k, spark in ipairs(medal.sparks) do
        local angle = now * 0.8 + (k - 1) * math.pi / 3
        local radius = MED_R * 1.18
        spark:SetTexture(ART .. SpriteOf("hot"))
        At(spark, x + math.cos(angle) * radius, y + math.sin(angle) * radius * 0.92, 20)
        spark:SetAlpha(0.9)
        spark:Show()
    end
end

-- The cards ---------------------------------------------------------------------------------------------------------

local cardArt = art.card
local TILE = 10                                         -- the burn's pieces, in points
local TILES_X, TILES_Y = CW / TILE, CH / TILE

local cards = {}
local hits = {}
for index = 1, 3 do
    local card = { index = index }
    local holder = CreateFrame("Frame", nil, layers.cards)
    holder:SetWidth(CW)
    holder:SetHeight(CH)
    holder:SetFrameLevel(layers.cards:GetFrameLevel() + index)
    card.holder = holder
    card.aura = Tex(holder, "BACKGROUND", "Soft", "ADD")
    card.aura:SetPoint("CENTER")
    card.back = Tex(holder, "BORDER", "Soft", "ADD")
    card.back:SetPoint("CENTER")
    card.back:SetWidth(CW * 1.6)
    card.back:SetHeight(CH * 1.44)
    card.art = Tex(holder, "ARTWORK")
    card.art:SetPoint("CENTER")
    card.art:SetHeight(CH)
    card.art:SetTexCoord(0, cardArt.u, 0, cardArt.v)
    card.emblem = Tex(holder, "OVERLAY", "Soft", "ADD")
    card.emblem:SetPoint("CENTER", 0, CH / 2 - 146)
    card.emblem:SetWidth(180)
    card.emblem:SetHeight(180)
    card.status = Text(layers.ui, SANS, 16, art.arcane.muted, "CENTER", 300)
    holder:Hide()
    cards[index] = card

    -- The card's hit area stays where the card sits (the mockup's .hit).
    local hit = CreateFrame("Button", nil, layers.ui)
    hit:SetWidth(CW)
    hit:SetHeight(CH)
    hit.card = card
    hit:SetScript("OnEnter", function(self) self.card.hover = true end)
    hit:SetScript("OnLeave", function(self) self.card.hover = false end)
    hit:Hide()
    hits[index] = hit
end

-- The burn: the card in TILE-point pieces, each going at its noise (the mockup's: three octaves of value noise and a
-- fire that catches near a bottom corner and climbs), rimmed in flame and charred just behind it.
local function NoiseMap(seed)
    local function octave(cell)
        local cw, ch = math.ceil(CW / cell) + 2, math.ceil(CH / cell) + 2
        local values = {}
        for i = 1, cw * ch do
            values[i] = math.random()
        end
        return function(x, y)
            local fx, fy = x / cell, y / cell
            local x0, y0 = math.floor(fx), math.floor(fy)
            local tx, ty = fx - x0, fy - y0
            local sx, sy = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
            local function at(ix, iy) return values[iy * cw + ix + 1] end
            return lerp(lerp(at(x0, y0), at(x0 + 1, y0), sx), lerp(at(x0, y0 + 1), at(x0 + 1, y0 + 1), sx), sy)
        end
    end
    local o1, o2, o3 = octave(90), octave(36), octave(14)
    local ix, iy, maxD = seed * CW, CH * 1.05, math.sqrt(CW * CW + CH * CH)
    local map, lo, hi = {}, 1e9, -1e9
    for row = 0, TILES_Y - 1 do
        for column = 0, TILES_X - 1 do
            local x, y = (column + 0.5) * TILE, (row + 0.5) * TILE
            local n = o1(x, y) * 0.55 + o2(x, y) * 0.3 + o3(x, y) * 0.15
            local d = math.sqrt((x - ix) ^ 2 + (y - iy) ^ 2) / maxD
            local v = d * 0.72 + n * 0.42
            map[#map + 1] = v
            lo, hi = math.min(lo, v), math.max(hi, v)
        end
    end
    for i = 1, #map do
        map[i] = (map[i] - lo) / (hi - lo)
    end
    return map
end

local function LightBurn(card)
    card.noise = NoiseMap(rand(0.1, 0.9))
    card.tiles = card.tiles or {}
    local file = "Card" .. card.rune.id
    local i = 0
    for row = 0, TILES_Y - 1 do
        for column = 0, TILES_X - 1 do
            i = i + 1
            local tile = card.tiles[i]
            if not tile then
                tile = Tex(card.holder, "ARTWORK")
                tile:SetWidth(TILE)
                tile:SetHeight(TILE)
                tile:SetPoint("TOPLEFT", card.holder, "TOPLEFT", column * TILE, -row * TILE)
                card.tiles[i] = tile
            end
            tile.file = ART .. file
            tile.u0, tile.u1 = column / TILES_X * cardArt.u, (column + 1) / TILES_X * cardArt.u
            tile.v0, tile.v1 = row / TILES_Y * cardArt.v, (row + 1) / TILES_Y * cardArt.v
            tile:SetTexture(tile.file)
            tile:SetTexCoord(tile.u0, tile.u1, tile.v0, tile.v1)
            tile:SetVertexColor(1, 1, 1)
            tile:Show()
            tile.look = "whole"
        end
    end
    card.art:Hide()
end

local function EndBurn(card)
    for _, tile in ipairs(card.tiles or {}) do
        tile:Hide()
    end
    card.noise = nil
end

-- Each tile is touched only when its look changes: gone, a step of the flame (solid, as the mockup paints its burning
-- edge) or of the char (the card darkened), or whole.
local function TileState(tile, look, flame, r, g, b)
    if tile.look == look then
        return
    end
    tile.look = look
    if look == "gone" then
        tile:Hide()
        return
    end
    tile:Show()
    if flame then
        tile:SetTexture(r, g, b)
        tile:SetVertexColor(1, 1, 1)
    else
        tile:SetTexture(tile.file)
        tile:SetTexCoord(tile.u0, tile.u1, tile.v0, tile.v1)
        tile:SetVertexColor(r, g, b)
    end
end

local function BurnStep(card, p)
    local edge, charW = 0.06, 0.12
    local embers = 0
    for i, tile in ipairs(card.tiles) do
        local v = card.noise[i] - p
        if v < 0 then
            TileState(tile, "gone")
        else
            if v < edge then
                local k = math.floor(v / edge * 4) / 4
                TileState(tile, "flame" .. k, true, 1, (120 + 135 * (1 - k) ^ 2) / 255, (40 + 150 * (1 - k) ^ 3) / 255)
                if embers < 6 and math.random() < 0.08 then
                    embers = embers + 1
                    local column, row = (i - 1) % TILES_X, math.floor((i - 1) / TILES_X)
                    local px, py = (column + 0.5) * TILE - CW / 2, (row + 0.5) * TILE - CH / 2
                    Emit({ x = card.x + px * card.s, y = card.y + py * card.s, vx = rand(-30, 30), vy = rand(-140, -50),
                        grav = -40, life = rand(0.6, 1.4), size = rand(2, 5), drag = 0.97, alpha = 0.95,
                        kind = math.random() < 0.6 and "SpriteEmber" or "SpriteCream" })
                end
            elseif v < edge + charW then
                local k = math.floor((v - edge) / charW * 6) / 6
                TileState(tile, "char" .. k, false, 0.15 + 0.85 * k + 0.2 * (1 - k), 0.1 + 0.9 * k, 0.1 + 0.9 * k)
            else
                TileState(tile, "whole", false, 1, 1, 1)
            end
        end
    end
end

local function DrawCard(card)
    if card.gone or card.a <= 0 then
        card.holder:Hide()
        return
    end
    local scale = math.max(0.01, card.s)
    local holder = card.holder
    holder:SetScale(scale)
    holder:ClearAllPoints()
    holder:SetPoint("CENTER", stage, "TOPLEFT", card.x / scale, -(card.y - card.lift) / scale)
    holder:Show()
    local fade = card.burn >= 0 and clamp(1 - card.burn * 1.1) or 1
    local glow = card.theme.rgb
    local auraR = 260 + card.glow * 160
    card.aura:SetWidth(auraR * 2)
    card.aura:SetHeight(auraR * 2)
    card.aura:SetVertexColor(glow[1], glow[2], glow[3])
    card.aura:SetAlpha(clamp(card.a * (0.22 + card.glow * 0.5) * fade))
    card.back:SetVertexColor(glow[1], glow[2], glow[3])
    card.back:SetAlpha(clamp(card.a * (0.55 + card.glow * 0.6) * fade * 0.8))
    if card.noise then
        BurnStep(card, card.burn)
    else
        card.art:SetWidth(CW * (card.flip or 1))
        card.art:SetAlpha(card.a)
        card.art:Show()
    end
    local pulse = 0.35 + 0.25 * math.sin(view.now * 2.2 + card.index) + card.glow * 1.2 + (card.hover and 0.4 or 0)
    card.emblem:SetVertexColor(glow[1], glow[2], glow[3])
    card.emblem:SetAlpha(clamp(card.a * clamp(pulse, 0, 1.4) * 0.9 * fade))
end

local function HideCards()
    for index, card in ipairs(cards) do
        if card.noise then
            EndBurn(card)
        end
        card.holder:Hide()
        card.status:Hide()
        card.rune = nil
        hits[index]:Hide()
    end
end

-- The runes on show: the server's suggestions, or a page of every rune.
local function Shown()
    local list = {}
    if view.mode == "browse" then
        for i = (view.page - 1) * 3 + 1, math.min(#catalog, view.page * 3) do
            list[#list + 1] = catalog[i]
        end
    else
        for _, id in ipairs(state.offers) do
            local rune = RuneById(id)
            if rune and #list < 3 then
                list[#list + 1] = rune
            end
        end
    end
    return list
end

-- The flow ----------------------------------------------------------------------------------------------------------

local timers = {}
local function After(delay, fn)
    timers[#timers + 1] = { at = view.now + delay, fn = fn }
end

local function UseTheme(runeId)
    theme = Theme(runeId)
    for _, pill in ipairs({ browseAsk.pill, pager.previous, pager.back, pager.next, closer.pill }) do
        PillArt(pill)
    end
    head.subtitle:SetTextColor(theme.muted[1], theme.muted[2], theme.muted[3])
end

local function HideAll(instant)
    for _, group in ipairs(groups) do
        Fade(group, false, instant)
    end
end

-- Offers or a page of every rune: the cards rise into place (the mockup's intro).
function StartIntro()
    UseTheme(nil)
    ClearParticles()
    HideCards()
    view.medal, view.chosen = nil, nil
    HideMedal()
    rays:Hide()
    local list = Shown()
    local layout = LAYOUTS[#list] or {}
    for index, rune in ipairs(list) do
        local card = cards[index]
        card.rune, card.theme = rune, Theme(rune.id)
        card.homeX = layout[index]
        card.x, card.y, card.s, card.a, card.lift, card.glow = card.homeX, CARD_Y + 60, 0.92, 0, 0, 0
        card.burn, card.gone, card.flip, card.shake, card.erupted, card.hover = -1, false, 1, nil, false, false
        card.art:SetTexture(ART .. "Card" .. rune.id)
        card.art:SetDesaturated(false)
        local hit = hits[index]
        At(hit, card.homeX, CARD_Y)
        hit.rune = rune
        hit:Show()
        if view.mode == "browse" then
            local standing, missing = Standing(rune)
            card.status:SetText(state.rune == rune.id and "Bound" or (standing == "soon" and "Coming soon"
                or (standing == "missing" and ("Needs " .. missing) or "")))
            At(card.status, card.homeX, CARD_Y + CH / 2 + 22)
            card.status:Show()
        end
    end
    view.phase, view.t0 = "intro", view.now
    view.offersKey = table.concat(state.offers, ".")
    Fade(empty, view.mode == "offers" and #list == 0)
    After(0.15, function() Fade(head, true) end)
    if view.mode == "browse" then
        pager.text:SetText("Page " .. view.page .. " of " .. Pages())
        After(1.1, function() Fade(pager, true) end)
    else
        After(1.1, function() Fade(browseAsk, true) end)
    end
end

-- A card that cannot be chosen shakes, and the note says why.
local function Refuse(card, why)
    card.shake = view.now
    Note(why)
end

-- Binding a rune is for good (until a prestige): asked first; the eruption starts when the server binds it.
StaticPopupDialogs["EMBERFORGE_RUNE_BIND"] = {
    text = "Bind %s to this specialization?\n\nOnly a prestige can change it. Its modifiers stay free to change.",
    button1 = ACCEPT or "Accept",
    button2 = CANCEL or "Cancel",
    OnAccept = function(_, data)
        view.pending = data
        Send("PICK," .. data)
    end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
}

local function ClickCard(card)
    if view.phase ~= "offer" or not card.rune then
        return
    end
    local rune = card.rune
    if state.rune ~= 0 then
        Refuse(card, state.rune == rune.id and "This rune is bound to your specialization."
            or "Your specialization's rune is bound until your next prestige.")
        return
    end
    local standing, missing = Standing(rune)
    if standing == "soon" then
        Refuse(card, rune.name .. " is coming soon.")
    elseif standing == "missing" then
        Refuse(card, rune.name .. " needs " .. missing .. ".")
    else
        local dialog = StaticPopup_Show("EMBERFORGE_RUNE_BIND", rune.name)
        if dialog then
            dialog.data = rune.id
        end
    end
end
for _, hit in ipairs(hits) do
    hit:SetScript("OnClick", function(self) ClickCard(self.card) end)
end

-- The chosen card erupts; the rest burn away (the mockup's choose).
local function Erupt(index)
    view.chosen, view.phase, view.t0 = index, "erupt", view.now
    UseTheme(cards[index].rune.id)
    SetBuildHead(cards[index].rune.id)
    for _, hit in ipairs(hits) do
        hit:Hide()
    end
    Fade(head, false)
    Fade(browseAsk, false)
    Fade(pager, false)
    Fade(note, false)
    for i, card in ipairs(cards) do
        card.hover = false
        card.status:Hide()
        if i ~= index and card.rune then
            LightBurn(card)
        end
    end
end

-- The build view's words and options for the picks as they stand.
local function BuildName(rune)
    for _, build in ipairs(rune.builds or {}) do
        local match = true
        for slot = 1, 3 do
            local mod = rune.mods[(slot - 1) * 3 + view.picks[slot]]
            if view.picks[slot] == 0 or not mod or mod.name ~= build.mods[slot] then
                match = false
            end
        end
        if match then
            return (build.name:gsub("(%a)([%w']*)", function(first, rest) return first:upper() .. rest end))
        end
    end
end

local function FirstEmpty()
    for slot = 1, 3 do
        if view.picks[slot] == 0 then
            return slot
        end
    end
end

local function ShowSlots(active)
    for index, slot in ipairs(slots) do
        local pick = view.picks[index]
        local rune = RuneById(state.rune)
        local mod = rune and pick > 0 and rune.mods[(index - 1) * 3 + pick]
        local kind = index == active and "Active" or (pick > 0 and "Done" or "Plain")
        slot.kind = kind
        slot.disc:SetTexture(ART .. "Num" .. kind .. (theme.key or "Arcane"))
        slot.glow:SetVertexColor(theme.rgb[1], theme.rgb[2], theme.rgb[3])
        if kind == "Active" then
            slot.numeral:SetTextColor(1, 1, 1)
            slot.pick:SetText("Choose")
            slot.pick:SetTextColor(GOLDEN[1], GOLDEN[2], GOLDEN[3])
        elseif kind == "Done" then
            slot.numeral:SetTextColor(0.063, 0.024, 0.118)
            slot.pick:SetText(mod and mod.name or "")
            slot.pick:SetTextColor(1, 1, 1)
        else
            slot.numeral:SetTextColor(theme.rgb[1], theme.rgb[2], theme.rgb[3], 0.55)
            slot.pick:SetText("Locked")
            slot.pick:SetTextColor(theme.muted[1], theme.muted[2], theme.muted[3], 0.6)
        end
        slot.small:SetVertexColor(theme.muted[1], theme.muted[2], theme.muted[3])
    end
end

local function ShowOptions(slot)
    local rune = RuneById(state.rune)
    local entry = art.runes[state.rune] or {}
    view.active = slot
    for index, label in ipairs(choose.labels) do
        if index == slot then
            label:Show()
        else
            label:Hide()
        end
    end
    choose.ask:SetText(entry.asks and entry.asks[slot] or "")
    choose.ask:SetTextColor(theme.muted[1], theme.muted[2], theme.muted[3])
    for index, option in ipairs(options) do
        local number = (slot - 1) * 3 + index
        local mod = rune and rune.mods[number]
        local missing = MissingText(state.needs[state.rune .. "." .. slot .. "." .. index])
        option.slot, option.choice, option.missing = slot, index, missing
        option.picked = view.picks[slot] == index
        option.name:SetText(mod and mod.name or "")
        option.name:SetTextColor(theme.ink[1], theme.ink[2], theme.ink[3])
        option.ability:SetText(missing and ("Needs " .. missing):upper()
            or (entry.abilities and entry.abilities[number] or ""):upper())
        if missing then
            option.ability:SetTextColor(1, 0.47, 0.4)
        else
            option.ability:SetTextColor(theme.rgb[1], theme.rgb[2], theme.rgb[3])
        end
        option.text:SetText(entry.texts and entry.texts[number] or (mod and mod.text) or "")
        option.text:SetTextColor(theme.muted[1], theme.muted[2], theme.muted[3])
        option.icon:SetTexture(ART .. "Icon" .. state.rune .. "_" .. number)
        option.icon:SetDesaturated(missing ~= nil)
        option.shown, option.inAt, option.leaving = 0, view.now + 0.08 + (index - 1) * 0.11, nil
        option:Enable()
    end
    ShowSlots(slot)
    Fade(done, false)
    Fade(choose, true)
end

local function ShowDone()
    local rune = RuneById(state.rune)
    view.active = nil
    ShowSlots(nil)
    local names = {}
    for slot = 1, 3 do
        local mod = rune and rune.mods[(slot - 1) * 3 + view.picks[slot]]
        names[#names + 1] = mod and mod.name or ""
    end
    done.name:SetText(rune and BuildName(rune) or "Your own build")
    done.list:SetText(table.concat(names, "  \194\183  "))
    done.list:SetTextColor(theme.muted[1], theme.muted[2], theme.muted[3])
    Fade(choose, false)
    Fade(done, true)
end

local function ShowBuild()
    local slot = FirstEmpty()
    if slot then
        ShowOptions(slot)
    else
        ShowDone()
    end
end

-- Bound: the medallion already open, its slots in place (no eruption).
local function OpenBuild()
    UseTheme(state.rune)
    ClearParticles()
    HideCards()
    SetBuildHead(state.rune)
    view.picks = { state.mods[1], state.mods[2], state.mods[3] }
    view.medal = { x = CENTER_X, y = CENTER_Y, split = 1, charge = 0, cracked = true, slotted = true }
    for slot = 1, 3 do
        if view.picks[slot] > 0 then
            view.medal.charge = view.medal.charge + 1
        end
    end
    medal.core:SetTexture(ART .. "Core" .. state.rune)
    view.phase, view.t0 = "build", view.now
    Fade(buildHead, true)
    for _, slot in ipairs(slots) do
        Fade(slot, true)
    end
    ShowBuild()
end

-- A pick: light streams from the option into its slot, which takes it (the mockup's enhance).
local function Enhance(option)
    if view.busy or view.phase ~= "build" then
        return
    end
    if option.missing then
        Note(option.name:GetText() .. " needs " .. option.missing .. ".")
        return
    end
    local slot, choice = option.slot, option.choice
    view.busy = true
    Send("MOD," .. slot .. "," .. choice)
    local fx, fy = choose.x + option.dx, choose.y + 68
    local place = SLOT_POS[slot]
    local sx, sy = place[1], place[2]
    for k = 0, 69 do
        After(k / 70 * 0.26, function()
            Emit({ x = fx + rand(-30, 30), y = fy + rand(-20, 20), tx = sx, ty = sy, homing = true,
                vx = rand(-200, 200), vy = rand(-260, -60), life = 0.9, size = rand(4, 9), kind = "hot", alpha = 0.95 })
        end)
    end
    for _, other in ipairs(options) do
        other.leaving = true
        other:Disable()
    end
    After(0.52, function()
        view.busy = false
        for _ = 1, 60 do
            local angle, speed = rand(0, TAU), rand(60, 320)
            Emit({ x = sx, y = sy, vx = math.cos(angle) * speed, vy = math.sin(angle) * speed, life = rand(0.4, 0.9),
                size = rand(3, 7), kind = "hot", alpha = 0.95 })
        end
        Shock(sx, sy, 110, theme.hot)
        local wasEmpty = view.picks[slot] == 0
        view.picks[slot] = choice
        if view.medal and wasEmpty then
            view.medal.charge = math.min(3, (view.medal.charge or 0) + 1)
        end
        local next = FirstEmpty()
        if next then
            ShowOptions(next)
        else
            ShowDone()
            Shock(CENTER_X, CENTER_Y, 520, theme.hot)
            flash.level = 0.7
        end
    end)
end
for _, option in ipairs(options) do
    option:SetScript("OnClick", function(self) Enhance(self) end)
end
-- A done slot can be chosen again; an empty one after it waits its turn.
for _, slot in ipairs(slots) do
    slot:SetScript("OnClick", function(self)
        if view.phase == "build" and not view.busy and view.picks[self.index] > 0 then
            ShowOptions(self.index)
        end
    end)
end

-- The panel as the state has it: locked, offers, browse or the build.
function Enter()
    HideAll()
    Fade(closer, true)
    if state.level < state.unlock then
        view.mode = "locked"
        UseTheme(nil)
        HideCards()
        view.medal = nil
        HideMedal()
        head.subtitle:SetText("Legendary Runes unlock at level " .. state.unlock .. ".")
        Fade(head, true)
        view.phase = nil
        return
    end
    if state.rune ~= 0 and view.mode ~= "browse" then
        view.mode = "build"
        head.subtitle:SetText("")
        OpenBuild()
        return
    end
    if view.mode ~= "browse" then
        view.mode = "offers"
    end
    head.subtitle:SetText(view.mode == "browse" and "Every Legendary Rune, three at a time"
        or "Choices based off of your current abilities")
    browseAsk.text:SetText("Not seeing what you're looking for?")
    StartIntro()
end

-- A new state from the server, while the panel is open.
function Refresh()
    if not frame:IsShown() then
        return
    end
    if state.level < state.unlock then
        if view.mode ~= "locked" then
            Enter()
        end
        return
    end
    if state.rune ~= 0 then
        if view.pending == state.rune and (view.phase == "offer" or view.phase == "intro") then
            view.pending = nil
            for index, card in ipairs(cards) do
                if card.rune and card.rune.id == state.rune then
                    Erupt(index)
                    return
                end
            end
        end
        if view.mode == "build" and view.phase == "build" then
            view.picks = { state.mods[1], state.mods[2], state.mods[3] }
            if not view.busy then
                if view.active and view.picks[view.active] == 0 or not view.active and FirstEmpty() then
                    ShowBuild()
                elseif view.active then
                    ShowOptions(view.active)
                else
                    ShowDone()
                end
            end
        elseif view.mode ~= "build" and view.mode ~= "browse" and not view.chosen then
            Enter()
        end
    elseif view.mode == "build" or view.mode == "locked" or not view.mode then
        view.mode = "offers"
        Enter()
    elseif view.mode == "offers" and view.phase ~= "erupt" then
        -- New suggestions: deal them again only when they changed.
        if table.concat(state.offers, ".") ~= view.offersKey then
            StartIntro()
        end
    end
end

-- One frame of the mockup's animation.
local function Step(dt)
    view.now = view.now + dt
    local now, t = view.now, view.now - view.t0
    local index = 1
    while index <= #timers do
        if timers[index].at <= now then
            local timer = table.remove(timers, index)
            timer.fn()
        else
            index = index + 1
        end
    end
    for _, group in ipairs(groups) do
        if group.alpha ~= group.target then
            local step = dt / 0.6
            group.alpha = group.target > group.alpha and math.min(1, group.alpha + step)
                or math.max(0, group.alpha - step)
            group:SetAlpha(group.alpha)
            PlaceGroup(group)
            if group.alpha <= 0 then
                group:Hide()
            end
        end
    end
    if note.until_ and now > note.until_ then
        note.until_ = nil
        Fade(note, false)
    end
    Ambient(dt)

    local phase = view.phase
    if phase == "intro" then
        for i, card in ipairs(cards) do
            if card.rune then
                local k = backOut(seg(t, 0.25 + (i - 1) * 0.16, 1.05 + (i - 1) * 0.16))
                card.a = clamp(seg(t, 0.25 + (i - 1) * 0.16, 0.7 + (i - 1) * 0.16))
                card.y = CARD_Y + 70 * (1 - k)
                card.s = 0.9 + 0.1 * k
            end
        end
        if t > 1.5 then
            view.phase, view.t0 = "offer", now
        end
    elseif phase == "offer" then
        for i, card in ipairs(cards) do
            if card.rune then
                card.a = 1
                card.lift = lerp(card.lift, card.hover and 18 or 0, 0.15)
                card.y = CARD_Y + math.sin(now * 1.3 + (i - 1) * 1.7) * 6
                local shake = card.shake and now - card.shake or 9
                card.x = card.homeX + (shake < 0.45 and math.sin(shake * 60) * 10 * (1 - shake / 0.45) or 0)
                card.s = lerp(card.s, card.hover and 1.05 or 1, 0.15)
                card.glow = lerp(card.glow, card.hover and 0.45 or 0, 0.12)
                if card.hover and math.random() < 0.5 then
                    Emit({ x = card.x + rand(-CW / 2, CW / 2) * card.s, y = card.y + CH / 2 * card.s - card.lift,
                        vx = rand(-10, 10), vy = rand(-90, -40), life = rand(0.6, 1.2), size = rand(2, 5),
                        kind = card.theme.key and ("Sprite" .. card.theme.key) or "Soft",
                        tint = not card.theme.key and card.theme.rgb or nil })
                end
            end
        end
        -- The server bound a rune the panel was waiting on before the cards settled.
        if view.pending and state.rune == view.pending then
            Refresh()
        end
    elseif phase == "erupt" then
        local me = cards[view.chosen]
        local charge = seg(t, 0, 0.45)
        me.glow = charge < 1 and charge * 1.2 or lerp(1.2, 0.6, seg(t, 0.45, 2.0))
        me.s = charge < 1 and (1 + 0.06 * easeOut(charge) + math.sin(now * 60) * 0.004 * charge)
            or lerp(1.12, 1.04, easeOut(seg(t, 0.45, 1.2)))
        me.lift = lerp(me.lift, 10, 0.1)
        if charge < 1 then
            for _ = 1, 6 do
                local angle, d = rand(0, TAU), rand(220, 340)
                Emit({ x = me.x + math.cos(angle) * d, y = me.y + math.sin(angle) * d, tx = me.x, ty = me.y - 10,
                    homing = true, life = 0.5, size = rand(3, 6), alpha = 0.95 })
            end
        end
        if not me.erupted and t >= 0.45 then
            me.erupted = true
            flash.level = 1
            Shock(me.x, me.y, 760, theme.hot)
            Shock(me.x, me.y, 460, { 1, 0.88, 0.71 }, 0.1)
            for _ = 1, 260 do
                local angle = rand(0, TAU)
                local speed = rand(200, 900) * (math.random() < 0.2 and 1.5 or 1)
                Emit({ x = me.x + rand(-20, 20), y = me.y + rand(-40, 40), vx = math.cos(angle) * speed,
                    vy = math.sin(angle) * speed, drag = 0.93, life = rand(0.5, 1.5), size = rand(3, 10),
                    kind = math.random() < 0.7 and "acc" or "hot", alpha = 0.95, streak = true })
            end
        end
        for i, card in ipairs(cards) do
            if i ~= view.chosen and card.rune then
                local bk = seg(t, 0.35, 2.15)
                card.burn = (bk * bk * 0.35 + bk * 0.65) * 1.08
                card.lift = lerp(card.lift, 0, 0.1)
                card.x = card.x + math.sin(now * 40 + i) * 0.4 * seg(t, 0.3, 0.6)
            end
        end
        if me.erupted then
            local k = seg(t, 0.45, 2.2)
            DrawRays(me.x, me.y, 480 + 320 * easeOut(k), (1 - k) * 0.5 + 0.08, now * 0.5)
        end
        if t > 2.2 then
            view.phase, view.t0 = "condense", now
            for i, card in ipairs(cards) do
                if i ~= view.chosen and card.rune then
                    card.gone = true
                    EndBurn(card)
                end
            end
            me.from = { x = me.x, y = me.y, s = me.s }
            rays:Hide()
        end
    elseif phase == "condense" then
        local me = cards[view.chosen]
        local k = easeInOut(seg(t, 0, 0.9))
        me.x, me.y = lerp(me.from.x, CENTER_X, k), lerp(me.from.y, CENTER_Y, k)
        me.s, me.glow, me.lift = lerp(me.from.s, 0.38, k), 0.6 + 0.8 * k, 0
        me.flip = math.max(0.04, math.abs(math.cos(easeInOut(seg(t, 0.1, 0.95)) * TAU)))
        for _ = 1, 8 do
            local angle, d = rand(0, TAU), rand(160, 420)
            Emit({ x = me.x + math.cos(angle) * d, y = me.y + math.sin(angle) * d, tx = me.x, ty = me.y, homing = true,
                swirl = true, vx = -math.sin(angle) * 260, vy = math.cos(angle) * 260, life = 0.7, size = rand(3, 7),
                kind = math.random() < 0.7 and "acc" or "SpriteCream", alpha = 0.95 })
        end
        if t > 0.9 and not view.medal then
            view.medal = { x = CENTER_X, y = CENTER_Y, split = 0, charge = 0 }
            medal.core:SetTexture(ART .. "Core" .. state.rune)
            flash.level = 0.8
            Shock(CENTER_X, CENTER_Y, 420, theme.hot)
            for _ = 1, 120 do
                local angle, speed = rand(0, TAU), rand(120, 520)
                Emit({ x = CENTER_X, y = CENTER_Y, vx = math.cos(angle) * speed, vy = math.sin(angle) * speed,
                    drag = 0.92, life = rand(0.5, 1.1), size = rand(3, 8), kind = "hot", alpha = 0.95, streak = true })
            end
        end
        me.a = 1 - seg(t, 0.82, 1.0)
        if t > 1.3 then
            view.phase, view.t0 = "split", now
            me.gone = true
            view.mode = "build"
            Fade(buildHead, true)
        end
    elseif phase == "split" then
        local m = view.medal
        m.split = easeOut(seg(t, 0.15, 1.2))
        if t > 0.15 and not m.cracked then
            m.cracked = true
            flash.level = 0.35
            for _, place in ipairs(SLOT_POS) do
                for _ = 1, 18 do
                    local angle = math.atan2(place[2] - CENTER_Y, place[1] - CENTER_X) + rand(-0.4, 0.4)
                    local speed = rand(150, 420)
                    Emit({ x = CENTER_X + math.cos(angle) * 80, y = CENTER_Y + math.sin(angle) * 80,
                        vx = math.cos(angle) * speed, vy = math.sin(angle) * speed, drag = 0.94, life = rand(0.4, 0.9),
                        size = rand(2, 6), kind = "SpriteCream", alpha = 0.95, streak = true })
                end
            end
        end
        if t > 1.05 and not m.slotted then
            m.slotted = true
            view.picks = { state.mods[1], state.mods[2], state.mods[3] }
            ShowSlots(FirstEmpty())
            for i, slot in ipairs(slots) do
                After((i - 1) * 0.07, function() Fade(slot, true) end)
                for _ = 1, 18 do
                    local angle, speed = rand(0, TAU), rand(40, 160)
                    Emit({ x = slot.x, y = slot.y, vx = math.cos(angle) * speed, vy = math.sin(angle) * speed,
                        drag = 0.92, life = rand(0.4, 0.8), size = rand(2, 5), kind = "hot", alpha = 0.95 })
                end
            end
        end
        if t > 1.5 then
            view.phase, view.t0 = "build", now
            view.picks = { state.mods[1], state.mods[2], state.mods[3] }
            ShowBuild()
        end
    end

    if view.medal and (phase == "split" or phase == "build") then
        DrawRays(CENTER_X, CENTER_Y, 300, 0.22 + 0.05 * math.sin(now * 2), now * 0.15)
    elseif phase ~= "erupt" then
        rays:Hide()
    end
    for _, card in ipairs(cards) do
        if card.rune then
            DrawCard(card)
        end
    end
    DrawMedal()

    -- The options rise in one after another; hovered, they lift 8 points.
    if choose:IsShown() then
        for _, option in ipairs(options) do
            local target = option.leaving and 0 or (now >= (option.inAt or 0) and 1 or 0)
            option.shown = clamp(option.shown + (target > option.shown and dt or -dt) / 0.45)
            local k = target == 1 and backOut(option.shown) or option.shown
            option.lift = lerp(option.lift, (option.hover and option.shown >= 1) and 8 or 0, 0.2)
            option:ClearAllPoints()
            option:SetPoint("CENTER", choose, "CENTER", option.dx, -(68 + 26 * (1 - k) - option.lift))
            option:SetAlpha(option.missing and option.shown * 0.55 or option.shown)
            local hot = option.picked or (option.hover and not option.missing)
            option.bg:SetTexture(ART .. (hot and "OptionHot" or "Option") .. (theme.key or "Arcane"))
        end
    end
    -- The active slot's disc beats.
    for _, slot in ipairs(slots) do
        if slot.kind == "Active" then
            slot.glow:SetAlpha(0.35 + 0.3 * math.sin(now * math.pi / 0.8))
            slot.glow:Show()
        elseif slot.kind == "Done" then
            slot.glow:SetVertexColor(theme.hot[1], theme.hot[2], theme.hot[3])
            slot.glow:SetAlpha(0.35)
            slot.glow:Show()
        else
            slot.glow:Hide()
        end
    end

    StepShocks(dt)
    StepParticles(dt)
    if flash.level > 0 then
        flash.texture:SetVertexColor(theme.hot[1], theme.hot[2], theme.hot[3])
        flash.texture:SetAlpha(flash.level * 0.3)
        flash.texture:Show()
        flash.level = math.max(0, flash.level - dt * 2.6)
    else
        flash.texture:Hide()
    end
end

frame:SetScript("OnUpdate", function(_, elapsed)
    Step(math.min(0.05, elapsed))
end)

frame:SetScript("OnShow", function()
    Fit()
    state.error = nil
    Send("STATE")
end)
frame:SetScript("OnHide", function()
    HideAll(true)
    HideCards()
    ClearParticles()
    view.medal, view.chosen, view.phase, view.busy, view.pending = nil, nil, nil, false, nil
    HideMedal()
    rays:Hide()
    timers = {}
end)
frame:RegisterEvent("DISPLAY_SIZE_CHANGED")
frame:RegisterEvent("UI_SCALE_CHANGED")
frame:SetScript("OnEvent", Fit)

local function Open()
    if frame:IsShown() then
        return
    end
    view.mode = nil
    frame:Show()
    Enter()
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
        if frame:IsShown() then
            Note(state.error)
            view.busy = false
            if view.phase == "build" then
                view.picks = { state.mods[1], state.mods[2], state.mods[3] }
                ShowBuild()
            end
        end
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
EmberforgeRunes_Debug = { cards = cards, hits = hits, options = options, slots = slots, head = head, note = note,
    browseAsk = browseAsk, pager = pager, choose = choose, done = done, buildHead = buildHead, empty = empty,
    closer = closer, Step = Step, medal = medal }

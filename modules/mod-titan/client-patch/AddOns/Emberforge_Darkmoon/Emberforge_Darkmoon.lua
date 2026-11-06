-- Emberforge Darkmoon: Silas Darkmoon's window (opened by talking to him), Darkmoon Cards.
--   Specializations: the Hero's loadouts, and the Eyes of Unlearning: claimed one a level from 8 to 60 (four at the
--     milestones) on the leveling track, bought at 60.
--   Packs: how close the account is to its free packs; Skill, Talent, Golden Skill and Golden Talent Packs, opened
--     here (the reveal, or "Open all": the new cards in one list) or from the bags, and bought for Emberspark (Skill
--     Packs for gold too, a few a week).
--   Cards: every Skill and Talent Card, what the account owns, any one bought outright.
--
-- The server (mod-titan, src/cards.cpp) talks in addon whispers to the player, prefix EFDM:
--   from server   S,<Emberspark>,<Skill Pack>,<Talent Pack>,<Golden Pack>,<gold packs left>,<gold price, copper>,
--                   <duplicates>,<a pack every>,<a Golden Pack every>,<card price by rarity x4>
--                 E,<Eyes>,<claimed through level>,<level>,<Eye price>,<Eyes a purchase>,<can buy 0|1>,<Eyes bought>,
--                   <abilities rerolled>,<talents rerolled>,<first level>,<last level>,<milestone Eyes>,<m.m.m...>
--                 K0 / K,<kind>.<id>.<spell>.<rarity>.<owned>.<max>.<golden owned>;... / KE   the catalog
--                 U,<kind>.<id>.<owned>.<golden owned>;...   owned changes
--                 R,<pack>,<kind>.<id>.<spell>.<rarity>.<new 0|1>.<refund>;...   a pack opened
--                 M,<pack>,<count> ... ME   several packs opened at once (their R between)
--                 B,<kind>.<id>.<spell>.<rarity>   a card bought      O   open the window
--   to server     LIST, BUY,<pack>,<0 Emberspark|1 gold>, CARD,<kind>,<id>, OPEN,<pack>,<count>, CLAIM, EYES
-- Packs: 0 Skill, 1 Talent, 2 Golden Skill, 3 Golden Talent. Kinds: 0 Skill Card, 1 Talent Card.
-- Rarities: 0 Common, 1 Rare, 2 Epic, 3 Legendary.

local PREFIX = "EFDM"
local ART = "Interface\\AddOns\\Emberforge_Darkmoon\\Art\\"
local SPARK = "Interface\\AddOns\\Emberforge_Classless\\Art\\Spark"
local SPARK_ICON = "Interface\\Icons\\INV_Emberforge_Emberspark"
local LOCK = "Interface\\LFGFrame\\UI-LFG-ICON-LOCK"
local DISPLAY_FONT, BODY_FONT = "Fonts\\MORPHEUS.TTF", "Fonts\\FRIZQT__.TTF"
-- The window (the mockup's, three quarters the size), and the border around it.
local WIDTH, HEIGHT = 960, 620
local GOLD, GOLD_HI, MUTED = { 0.94, 0.75, 0.38 }, { 1, 0.91, 0.64 }, { 0.8, 0.71, 0.55 }
local GREEN = { 0.55, 1, 0.54 }
local RARITY_NAMES = { [0] = "Common", "Rare", "Epic", "Legendary" }
local RARITY_RGB = { [0] = { 0.9, 0.88, 0.96 }, { 0.31, 0.63, 1 }, { 0.77, 0.42, 1 }, { 1, 0.63, 0.24 } }
local NAME_RGB = { [0] = { 0.96, 0.94, 0.92 }, { 0.37, 0.7, 1 }, { 0.81, 0.53, 1 }, { 1, 0.65, 0.25 } }
local INK_RGB = { [0] = { 0.18, 0.16, 0.15 }, { 0.04, 0.25, 0.5 }, { 0.33, 0.08, 0.53 }, { 0.56, 0.24, 0 } }
local GOLDEN_INK, GOLDEN_RGB = { 0.42, 0.24, 0 }, { 1, 0.84, 0.3 }
-- Where the icon, the banner and the parchment's lines sit on each kind's card (fractions of the card).
local LAYOUT = {
    [0] = { x = 0.5, y = 0.287, r = 0.25, banner = 0.512, rarity = 0.705, kind = 0.765, note = 0.815, gems = 0.88 },
    [1] = { x = 0.5, y = 0.268, r = 0.285, banner = 0.508, rarity = 0.665, kind = 0.725, note = 0.775, gems = 0.845 },
}
local PACKS = {
    [0] = { name = "Skill Card Pack", item = 910010, art = "PackSkill", kind = 0 },
    [1] = { name = "Talent Card Pack", item = 910011, art = "PackTalent", kind = 1 },
    [2] = { name = "Golden Skill Pack", item = 910012, art = "PackSkillGolden", kind = 0, golden = true },
    [3] = { name = "Golden Talent Pack", item = 910013, art = "PackTalentGolden", kind = 1, golden = true },
}
local SPECS = {
    { name = "Specialization I", text = "The starter specialization that comes with every Hero. Its abilities and "
        .. "talents are the ones your level-up rolls gave you." },
    { name = "Specialization II", text = "A second set of abilities and talents, rolled fresh. Switch between them "
        .. "out of combat; each keeps its own rerolls." },
    { name = "Specialization III", text = "A third loadout: keep one for each role you play, or one for raiding and "
        .. "one for the arena." },
    { name = "Specialization IV", text = "The fourth and last loadout." },
}
local ROMAN = { "I", "II", "III", "IV" }
local CARDS_PER_PAGE, GRID_COLUMNS = 14, 7

local state = { spark = 0, prices = { [0] = 0, 0, 0, 0 }, goldLeft = 0, goldPrice = 750000, duplicates = 0,
    packEvery = 10, goldenEvery = 50, cardPrices = { [0] = 500, 5000, 12000, 30000 } }
local eyes = { count = 0, claimed = 7, level = 1, price = 100, perBuy = 2, canBuy = false, bought = 0, abilities = 0,
    talents = 0, first = 8, last = 60, milestoneEyes = 4, milestones = { 8, 10, 20, 30, 40, 50, 60 } }
local catalog, byKey, catalogLoaded, asked = {}, {}, false, false
local view = { tab = 1, spec = 1, kind = 0, rarity = -1, missing = false, golden = false, search = "", offset = 0 }

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function Thousands(value)
    local text = tostring(math.floor(value or 0))
    while true do
        local done
        text, done = text:gsub("^(%d+)(%d%d%d)", "%1,%2")
        if done == 0 then
            return text
        end
    end
end

local function CardName(card)
    return GetSpellInfo(card.spell) or ("Spell " .. card.spell)
end

local function CardIcon(card)
    local _, _, icon = GetSpellInfo(card.spell)
    return icon or "Interface\\Icons\\INV_Misc_QuestionMark"
end

local function IsMilestone(level)
    for _, milestone in ipairs(eyes.milestones) do
        if milestone == level then
            return true
        end
    end
end

local function LevelEyes(level)
    return IsMilestone(level) and eyes.milestoneEyes or 1
end

-- Widgets ------------------------------------------------------------------------------------------------------------

-- Shows or hides (the 3.3.5 client has no SetShown).
local function Shown(region, on)
    if on then
        region:Show()
    else
        region:Hide()
    end
end

local function Text(parent, size, color, font, flags)
    local text = parent:CreateFontString(nil, "OVERLAY")
    text:SetFont(font or BODY_FONT, size, flags)
    text:SetShadowOffset(1, -1)
    text:SetShadowColor(0, 0, 0, 0.9)
    text:SetTextColor(unpack(color or MUTED))
    return text
end

-- A texture of the art, sliced nine ways (its 64 px source cut at a quarter from each edge), so a rounded box keeps
-- its corners at any size; drawn again with another file, the same pieces are reused.
local SLICES = { { 0, 0.25 }, { 0.25, 0.75 }, { 0.75, 1 } }
local function Slice9(target, file, corner, layer)
    corner = corner or 12
    if not target.slices then
        target.slices = {}
        for row = 1, 3 do
            for column = 1, 3 do
                local piece = target:CreateTexture(nil, layer or "BACKGROUND")
                piece:SetTexCoord(SLICES[column][1], SLICES[column][2], SLICES[row][1], SLICES[row][2])
                local x = { 0, corner, 0 }
                local y = { 0, -corner, 0 }
                local hPoint = { "LEFT", "LEFT", "RIGHT" }
                local vPoint = { "TOP", "TOP", "BOTTOM" }
                -- The first anchor: this piece's own corner, inset by the corners before it.
                piece:SetPoint(vPoint[row] .. hPoint[column], x[column], y[row])
                if column == 2 then
                    piece:SetPoint(vPoint[row] .. "RIGHT", -corner, y[row])
                else
                    piece:SetWidth(corner)
                end
                if row == 2 then
                    piece:SetPoint("BOTTOM" .. hPoint[column], column == 2 and corner or x[column], corner)
                else
                    piece:SetHeight(corner)
                end
                target.slices[#target.slices + 1] = piece
            end
        end
    end
    for _, piece in ipairs(target.slices) do
        piece:SetTexture(ART .. file)
    end
    return target.slices
end

local function Tint(target, r, g, b, a)
    for _, piece in ipairs(target.slices or {}) do
        piece:SetVertexColor(r, g, b, a or 1)
    end
end

local function Panel(parent, file, x, y, width, height)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetPoint("TOPLEFT", x, -y)
    panel:SetWidth(width)
    panel:SetHeight(height)
    Slice9(panel, file or "Panel", 14)
    return panel
end

local function Heading(parent, label, x, y)
    local gem = parent:CreateTexture(nil, "ARTWORK")
    gem:SetTexture(ART .. "Gem")
    gem:SetWidth(18)
    gem:SetHeight(18)
    gem:SetPoint("TOPLEFT", x, -y)
    local text = Text(parent, 13, GOLD, DISPLAY_FONT)
    text:SetPoint("LEFT", gem, "RIGHT", 8, 0)
    text:SetText(label)
    return text, gem
end

-- Buttons: "Button" (dark), "ButtonHot" (orange), "ButtonRed" (the tab chosen); greyed out when disabled.
local function SetLook(button)
    local file = not button.enabled and "ButtonOff" or button.on and "ButtonRed" or button.look
    if button.file ~= file then
        button.file = file
        Slice9(button, file, 10)
    end
    local color = not button.enabled and { 0.42, 0.35, 0.28 } or (button.look == "ButtonHot" or button.on)
        and { 1, 0.96, 0.86 } or GOLD
    button.label:SetTextColor(unpack(color))
end

local function Button(parent, width, height, label, onClick, look, size)
    local button = CreateFrame("Button", nil, parent)
    button:SetWidth(width)
    button:SetHeight(height)
    button.look, button.enabled = look or "Button", true
    button.label = Text(button, size or 12, GOLD, BODY_FONT)
    button.label:SetPoint("CENTER", 0, 0)
    button.label:SetText(label)
    local hover = button:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(ART .. "Glow")
    hover:SetBlendMode("ADD")
    hover:SetVertexColor(1, 0.6, 0.25, 0.35)
    hover:SetAllPoints()
    button:SetScript("OnClick", function(self)
        if self.enabled then
            PlaySound("igMainMenuOptionCheckBoxOn")
            onClick(self)
        end
    end)
    function button:SetOn(on)
        self.on = on
        SetLook(self)
    end
    function button:SetEnabled(enabled)
        self.enabled = enabled and true or false
        SetLook(self)
    end
    function button:SetLabel(text)
        self.label:SetText(text)
    end
    SetLook(button)
    return button
end

local function Tooltip(region, title, text, note)
    region:EnableMouse(true)
    region:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_BOTTOMRIGHT")
        GameTooltip:AddLine(type(title) == "function" and title() or title, 1, 0.91, 0.64)
        GameTooltip:AddLine(type(text) == "function" and text() or text, 0.8, 0.71, 0.55, true)
        if note then
            GameTooltip:AddLine(note, 0.85, 0.64, 0.42, true)
        end
        GameTooltip:Show()
    end)
    region:SetScript("OnLeave", function() GameTooltip:Hide() end)
end

-- The window ---------------------------------------------------------------------------------------------------------

local frame = CreateFrame("Frame", "EmberforgeDarkmoon", UIParent)
frame:SetWidth(WIDTH)
frame:SetHeight(HEIGHT)
frame:SetPoint("CENTER", 0, 10)
frame:SetFrameStrata("DIALOG")
frame:SetToplevel(true)
frame:SetClampedToScreen(true)
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeDarkmoon")

local background = frame:CreateTexture(nil, "BACKGROUND")
background:SetTexture(ART .. "Background")
background:SetPoint("TOPLEFT", 8, -8)
background:SetPoint("BOTTOMRIGHT", -8, 8)

-- The gold border: corners and crest whole, the rails stretched between (the border art is cut by
-- build_darkmoon_art.py: corners 180 px, the crest 380 px wide, drawn at 0.6).
local border = CreateFrame("Frame", nil, frame)
border:SetPoint("TOPLEFT", -22, 22)
border:SetPoint("BOTTOMRIGHT", 22, -22)
border:SetFrameLevel(frame:GetFrameLevel() + 30)
do
    local corner, crest = 108, 228
    local function Piece(file, ...)
        local texture = border:CreateTexture(nil, "OVERLAY")
        texture:SetTexture(ART .. file)
        for index = 1, select("#", ...), 3 do
            local point, x, y = select(index, ...)
            texture:SetPoint(point, x, y)
        end
        return texture
    end
    for _, spec in ipairs({ { "BorderTL", "TOPLEFT" }, { "BorderTR", "TOPRIGHT" }, { "BorderBL", "BOTTOMLEFT" },
        { "BorderBR", "BOTTOMRIGHT" } }) do
        local texture = Piece(spec[1], spec[2], 0, 0)
        texture:SetWidth(corner)
        texture:SetHeight(corner)
    end
    local crestTexture = Piece("BorderCrest", "TOP", 0, 0)
    crestTexture:SetWidth(crest)
    crestTexture:SetHeight(corner)
    local topLeft = Piece("BorderTopL", "TOPLEFT", corner, 0)
    topLeft:SetPoint("BOTTOMRIGHT", crestTexture, "BOTTOMLEFT", 0, 0)
    local topRight = Piece("BorderTopR", "TOPRIGHT", -corner, 0)
    topRight:SetPoint("BOTTOMLEFT", crestTexture, "BOTTOMRIGHT", 0, 0)
    local bottom = Piece("BorderBottom", "BOTTOMLEFT", corner, 0)
    bottom:SetPoint("BOTTOMRIGHT", -corner, 0)
    bottom:SetHeight(corner)
    local left = Piece("BorderLeft", "TOPLEFT", 0, -corner)
    left:SetPoint("BOTTOMLEFT", 0, corner)
    left:SetWidth(corner)
    local right = Piece("BorderRight", "TOPRIGHT", 0, -corner)
    right:SetPoint("BOTTOMRIGHT", 0, corner)
    right:SetWidth(corner)
end

local title = Text(frame, 22, GOLD_HI, DISPLAY_FONT)
title:SetPoint("TOP", 0, -40)
title:SetText("Darkmoon Cards")

local close = CreateFrame("Button", nil, frame)
close:SetWidth(30)
close:SetHeight(30)
close:SetPoint("TOPRIGHT", -42, -18)
close:SetFrameLevel(border:GetFrameLevel() + 1)
do
    local ring = close:CreateTexture(nil, "ARTWORK")
    ring:SetTexture(ART .. "Ring")
    ring:SetVertexColor(0.88, 0.66, 0.3)
    ring:SetAllPoints()
    local x = Text(close, 15, { 1, 0.91, 0.64 }, BODY_FONT)
    x:SetPoint("CENTER", 0, 0)
    x:SetText("X")
    local hover = close:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(ART .. "Glow")
    hover:SetBlendMode("ADD")
    hover:SetAllPoints()
end
close:SetScript("OnClick", function() frame:Hide() end)

-- The currencies, top left: Emberspark (the account's) and Eyes of Unlearning (the Hero's), with what each is for.
local function Pill(x, icon, iconSize)
    local pill = CreateFrame("Frame", nil, frame)
    pill:SetPoint("TOPLEFT", x, -18)
    pill:SetHeight(30)
    pill:SetWidth(90)
    pill:SetFrameLevel(border:GetFrameLevel() + 1)
    Slice9(pill, "Pill", 9)
    pill.icon = pill:CreateTexture(nil, "ARTWORK")
    pill.icon:SetTexture(icon)
    pill.icon:SetWidth(iconSize)
    pill.icon:SetHeight(iconSize)
    pill.icon:SetPoint("LEFT", 6, 0)
    pill.value = Text(pill, 14, GOLD_HI, BODY_FONT)
    pill.value:SetPoint("LEFT", pill.icon, "RIGHT", 6, 0)
    function pill:SetValue(value)
        self.value:SetText(Thousands(value))
        self:SetWidth(math.max(70, (self.value:GetStringWidth() or 30) + iconSize + 26))
    end
    return pill
end
local sparkPill = Pill(42, SPARK_ICON, 20)
Tooltip(sparkPill, "Emberspark", "Your account's currency, shared by every character on it. It buys card packs, "
    .. "cards, Eyes of Unlearning and new specializations.", "Earned by prestiging, and from duplicate cards.")
local eyePill = Pill(42, ART .. "Eye", 26)
eyePill:SetPoint("TOPLEFT", sparkPill, "TOPRIGHT", 8, 0)
Tooltip(eyePill, "Eyes of Unlearning", "Spend one to reroll an ability or talent your Hero has, or a card you just "
    .. "opened before it joins your collection.", "One for each level from 8 to 60, and four at levels 8, 10, 20, 30, "
    .. "40, 50 and 60. Then buy more without limit at 60.")

-- Effects: particles, rings and tweens, drawn over the window ------------------------------------------------------

local fx = CreateFrame("Frame", nil, frame)
fx:SetAllPoints()
fx:SetFrameLevel(border:GetFrameLevel() + 5)
local particles, pool, tweens = {}, {}, {}

local function Tween(duration, step, done)
    tweens[#tweens + 1] = { t = 0, duration = duration, step = step, done = done }
end

-- Where a region's center is, in the window's coordinates (from its bottom left).
local function Where(region)
    local x, y = region:GetCenter()
    return (x or 0) - (frame:GetLeft() or 0), (y or 0) - (frame:GetBottom() or 0)
end

local function Emit(p)
    local texture = table.remove(pool) or fx:CreateTexture(nil, "OVERLAY")
    texture:SetTexture(p.texture or SPARK)
    texture:SetBlendMode("ADD")
    texture:SetVertexColor(p.r or 1, p.g or 0.8, p.b or 0.5)
    texture:Show()
    p.tex, p.age, p.life, p.size = texture, 0, p.life or 0.9, p.size or 10
    particles[#particles + 1] = p
end

-- n sparks from x, y, outward at a speed of [min, max] (a cone: upward within spread radians of straight up).
local function Burst(x, y, n, rgb, minSpeed, maxSpeed, spread)
    for _ = 1, n do
        local angle = spread and (math.pi / 2 + (math.random() * 2 - 1) * spread) or math.random() * math.pi * 2
        local speed = minSpeed + math.random() * (maxSpeed - minSpeed)
        Emit({ x = x, y = y, vx = math.cos(angle) * speed, vy = math.sin(angle) * speed, r = rgb[1], g = rgb[2],
            b = rgb[3], size = 6 + math.random() * 10, life = 0.6 + math.random() * 0.6, drag = 0.9, grav = -60 })
    end
end

local function Ring(x, y, size, rgb)
    Emit({ x = x, y = y, vx = 0, vy = 0, r = rgb[1], g = rgb[2], b = rgb[3], texture = ART .. "Glow", size = 10,
        grow = size, life = 0.6 })
end

-- n sparks from one region homing into another (Eyes into their counter).
local function Fly(from, to, n, rgb, done)
    local x, y = Where(from)
    local tx, ty = Where(to)
    for index = 1, n do
        Emit({ x = x + math.random(-8, 8), y = y + math.random(-8, 8), vx = math.random(-220, 220),
            vy = math.random(60, 300), tx = tx, ty = ty, delay = index * 0.02, r = rgb[1], g = rgb[2], b = rgb[3],
            size = 8 + math.random() * 6, life = 1.1 })
    end
    Tween(0.75, function() end, function()
        Ring(tx, ty, 70, rgb)
        Burst(tx, ty, 18, rgb, 40, 160)
        if done then
            done()
        end
    end)
end

fx:SetScript("OnUpdate", function(_, delta)
    delta = math.min(delta, 0.05)
    for index = #tweens, 1, -1 do
        local tween = tweens[index]
        tween.t = tween.t + delta
        local p = math.min(1, tween.t / tween.duration)
        tween.step(p)
        if p >= 1 then
            table.remove(tweens, index)
            if tween.done then
                tween.done()
            end
        end
    end
    for index = #particles, 1, -1 do
        local p = particles[index]
        if p.delay and p.delay > 0 then
            p.delay = p.delay - delta
            p.tex:Hide()
        else
            p.tex:Show()
            p.age = p.age + delta
            local k = p.age / p.life
            if p.tx then
                -- Homing: steer toward the target, faster as it ages.
                local pull = math.min(1, k * k * 3)
                p.vx = p.vx + ((p.tx - p.x) * 18 - p.vx) * pull * delta * 4
                p.vy = p.vy + ((p.ty - p.y) * 18 - p.vy) * pull * delta * 4
            else
                p.vx, p.vy = p.vx * (p.drag or 1), p.vy * (p.drag or 1) + (p.grav or 0) * delta
            end
            p.x, p.y = p.x + p.vx * delta, p.y + p.vy * delta
            local size = p.grow and (p.size + p.grow * k) or p.size * (1 - k * 0.5)
            p.tex:SetWidth(size)
            p.tex:SetHeight(size)
            p.tex:ClearAllPoints()
            p.tex:SetPoint("CENTER", frame, "BOTTOMLEFT", p.x, p.y)
            p.tex:SetAlpha(math.max(0, 1 - k))
            if k >= 1 then
                p.tex:Hide()
                pool[#pool + 1] = p.tex
                table.remove(particles, index)
            end
        end
    end
end)

-- Tabs and pages -----------------------------------------------------------------------------------------------------

local pages, tabs = {}, {}
local ShowTab
for index, label in ipairs({ "Specializations", "Packs", "Cards" }) do
    local tab = Button(frame, 135, 27, label, function() ShowTab(index) end, "Button", 13)
    tab.label:SetFont(DISPLAY_FONT, 14)
    tab:SetPoint("TOPLEFT", 22 + (index - 1) * 143, -93)
    tabs[index] = tab
    local page = CreateFrame("Frame", nil, frame)
    page:SetPoint("TOPLEFT", 22, -129)
    page:SetPoint("BOTTOMRIGHT", -22, 25)
    page:Hide()
    pages[index] = page
end
local PAGE_WIDTH, PAGE_HEIGHT = WIDTH - 44, HEIGHT - 154
local Refresh

-- Specializations ----------------------------------------------------------------------------------------------------

local specPage = pages[1]
local specList = Panel(specPage, "Panel", 0, 0, 225, PAGE_HEIGHT)
Heading(specList, "SPECIALIZATIONS", 12, 12)
local specItems = {}
for index, spec in ipairs(SPECS) do
    local item = CreateFrame("Button", nil, specList)
    item:SetWidth(201)
    item:SetHeight(46)
    item:SetPoint("TOPLEFT", 12, -38 - (index - 1) * 54)
    item.numeral = Text(item, 22, GOLD_HI, DISPLAY_FONT)
    item.numeral:SetPoint("CENTER", item, "LEFT", 24, 0)
    item.numeral:SetText(ROMAN[index])
    item.name = Text(item, 12, GOLD_HI, DISPLAY_FONT)
    item.name:SetPoint("TOPLEFT", 52, -9)
    item.name:SetText(spec.name)
    item.sub = Text(item, 10, MUTED, BODY_FONT)
    item.sub:SetPoint("TOPLEFT", item.name, "BOTTOMLEFT", 0, -3)
    local hover = item:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(ART .. "Glow")
    hover:SetBlendMode("ADD")
    hover:SetVertexColor(1, 0.6, 0.25, 0.25)
    hover:SetAllPoints()
    item:SetScript("OnClick", function()
        view.spec = index
        Refresh()
    end)
    specItems[index] = item
end
local specHint = Text(specList, 10, MUTED, BODY_FONT)
specHint:SetPoint("BOTTOMLEFT", 14, 14)
specHint:SetWidth(196)
specHint:SetJustifyH("LEFT")
specHint:SetText("Each specialization keeps its own abilities, talents and slotted cards.")

local specView = Panel(specPage, "PanelGold", 237, 0, PAGE_WIDTH - 237, PAGE_HEIGHT - 181)
local svNumeral = Text(specView, 48, GOLD_HI, DISPLAY_FONT)
svNumeral:SetPoint("TOPLEFT", 26, -16)
local svName = Text(specView, 21, GOLD_HI, DISPLAY_FONT)
svName:SetPoint("TOPLEFT", 110, -20)
local svTag = Text(specView, 10, GREEN, BODY_FONT)
svTag:SetPoint("TOPLEFT", svName, "BOTTOMLEFT", 0, -5)
local svText = Text(specView, 12, { 0.9, 0.83, 0.69 }, BODY_FONT)
svText:SetPoint("TOPLEFT", 26, -78)
svText:SetWidth(560)
svText:SetJustifyH("LEFT")
local svStats = {}
for index = 1, 4 do
    local stat = Panel(specView, "Panel", 26 + (index - 1) * 158, 112, 148, 54)
    Tint(stat, 0.8, 0.8, 0.8)
    stat.value = Text(stat, 21, GOLD_HI, DISPLAY_FONT)
    stat.value:SetPoint("TOPLEFT", 12, -8)
    stat.label = Text(stat, 10, MUTED, BODY_FONT)
    stat.label:SetPoint("TOPLEFT", stat.value, "BOTTOMLEFT", 0, -3)
    svStats[index] = stat
end
local slotLabel = Text(specView, 10, MUTED, BODY_FONT)
slotLabel:SetPoint("TOPLEFT", 26, -184)
slotLabel:SetText("CARD SLOTS  -  LOADOUTS ARE COMING NEXT")
for index = 1, 8 do
    local slot = specView:CreateTexture(nil, "ARTWORK")
    slot:SetTexture(ART .. "Slot")
    slot:SetWidth(35)
    slot:SetHeight(48)
    slot:SetPoint("TOPLEFT", 26 + (index - 1) * 44, -200)
end
local svButton = Button(specView, 165, 28, "Edit loadout (soon)", function() end, "Button")
svButton:SetPoint("BOTTOMRIGHT", -26, 26)

-- The leveling track: a gold-framed rail from the first level to the last, filled to the Hero's level; the levels
-- not claimed yet glow, the milestones are diamonds, and past the last level infinity: the Eyes for sale.
local track = Panel(specPage, "Panel", 237, PAGE_HEIGHT - 169, PAGE_WIDTH - 237, 99)
Heading(track, "LEVELING REWARDS", 14, 12)
local trackSum = Text(track, 10, MUTED, BODY_FONT)
trackSum:SetPoint("TOPLEFT", 190, -16)
local claimButton = Button(track, 142, 24, "Claim", function(self)
    if eyes.ready and eyes.ready > 0 then
        Fly(self, eyePill.icon, 24, GREEN)
        Send("CLAIM")
    end
end, "ButtonHot")
claimButton:SetPoint("TOPRIGHT", -14, -9)
local RAIL_WIDTH = PAGE_WIDTH - 237 - 28 - 70
local rail = CreateFrame("Frame", nil, track)
rail:SetPoint("TOPLEFT", 14, -56)
rail:SetWidth(RAIL_WIDTH)
rail:SetHeight(20)
Slice9(rail, "Solid", 8)
Tint(rail, 0.85, 0.63, 0.29)
local groove = rail:CreateTexture(nil, "BORDER")
groove:SetTexture(0.03, 0.02, 0.01, 1)
groove:SetPoint("TOPLEFT", 3, -3)
groove:SetPoint("BOTTOMRIGHT", -3, 3)
local GROOVE = RAIL_WIDTH - 6
local fill = rail:CreateTexture(nil, "ARTWORK")
fill:SetTexture(1, 1, 1, 1)
fill:SetGradient("VERTICAL", 0.55, 0.12, 0.03, 1, 0.62, 0.25)
fill:SetPoint("TOPLEFT", 3, -3)
fill:SetHeight(14)
local shimmer = rail:CreateTexture(nil, "OVERLAY")
shimmer:SetTexture(ART .. "Glow")
shimmer:SetBlendMode("ADD")
shimmer:SetVertexColor(1, 0.95, 0.8, 0.8)
shimmer:SetWidth(60)
shimmer:SetHeight(14)
local pend = rail:CreateTexture(nil, "OVERLAY")
pend:SetTexture(1, 1, 1, 1)
pend:SetBlendMode("ADD")
pend:SetHeight(14)
local gems, ticks = {}, {}
for index = 1, 7 do
    local gem = rail:CreateTexture(nil, "OVERLAY")
    gem:SetTexture(ART .. "Diamond")
    gem:SetWidth(16)
    gem:SetHeight(16)
    gems[index] = gem
    local tick = Text(rail, 9, MUTED, BODY_FONT)
    ticks[index] = tick
end
local head = CreateFrame("Frame", nil, rail)
head:SetWidth(26)
head:SetHeight(26)
head:SetFrameLevel(rail:GetFrameLevel() + 3)
do
    local orb = head:CreateTexture(nil, "ARTWORK")
    orb:SetTexture(ART .. "Orb")
    orb:SetAllPoints()
    local halo = head:CreateTexture(nil, "BACKGROUND")
    halo:SetTexture(ART .. "Glow")
    halo:SetBlendMode("ADD")
    halo:SetVertexColor(1, 0.55, 0.2)
    halo:SetPoint("TOPLEFT", -12, 12)
    halo:SetPoint("BOTTOMRIGHT", 12, -12)
end
head.level = Text(head, 10, { 0.23, 0.07, 0.02 }, BODY_FONT)
head.level:SetShadowOffset(0, 0)
head.level:SetPoint("CENTER", 0, 0)
local chip = CreateFrame("Button", nil, rail)
chip:SetWidth(58)
chip:SetHeight(20)
chip:SetFrameLevel(rail:GetFrameLevel() + 4)
Slice9(chip, "Solid", 8)
Tint(chip, 0.04, 0.16, 0.04)
do
    local glow = chip:CreateTexture(nil, "BACKGROUND")
    glow:SetTexture(ART .. "Glow")
    glow:SetBlendMode("ADD")
    glow:SetVertexColor(0.4, 1, 0.45, 0.7)
    glow:SetPoint("TOPLEFT", -10, 8)
    glow:SetPoint("BOTTOMRIGHT", 10, -8)
    local icon = chip:CreateTexture(nil, "ARTWORK")
    icon:SetTexture(ART .. "Eye")
    icon:SetWidth(18)
    icon:SetHeight(18)
    icon:SetPoint("LEFT", 4, 0)
    chip.count = Text(chip, 11, { 0.78, 1, 0.75 }, BODY_FONT)
    chip.count:SetPoint("LEFT", icon, "RIGHT", 3, 0)
end
chip:SetScript("OnClick", function() claimButton:Click() end)
local infinity = CreateFrame("Frame", nil, track)
infinity:SetWidth(46)
infinity:SetHeight(46)
infinity:SetPoint("LEFT", rail, "RIGHT", 12, 0)
infinity.ring = infinity:CreateTexture(nil, "ARTWORK")
infinity.ring:SetTexture(ART .. "Ring")
infinity.ring:SetAllPoints()
infinity.mark = infinity:CreateTexture(nil, "OVERLAY")
infinity.mark:SetTexture(ART .. "Infinity")
infinity.mark:SetWidth(30)
infinity.mark:SetHeight(15)
infinity.mark:SetPoint("CENTER", 0, 0)
infinity.glow = infinity:CreateTexture(nil, "BACKGROUND")
infinity.glow:SetTexture(ART .. "Glow")
infinity.glow:SetBlendMode("ADD")
infinity.glow:SetVertexColor(1, 0.55, 0.2)
infinity.glow:SetPoint("TOPLEFT", -16, 16)
infinity.glow:SetPoint("BOTTOMRIGHT", 16, -16)
Tooltip(infinity, "Eyes without limit", "At level 60 the leveling Eyes are all yours, and the shop below sells more.")

-- Where a level sits on the groove: each level from the first is a segment, its end at (level - first + 1) / count.
local function At(level)
    local count = eyes.last - eyes.first + 1
    local done = math.max(0, math.min(level, eyes.last) - eyes.first + 1)
    return GROOVE * done / count
end

local shop = Panel(specPage, "Panel", 237, PAGE_HEIGHT - 60, PAGE_WIDTH - 237, 60)
local shopEye = shop:CreateTexture(nil, "ARTWORK")
shopEye:SetTexture(ART .. "Eye")
shopEye:SetWidth(44)
shopEye:SetHeight(44)
shopEye:SetPoint("LEFT", 14, 0)
local shopTitle = Text(shop, 14, GOLD_HI, DISPLAY_FONT)
shopTitle:SetPoint("TOPLEFT", 70, -12)
shopTitle:SetText("Eyes of Unlearning")
local shopNote = Text(shop, 10, { 0.85, 0.64, 0.42 }, BODY_FONT)
shopNote:SetPoint("TOPLEFT", shopTitle, "BOTTOMLEFT", 0, -4)
shopNote:SetWidth(380)
shopNote:SetJustifyH("LEFT")
local buyEyes = Button(shop, 225, 32, "", function(self)
    Fly(self, eyePill.icon, 18, GREEN)
    Send("EYES")
end, "ButtonHot", 13)
buyEyes:SetPoint("RIGHT", -14, 0)

local function RefreshSpecs()
    for index, item in ipairs(specItems) do
        local chosen, active = index == view.spec, index == 1
        if item.chosen ~= chosen or not item.slices then
            item.chosen = chosen
            Slice9(item, chosen and "ButtonRed" or "Button", 10)
        end
        item.sub:SetText(active and "ACTIVE" or "Coming soon")
        item.sub:SetTextColor(unpack(active and GREEN or MUTED))
        local dim = active and 1 or 0.55
        item.numeral:SetTextColor(GOLD_HI[1] * dim, GOLD_HI[2] * dim, GOLD_HI[3] * dim)
        item.name:SetTextColor(GOLD_HI[1] * dim, GOLD_HI[2] * dim, GOLD_HI[3] * dim)
    end
    local spec, owned = SPECS[view.spec], view.spec == 1
    svNumeral:SetText(ROMAN[view.spec])
    svName:SetText(spec.name)
    svTag:SetText(owned and "ACTIVE" or "LOCKED")
    svTag:SetTextColor(unpack(owned and GREEN or { 0.85, 0.64, 0.42 }))
    svText:SetText(spec.text)
    local stats = owned and { { Thousands(eyes.bought), "Eyes of Unlearning bought" },
        { Thousands(eyes.abilities), "Abilities rerolled" }, { Thousands(eyes.talents), "Talents rerolled" },
        { "0", "Cards slotted" } }
        or { { "-", "Eyes of Unlearning bought" }, { "-", "Abilities rerolled" }, { "-", "Talents rerolled" },
        { "-", "Cards slotted" } }
    for index, stat in ipairs(svStats) do
        stat.value:SetText(stats[index][1])
        stat.label:SetText(stats[index][2])
    end
    svButton:SetLabel(owned and "Edit loadout (soon)" or "Coming soon")
    svButton:SetEnabled(false)

    -- The track.
    local ready = 0
    for level = eyes.claimed + 1, math.min(eyes.level, eyes.last) do
        ready = ready + LevelEyes(level)
    end
    eyes.ready = ready
    local reached, claimedTo = At(eyes.level), At(eyes.claimed)
    fill:SetWidth(math.max(0.01, reached))
    Shown(fill, reached > 0)
    pend:ClearAllPoints()
    pend:SetPoint("TOPLEFT", rail, "TOPLEFT", 3 + claimedTo, -3)
    pend:SetWidth(math.max(0.01, reached - claimedTo))
    Shown(pend, reached - claimedTo > 0.5)
    for index, level in ipairs(eyes.milestones) do
        local gem, tick = gems[index], ticks[index]
        if gem then
            local x = 3 + At(level)
            gem:ClearAllPoints()
            gem:SetPoint("CENTER", rail, "LEFT", x, 0)
            if level <= eyes.claimed then
                gem:SetVertexColor(1, 0.72, 0.3)
            elseif level <= eyes.level then
                gem:SetVertexColor(0.45, 1, 0.42)
            else
                gem:SetVertexColor(0.35, 0.26, 0.16)
            end
            tick:ClearAllPoints()
            tick:SetPoint("TOP", rail, "BOTTOMLEFT", x, -4)
            tick:SetText(level)
            tick:SetTextColor(unpack(level <= eyes.level and GOLD_HI or MUTED))
        end
    end
    head:ClearAllPoints()
    head:SetPoint("CENTER", rail, "LEFT", 3 + reached, 0)
    head.level:SetText(eyes.level)
    Shown(chip, ready > 0)
    if ready > 0 then
        chip:ClearAllPoints()
        chip:SetPoint("BOTTOM", rail, "TOPLEFT", 3 + (claimedTo + reached) / 2, 4)
        chip.count:SetText("+" .. ready)
    end
    local open = eyes.level >= eyes.last
    infinity.ring:SetVertexColor(unpack(open and { 1, 0.85, 0.5 } or { 0.4, 0.3, 0.2 }))
    infinity.mark:SetVertexColor(unpack(open and { 1, 0.91, 0.64 } or { 0.35, 0.29, 0.22 }))
    Shown(infinity.glow, open)
    local total = 0
    for level = eyes.first, eyes.last do
        total = total + LevelEyes(level)
    end
    local claimedEyes = 0
    for level = eyes.first, math.min(eyes.claimed, eyes.last) do
        claimedEyes = claimedEyes + LevelEyes(level)
    end
    trackSum:SetText(string.format("Level %d  -  %d of %d Eyes claimed  -  milestones give %d", eyes.level,
        claimedEyes, total, eyes.milestoneEyes) .. (open and "  -  the shop is open without limit" or ""))
    claimButton:SetEnabled(ready > 0)
    claimButton:SetLabel(ready > 0 and ("Claim " .. ready .. (ready == 1 and " Eye" or " Eyes")) or "Nothing to claim")

    -- The shop.
    buyEyes:SetLabel(string.format("Buy %d Eyes  -  %s Emberspark", eyes.perBuy, Thousands(eyes.price)))
    buyEyes:SetEnabled(eyes.canBuy and state.spark >= eyes.price)
    shopNote:SetText(not eyes.canBuy and string.format("Opens at level %d. Until then, claim them on the track "
        .. "above: one each level from %d, %d at each milestone.", eyes.last, eyes.first, eyes.milestoneEyes)
        or state.spark < eyes.price and ("You need " .. Thousands(eyes.price - state.spark) .. " more Emberspark.")
        or "Buy as many as you like. The price climbs with each purchase and starts over when you prestige.")
end

-- The breathing light over the unclaimed levels, and the shimmer running along the fill.
local trackClock = 0
track:SetScript("OnUpdate", function(_, delta)
    trackClock = trackClock + delta
    pend:SetGradientAlpha("HORIZONTAL", 1, 0.95, 0.75, 0, 1, 0.95, 0.75, 0.25 + 0.2 * math.sin(trackClock * 3.5))
    local width = fill:GetWidth() or 0
    if width > 30 then
        local x = (trackClock * 160) % (width + 60) - 30
        shimmer:ClearAllPoints()
        shimmer:SetPoint("CENTER", rail, "LEFT", 3 + math.max(10, math.min(width - 10, x)), 0)
        shimmer:SetAlpha(math.max(0, 1 - math.abs(x - width / 2) / (width / 2 + 30)) * 0.8)
        shimmer:Show()
    else
        shimmer:Hide()
    end
    if eyes.level >= eyes.last then
        infinity.glow:SetAlpha(0.45 + 0.3 * math.sin(trackClock * 3))
    end
end)

-- Packs: the duplicate tallies, the display where packs open, and the shelves ---------------------------------------

local packPage = pages[2]
local function Meter(x, label, sub, pips)
    local meter = Panel(packPage, "Panel", x, 0, (PAGE_WIDTH - 14) / 2, 52)
    meter.label = Text(meter, 12, GOLD_HI, DISPLAY_FONT)
    meter.label:SetPoint("TOPLEFT", 12, -7)
    meter.label:SetText(label)
    meter.sub = Text(meter, 9, MUTED, BODY_FONT)
    meter.sub:SetPoint("TOPLEFT", meter.label, "BOTTOMLEFT", 0, -2)
    meter.sub:SetText(sub)
    meter.count = Text(meter, 12, GOLD_HI, BODY_FONT)
    meter.count:SetPoint("TOPRIGHT", -12, -8)
    meter.pips = {}
    local width = ((PAGE_WIDTH - 14) / 2 - 24 - (pips - 1) * 3) / pips
    for index = 1, pips do
        local pip = meter:CreateTexture(nil, "ARTWORK")
        pip:SetTexture(1, 1, 1, 1)
        pip:SetWidth(width)
        pip:SetHeight(pips > 20 and 5 or 6)
        pip:SetPoint("BOTTOMLEFT", 12 + (index - 1) * (width + 3), 7)
        meter.pips[index] = pip
    end
    return meter
end
local dupMeter = Meter(0, "Duplicate Tally", "Every 10 duplicates: a free pack", 10)
local goldMeter = Meter((PAGE_WIDTH + 14) / 2, "Golden Tally",
    "Every 50 duplicates: a Golden Pack and 2,000 Emberspark", 50)

local display = Panel(packPage, "Panel", 0, 59, PAGE_WIDTH, 260)
Tint(display, 0.85, 0.85, 0.85, 0.95)
local idle = Text(display, 15, { 0.8, 0.65, 0.45 }, DISPLAY_FONT)
idle:SetPoint("CENTER", 0, 0)
idle:SetText("Choose a pack below to open it")

local packWidgets = {}
local function Shelf(x, file, label, packs)
    local shelf = Panel(packPage, file, x, PAGE_HEIGHT - 144, (PAGE_WIDTH - 14) / 2, 144)
    Heading(shelf, label, 12, 8)
    for slot, pack in ipairs(packs) do
        local info = PACKS[pack]
        local widget = CreateFrame("Frame", nil, shelf)
        widget:SetPoint("TOPLEFT", 12 + (slot - 1) * 222, -30)
        widget:SetWidth(214)
        widget:SetHeight(108)
        widget.art = CreateFrame("Button", nil, widget)
        widget.art:SetWidth(68)
        widget.art:SetHeight(102)
        widget.art:SetPoint("TOPLEFT", 0, 0)
        local art = widget.art:CreateTexture(nil, "ARTWORK")
        art:SetTexture(ART .. info.art)
        art:SetAllPoints()
        widget.artTexture = art
        local hover = widget.art:CreateTexture(nil, "HIGHLIGHT")
        hover:SetTexture(ART .. "Glow")
        hover:SetBlendMode("ADD")
        hover:SetVertexColor(1, 0.6, 0.25, 0.5)
        hover:SetPoint("TOPLEFT", -10, 10)
        hover:SetPoint("BOTTOMRIGHT", 10, -10)
        widget.name = Text(widget, 12, GOLD_HI, DISPLAY_FONT)
        widget.name:SetPoint("TOPLEFT", 76, -4)
        widget.name:SetWidth(138)
        widget.name:SetJustifyH("LEFT")
        widget.name:SetText(info.name)
        widget.owned = Text(widget, 10, MUTED, BODY_FONT)
        widget.owned:SetPoint("TOPLEFT", 76, -22)
        widget.open = Button(widget, 64, 22, "Open", function() Send("OPEN," .. pack .. ",1") end, "ButtonHot")
        widget.open:SetPoint("TOPLEFT", 76, -38)
        widget.all = Button(widget, 70, 22, "Open all", function()
            Send("OPEN," .. pack .. "," .. (GetItemCount(info.item) or 0))
        end, "ButtonRed")
        widget.all:SetPoint("LEFT", widget.open, "RIGHT", 4, 0)
        widget.buy = Button(widget, 138, 22, "", function() Send("BUY," .. pack .. ",0") end, "Button", 11)
        widget.buy:SetPoint("TOPLEFT", 76, -64)
        if pack == 0 then
            widget.gold = Button(widget, 138, 16, "", function() Send("BUY,0,1") end, "Button", 9)
            widget.gold:SetPoint("TOPLEFT", 76, -88)
        end
        widget.art:SetScript("OnClick", function() widget.open:Click() end)
        packWidgets[pack] = widget
    end
    return shelf
end
Shelf(0, "Panel", "CARD PACKS", { 0, 1 })
Shelf((PAGE_WIDTH + 14) / 2, "PanelGold", "GOLDEN PACKS", { 2, 3 })

-- Card faces: the frame art, the icon in its circle under the frame's ring (build_darkmoon_art.py cuts it from the
-- frame: see-through inside the circle, solid outside, so the square icon's corners never show), the name on the
-- banner and the rarity on the parchment. What is drawn on the frame is a child of its own (content), hidden while a
-- card turns over.
local RING_SPAN = 2.4      -- the ring over the icon: this many radii across (build_darkmoon_art.py)
local ICON_CROP = 0.07     -- the game's icons have a bevelled border: cut it off each edge
local function CardFace(parent, width, height)
    local card = CreateFrame("Frame", nil, parent)
    card:SetWidth(width)
    card:SetHeight(height)
    card.w, card.h = width, height
    card.glow = card:CreateTexture(nil, "BACKGROUND")
    card.glow:SetTexture(ART .. "Glow")
    card.glow:SetBlendMode("ADD")
    card.glow:SetPoint("TOPLEFT", -width * 0.35, width * 0.35)
    card.glow:SetPoint("BOTTOMRIGHT", width * 0.35, -width * 0.35)
    card.glow:Hide()
    card.frameArt = card:CreateTexture(nil, "ARTWORK")
    card.frameArt:SetAllPoints()
    card.back = card:CreateTexture(nil, "OVERLAY")
    card.back:SetAllPoints()
    card.back:Hide()
    local content = CreateFrame("Frame", nil, card)
    content:SetWidth(width)
    content:SetHeight(height)
    content:SetPoint("CENTER", 0, 0)
    card.content = content
    -- The icon (square, its border cut off) and over it the frame's ring, solid outside the circle.
    card.icon = content:CreateTexture(nil, "BACKGROUND")
    card.icon:SetTexCoord(ICON_CROP, 1 - ICON_CROP, ICON_CROP, 1 - ICON_CROP)
    card.ring = content:CreateTexture(nil, "BORDER")
    -- The text, as large as the card allows: the name outlined on its dark banner, the rest in dark ink on the
    -- parchment, lifted by a faint light edge.
    local scale = width / 190
    card.name = Text(content, math.max(9, math.floor(16 * scale)), NAME_RGB[0], BODY_FONT, "OUTLINE")
    card.name:SetWidth(width * 0.86)
    card.name:SetShadowOffset(0, 0)
    local function Ink(size, color)
        local text = Text(content, size, color, BODY_FONT)
        text:SetShadowColor(1, 0.93, 0.78, 0.45)
        text:SetShadowOffset(1, -1)
        return text
    end
    card.rarity = Ink(math.max(8, math.floor(15 * scale)), INK_RGB[0])
    card.kind = Ink(math.max(8, math.floor(13 * scale)), { 0.1, 0.06, 0.03 })
    card.note = Ink(math.max(7, math.floor(11 * scale)), { 0.16, 0.1, 0.05 })
    card.gems = {}
    for index = 1, 4 do
        local gem = content:CreateTexture(nil, "OVERLAY")
        gem:SetTexture(ART .. "Diamond")
        gem:SetWidth(math.max(5, 10 * scale))
        gem:SetHeight(math.max(5, 10 * scale))
        card.gems[index] = gem
    end
    card.lock = content:CreateTexture(nil, "OVERLAY")
    card.lock:SetTexture(LOCK)
    card.lock:SetWidth(width * 0.28)
    card.lock:SetHeight(width * 0.28)
    card.lock:Hide()

    function card:SetCard(entry, golden, locked)
        local layout = LAYOUT[entry.kind] or LAYOUT[0]
        local w, h = self.w, self.h
        self.layout = layout
        self.back:Hide()
        local frameFile = (entry.kind == 1 and "CardTalent" or "CardSkill") .. (golden and "Golden" or "")
        self.frameArt:SetTexture(ART .. frameFile)
        local cx, cy, size = w * layout.x, -h * layout.y, w * layout.r * 2
        self.icon:ClearAllPoints()
        self.icon:SetWidth(size)
        self.icon:SetHeight(size)
        self.icon:SetPoint("CENTER", self.content, "TOPLEFT", cx, cy)
        self.icon:SetTexture(CardIcon(entry))
        self.ring:SetTexture(ART .. frameFile .. "Ring")
        self.ring:ClearAllPoints()
        self.ring:SetWidth(size / 2 * RING_SPAN)
        self.ring:SetHeight(size / 2 * RING_SPAN)
        self.ring:SetPoint("CENTER", self.content, "TOPLEFT", cx, cy)
        self.name:ClearAllPoints()
        self.name:SetPoint("CENTER", self.content, "TOPLEFT", w / 2, -h * layout.banner)
        self.name:SetText(CardName(entry))
        self.name:SetTextColor(unpack(golden and GOLDEN_RGB or NAME_RGB[entry.rarity]))
        self.rarity:ClearAllPoints()
        self.rarity:SetPoint("CENTER", self.content, "TOPLEFT", w / 2, -h * layout.rarity)
        self.rarity:SetText(string.upper((golden and "Golden " or "") .. RARITY_NAMES[entry.rarity]))
        self.rarity:SetTextColor(unpack(golden and GOLDEN_INK or INK_RGB[entry.rarity]))
        self.kind:ClearAllPoints()
        self.kind:SetPoint("CENTER", self.content, "TOPLEFT", w / 2, -h * layout.kind)
        self.kind:SetText(entry.kind == 1 and "Talent Card" or "Skill Card")
        self.note:ClearAllPoints()
        self.note:SetPoint("CENTER", self.content, "TOPLEFT", w / 2, -h * layout.note)
        self.note:SetText(golden and "One more, past your slots." or "Yours forever.")
        for index, gem in ipairs(self.gems) do
            Shown(gem, index <= entry.rarity + 1)
            gem:ClearAllPoints()
            gem:SetPoint("CENTER", self.content, "TOPLEFT", w / 2 + (index - 1 - entry.rarity / 2) * w * 0.07,
                -h * layout.gems)
            gem:SetVertexColor(unpack(golden and { 0.78, 0.54, 0.07 } or RARITY_RGB[entry.rarity]))
        end
        local glow = golden and GOLDEN_RGB or RARITY_RGB[entry.rarity]
        self.glow:SetVertexColor(glow[1], glow[2], glow[3], 0.7)
        self.lock:ClearAllPoints()
        self.lock:SetPoint("CENTER", self.content, "TOPLEFT", w / 2, -h * layout.y)
        Shown(self.lock, locked)
        -- Not owned: greyed and darker, the text still readable.
        local shade = locked and 0.62 or 1
        for _, texture in ipairs({ self.frameArt, self.icon, self.ring }) do
            texture:SetVertexColor(shade, shade, shade)
            texture:SetDesaturated(locked and true or false)
        end
        for _, text in ipairs({ self.name, self.rarity, self.kind, self.note }) do
            text:SetAlpha(locked and 0.9 or 1)
        end
        self.content:Show()
    end

    -- While it turns: the frame art alone, squeezed; afterwards what is drawn on it again.
    function card:SetContent(on)
        Shown(self.content, on)
    end

    function card:SetBack(kind, golden)
        self.back:SetTexture(ART .. (kind == 1 and "BackTalent" or "BackSkill") .. (golden and "Golden" or ""))
        self.back:Show()
        self.frameArt:SetTexture(nil)
        self.layout = nil
        self.content:Hide()
        self.glow:Hide()
    end
    return card
end

-- The reveal: the pack rips open along its top, five cards deal out face down and turn over one by one; a click
-- (or Done) dissolves them away. "Open all" instead bursts the packs at once and lists the new cards.
local stage = CreateFrame("Frame", nil, display)
stage:SetAllPoints()
stage:SetFrameLevel(display:GetFrameLevel() + 2)
stage:EnableMouse(true)
local CARD_W, CARD_H = 128, 192
local CARD_Y = -124        -- the cards' centers, from the display's top
-- Each card sits in a clip of its own (a ScrollFrame, which clips what it holds): burning, the clip shrinks from the
-- bottom up, so the card is eaten away behind its glowing edge. PAD: room in the clip for the card's glow.
local PAD = 40
local SLOT_W, SLOT_H = CARD_W + PAD * 2, CARD_H + PAD * 2
local burnLayer = CreateFrame("Frame", nil, stage)
burnLayer:SetAllPoints()
burnLayer:SetFrameLevel(stage:GetFrameLevel() + 12)
local revealCards = {}
for index = 1, 5 do
    local slot = CreateFrame("ScrollFrame", nil, stage)
    slot:SetWidth(SLOT_W)
    slot:SetHeight(SLOT_H)
    local holder = CreateFrame("Frame", nil, slot)
    holder:SetWidth(SLOT_W)
    holder:SetHeight(SLOT_H)
    slot:SetScrollChild(holder)
    local card = CardFace(holder, CARD_W, CARD_H)
    card:SetPoint("CENTER", 0, 0)
    card.slot = slot
    card:Hide()
    -- The burning edge: a bright streak over a wider ember glow, riding the clip's bottom.
    card.edgeGlow = burnLayer:CreateTexture(nil, "ARTWORK")
    card.edgeGlow:SetTexture(ART .. "Glow")
    card.edgeGlow:SetBlendMode("ADD")
    card.edgeGlow:SetVertexColor(1, 0.35, 0.08)
    card.edgeGlow:SetWidth(CARD_W + 40)
    card.edgeGlow:SetHeight(46)
    card.edgeGlow:SetPoint("CENTER", slot, "BOTTOM", 0, 0)
    card.edge = burnLayer:CreateTexture(nil, "OVERLAY")
    card.edge:SetTexture(ART .. "Slash")
    card.edge:SetBlendMode("ADD")
    card.edge:SetVertexColor(1, 0.78, 0.35)
    card.edge:SetWidth(CARD_W + 14)
    card.edge:SetHeight(20)
    card.edge:SetPoint("CENTER", slot, "BOTTOM", 0, 0)
    card.edge:Hide()
    card.edgeGlow:Hide()
    card.result = Text(card, 10, MUTED, BODY_FONT)
    card.result:SetPoint("TOP", card, "BOTTOM", 0, -3)
    card:EnableMouse(true)
    card:SetScript("OnEnter", function(self)
        if self.entry and self.turned then
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetHyperlink("spell:" .. self.entry.spell)
            GameTooltip:Show()
        end
    end)
    card:SetScript("OnLeave", function() GameTooltip:Hide() end)
    revealCards[index] = card
end
local packTop, packBottom = stage:CreateTexture(nil, "OVERLAY"), stage:CreateTexture(nil, "OVERLAY")
local slash = stage:CreateTexture(nil, "OVERLAY")
slash:SetTexture(ART .. "Slash")
slash:SetBlendMode("ADD")
slash:SetHeight(18)
local TOP_CUT = 0.17
local doneButton = Button(stage, 120, 24, "Done", function() end, "ButtonHot")
doneButton:SetPoint("TOPRIGHT", -12, -8)
local skipText = Text(stage, 10, MUTED, BODY_FONT)
skipText:SetPoint("TOPLEFT", 14, -12)

local reveal = { phase = "idle", t = 0, cards = {}, queue = {} }

local function SlotX(index, count)
    return PAGE_WIDTH / 2 + (index - (count + 1) / 2) * (CARD_W + 18)
end

-- A card's slot, centered at x, y on the stage (from its top left); burnt: how much of the card is gone, bottom up.
local function PlaceCard(card, x, y, burnt)
    card.slot:ClearAllPoints()
    card.slot:SetPoint("TOP", stage, "TOPLEFT", x, y + SLOT_H / 2)
    card.slot:SetHeight((burnt or 0) > 0 and math.max(1, PAD + CARD_H * (1 - burnt)) or SLOT_H)
    card.x, card.y = x, y
end

local function ResetStage()
    for _, card in ipairs(revealCards) do
        card:Hide()
        card:SetAlpha(1)
        card:SetWidth(CARD_W)
        card.edge:Hide()
        card.edgeGlow:Hide()
        PlaceCard(card, card.x or PAGE_WIDTH / 2, card.y or CARD_Y, 0)
        card.slot:SetHeight(SLOT_H)
    end
    packTop:Hide()
    packBottom:Hide()
    slash:Hide()
    doneButton:Hide()
    skipText:SetText("")
end

local StartReveal
local function NextOrIdle()
    reveal.phase = "idle"
    ResetStage()
    local nextPack = table.remove(reveal.queue, 1)
    if nextPack then
        StartReveal(nextPack.pack, nextPack.cards)
    else
        idle:Show()
    end
    Refresh()
end

local function Dissolve()
    if reveal.phase ~= "shown" then
        return
    end
    reveal.phase = "dissolve"
    reveal.t = 0
    doneButton:Hide()
    skipText:SetText("")
    PlaySound("igQuestLogAbandonQuest")
end
doneButton:SetScript("OnClick", Dissolve)

function StartReveal(pack, cards)
    if reveal.phase ~= "idle" then
        -- Another pack opened: the cards on show burn away, then it plays.
        reveal.queue[#reveal.queue + 1] = { pack = pack, cards = cards }
        Dissolve()
        return
    end
    idle:Hide()
    ResetStage()
    local info = PACKS[pack] or PACKS[0]
    reveal.phase, reveal.t, reveal.cards, reveal.golden, reveal.pack = "rip", 0, cards, info.golden, pack
    for _, texture in ipairs({ packTop, packBottom }) do
        texture:SetTexture(ART .. info.art)
        texture:SetWidth(96)
        texture:Show()
        texture:SetAlpha(1)
    end
    packTop:SetTexCoord(0, 1, 0, TOP_CUT)
    packTop:SetHeight(144 * TOP_CUT)
    packBottom:SetTexCoord(0, 1, TOP_CUT, 1)
    packBottom:SetHeight(144 * (1 - TOP_CUT))
    for index, card in ipairs(revealCards) do
        local entry = cards[index]
        card.entry, card.turned = entry, false
        card:Hide()
        card.glow:Hide()
        card.result:SetText("")
        if entry then
            card:SetBack(info.kind, info.golden)
        end
    end
    skipText:SetText("Click to skip")
    PlaySound("igQuestListOpen")
end

local function Ease(p)
    return 1 - (1 - p) * (1 - p) * (1 - p)
end

local function TurnCard(card)
    card.turned = true
    card:SetCard(card.entry, reveal.golden, false)
    card:SetContent(false)
    Shown(card.glow, card.entry.rarity >= 2 or reveal.golden)
    local x, y = Where(card)
    local rgb = reveal.golden and GOLDEN_RGB or RARITY_RGB[card.entry.rarity]
    Burst(x, y, 8 + card.entry.rarity * 14, rgb, 60, 160 + card.entry.rarity * 90)
    Ring(x, y, 90, { 1, 0.95, 0.85 })
    if card.entry.rarity >= 2 then
        Ring(x, y, 160 + card.entry.rarity * 40, rgb)
    end
    card.result:SetText(card.entry.new and "|cff8dff8aNew!|r"
        or ("|cffd9a46aDuplicate  +" .. Thousands(card.entry.refund) .. " Emberspark|r"))
    PlaySound(card.entry.rarity >= 3 and "LEVELUPSOUND" or card.entry.rarity >= 1 and "igMainMenuOptionCheckBoxOn"
        or "igMainMenuOptionCheckBoxOff")
end

stage:SetScript("OnUpdate", function(_, delta)
    if reveal.phase == "idle" then
        return
    end
    reveal.t = reveal.t + math.min(delta, 0.05)
    local t, count = reveal.t, #reveal.cards
    local cx, cy = PAGE_WIDTH / 2, CARD_Y
    if reveal.phase == "rip" then
        -- The pack shakes, a slash runs across its crimp, the top flies off and the rest drops away.
        local shake = t < 0.45 and math.sin(t * 60) * 3 * (1 - t / 0.45) or 0
        local bottomY = cy - 144 * TOP_CUT / 2
        packBottom:ClearAllPoints()
        packBottom:SetPoint("CENTER", stage, "TOPLEFT", cx + shake, bottomY - math.max(0, t - 0.6) * 300)
        packBottom:SetAlpha(math.max(0, 1 - math.max(0, t - 0.6) * 3))
        local topP = math.max(0, t - 0.5)
        packTop:ClearAllPoints()
        packTop:SetPoint("BOTTOM", stage, "TOPLEFT", cx + shake + topP * 120, bottomY + 144 * (1 - TOP_CUT) / 2
            + topP * 260)
        packTop:SetAlpha(math.max(0, 1 - topP * 2.5))
        if t > 0.35 and t < 0.6 then
            local p = (t - 0.35) / 0.25
            slash:Show()
            slash:ClearAllPoints()
            slash:SetWidth(40 + 110 * p)
            slash:SetPoint("LEFT", stage, "TOPLEFT", cx - 60, bottomY + 144 * (1 - TOP_CUT) / 2)
            slash:SetAlpha(1 - p * 0.5)
        else
            slash:Hide()
        end
        if t > 0.5 and not reveal.burst then
            reveal.burst = true
            local sx, sy = Where(stage)
            local x = sx - PAGE_WIDTH / 2 + cx
            local y = sy + (stage:GetHeight() or 260) / 2 + bottomY + 144 * (1 - TOP_CUT) / 2
            Burst(x, y, 24, reveal.golden and GOLDEN_RGB or { 1, 0.62, 0.3 }, 60, 220, 0.9)
        end
        if t > 0.9 then
            reveal.phase, reveal.t, reveal.burst = "deal", 0, nil
            packTop:Hide()
            packBottom:Hide()
        end
    elseif reveal.phase == "deal" then
        for index, card in ipairs(revealCards) do
            if card.entry then
                local p = Ease(math.max(0, math.min(1, (t - (index - 1) * 0.08) / 0.4)))
                card:Show()
                PlaceCard(card, cx + (SlotX(index, count) - cx) * p, cy + 30 * (1 - p))
                card:SetAlpha(p)
            end
        end
        if t > 0.45 + count * 0.08 then
            reveal.phase, reveal.t = "turn", 0
        end
    elseif reveal.phase == "turn" then
        -- Each card squeezes to its edge and opens again face up.
        local all = true
        for index, card in ipairs(revealCards) do
            if card.entry then
                local start = (index - 1) * 0.35
                local p = math.max(0, math.min(1, (t - start) / 0.3))
                if p < 0.5 then
                    card:SetWidth(math.max(1, CARD_W * (1 - p * 2)))
                    all = false
                elseif not card.turned then
                    TurnCard(card)
                    card:SetWidth(math.max(1, CARD_W * (p - 0.5) * 2))
                    all = false
                elseif p < 1 then
                    card:SetWidth(math.max(1, CARD_W * (p - 0.5) * 2))
                    all = false
                else
                    card:SetWidth(CARD_W)
                    card:SetContent(true)
                end
            end
        end
        if all then
            reveal.phase, reveal.t = "shown", 0
            doneButton:Show()
            skipText:SetText("Hover a card for its spell. Click, or Done, to put them away.")
        end
    elseif reveal.phase == "shown" then
        for _, card in ipairs(revealCards) do
            if card.entry and card.glow:IsShown() then
                card.glow:SetAlpha(0.55 + 0.25 * math.sin(t * 3))
            end
        end
    elseif reveal.phase == "dissolve" then
        -- Each card burns from the bottom up, one after another: the clip shrinks behind a flickering edge that
        -- sheds embers.
        for index, card in ipairs(revealCards) do
            if card.entry and card:IsShown() then
                local p = math.max(0, math.min(1, (t - (index - 1) * 0.12) / 0.9))
                PlaceCard(card, card.x, card.y, p)
                local burning = p > 0 and p < 1
                Shown(card.edge, burning)
                Shown(card.edgeGlow, burning)
                if burning then
                    local flicker = 0.75 + 0.25 * math.sin(t * 40 + index * 2)
                    card.edge:SetAlpha(flicker)
                    card.edgeGlow:SetAlpha(0.8 * flicker)
                    local sx, sy = Where(stage)
                    local x = sx - PAGE_WIDTH / 2 + card.x
                    local y = sy + (stage:GetHeight() or 260) / 2 + card.y - CARD_H / 2 + CARD_H * p
                    for _ = 1, 3 do
                        local hot = math.random() < 0.5
                        Emit({ x = x + (math.random() - 0.5) * CARD_W, y = y, vx = math.random(-25, 25),
                            vy = math.random(50, 150), r = 1, g = hot and 0.75 or 0.4, b = hot and 0.3 or 0.08,
                            size = 4 + math.random() * 6, life = 0.5 + math.random() * 0.6, drag = 0.97, grav = 40 })
                    end
                elseif p >= 1 then
                    card:Hide()
                end
            end
        end
        if t > 1.1 + #reveal.cards * 0.12 then
            NextOrIdle()
        end
    end
end)
stage:SetScript("OnMouseDown", function()
    if reveal.phase == "rip" or reveal.phase == "deal" or reveal.phase == "turn" then
        -- Skip ahead: every card face up.
        for index, card in ipairs(revealCards) do
            if card.entry then
                card:Show()
                card:SetAlpha(1)
                PlaceCard(card, SlotX(index, #reveal.cards), CARD_Y)
                if not card.turned then
                    TurnCard(card)
                end
                card:SetWidth(CARD_W)
                card:SetContent(true)
            end
        end
        packTop:Hide()
        packBottom:Hide()
        slash:Hide()
        reveal.phase, reveal.t = "shown", 0
        doneButton:Show()
    elseif reveal.phase == "shown" then
        Dissolve()
    end
end)

-- "Open all": the packs burst together, then one list of the new cards; the duplicates fill the tallies above.
local results = CreateFrame("Frame", nil, display)
results:SetAllPoints()
results:SetFrameLevel(display:GetFrameLevel() + 6)
results:EnableMouse(true)
results:Hide()
do
    local shade = results:CreateTexture(nil, "BACKGROUND")
    shade:SetTexture(0.02, 0.01, 0.01, 0.92)
    shade:SetPoint("TOPLEFT", 4, -4)
    shade:SetPoint("BOTTOMRIGHT", -4, 4)
end
local resultsTitle = Text(results, 17, GOLD_HI, DISPLAY_FONT)
resultsTitle:SetPoint("TOPLEFT", 18, -14)
local resultsSum = Text(results, 11, MUTED, BODY_FONT)
resultsSum:SetPoint("LEFT", resultsTitle, "RIGHT", 14, 0)
local resultsNone = Text(results, 13, MUTED, BODY_FONT)
resultsNone:SetPoint("CENTER", 0, 0)
local RESULT_ROWS, RESULT_COLUMNS = 5, 4
local resultItems = {}
for index = 1, RESULT_ROWS * RESULT_COLUMNS do
    local item = CreateFrame("Frame", nil, results)
    item:SetWidth((PAGE_WIDTH - 36 - 3 * 8) / RESULT_COLUMNS)
    item:SetHeight(34)
    item:SetPoint("TOPLEFT", 18 + ((index - 1) % RESULT_COLUMNS) * ((PAGE_WIDTH - 36 - 24) / RESULT_COLUMNS + 8),
        -44 - math.floor((index - 1) / RESULT_COLUMNS) * 38)
    Slice9(item, "Panel", 8)
    item.icon = item:CreateTexture(nil, "ARTWORK")
    item.icon:SetWidth(28)
    item.icon:SetHeight(28)
    item.icon:SetPoint("LEFT", 4, 0)
    item.name = Text(item, 11, GOLD_HI, DISPLAY_FONT)
    item.name:SetPoint("TOPLEFT", 38, -5)
    item.name:SetWidth(item:GetWidth() - 44)
    item.name:SetJustifyH("LEFT")
    item.sub = Text(item, 9, MUTED, BODY_FONT)
    item.sub:SetPoint("TOPLEFT", item.name, "BOTTOMLEFT", 0, -2)
    item:EnableMouse(true)
    item:SetScript("OnEnter", function(self)
        if self.entry then
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetHyperlink("spell:" .. self.entry.spell)
            GameTooltip:Show()
        end
    end)
    item:SetScript("OnLeave", function() GameTooltip:Hide() end)
    resultItems[index] = item
end
local mass = { cards = {}, offset = 0 }
local function ShowResults()
    local fresh = {}
    for _, entry in ipairs(mass.cards) do
        if entry.new then
            fresh[#fresh + 1] = entry
        end
    end
    table.sort(fresh, function(a, b) return a.rarity > b.rarity end)
    for index, item in ipairs(resultItems) do
        local entry = fresh[mass.offset + index]
        item.entry = entry
        Shown(item, entry ~= nil)
        if entry then
            item.icon:SetTexture(CardIcon(entry))
            item.name:SetText(CardName(entry))
            item.name:SetTextColor(unpack(mass.golden and GOLDEN_RGB or NAME_RGB[entry.rarity]))
            item.sub:SetText((mass.golden and "Golden " or "") .. RARITY_NAMES[entry.rarity]
                .. (entry.kind == 1 and " Talent Card" or " Skill Card"))
            Tint(item, unpack(RARITY_RGB[entry.rarity]))
        end
    end
    Shown(resultsNone, #fresh == 0)
    resultsNone:SetText("No new cards this time: every one was a duplicate.")
    local refunds = 0
    for _, entry in ipairs(mass.cards) do
        refunds = refunds + (entry.refund or 0)
    end
    resultsTitle:SetText(string.format("%d %s opened", mass.count or 0, (mass.count or 0) == 1 and "pack" or "packs"))
    resultsSum:SetText(string.format("%d cards  -  %d new  -  %d duplicates (+%s Emberspark)%s", #mass.cards, #fresh,
        #mass.cards - #fresh, Thousands(refunds), #fresh > #resultItems and "  -  scroll for more" or ""))
end
results:EnableMouseWheel(true)
results:SetScript("OnMouseWheel", function(_, direction)
    local fresh = 0
    for _, entry in ipairs(mass.cards) do
        if entry.new then
            fresh = fresh + 1
        end
    end
    mass.offset = math.max(0, math.min(mass.offset - direction * RESULT_COLUMNS,
        math.max(0, math.ceil((fresh - #resultItems) / RESULT_COLUMNS) * RESULT_COLUMNS)))
    ShowResults()
end)
local resultsDone = Button(results, 120, 24, "Done", function()
    results:Hide()
    idle:Show()
end, "ButtonHot")
resultsDone:SetPoint("TOPRIGHT", -12, -8)

local function RefreshPacks()
    local dup = state.duplicates or 0
    dupMeter.count:SetText((dup % state.packEvery) .. " / " .. state.packEvery)
    for index, pip in ipairs(dupMeter.pips) do
        if index <= dup % state.packEvery then
            pip:SetTexture(1, 0.48, 0.14, 1)
        else
            pip:SetTexture(0.16, 0.1, 0.07, 1)
        end
    end
    goldMeter.count:SetText((dup % state.goldenEvery) .. " / " .. state.goldenEvery)
    for index, pip in ipairs(goldMeter.pips) do
        if index <= dup % state.goldenEvery then
            pip:SetTexture(1, 0.82, 0.4, 1)
        else
            pip:SetTexture(0.16, 0.1, 0.07, 1)
        end
    end
    for pack, widget in pairs(packWidgets) do
        local owned = GetItemCount(PACKS[pack].item) or 0
        widget.owned:SetText("You have " .. owned)
        widget.open:SetEnabled(owned > 0)
        widget.all:SetEnabled(owned > 1)
        widget.all:SetLabel(owned > 1 and ("Open all " .. math.min(owned, 50)) or "Open all")
        local price = state.prices[pack] or 0
        widget.buy:SetLabel("Buy   |T" .. SPARK_ICON .. ":12|t " .. Thousands(price))
        widget.buy:SetEnabled(state.spark >= price)
        widget.artTexture:SetDesaturated(owned == 0)
        widget.artTexture:SetVertexColor(owned == 0 and 0.6 or 1, owned == 0 and 0.6 or 1, owned == 0 and 0.6 or 1)
        if widget.gold then
            widget.gold:SetLabel(string.format("or %dg  -  %d left this week", math.floor(state.goldPrice / 10000),
                state.goldLeft))
            widget.gold:SetEnabled(state.goldLeft > 0 and GetMoney() >= state.goldPrice)
        end
    end
end

-- Cards: the collection ----------------------------------------------------------------------------------------------

local cardsPage = pages[3]
local chips = {}
local function Chip(label, width, onClick)
    local chipButton = Button(cardsPage, width, 22, label, onClick, "Button", 11)
    chips[#chips + 1] = chipButton
    return chipButton
end
local chipX = 0
local function Place(chipButton, gap)
    chipButton:SetPoint("TOPLEFT", chipX, 0)
    chipX = chipX + chipButton:GetWidth() + (gap or 5)
end
local kindChips, rarityChips = {}, {}
for kind, label in pairs({ [0] = "Skill Cards", [1] = "Talent Cards" }) do
    kindChips[kind] = Chip(label, 84, function()
        view.kind, view.offset = kind, 0
        Refresh()
    end)
end
Place(kindChips[0])
Place(kindChips[1], 14)
for rarity = -1, 3 do
    rarityChips[rarity] = Chip(rarity == -1 and "All" or RARITY_NAMES[rarity], rarity == -1 and 40 or 66, function()
        view.rarity, view.offset = rarity, 0
        Refresh()
    end)
    Place(rarityChips[rarity], rarity == 3 and 14 or 5)
end
local missingChip = Chip("Not owned", 72, function()
    view.missing, view.offset = not view.missing, 0
    Refresh()
end)
Place(missingChip)
local goldenChip = Chip("Golden", 60, function()
    view.golden, view.offset = not view.golden, 0
    Refresh()
end)
Place(goldenChip)
local search = CreateFrame("EditBox", nil, cardsPage)
search:SetWidth(150)
search:SetHeight(22)
search:SetPoint("TOPRIGHT", 0, 0)
search:SetAutoFocus(false)
search:SetFont(BODY_FONT, 11)
search:SetTextInsets(8, 8, 0, 0)
search:SetTextColor(unpack(GOLD_HI))
Slice9(search, "Button", 8)
local searchHint = Text(search, 11, { 0.5, 0.42, 0.32 }, BODY_FONT)
searchHint:SetPoint("LEFT", 8, 0)
searchHint:SetText("Search cards")
search:SetScript("OnTextChanged", function(self)
    view.search, view.offset = string.lower(self:GetText() or ""), 0
    Shown(searchHint, (self:GetText() or "") == "")
    Refresh()
end)
search:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
search:SetScript("OnEnterPressed", function(self) self:ClearFocus() end)
local collectionText = Text(cardsPage, 10, MUTED, BODY_FONT)
collectionText:SetPoint("TOPLEFT", 0, -30)
local pageText = Text(cardsPage, 10, MUTED, BODY_FONT)
pageText:SetPoint("TOPRIGHT", -62, -30)
local prevPage = Button(cardsPage, 26, 18, "<", function()
    view.offset = math.max(0, view.offset - CARDS_PER_PAGE)
    Refresh()
end, "Button", 11)
prevPage:SetPoint("TOPRIGHT", -30, -27)
local nextPage = Button(cardsPage, 26, 18, ">", function()
    view.offset = view.offset + CARDS_PER_PAGE
    Refresh()
end, "Button", 11)
nextPage:SetPoint("TOPRIGHT", 0, -27)

local CELL_W, CELL_H = 116, 174
local cells = {}
local ShowBuy
for index = 1, CARDS_PER_PAGE do
    local cell = CreateFrame("Button", nil, cardsPage)
    cell:SetWidth(CELL_W)
    cell:SetHeight(CELL_H + 16)
    local column, row = (index - 1) % GRID_COLUMNS, math.floor((index - 1) / GRID_COLUMNS)
    local gap = (PAGE_WIDTH - GRID_COLUMNS * CELL_W) / (GRID_COLUMNS - 1)
    cell:SetPoint("TOPLEFT", column * (CELL_W + gap), -50 - row * (CELL_H + 30))
    cell.face = CardFace(cell, CELL_W, CELL_H)
    cell.face:SetPoint("TOP", 0, 0)
    cell.face:EnableMouse(false)
    cell.label = Text(cell, 11, MUTED, BODY_FONT)
    cell.label:SetPoint("BOTTOM", 0, 0)
    cell:SetScript("OnEnter", function(self)
        if self.entry then
            self.face.glow:Show()
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetHyperlink("spell:" .. self.entry.spell)
            GameTooltip:Show()
        end
    end)
    cell:SetScript("OnLeave", function(self)
        self.face.glow:Hide()
        GameTooltip:Hide()
    end)
    cell:SetScript("OnClick", function(self)
        if self.entry and not view.golden and self.entry.owned < self.entry.max then
            ShowBuy(self.entry, self)
        end
    end)
    cells[index] = cell
end
cardsPage:EnableMouseWheel(true)
cardsPage:SetScript("OnMouseWheel", function(_, direction)
    view.offset = math.max(0, view.offset - direction * GRID_COLUMNS)
    Refresh()
end)

local function Matches(card)
    if card.kind ~= view.kind or (view.rarity >= 0 and card.rarity ~= view.rarity) then
        return false
    end
    local owned = view.golden and card.golden or card.owned
    if view.missing and owned >= (view.golden and 1 or card.max) then
        return false
    end
    return view.search == "" or string.find(string.lower(CardName(card)), view.search, 1, true) ~= nil
end

local function RefreshCards()
    for kind, chipButton in pairs(kindChips) do
        chipButton:SetOn(view.kind == kind)
    end
    for rarity, chipButton in pairs(rarityChips) do
        chipButton:SetOn(view.rarity == rarity)
    end
    missingChip:SetOn(view.missing)
    goldenChip:SetOn(view.golden)
    local shown, owned, total, golden = {}, 0, 0, 0
    for _, card in ipairs(catalog) do
        if card.kind == view.kind then
            total = total + 1
            if card.owned >= card.max then
                owned = owned + 1
            end
            if card.golden > 0 then
                golden = golden + 1
            end
        end
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
    local last = math.max(0, math.ceil(#shown / GRID_COLUMNS) * GRID_COLUMNS - CARDS_PER_PAGE)
    view.offset = math.max(0, math.min(view.offset, last))
    collectionText:SetText(not catalogLoaded and "Loading your collection..." or string.format(
        "Your collection: %d / %d %s  -  %d Golden", owned, total, view.kind == 1 and "Talent Cards" or "Skill Cards",
        golden))
    pageText:SetText(#shown == 0 and "" or string.format("%d-%d of %d", view.offset + 1,
        math.min(#shown, view.offset + CARDS_PER_PAGE), #shown))
    prevPage:SetEnabled(view.offset > 0)
    nextPage:SetEnabled(view.offset < last)
    for index, cell in ipairs(cells) do
        local card = shown[view.offset + index]
        cell.entry = card
        Shown(cell, card ~= nil)
        if card then
            local have = view.golden and card.golden or card.owned
            local full = view.golden and have > 0 or have >= card.max
            cell.face:SetCard(card, view.golden, not full and have == 0)
            if view.golden then
                cell.label:SetText(have > 0 and ("|cff8dff8aOwned" .. (have > 1 and (" x" .. have) or "") .. "|r")
                    or "Golden Packs only")
            elseif full then
                cell.label:SetText("|cff8dff8aOwned|r")
            else
                local price = state.cardPrices[card.rarity] or 0
                cell.label:SetText((card.max > 1 and (have .. "/" .. card.max .. "  ") or "")
                    .. "|T" .. SPARK_ICON .. ":12|t |cffffe7a3" .. Thousands(price) .. "|r")
            end
        end
    end
end

-- Buying a card: a dialog over the page.
local modal = CreateFrame("Frame", nil, frame)
modal:SetPoint("TOPLEFT", 12, -12)
modal:SetPoint("BOTTOMRIGHT", -12, 12)
modal:SetFrameLevel(frame:GetFrameLevel() + 20)
modal:EnableMouse(true)
modal:Hide()
do
    local shade = modal:CreateTexture(nil, "BACKGROUND")
    shade:SetTexture(0.01, 0.01, 0.01, 0.72)
    shade:SetAllPoints()
end
local box = Panel(modal, "PanelGold", 0, 0, 470, 250)
box:ClearAllPoints()
box:SetPoint("CENTER", 0, 0)
local boxCard = CardFace(box, 140, 210)
boxCard:SetPoint("LEFT", 20, 0)
local boxName = Text(box, 20, GOLD_HI, DISPLAY_FONT)
boxName:SetPoint("TOPLEFT", 180, -46)
boxName:SetWidth(270)
boxName:SetJustifyH("LEFT")
local boxText = Text(box, 11, { 0.9, 0.83, 0.69 }, BODY_FONT)
boxText:SetPoint("TOPLEFT", boxName, "BOTTOMLEFT", 0, -10)
boxText:SetWidth(270)
boxText:SetJustifyH("LEFT")
local boxPrice = Text(box, 14, GOLD_HI, BODY_FONT)
boxPrice:SetPoint("TOPLEFT", boxText, "BOTTOMLEFT", 0, -12)
boxPrice:SetWidth(270)
boxPrice:SetJustifyH("LEFT")
local boxBuy = Button(box, 130, 28, "Buy", function() end, "ButtonHot", 12)
boxBuy:SetPoint("BOTTOMLEFT", 180, 30)
local boxCancel = Button(box, 120, 28, "Cancel", function() modal:Hide() end, "Button", 12)
boxCancel:SetPoint("LEFT", boxBuy, "RIGHT", 10, 0)
local buying
function ShowBuy(card, cell)
    buying = { card = card, cell = cell }
    boxCard:SetCard(card, false, false)
    boxName:SetText(CardName(card))
    boxName:SetTextColor(unpack(NAME_RGB[card.rarity]))
    boxText:SetText(RARITY_NAMES[card.rarity] .. (card.kind == 1 and " Talent Card" or " Skill Card")
        .. (card.max > 1 and (" (a rank: you have " .. card.owned .. " of " .. card.max .. ")") or "")
        .. ". Buy it and it is yours forever, on every character of your account.")
    local price = state.cardPrices[card.rarity] or 0
    boxPrice:SetText("|T" .. SPARK_ICON .. ":16|t " .. Thousands(price) .. " Emberspark")
    boxBuy:SetEnabled(state.spark >= price)
    boxBuy:SetLabel(state.spark >= price and "Buy" or ("Need " .. Thousands(price - state.spark) .. " more"))
    modal:Show()
end
boxBuy:SetScript("OnClick", function(self)
    if self.enabled and buying then
        PlaySound("igMainMenuOptionCheckBoxOn")
        Send("CARD," .. buying.card.kind .. "," .. buying.card.id)
        modal:Hide()
    end
end)

-- Refresh and tabs ---------------------------------------------------------------------------------------------------

function Refresh()
    sparkPill:SetValue(state.spark)
    eyePill:SetValue(eyes.count)
    if pages[1]:IsShown() then
        RefreshSpecs()
    end
    if pages[2]:IsShown() then
        RefreshPacks()
    end
    if pages[3]:IsShown() then
        RefreshCards()
    end
end

function ShowTab(index)
    view.tab = index
    for other, page in ipairs(pages) do
        Shown(page, other == index)
        tabs[other]:SetOn(other == index)
    end
    modal:Hide()
    Refresh()
end

frame:SetScript("OnShow", function()
    local height = UIParent:GetHeight() or 768
    frame:SetScale(math.min(1, (height - 30) / (HEIGHT + 44)))
    if not asked then
        asked = true
        Send("LIST")
    end
    PlaySound("igCharacterInfoOpen")
    ShowTab(view.tab)
end)
frame:SetScript("OnHide", function()
    PlaySound("igCharacterInfoClose")
    GameTooltip:Hide()
end)

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

local function ParseCards(text)
    local cards = {}
    for _, item in ipairs(Split(text or "", ";")) do
        local n = Numbers(item)
        if n[3] and n[3] > 0 then
            cards[#cards + 1] = { kind = n[1], id = n[2], spell = n[3], rarity = n[4], new = n[5] == 1,
                refund = n[6] }
        end
    end
    return cards
end

local function OpenOn(tab)
    view.tab = tab
    if frame:IsShown() then
        ShowTab(tab)
    else
        frame:Show()
    end
end

local function OnServerMessage(message)
    local kind = message:match("^([^,]+)")
    local body = message:match("^[^,]+,(.*)$") or ""
    local f = Split(body, ",")
    if kind == "S" then
        state.spark = tonumber(f[1]) or 0
        state.prices = { [0] = tonumber(f[2]) or 0, tonumber(f[3]) or 0, tonumber(f[4]) or 0, tonumber(f[4]) or 0 }
        state.goldLeft, state.goldPrice = tonumber(f[5]) or 0, tonumber(f[6]) or 0
        state.duplicates, state.packEvery, state.goldenEvery = tonumber(f[7]) or 0, tonumber(f[8]) or 10,
            tonumber(f[9]) or 50
        state.cardPrices = { [0] = tonumber(f[10]) or 500, tonumber(f[11]) or 5000, tonumber(f[12]) or 12000,
            tonumber(f[13]) or 30000 }
        Refresh()
    elseif kind == "E" then
        eyes.count, eyes.claimed, eyes.level = tonumber(f[1]) or 0, tonumber(f[2]) or 7, tonumber(f[3]) or 1
        eyes.price, eyes.perBuy, eyes.canBuy = tonumber(f[4]) or 0, tonumber(f[5]) or 2, f[6] == "1"
        eyes.bought, eyes.abilities, eyes.talents = tonumber(f[7]) or 0, tonumber(f[8]) or 0, tonumber(f[9]) or 0
        eyes.first, eyes.last, eyes.milestoneEyes = tonumber(f[10]) or 8, tonumber(f[11]) or 60, tonumber(f[12]) or 4
        if f[13] and f[13] ~= "" then
            eyes.milestones = Numbers(f[13])
        end
        Refresh()
    elseif kind == "K0" then
        wipe(catalog)
        wipe(byKey)
        catalogLoaded = false
    elseif kind == "K" then
        for _, item in ipairs(Split(body, ";")) do
            local n = Numbers(item)
            local card = { kind = n[1], id = n[2], spell = n[3], rarity = n[4], owned = n[5], max = n[6],
                golden = n[7] }
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
    elseif kind == "M" then
        mass = { pack = tonumber(f[1]) or 0, count = tonumber(f[2]) or 0, cards = {}, offset = 0, collecting = true }
        mass.golden = PACKS[mass.pack] and PACKS[mass.pack].golden
    elseif kind == "ME" then
        mass.collecting = false
        OpenOn(2)
        results:Show()
        idle:Hide()
        local x, y = Where(display)
        Burst(x, y, 60, mass.golden and GOLDEN_RGB or { 1, 0.62, 0.3 }, 80, 360)
        Ring(x, y, 300, mass.golden and GOLDEN_RGB or { 1, 0.62, 0.3 })
        PlaySound("LEVELUPSOUND")
        ShowResults()
    elseif kind == "R" then
        local pack, rest = body:match("^(%d+),(.*)$")
        local cards = ParseCards(rest)
        if mass.collecting then
            for _, card in ipairs(cards) do
                mass.cards[#mass.cards + 1] = card
            end
        else
            OpenOn(2)
            results:Hide()
            StartReveal(tonumber(pack) or 0, cards)
        end
    elseif kind == "B" then
        local n = Numbers(body)
        local card = byKey[n[1] .. "." .. n[2]]
        for _, cell in ipairs(cells) do
            if cell.entry == card and cell:IsShown() then
                local x, y = Where(cell)
                Burst(x, y, 40, RARITY_RGB[n[4]] or GOLD, 60, 260)
                Ring(x, y, 160, RARITY_RGB[n[4]] or GOLD)
            end
        end
        PlaySound("LEVELUPSOUND")
    elseif kind == "O" then
        if not frame:IsShown() then
            frame:Show()
        end
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_MONEY")
events:RegisterEvent("BAG_UPDATE")
events:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            OnServerMessage(message)
        end
    elseif frame:IsShown() then
        Refresh()
    end
end)

SLASH_EMBERFORGEDARKMOON1 = "/darkmoon"
SlashCmdList.EMBERFORGEDARKMOON = function()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
    end
end

-- for tests
EmberforgeDarkmoon_State = { state = state, eyes = eyes, catalog = catalog, view = view, reveal = reveal,
    stage = stage, results = results, modal = modal, cells = cells, packWidgets = packWidgets, tabs = tabs,
    pages = pages, claimButton = claimButton, buyEyes = buyEyes, mass = function() return mass end, chip = chip,
    doneButton = doneButton }

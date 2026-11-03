-- Emberforge Journal: the Dungeon Journal, laid out as Cataclysm's. Open with /journal or /dj.
--
-- - Home: the expansion's name as the title (click it to choose Classic, the Burning Crusade or Wrath), Dungeons and
--   Raids tabs, a search box, and a grid of tiles, one per dungeon or raid, with its creature levels.
-- - An instance: an open book. Left page: the instance (type, expansion, creature levels as the server has them after
--   the level squish, map, each difficulty with its size, queue levels and dungeon ID). Right page: its difficulty
--   buttons and its bosses, each with its portrait (the creature's own model).
-- - A boss: the boss list moves to the left page, the boss's model fills the right one (drag to turn it).
-- The breadcrumbs (Home, the instance, the boss) go back. The art is 3.3.5's own (the Achievement window's parchment,
-- the creatures' models): Cataclysm's thumbnails and portraits are not in this client.
--
-- The server (mod-titan, src/journal.cpp) sends the data as addon whispers to the player, prefix EFDJ:
--   to server     LIST
--   from server   B   D,<lfg id>~<map>~<type>~<expansion>~<difficulty>~<players>~<creature min>~<creature max>
--                       ~<queue min>~<queue max>~<seasonal>~<name>;...
--                     F,<map>~<difficulty>~<boss>~<creature entry>;...   Z

local PREFIX = "EFDJ"
local ART = "Interface\\AddOns\\Emberforge_Resources\\Art\\"
local PARCHMENT = "Interface\\AchievementFrame\\UI-Achievement-Parchment-Horizontal"
local BAR = "Interface\\AchievementFrame\\UI-Achievement-Category-Background"
local WIDTH, HEIGHT = 820, 520
local COLUMNS, TILE_WIDTH, TILE_HEIGHT, TILE_GAP = 4, 176, 90, 12
local BOSS_HEIGHT, BOSS_GAP = 50, 8
local GOLD = { 1, 0.82, 0 }
local INK = { 0.22, 0.12, 0.04 }
local EXPANSIONS = { [0] = "Classic", [1] = "Burning Crusade", [2] = "Wrath of the Lich King" }
-- Each expansion's tile tint (top, bottom) and its home page's glow.
local TINTS = {
    [0] = { { 0.42, 0.26, 0.12 }, { 0.12, 0.07, 0.03 }, { 0.45, 0.12, 0.04 } },
    [1] = { { 0.22, 0.4, 0.14 }, { 0.05, 0.1, 0.03 }, { 0.16, 0.32, 0.06 } },
    [2] = { { 0.2, 0.34, 0.5 }, { 0.04, 0.07, 0.12 }, { 0.1, 0.22, 0.4 } },
}
local TYPES = { [1] = "Dungeon", [2] = "Raid" }

local data = { instances = {}, order = {}, bosses = {}, loaded = false, loading = false }
local view = { page = "home", expansion = 0, type = 1, instance = nil, difficulty = 1, boss = nil, search = "" }
local frame, Refresh

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function OnServerMessage(message)
    local kind = message:sub(1, 1)
    if message == "B" then
        wipe(data.instances)
        wipe(data.order)
        wipe(data.bosses)
    elseif kind == "D" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local id, map, kindId, expansion, difficulty, players, low, high, qmin, qmax, seasonal, name =
                strsplit("~", entry, 12)
            map = tonumber(map)
            local key = map .. ":" .. name
            local instance = data.instances[key]
            if not instance then
                instance = { key = key, name = name, map = map, type = tonumber(kindId),
                    expansion = tonumber(expansion), low = tonumber(low), high = tonumber(high),
                    seasonal = seasonal == "1", difficulties = {} }
                data.instances[key] = instance
                data.order[#data.order + 1] = instance
            end
            instance.difficulties[#instance.difficulties + 1] = { id = tonumber(id), difficulty = tonumber(difficulty),
                players = tonumber(players), qmin = tonumber(qmin), qmax = tonumber(qmax) }
        end
    elseif kind == "F" then
        for entry in message:sub(3):gmatch("[^;]+") do
            local map, difficulty, boss, creature = strsplit("~", entry, 4)
            local key = map .. ":" .. difficulty
            data.bosses[key] = data.bosses[key] or {}
            tinsert(data.bosses[key], { name = boss, creature = tonumber(creature) or 0 })
        end
    elseif message == "Z" then
        table.sort(data.order, function(a, b)
            if a.low ~= b.low then
                return a.low < b.low
            end
            return a.name < b.name
        end)
        for _, instance in ipairs(data.order) do
            table.sort(instance.difficulties, function(a, b) return a.difficulty < b.difficulty end)
        end
        data.loaded, data.loading = true, false
        Refresh()
    end
end

-- Rounded, gold-edged boxes (Emberforge_Resources' art, the Classless window's look).
local Box, BoxColor
do
    local FLIPS = { { 0, 1, 0, 1 }, { 1, 0, 0, 1 }, { 0, 1, 1, 0 }, { 1, 0, 1, 0 } }
    local CORNERS = { "TOPLEFT", "TOPRIGHT", "BOTTOMLEFT", "BOTTOMRIGHT" }
    BoxColor = function(target, fill, edge)
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
    Box = function(target, fill, edge)
        local size = 8
        target.boxFill, target.boxEdge = {}, {}
        local function Piece(list, layer, file, flip)
            local texture = target:CreateTexture(nil, layer)
            if file then
                texture:SetTexture(ART .. file .. "Small")
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
end
local CELL_FILL, CELL_EDGE = { 0.075, 0.055, 0.043, 0.96 }, { 0.38, 0.29, 0.16, 1 }
local ON_FILL, ON_EDGE = { 0.3, 0.13, 0.03, 0.96 }, { 1, 0.72, 0.32, 1 }

-- A creature's model in a frame (PlayerModel:SetCreature); portrait: the face camera.
local function ShowCreature(model, creature, portrait)
    model:ClearModel()
    if creature and creature > 0 and model.SetCreature then
        pcall(model.SetCreature, model, creature)
        if portrait and model.SetCamera then
            pcall(model.SetCamera, model, 0)
        end
    end
end

-- The window -----------------------------------------------------------------------------------------------------
frame = CreateFrame("Frame", "EmberforgeJournal", UIParent)
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
frame:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 24,
    insets = { left = 6, right = 6, top = 6, bottom = 6 },
})
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeJournal")

local ui = {}       -- the window's parts, in a table (the main chunk's locals are limited)

do
    local title = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    title:SetPoint("TOP", 0, -12)
    title:SetText("Dungeon Journal")
    local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -4, -4)

    -- The portrait: a skull in a gold ring, top left.
    local portrait = frame:CreateTexture(nil, "OVERLAY")
    portrait:SetWidth(58)
    portrait:SetHeight(58)
    portrait:SetPoint("TOPLEFT", -10, 12)
    SetPortraitToTexture(portrait, "Interface\\Icons\\INV_Misc_Bone_HumanSkull_01")
    local ring = frame:CreateTexture(nil, "OVERLAY")
    ring:SetTexture(ART .. "Ring")
    ring:SetWidth(66)
    ring:SetHeight(66)
    ring:SetPoint("CENTER", portrait, "CENTER")

    -- The navigation bar: breadcrumbs on the left, the search box on the right.
    local nav = CreateFrame("Frame", nil, frame)
    nav:SetHeight(28)
    nav:SetPoint("TOPLEFT", 54, -30)
    nav:SetPoint("TOPRIGHT", -16, -30)
    Box(nav, { 0.02, 0.015, 0.01, 0.9 }, CELL_EDGE)
    ui.crumbs = {}
    for index = 1, 3 do
        local crumb = CreateFrame("Button", nil, nav)
        crumb:SetHeight(22)
        Box(crumb, CELL_FILL, CELL_EDGE)
        crumb.text = crumb:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
        crumb.text:SetPoint("CENTER", -4, 0)
        local arrow = crumb:CreateTexture(nil, "ARTWORK")
        arrow:SetTexture(ART .. "ArrowRight")
        arrow:SetVertexColor(1, 0.82, 0.4)
        arrow:SetWidth(10)
        arrow:SetHeight(10)
        arrow:SetPoint("RIGHT", -6, 0)
        local hover = crumb:CreateTexture(nil, "HIGHLIGHT")
        hover:SetTexture(1, 0.6, 0.2, 0.12)
        hover:SetPoint("TOPLEFT", 2, -2)
        hover:SetPoint("BOTTOMRIGHT", -2, 2)
        crumb:SetPoint("LEFT", index == 1 and nav or ui.crumbs[index - 1], index == 1 and "LEFT" or "RIGHT",
            index == 1 and 4 or 2, 0)
        crumb.level = index
        crumb:SetScript("OnClick", function(self)
            if self.level == 1 then
                view.page, view.instance, view.boss = "home", nil, nil
            elseif self.level == 2 then
                view.page, view.boss = "instance", nil
            end
            Refresh()
        end)
        ui.crumbs[index] = crumb
    end

    local search = CreateFrame("EditBox", "EmberforgeJournalSearch", nav, "InputBoxTemplate")
    search:SetWidth(190)
    search:SetHeight(18)
    search:SetPoint("RIGHT", -10, 0)
    search:SetAutoFocus(false)
    local hint = search:CreateFontString(nil, "ARTWORK", "GameFontDisableSmall")
    hint:SetPoint("LEFT", 4, 0)
    hint:SetText("Search")
    search:SetScript("OnTextChanged", function(self)
        view.search = self:GetText():lower()
        if view.search == "" then
            hint:Show()
        else
            hint:Hide()
        end
        if view.page ~= "home" and view.search ~= "" then
            view.page, view.instance, view.boss = "home", nil, nil
        end
        Refresh()
    end)
    search:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
    ui.search = search
end

-- Home -----------------------------------------------------------------------------------------------------------
do
    local home = CreateFrame("Frame", nil, frame)
    home:SetPoint("TOPLEFT", 14, -64)
    home:SetPoint("BOTTOMRIGHT", -14, 14)
    -- A glow in the expansion's color behind it all, as Cataclysm's fire.
    home.glow = home:CreateTexture(nil, "BACKGROUND")
    home.glow:SetTexture(1, 1, 1, 1)
    home.glow:SetAllPoints()
    ui.home = home

    local title = CreateFrame("Button", nil, home)
    title:SetHeight(32)
    title:SetWidth(360)
    title:SetPoint("TOPLEFT", 12, -6)
    title.text = title:CreateFontString(nil, "ARTWORK")
    title.text:SetFont("Fonts\\MORPHEUS.TTF", 26)
    title.text:SetShadowOffset(2, -2)
    title.text:SetPoint("LEFT")
    title.text:SetTextColor(1, 0.62, 0.3)
    local arrow = title:CreateTexture(nil, "ARTWORK")
    arrow:SetTexture(ART .. "ArrowDown")
    arrow:SetVertexColor(1, 0.7, 0.35)
    arrow:SetWidth(14)
    arrow:SetHeight(14)
    arrow:SetPoint("LEFT", title.text, "RIGHT", 8, -2)
    local menu = CreateFrame("Frame", "EmberforgeJournalMenu", UIParent, "UIDropDownMenuTemplate")
    title:SetScript("OnClick", function(self)
        local items = {}
        for expansion = 0, 2 do
            items[#items + 1] = { text = EXPANSIONS[expansion], checked = view.expansion == expansion,
                func = function()
                    view.expansion = expansion
                    Refresh()
                end }
        end
        EasyMenu(items, menu, self, 0, 0, "MENU")
    end)
    ui.title = title

    ui.typeTabs = {}
    for index, kind in ipairs({ 1, 2 }) do
        local tab = CreateFrame("Button", nil, home)
        tab:SetWidth(140)
        tab:SetHeight(28)
        tab:SetPoint("TOPRIGHT", -12 - (2 - index) * 148, -8)
        Box(tab, CELL_FILL, CELL_EDGE)
        tab.label = tab:CreateFontString(nil, "ARTWORK", "GameFontNormal")
        tab.label:SetPoint("CENTER")
        tab.label:SetText(TYPES[kind] .. "s")
        tab.kind = kind
        tab:SetScript("OnClick", function(self)
            view.type = self.kind
            Refresh()
        end)
        ui.typeTabs[index] = tab
    end

    local scroll = CreateFrame("ScrollFrame", "EmberforgeJournalTiles", home, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 12, -48)
    scroll:SetPoint("BOTTOMRIGHT", -30, 10)
    local grid = CreateFrame("Frame", nil, scroll)
    grid:SetWidth(COLUMNS * (TILE_WIDTH + TILE_GAP))
    grid:SetHeight(1)
    scroll:SetScrollChild(grid)
    ui.grid, ui.tiles = grid, {}
end

local function Tile(index)
    local tile = ui.tiles[index]
    if tile then
        return tile
    end
    tile = CreateFrame("Button", nil, ui.grid)
    tile:SetWidth(TILE_WIDTH)
    tile:SetHeight(TILE_HEIGHT)
    local column, row = (index - 1) % COLUMNS, math.floor((index - 1) / COLUMNS)
    tile:SetPoint("TOPLEFT", column * (TILE_WIDTH + TILE_GAP), -row * (TILE_HEIGHT + TILE_GAP))
    tile:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 14,
        insets = { left = 3, right = 3, top = 3, bottom = 3 } })
    tile:SetBackdropBorderColor(0.75, 0.6, 0.4, 1)
    tile.fill = tile:CreateTexture(nil, "BACKGROUND")
    tile.fill:SetTexture(1, 1, 1, 1)
    tile.fill:SetPoint("TOPLEFT", 3, -3)
    tile.fill:SetPoint("BOTTOMRIGHT", -3, 3)
    tile.shine = tile:CreateTexture(nil, "BORDER")
    tile.shine:SetTexture(1, 1, 1, 1)
    tile.shine:SetGradientAlpha("VERTICAL", 1, 1, 1, 0, 1, 1, 1, 0.12)
    tile.shine:SetPoint("TOPLEFT", 3, -3)
    tile.shine:SetPoint("BOTTOMRIGHT", tile, "RIGHT", -3, 0)
    tile.name = tile:CreateFontString(nil, "ARTWORK")
    tile.name:SetFont("Fonts\\MORPHEUS.TTF", 15)
    tile.name:SetShadowOffset(1, -1)
    tile.name:SetTextColor(1, 0.86, 0.45)
    tile.name:SetPoint("TOPLEFT", 8, -10)
    tile.name:SetPoint("TOPRIGHT", -8, -10)
    tile.name:SetHeight(44)
    tile.name:SetJustifyV("MIDDLE")
    tile.levels = tile:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    tile.levels:SetPoint("BOTTOM", 0, 10)
    local hover = tile:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.75, 0.35, 0.16)
    hover:SetPoint("TOPLEFT", 3, -3)
    hover:SetPoint("BOTTOMRIGHT", -3, 3)
    tile:SetScript("OnClick", function(self)
        view.page, view.instance, view.boss, view.difficulty = "instance", self.instance.key, nil, 1
        Refresh()
    end)
    ui.tiles[index] = tile
    return tile
end

local function Levels(low, high)
    if not low or low == 0 then
        return "-"
    end
    return low == high and tostring(low) or (low .. "-" .. high)
end

local function RefreshHome()
    ui.title.text:SetText(EXPANSIONS[view.expansion])
    ui.title:SetWidth(ui.title.text:GetStringWidth() + 30)
    local tint = TINTS[view.expansion]
    ui.home.glow:SetGradientAlpha("VERTICAL", tint[3][1] * 0.25, tint[3][2] * 0.25, tint[3][3] * 0.25, 0.9,
        tint[3][1], tint[3][2], tint[3][3], 0.45)
    for _, tab in ipairs(ui.typeTabs) do
        local on = tab.kind == view.type
        BoxColor(tab, on and ON_FILL or CELL_FILL, on and ON_EDGE or CELL_EDGE)
        tab.label:SetTextColor(on and 1 or GOLD[1], on and 1 or GOLD[2], on and 1 or GOLD[3])
    end
    local shown = {}
    for _, instance in ipairs(data.order) do
        local match = view.search ~= "" and instance.name:lower():find(view.search, 1, true)
        if (view.search == "" and instance.expansion == view.expansion and instance.type == view.type) or match then
            shown[#shown + 1] = instance
        end
    end
    for index, instance in ipairs(shown) do
        local tile = Tile(index)
        local colors = TINTS[instance.expansion]
        tile.fill:SetGradientAlpha("VERTICAL", colors[2][1], colors[2][2], colors[2][3], 1,
            colors[1][1], colors[1][2], colors[1][3], 1)
        tile.name:SetText(instance.name)
        tile.levels:SetText((instance.seasonal and "Holiday event, " or "") .. "Level "
            .. Levels(instance.low, instance.high))
        tile.instance = instance
        tile:Show()
    end
    for index = #shown + 1, #ui.tiles do
        ui.tiles[index]:Hide()
    end
    ui.grid:SetHeight(math.max(1, math.ceil(#shown / COLUMNS) * (TILE_HEIGHT + TILE_GAP)))
end

-- The book -------------------------------------------------------------------------------------------------------
do
    local book = CreateFrame("Frame", nil, frame)
    book:SetPoint("TOPLEFT", 16, -64)
    book:SetPoint("BOTTOMRIGHT", -16, 16)
    Box(book, { 0.16, 0.09, 0.04, 1 }, { 0.55, 0.38, 0.18, 1 })
    ui.book = book
    local pages = {}
    for index, side in ipairs({ "LEFT", "RIGHT" }) do
        local page = CreateFrame("Frame", nil, book)
        page:SetPoint("TOP", 0, -8)
        page:SetPoint("BOTTOM", 0, 8)
        page:SetPoint(side, side == "LEFT" and 8 or -8, 0)
        page:SetPoint(side == "LEFT" and "RIGHT" or "LEFT", book, "CENTER", side == "LEFT" and -3 or 3, 0)
        page.paper = page:CreateTexture(nil, "BACKGROUND")
        page.paper:SetTexture(PARCHMENT)
        page.paper:SetAllPoints()
        page.paper:SetVertexColor(1, 0.94, 0.82)
        -- The spine's shade, deepest at the fold.
        local shade = page:CreateTexture(nil, "BORDER")
        shade:SetTexture(1, 1, 1, 1)
        shade:SetWidth(36)
        shade:SetPoint("TOP")
        shade:SetPoint("BOTTOM")
        if side == "LEFT" then
            shade:SetPoint("RIGHT")
            shade:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0.45)
        else
            shade:SetPoint("LEFT")
            shade:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.45, 0, 0, 0, 0)
        end
        pages[index] = page
    end
    ui.left, ui.right = pages[1], pages[2]

    -- Left page, the instance: a framed title plate, then what it is.
    local plate = CreateFrame("Frame", nil, ui.left)
    plate:SetHeight(150)
    plate:SetPoint("TOPLEFT", 16, -16)
    plate:SetPoint("TOPRIGHT", -16, -16)
    plate:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 } })
    plate:SetBackdropBorderColor(0.8, 0.62, 0.3, 1)
    plate.fill = plate:CreateTexture(nil, "BACKGROUND")
    plate.fill:SetTexture(1, 1, 1, 1)
    plate.fill:SetPoint("TOPLEFT", 4, -4)
    plate.fill:SetPoint("BOTTOMRIGHT", -4, 4)
    plate.name = plate:CreateFontString(nil, "ARTWORK")
    plate.name:SetFont("Fonts\\MORPHEUS.TTF", 24)
    plate.name:SetShadowOffset(2, -2)
    plate.name:SetTextColor(1, 0.86, 0.45)
    plate.name:SetPoint("LEFT", 12, 8)
    plate.name:SetPoint("RIGHT", -12, 8)
    plate.sub = plate:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    plate.sub:SetPoint("TOP", plate.name, "BOTTOM", 0, -8)
    ui.plate = plate
    local about = ui.left:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    about:SetPoint("TOPLEFT", plate, "BOTTOMLEFT", 6, -14)
    about:SetPoint("RIGHT", ui.left, "RIGHT", -24, 0)
    about:SetJustifyH("LEFT")
    about:SetJustifyV("TOP")
    about:SetSpacing(4)
    about:SetTextColor(unpack(INK))
    about:SetShadowOffset(0, 0)
    ui.about = about

    -- Right page header: the instance's name and its difficulty buttons.
    local header = ui.right:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
    header:SetPoint("TOPLEFT", 18, -16)
    header:SetTextColor(unpack(INK))
    header:SetShadowOffset(0, 0)
    ui.header = header
    ui.difficultyButtons = {}
    for index = 1, 4 do
        local button = CreateFrame("Button", nil, ui.right)
        button:SetWidth(34)
        button:SetHeight(22)
        button:SetPoint("TOPRIGHT", -16 - (index - 1) * 38, -14)
        Box(button, CELL_FILL, CELL_EDGE)
        button.text = button:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
        button.text:SetPoint("CENTER")
        button:SetScript("OnClick", function(self)
            view.difficulty, view.boss = self.index, nil
            Refresh()
        end)
        ui.difficultyButtons[index] = button
    end

    -- The boss model (the boss page's right page): drag to turn it.
    local model = CreateFrame("PlayerModel", nil, ui.right)
    model:SetPoint("TOPLEFT", 12, -12)
    model:SetPoint("BOTTOMRIGHT", -12, 44)
    model:EnableMouse(true)
    model:SetScript("OnMouseDown", function(self, button)
        if button == "LeftButton" then
            self.turning, self.lastX, self.facing = true, GetCursorPosition(), self.facing or 0
        end
    end)
    model:SetScript("OnMouseUp", function(self) self.turning = false end)
    model:SetScript("OnUpdate", function(self)
        if self.turning then
            local x = GetCursorPosition()
            self.facing = (self.facing or 0) + (x - self.lastX) / 80
            self.lastX = x
            self:SetFacing(self.facing)
        end
    end)
    local bossName = ui.right:CreateFontString(nil, "ARTWORK")
    bossName:SetFont("Fonts\\MORPHEUS.TTF", 22)
    bossName:SetShadowOffset(2, -2)
    bossName:SetTextColor(1, 0.86, 0.45)
    bossName:SetPoint("BOTTOM", 0, 16)
    ui.model, ui.bossName = model, bossName
end

-- A boss's button: a brown bar, its portrait in a ring on the left, its name.
local function BossButton(index)
    ui.bossButtons = ui.bossButtons or {}
    local button = ui.bossButtons[index]
    if button then
        return button
    end
    button = CreateFrame("Button", nil, ui.book)
    button:SetHeight(BOSS_HEIGHT)
    button.bar = button:CreateTexture(nil, "BACKGROUND")
    button.bar:SetTexture(BAR)
    button.bar:SetTexCoord(0, 1, 0, 0.75)
    button.bar:SetPoint("TOPLEFT", 22, -4)
    button.bar:SetPoint("BOTTOMRIGHT", 0, 4)
    button.selected = button:CreateTexture(nil, "BORDER")
    button.selected:SetTexture(1, 0.7, 0.3, 0.25)
    button.selected:SetAllPoints(button.bar)
    local portrait = CreateFrame("PlayerModel", nil, button)
    portrait:SetWidth(BOSS_HEIGHT - 6)
    portrait:SetHeight(BOSS_HEIGHT - 6)
    portrait:SetPoint("LEFT", 3, 0)
    button.portrait = portrait
    local back = button:CreateTexture(nil, "BACKGROUND")
    back:SetTexture(0, 0, 0, 0.85)
    back:SetAllPoints(portrait)
    local ring = portrait:CreateTexture(nil, "OVERLAY")
    ring:SetTexture(ART .. "Ring")
    ring:SetWidth(BOSS_HEIGHT + 4)
    ring:SetHeight(BOSS_HEIGHT + 4)
    ring:SetPoint("CENTER")
    button.name = button:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    button.name:SetPoint("LEFT", portrait, "RIGHT", 16, 0)
    button.name:SetPoint("RIGHT", -10, 0)
    button.name:SetJustifyH("LEFT")
    button.name:SetTextColor(1, 0.95, 0.85)
    local hover = button:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.8, 0.4, 0.15)
    hover:SetAllPoints(button.bar)
    button:SetScript("OnClick", function(self)
        view.page, view.boss = "boss", self.index
        Refresh()
    end)
    ui.bossButtons[index] = button
    return button
end

local function DifficultyLabel(instance, difficulty)
    if instance.type == 2 then
        return difficulty.players > 0 and tostring(difficulty.players) or "R"
    end
    return difficulty.difficulty == 1 and "H" or "N"
end

local function DifficultyName(instance, difficulty)
    if instance.type == 2 then
        return difficulty.players > 0 and (difficulty.players .. "-player") or "Raid"
    end
    return (difficulty.difficulty == 1 and "Heroic" or "Normal")
        .. (difficulty.players > 0 and (", " .. difficulty.players .. " players") or "")
end

local function Bosses(instance)
    local chosen = instance.difficulties[view.difficulty] or instance.difficulties[1]
    local bosses = chosen and data.bosses[instance.map .. ":" .. chosen.difficulty]
    if not bosses then
        for _, difficulty in ipairs(instance.difficulties) do
            bosses = bosses or data.bosses[instance.map .. ":" .. difficulty.difficulty]
        end
    end
    return bosses or {}
end

local function RefreshBook()
    local instance = data.instances[view.instance]
    if not instance then
        view.page = "home"
        return
    end
    local colors = TINTS[instance.expansion]
    ui.plate.fill:SetGradientAlpha("VERTICAL", colors[2][1], colors[2][2], colors[2][3], 1,
        colors[1][1], colors[1][2], colors[1][3], 1)
    ui.plate.name:SetText(instance.name)
    ui.plate.sub:SetText(TYPES[instance.type] .. ", " .. EXPANSIONS[instance.expansion])
    local lines = {
        "Creature levels: " .. Levels(instance.low, instance.high),
        "Map ID: " .. instance.map .. (instance.seasonal and "   (holiday event)" or ""),
        "",
    }
    for _, difficulty in ipairs(instance.difficulties) do
        lines[#lines + 1] = DifficultyName(instance, difficulty) .. ": queue levels "
            .. Levels(difficulty.qmin, difficulty.qmax) .. ", dungeon ID " .. difficulty.id
    end
    ui.about:SetText(table.concat(lines, "\n"))
    ui.header:SetText(instance.name)
    for index, button in ipairs(ui.difficultyButtons) do
        local difficulty = instance.difficulties[index]
        if difficulty and #instance.difficulties > 1 then
            button.index = index
            button.text:SetText(DifficultyLabel(instance, difficulty))
            local on = index == view.difficulty
            BoxColor(button, on and ON_FILL or CELL_FILL, on and ON_EDGE or CELL_EDGE)
            button:Show()
        else
            button:Hide()
        end
    end

    -- The boss list: on the right page for the instance, on the left beside a chosen boss.
    local bossPage = view.page == "boss"
    local page = bossPage and ui.left or ui.right
    local bosses = Bosses(instance)
    for _, part in ipairs({ ui.plate, ui.about }) do
        if bossPage then
            part:Hide()
        else
            part:Show()
        end
    end
    for index, boss in ipairs(bosses) do
        local button = BossButton(index)
        button:SetParent(page)
        button:ClearAllPoints()
        button:SetPoint("TOPLEFT", page, "TOPLEFT", 14,
            -(bossPage and 16 or 48) - (index - 1) * (BOSS_HEIGHT + BOSS_GAP))
        button:SetPoint("RIGHT", page, "RIGHT", -16, 0)
        button.index = index
        button.name:SetText(boss.name)
        ShowCreature(button.portrait, boss.creature, true)
        if bossPage and index == view.boss then
            button.selected:Show()
        else
            button.selected:Hide()
        end
        button:Show()
    end
    for index = #bosses + 1, #(ui.bossButtons or {}) do
        ui.bossButtons[index]:Hide()
    end

    local boss = bossPage and bosses[view.boss]
    if boss then
        ui.header:Hide()
        for _, button in ipairs(ui.difficultyButtons) do
            button:Hide()
        end
        ui.model:Show()
        ShowCreature(ui.model, boss.creature, false)
        ui.model.facing = 0
        ui.bossName:SetText(boss.name)
    else
        ui.header:Show()
        ui.model:Hide()
        ui.bossName:SetText("")
    end
end

local function RefreshCrumbs()
    local instance = data.instances[view.instance]
    local bosses = instance and Bosses(instance)
    local texts = { "Home", instance and instance.name, view.page == "boss" and bosses and bosses[view.boss]
        and bosses[view.boss].name }
    for index, crumb in ipairs(ui.crumbs) do
        if texts[index] then
            crumb.text:SetText(texts[index])
            crumb:SetWidth(crumb.text:GetStringWidth() + 34)
            crumb:Show()
        else
            crumb:Hide()
        end
    end
end

function Refresh()
    if not frame:IsShown() then
        return
    end
    if view.page ~= "home" then
        RefreshBook()
    end
    if view.page == "home" then
        ui.home:Show()
        ui.book:Hide()
        if ui.bossButtons then
            for _, button in ipairs(ui.bossButtons) do
                button:Hide()
            end
        end
        RefreshHome()
    else
        ui.home:Hide()
        ui.book:Show()
    end
    RefreshCrumbs()
end

frame:SetScript("OnShow", function()
    if not data.loaded and not data.loading then
        data.loading = true
        Send("LIST")
    end
    Refresh()
end)

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix == PREFIX and sender == UnitName("player") then
        OnServerMessage(message)
    end
end)

SLASH_EMBERFORGEJOURNAL1 = "/journal"
SLASH_EMBERFORGEJOURNAL2 = "/dj"
SlashCmdList.EMBERFORGEJOURNAL = function()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
    end
end

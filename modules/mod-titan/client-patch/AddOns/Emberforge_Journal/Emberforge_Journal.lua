-- Emberforge Journal: the Dungeon Journal, laid out as Cataclysm's. Open with /journal or /dj.
--
-- - Home: the expansion's name as the title (click it to choose Classic, the Burning Crusade or Wrath), Dungeons and
--   Raids tabs, a search box, and a grid of tiles, one per dungeon or raid, with its creature levels.
-- - An instance: an open book. Left page: its plate (its art where Art.lua has it) and details (type, expansion,
--   creature levels as the server has them after the level squish, map, each difficulty with its size, queue levels
--   and dungeon ID). Right page: its difficulty buttons and, by the tabs on the book's edge, its Bosses (each with
--   its portrait, the creature's own model) or its Loot (every boss's).
-- - A boss: left page its model (drag to turn it), level, classification and health; right page its Abilities (the
--   spells its script casts, with the client's own descriptions) or its Loot.
-- The breadcrumbs (Home, the instance, the boss) go back. The lists scroll. The art is 3.3.5's own: the Achievement
-- window's parchment, the creatures' models, and the instances' loading screens (EmberforgeJournalArt, by map).
--
-- The server (mod-titan, src/journal.cpp) sends the data as addon whispers to the player, prefix EFDJ:
--   to server     LIST
--   from server   B   D,<lfg id>~<map>~<type>~<expansion>~<difficulty>~<players>~<creature min>~<creature max>
--                       ~<queue min>~<queue max>~<seasonal>~<name>;...
--                     F,<map>~<difficulty>~<boss>~<creature entry>;...   Z
--   to server     BOSS,<map>,<difficulty>
--   from server   I,<boss>~<entry>~<level>~<rank>~<health>~<type>;...   A,<boss>~<spell>;...
--                 L,<boss>~<item>~<quality>~<slot>~<class>~<subclass>~<name>;...   Y,<map>~<difficulty>

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

local data = { instances = {}, order = {}, bosses = {}, details = {}, loaded = false, loading = false }
local view = { page = "home", expansion = 0, type = 1, instance = nil, difficulty = 1, boss = nil, search = "",
    tab = "bosses" }
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
    elseif kind == "I" or kind == "A" or kind == "L" then
        -- One instance's details, for the difficulty asked last (its Y names it).
        data.incoming = data.incoming or { info = {}, abilities = {}, loot = {} }
        for entry in message:sub(3):gmatch("[^;]+") do
            if kind == "I" then
                local boss, creature, level, rank, health, creatureType = strsplit("~", entry)
                data.incoming.info[tonumber(boss)] = { entry = tonumber(creature), level = tonumber(level),
                    rank = tonumber(rank), health = tonumber(health), type = tonumber(creatureType) }
            elseif kind == "A" then
                local boss, spell = strsplit("~", entry)
                boss = tonumber(boss)
                data.incoming.abilities[boss] = data.incoming.abilities[boss] or {}
                tinsert(data.incoming.abilities[boss], tonumber(spell))
            else
                local boss, item, quality, slot, class, subclass, name = strsplit("~", entry, 7)
                tinsert(data.incoming.loot, { boss = tonumber(boss), id = tonumber(item), quality = tonumber(quality),
                    slot = tonumber(slot), class = tonumber(class), subclass = tonumber(subclass), name = name })
            end
        end
    elseif kind == "Y" then
        local map, difficulty = strsplit("~", message:sub(3))
        local details = data.details[map .. ":" .. difficulty]
        local incoming = data.incoming or { info = {}, abilities = {}, loot = {} }
        data.incoming = nil
        if details then
            details.info, details.abilities, details.loot, details.ready = incoming.info, incoming.abilities,
                incoming.loot, true
        end
        Refresh()
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
        view.page, view.instance, view.boss, view.difficulty, view.tab = "instance", self.instance.key, nil, 1,
            "bosses"
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
        local art = EmberforgeJournalArt and EmberforgeJournalArt[instance.map]
        if art then
            -- The instance's own art (its loading screen), its middle band.
            tile.fill:SetTexture(art)
            tile.fill:SetTexCoord(0, 1, 0.3, 0.6)
            tile.fill:SetVertexColor(0.85, 0.85, 0.85)
        else
            tile.fill:SetTexture(1, 1, 1, 1)
            tile.fill:SetTexCoord(0, 1, 0, 1)
            tile.fill:SetGradientAlpha("VERTICAL", colors[2][1], colors[2][2], colors[2][3], 1,
                colors[1][1], colors[1][2], colors[1][3], 1)
        end
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
-- Two pages: the instance page (left: its plate and details; right: Bosses or Loot) and the boss page (left: its
-- model and details; right: Abilities or Loot). Tabs on the book's right edge switch the right page.
local SLOT_NAMES = {
    [1] = "Head", [2] = "Neck", [3] = "Shoulder", [4] = "Shirt", [5] = "Chest", [6] = "Waist", [7] = "Legs",
    [8] = "Feet", [9] = "Wrist", [10] = "Hands", [11] = "Finger", [12] = "Trinket", [13] = "One-Hand",
    [14] = "Off Hand", [15] = "Ranged", [16] = "Back", [17] = "Two-Hand", [18] = "Bag", [19] = "Tabard",
    [20] = "Chest", [21] = "Main Hand", [22] = "Off Hand", [23] = "Held In Off-hand", [25] = "Thrown",
    [26] = "Ranged", [28] = "Relic",
}
local WEAPON_TYPES = { [0] = "Axe", [1] = "Axe", [2] = "Bow", [3] = "Gun", [4] = "Mace", [5] = "Mace",
    [6] = "Polearm", [7] = "Sword", [8] = "Sword", [10] = "Staff", [13] = "Fist Weapon", [15] = "Dagger",
    [16] = "Thrown", [18] = "Crossbow", [19] = "Wand", [20] = "Fishing Pole" }
local ARMOR_TYPES = { [1] = "Cloth", [2] = "Leather", [3] = "Mail", [4] = "Plate", [6] = "Shield", [7] = "Libram",
    [8] = "Idol", [9] = "Totem", [10] = "Sigil" }
local RANKS = { [0] = "Normal", [1] = "Elite", [2] = "Rare Elite", [3] = "Boss", [4] = "Rare" }
local CREATURE_TYPES = { [1] = "Beast", [2] = "Dragonkin", [3] = "Demon", [4] = "Elemental", [5] = "Giant",
    [6] = "Undead", [7] = "Humanoid", [8] = "Critter", [9] = "Mechanical", [10] = "Not specified", [11] = "Totem",
    [12] = "Non-combat Pet", [13] = "Gas Cloud" }

-- A scrolling list on a page: rows are frames stacked in its child, the scrollbar on its right.
local lists = 0
local function List(page, top)
    lists = lists + 1
    local scroll = CreateFrame("ScrollFrame", "EmberforgeJournalList" .. lists, page, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 12, top)
    scroll:SetPoint("BOTTOMRIGHT", -30, 14)
    local child = CreateFrame("Frame", nil, scroll)
    child:SetWidth(10)
    child:SetHeight(10)
    scroll:SetScrollChild(child)
    scroll:SetScript("OnSizeChanged", function(self, width) child:SetWidth(width) end)
    return { scroll = scroll, child = child, rows = {} }
end

local function ListShown(list, shown)
    if shown then
        list.scroll:Show()
    else
        list.scroll:Hide()
    end
end

-- Stacks a list's rows (those shown) and sets its height.
local function Stack(list, count)
    local y = 0
    for index, row in ipairs(list.rows) do
        if index <= count then
            row:ClearAllPoints()
            row:SetPoint("TOPLEFT", list.child, "TOPLEFT", 0, -y)
            row:SetPoint("RIGHT", list.child, "RIGHT", 0, 0)
            row:Show()
            y = y + row:GetHeight() + 4
        else
            row:Hide()
        end
    end
    list.child:SetHeight(math.max(1, y))
    list.scroll:UpdateScrollChildRect()
end

-- A spell's description, read from a hidden tooltip (the client resolves its numbers).
local scan = CreateFrame("GameTooltip", "EmberforgeJournalScan", UIParent, "GameTooltipTemplate")
local function SpellDescription(spell)
    scan:SetOwner(WorldFrame, "ANCHOR_NONE")
    scan:ClearLines()
    scan:SetHyperlink("spell:" .. spell)
    local lines = scan:NumLines()
    local text = lines > 1 and _G["EmberforgeJournalScanTextLeft" .. lines]:GetText()
    scan:Hide()
    return text
end

do
    local book = CreateFrame("Frame", nil, frame)
    book:SetPoint("TOPLEFT", 16, -64)
    book:SetPoint("BOTTOMRIGHT", -44, 16)
    Box(book, { 0.16, 0.09, 0.04, 1 }, { 0.55, 0.38, 0.18, 1 })
    ui.book = book
    local pages = {}
    for index, side in ipairs({ "LEFT", "RIGHT" }) do
        local page = CreateFrame("Frame", nil, book)
        page:SetPoint("TOP", 0, -8)
        page:SetPoint("BOTTOM", 0, 8)
        page:SetPoint(side, side == "LEFT" and 8 or -8, 0)
        page:SetPoint(side == "LEFT" and "RIGHT" or "LEFT", book, "CENTER", side == "LEFT" and -3 or 3, 0)
        local paper = page:CreateTexture(nil, "BACKGROUND")
        paper:SetTexture(PARCHMENT)
        paper:SetAllPoints()
        paper:SetVertexColor(1, 0.94, 0.82)
        local shade = page:CreateTexture(nil, "BORDER")
        shade:SetTexture(1, 1, 1, 1)
        shade:SetWidth(36)
        shade:SetPoint("TOP")
        shade:SetPoint("BOTTOM")
        shade:SetPoint(side == "LEFT" and "RIGHT" or "LEFT")
        if side == "LEFT" then
            shade:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0.45)
        else
            shade:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.45, 0, 0, 0, 0)
        end
        pages[index] = page
    end
    ui.left, ui.right = pages[1], pages[2]

    -- Left page, the instance: its plate (its art when there is any, else its expansion's colors), then its details.
    local plate = CreateFrame("Frame", nil, ui.left)
    plate:SetHeight(170)
    plate:SetPoint("TOPLEFT", 14, -14)
    plate:SetPoint("TOPRIGHT", -14, -14)
    plate:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 } })
    plate:SetBackdropBorderColor(0.8, 0.62, 0.3, 1)
    plate.fill = plate:CreateTexture(nil, "BACKGROUND")
    plate.fill:SetPoint("TOPLEFT", 4, -4)
    plate.fill:SetPoint("BOTTOMRIGHT", -4, 4)
    plate.name = plate:CreateFontString(nil, "ARTWORK")
    plate.name:SetFont("Fonts\\MORPHEUS.TTF", 24)
    plate.name:SetShadowOffset(2, -2)
    plate.name:SetTextColor(1, 0.86, 0.45)
    plate.name:SetPoint("TOPLEFT", 12, -12)
    plate.name:SetPoint("TOPRIGHT", -12, -12)
    plate.sub = plate:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    plate.sub:SetPoint("BOTTOM", 0, 12)
    ui.plate = plate
    local about = ui.left:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    about:SetPoint("TOPLEFT", plate, "BOTTOMLEFT", 6, -12)
    about:SetPoint("RIGHT", ui.left, "RIGHT", -20, 0)
    about:SetJustifyH("LEFT")
    about:SetJustifyV("TOP")
    about:SetSpacing(4)
    about:SetTextColor(unpack(INK))
    about:SetShadowOffset(0, 0)
    ui.about = about

    -- Left page, the boss: its model (drag to turn it), its name, then its details.
    local model = CreateFrame("PlayerModel", nil, ui.left)
    model:SetPoint("TOPLEFT", 14, -14)
    model:SetPoint("TOPRIGHT", -14, -14)
    model:SetHeight(250)
    model:EnableMouse(true)
    model:SetScript("OnMouseDown", function(self, button)
        if button == "LeftButton" then
            self.turning, self.lastX = true, GetCursorPosition()
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
    local backdrop = ui.left:CreateTexture(nil, "BORDER")
    backdrop:SetTexture(1, 1, 1, 1)
    backdrop:SetAllPoints(model)
    backdrop:SetGradientAlpha("VERTICAL", 0.12, 0.07, 0.03, 0.55, 0.35, 0.22, 0.1, 0.25)
    local bossName = model:CreateFontString(nil, "OVERLAY")
    bossName:SetFont("Fonts\\MORPHEUS.TTF", 22)
    bossName:SetShadowOffset(2, -2)
    bossName:SetTextColor(1, 0.86, 0.45)
    bossName:SetPoint("BOTTOM", 0, 8)
    local bossInfo = ui.left:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    bossInfo:SetPoint("TOPLEFT", model, "BOTTOMLEFT", 6, -12)
    bossInfo:SetPoint("RIGHT", ui.left, "RIGHT", -20, 0)
    bossInfo:SetJustifyH("LEFT")
    bossInfo:SetJustifyV("TOP")
    bossInfo:SetSpacing(4)
    bossInfo:SetTextColor(unpack(INK))
    bossInfo:SetShadowOffset(0, 0)
    ui.model, ui.modelBack, ui.bossName, ui.bossInfo = model, backdrop, bossName, bossInfo

    -- Right page: its title and the difficulty buttons, then a list.
    local header = ui.right:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
    header:SetPoint("TOPLEFT", 16, -16)
    header:SetPoint("RIGHT", ui.right, "RIGHT", -170, 0)
    header:SetJustifyH("LEFT")
    header:SetTextColor(unpack(INK))
    header:SetShadowOffset(0, 0)
    ui.header = header
    ui.difficultyButtons = {}
    for index = 1, 4 do
        local button = CreateFrame("Button", nil, ui.right)
        button:SetWidth(36)
        button:SetHeight(22)
        button:SetPoint("TOPRIGHT", -16 - (index - 1) * 40, -14)
        Box(button, CELL_FILL, CELL_EDGE)
        button.text = button:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
        button.text:SetPoint("CENTER")
        button:SetScript("OnClick", function(self)
            view.difficulty = self.index
            Refresh()
        end)
        ui.difficultyButtons[index] = button
    end
    ui.bossList = List(ui.right, -46)
    ui.lootList = List(ui.right, -46)
    ui.abilityList = List(ui.right, -46)
    ui.empty = ui.right:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    ui.empty:SetPoint("TOP", 0, -80)
    ui.empty:SetTextColor(unpack(INK))
    ui.empty:SetShadowOffset(0, 0)

    -- The tabs on the book's right edge: Bosses (Abilities on a boss's page) and Loot.
    ui.sideTabs = {}
    for index, info in ipairs({ { "bosses", "Interface\\Icons\\INV_Misc_Bone_HumanSkull_01" },
        { "loot", "Interface\\Icons\\INV_Box_02" } }) do
        local tab = CreateFrame("Button", nil, book)
        tab:SetWidth(40)
        tab:SetHeight(44)
        tab:SetPoint("TOPLEFT", book, "TOPRIGHT", -2, -20 - (index - 1) * 50)
        Box(tab, CELL_FILL, CELL_EDGE)
        local icon = tab:CreateTexture(nil, "ARTWORK")
        icon:SetTexture(info[2])
        icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
        icon:SetWidth(28)
        icon:SetHeight(28)
        icon:SetPoint("CENTER", 1, 0)
        tab.icon = icon
        tab.key = info[1]
        tab:SetScript("OnClick", function(self)
            view.tab = self.key
            Refresh()
        end)
        tab:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(self.key == "loot" and "Loot" or (view.page == "boss" and "Abilities" or "Bosses"))
            GameTooltip:Show()
        end)
        tab:SetScript("OnLeave", function() GameTooltip:Hide() end)
        ui.sideTabs[index] = tab
    end
end

-- Rows.
local function BossRow(index)
    local list = ui.bossList
    local row = list.rows[index]
    if row then
        return row
    end
    row = CreateFrame("Button", nil, list.child)
    row:SetHeight(BOSS_HEIGHT)
    row.bar = row:CreateTexture(nil, "BACKGROUND")
    row.bar:SetTexture(BAR)
    row.bar:SetTexCoord(0, 1, 0, 0.75)
    row.bar:SetPoint("TOPLEFT", 22, -4)
    row.bar:SetPoint("BOTTOMRIGHT", 0, 4)
    local back = row:CreateTexture(nil, "BORDER")
    back:SetTexture(0, 0, 0, 0.9)
    back:SetPoint("LEFT", 3, 0)
    back:SetWidth(BOSS_HEIGHT - 6)
    back:SetHeight(BOSS_HEIGHT - 6)
    local portrait = CreateFrame("PlayerModel", nil, row)
    portrait:SetAllPoints(back)
    row.portrait = portrait
    local ring = portrait:CreateTexture(nil, "OVERLAY")
    ring:SetTexture(ART .. "Ring")
    ring:SetWidth(BOSS_HEIGHT + 2)
    ring:SetHeight(BOSS_HEIGHT + 2)
    ring:SetPoint("CENTER")
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    row.name:SetPoint("LEFT", portrait, "RIGHT", 14, 0)
    row.name:SetPoint("RIGHT", -10, 0)
    row.name:SetJustifyH("LEFT")
    row.name:SetTextColor(1, 0.95, 0.85)
    local hover = row:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.8, 0.4, 0.15)
    hover:SetAllPoints(row.bar)
    row:SetScript("OnClick", function(self)
        view.page, view.boss, view.tab = "boss", self.index, "bosses"
        Refresh()
    end)
    list.rows[index] = row
    return row
end

local function LootRow(index)
    local list = ui.lootList
    local row = list.rows[index]
    if row then
        return row
    end
    row = CreateFrame("Button", nil, list.child)
    row:SetHeight(40)
    local bar = row:CreateTexture(nil, "BACKGROUND")
    bar:SetTexture(BAR)
    bar:SetTexCoord(0, 1, 0, 0.75)
    bar:SetPoint("TOPLEFT", 40, -1)
    bar:SetPoint("BOTTOMRIGHT", 0, 1)
    bar:SetVertexColor(0.85, 0.8, 0.7)
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(36)
    row.icon:SetHeight(36)
    row.icon:SetPoint("LEFT", 2, 0)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    row.name:SetPoint("TOPLEFT", row.icon, "TOPRIGHT", 8, -2)
    row.name:SetPoint("RIGHT", -8, 0)
    row.name:SetJustifyH("LEFT")
    row.slot = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.slot:SetPoint("BOTTOMLEFT", row.icon, "BOTTOMRIGHT", 8, 2)
    row.kind = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.kind:SetPoint("BOTTOMRIGHT", -8, 3)
    local hover = row:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.8, 0.4, 0.15)
    hover:SetAllPoints(bar)
    row:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetHyperlink("item:" .. self.item)
        GameTooltip:Show()
    end)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    row:SetScript("OnClick", function(self)
        if IsModifiedClick("CHATLINK") then
            local _, link = GetItemInfo(self.item)
            if link then
                ChatEdit_InsertLink(link)
            end
        elseif IsModifiedClick("DRESSUP") then
            DressUpItemLink("item:" .. self.item)
        end
    end)
    list.rows[index] = row
    return row
end

local function AbilityRow(index)
    local list = ui.abilityList
    local row = list.rows[index]
    if row then
        return row
    end
    row = CreateFrame("Button", nil, list.child)
    local bar = row:CreateTexture(nil, "BACKGROUND")
    bar:SetTexture(BAR)
    bar:SetTexCoord(0, 1, 0, 0.75)
    bar:SetPoint("TOPLEFT")
    bar:SetPoint("RIGHT")
    bar:SetHeight(28)
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(22)
    row.icon:SetHeight(22)
    row.icon:SetPoint("TOPLEFT", 4, -3)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 8, 0)
    row.name:SetTextColor(1, 0.95, 0.85)
    row.text = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    row.text:SetPoint("TOPLEFT", 10, -34)
    row.text:SetPoint("RIGHT", -6, 0)
    row.text:SetJustifyH("LEFT")
    row.text:SetJustifyV("TOP")
    row.text:SetTextColor(unpack(INK))
    row.text:SetShadowOffset(0, 0)
    row:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetHyperlink("spell:" .. self.spell)
        GameTooltip:Show()
    end)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    list.rows[index] = row
    return row
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

local function Chosen(instance)
    return instance.difficulties[view.difficulty] or instance.difficulties[1]
end

local function Bosses(instance)
    local chosen = Chosen(instance)
    local bosses = chosen and data.bosses[instance.map .. ":" .. chosen.difficulty]
    if not bosses then
        for _, difficulty in ipairs(instance.difficulties) do
            bosses = bosses or data.bosses[instance.map .. ":" .. difficulty.difficulty]
        end
    end
    return bosses or {}
end

-- The chosen difficulty's details (bosses' info, abilities, loot), asked for once.
local function Details(instance)
    local chosen = Chosen(instance)
    if not chosen then
        return nil
    end
    local key = instance.map .. ":" .. chosen.difficulty
    local details = data.details[key]
    if not details then
        details = { info = {}, abilities = {}, loot = {}, ready = false }
        data.details[key] = details
        Send("BOSS," .. instance.map .. "," .. chosen.difficulty)
    end
    return details
end

local function FillLoot(items)
    for index, item in ipairs(items) do
        local row = LootRow(index)
        row.item = item.id
        row.icon:SetTexture(GetItemIcon(item.id) or "Interface\\Icons\\INV_Misc_QuestionMark")
        local color = ITEM_QUALITY_COLORS[item.quality]
        row.name:SetText((color and color.hex or "") .. item.name .. "|r")
        row.slot:SetText(SLOT_NAMES[item.slot] or "")
        local kind = ""
        if item.class == 2 then
            kind = WEAPON_TYPES[item.subclass] or ""
        elseif item.class == 4 then
            kind = ARMOR_TYPES[item.subclass] or ""
        elseif item.class == 15 then
            kind = "Miscellaneous"
        elseif item.class == 9 then
            kind = "Recipe"
        end
        row.kind:SetText(kind)
    end
    Stack(ui.lootList, #items)
    return #items
end

local function FillAbilities(spells)
    local shown, seen = 0, {}
    for _, spell in ipairs(spells) do
        local name, _, icon = GetSpellInfo(spell)
        local text = name and not seen[name] and SpellDescription(spell)
        if text and text ~= "" then
            seen[name] = true
            shown = shown + 1
            local row = AbilityRow(shown)
            row.spell = spell
            row.icon:SetTexture(icon)
            row.name:SetText(name)
            row.text:SetText(text)
            row:SetHeight(40 + row.text:GetStringHeight())
        end
    end
    Stack(ui.abilityList, shown)
    return shown
end

local function RefreshBook()
    local instance = data.instances[view.instance]
    if not instance then
        view.page = "home"
        return
    end
    local details = Details(instance)
    local bosses = Bosses(instance)
    local bossPage = view.page == "boss" and bosses[view.boss]
    local colors = TINTS[instance.expansion]
    local art = EmberforgeJournalArt and EmberforgeJournalArt[instance.map]
    if art then
        ui.plate.fill:SetTexture(art)
        ui.plate.fill:SetTexCoord(0, 1, 0.15, 0.75)
        ui.plate.fill:SetVertexColor(1, 1, 1)
    else
        ui.plate.fill:SetTexture(1, 1, 1, 1)
        ui.plate.fill:SetTexCoord(0, 1, 0, 1)
        ui.plate.fill:SetGradientAlpha("VERTICAL", colors[2][1], colors[2][2], colors[2][3], 1,
            colors[1][1], colors[1][2], colors[1][3], 1)
    end
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

    -- Left page: the instance, or the boss.
    for _, part in ipairs({ ui.plate, ui.about }) do
        if bossPage then part:Hide() else part:Show() end
    end
    for _, part in ipairs({ ui.model, ui.modelBack, ui.bossInfo }) do
        if bossPage then part:Show() else part:Hide() end
    end
    local info = bossPage and details and details.info[view.boss]
    if bossPage then
        ShowCreature(ui.model, info and info.entry or bossPage.creature, false)
        ui.model.facing = 0
        ui.bossName:SetText(bossPage.name)
        if info then
            local health = info.health > 0 and (" " .. (info.health >= 1000000 and
                string.format("%.1fM", info.health / 1000000) or info.health)) or " -"
            ui.bossInfo:SetText("Level " .. info.level .. " " .. (RANKS[info.rank] or "") .. ", "
                .. (CREATURE_TYPES[info.type] or "") .. "\nHealth:" .. health)
        else
            ui.bossInfo:SetText("")
        end
    end

    -- Right page: its title, the difficulty buttons, and the tab's list.
    ui.header:SetText(bossPage and bossPage.name or instance.name)
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
    for _, tab in ipairs(ui.sideTabs) do
        local on = tab.key == view.tab
        BoxColor(tab, on and ON_FILL or CELL_FILL, on and ON_EDGE or CELL_EDGE)
        tab.icon:SetVertexColor(on and 1 or 0.6, on and 1 or 0.6, on and 1 or 0.6)
    end
    ListShown(ui.bossList, view.tab == "bosses" and not bossPage)
    ListShown(ui.abilityList, view.tab == "bosses" and bossPage)
    ListShown(ui.lootList, view.tab == "loot")
    ui.empty:SetText("")

    if view.tab == "bosses" and not bossPage then
        for index, boss in ipairs(bosses) do
            local row = BossRow(index)
            row.index = index
            row.name:SetText(boss.name)
            ShowCreature(row.portrait, boss.creature, true)
        end
        Stack(ui.bossList, #bosses)
        if #bosses == 0 then
            ui.empty:SetText("No bosses listed")
        end
    elseif not (details and details.ready) then
        ui.empty:SetText("Loading...")
        Stack(ui.lootList, 0)
        Stack(ui.abilityList, 0)
    elseif view.tab == "loot" then
        local items = {}
        for _, item in ipairs(details.loot) do
            if not bossPage or item.boss == view.boss then
                items[#items + 1] = item
            end
        end
        if FillLoot(items) == 0 then
            ui.empty:SetText("No loot listed")
        end
    else
        if FillAbilities(details.abilities[view.boss] or {}) == 0 then
            ui.empty:SetText("No abilities listed")
        end
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

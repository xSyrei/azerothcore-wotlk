-- Emberforge Journal: the Dungeon Journal, laid out as Blizzard's own (the Mists of Pandaria client's
-- Blizzard_EncounterJournal: its frames, sizes, anchors, fonts' colors and texture coordinates, its art). Open with
-- /journal or /dj.
--
-- - The window: the standard portrait frame (Interface\FrameGeneral: rock background, metal border, title bar, the
--   Journal's own portrait in the ring), a row of breadcrumbs (Home, the instance, the boss) and a search box, and
--   the content in a marble inset.
-- - Home: the expansion's background and name (click it to choose Classic, the Burning Crusade or Wrath), the
--   Dungeons and Raids tabs, and a grid of the instances' buttons (4 a row), each its Journal art (else its loading
--   screen) and name.
-- - An instance: the open book. Its left page: its title on the page's shadow, its bosses' buttons (the Journal's
--   portrait, else our head shot, else the model's face, else the default portrait). Its right page: its framed
--   picture with its name and story, or (by the tabs on the book's edge) its loot. The difficulty button at the top
--   right of the book.
-- - A boss (its button lit on the left page; clicked again, back to the instance): the right page, by the tabs, its
--   abilities (the Journal's: headings, icons, flags (tank, healer, heroic, interruptible...) and texts; else the
--   spells its script casts), or its loot.
-- A wing of a map (Scarlet Monastery's Cathedral...) lists its own bosses (the server's W). The lists scroll, their
-- bars showing only when they do. Data.lua and Art/ come from tools/journal/mop_journal.py (the Journal's texts,
-- art and frame), Headshots.lua and Art/EF-BOSS-* from tools/journal/portraits.py.
--
-- The server (mod-titan, src/journal.cpp) sends the data as addon whispers to the player, prefix EFDJ:
--   to server     LIST
--   from server   B   D,<lfg id>~<map>~<type>~<expansion>~<difficulty>~<players>~<creature min>~<creature max>
--                       ~<queue min>~<queue max>~<seasonal>~<name>;...
--                     F,<map>~<difficulty>~<boss>~<creature entry>;...   W,<lfg id>~<first boss>~<last boss>;...   Z
--   to server     BOSS,<map>,<difficulty>
--   from server   I,<boss>~<entry>~<level>~<rank>~<health>~<type>;...   A,<boss>~<spell>;...
--                 L,<boss>~<item>~<quality>~<slot>~<class>~<subclass>~<name>;...   Y,<map>~<difficulty>

local PREFIX = "EFDJ"
local JOURNAL_ART = "Interface\\AddOns\\Emberforge_Journal\\Art\\"
local FLAG_ICONS = JOURNAL_ART .. "UI-EJ-Icons"
local LOADING_SCREENS = "Interface\\Glues\\LoadingScreens\\"
-- A loading screen's picture: between its bands (the top one's logo reaching down to 0.27 of it), on a 4:3 screen.
local SCREEN_TOP, SCREEN_BOTTOM = 0.28, 0.86
local FLAG_NAMES = { "Tank alert", "Damage dealer alert", "Healer alert", "Heroic difficulty", "Deadly",
    "Important", "Interruptible", "Magic effect", "Curse effect", "Poison effect", "Disease effect", "Enrage" }
local EXPANSIONS = { [0] = "Classic", [1] = "Burning Crusade", [2] = "Wrath of the Lich King" }
local EXPANSION_ART = { [0] = "UI-EJ-Classic", [1] = "UI-EJ-BurningCrusade", [2] = "UI-EJ-WrathoftheLichKing" }
local TYPES = { [1] = "Dungeon", [2] = "Raid" }
local WIDTH, HEIGHT = 800, 496                  -- EncounterJournal
local BOSS_HEIGHT, BOSS_GAP = 55, 15            -- EncounterBossButtonTemplate, and their spacing
local COLUMNS, TILE_WIDTH, TILE_HEIGHT, TILE_GAP = 4, 174, 96, 15
local FONT_MORPHEUS = "Fonts\\MORPHEUS.TTF"
-- Blizzard's colors for the book: the page titles (on the pages' header shadows), the ink, the boss buttons' text.
local HEADER_TEXT = { 0.902, 0.788, 0.671 }
local INK = { 0.13, 0.07, 0.01 }
local DARK = { 0.25, 0.1484375, 0.02 }
local BOSS_TEXT, BOSS_TEXT_HOVER = { 0.827, 0.659, 0.463 }, { 0.973, 0.902, 0.581 }
local TIER_TEXT = { 0.961, 0.824, 0.412 }
local BUTTON_TEXT = { 0.973, 0.902, 0.581 }

-- The game's encounter names (DungeonEncounter.dbc) shown as the boss is known.
local NAMES = { ["Overlord Tyrannus"] = "Scourgelord Tyrannus" }

-- Blizzard's texture pieces: { file, left, right, top, bottom, file width, file height } (the coordinates as
-- Blizzard_EncounterJournal.xml and SharedUIPanelTemplates.xml give them).
local SHEET, TILE = "UI-ENCOUNTERJOURNALTEXTURES", "UI-EncounterJournalTextures_Tile"
local A = {
    BossUp = { SHEET, 0.00195313, 0.63671875, 0.21386719, 0.26757813 },
    BossDown = { SHEET, 0.00195313, 0.63671875, 0.10253906, 0.15625000 },
    BossHighlight = { SHEET, 0.00195313, 0.63671875, 0.15820313, 0.21191406 },
    DungeonUp = { SHEET, 0.00195313, 0.34179688, 0.42871094, 0.52246094 },
    DungeonDown = { SHEET, 0.00195313, 0.34179688, 0.33300781, 0.42675781 },
    DungeonHighlight = { SHEET, 0.34570313, 0.68554688, 0.33300781, 0.42675781 },
    DungeonNameBg = { SHEET, 0.34570313, 0.84570313, 0.42871094, 0.49121094 },
    TabUnselected = { SHEET, 0.25585938, 0.37890625, 0.90332031, 0.95898438 },
    TabSelected = { SHEET, 0.12890625, 0.25195313, 0.90332031, 0.95898438 },
    TabHighlight = { SHEET, 0.00195313, 0.12500000, 0.90332031, 0.95898438 },
    BossIconOn = { SHEET, 0.90234375, 0.99609375, 0.26953125, 0.31152344 },
    BossIconOff = { SHEET, 0.85546875, 0.94921875, 0.52441406, 0.56640625 },
    LootIconOn = { SHEET, 0.63281250, 0.72656250, 0.61816406, 0.66015625 },
    LootIconOff = { SHEET, 0.73046875, 0.82421875, 0.61816406, 0.66015625 },
    LeftPageHeader = { SHEET, 0, 0.755859375, 0.9599609375, 1 },
    RightPageHeader = { SHEET, 0.755859375, 0, 0.9599609375, 1 },
    LootFrame = { SHEET, 0.00195313, 0.62890625, 0.61816406, 0.66210938 },
    FilterUp = { SHEET, 0.90039063, 0.95117188, 0.04980469, 0.07519531 },
    FilterDown = { SHEET, 0.63476563, 0.68554688, 0.06738281, 0.09277344 },
    FilterHighlight = { SHEET, 0.72656250, 0.77734375, 0.06738281, 0.09277344 },
    GridTabLeft = { SHEET, 0.81835938, 0.84179688, 0.02246094, 0.05761719 },
    GridTabRight = { SHEET, 0.84570313, 0.86914063, 0.05957031, 0.09472656 },
    GridTabLeftOn = { SHEET, 0.84570313, 0.86914063, 0.02246094, 0.05761719 },
    GridTabRightOn = { SHEET, 0.87304688, 0.89648438, 0.05957031, 0.09472656 },
    GridTabLeftHi = { SHEET, 0.81835938, 0.84179688, 0.05957031, 0.09472656 },
    GridTabRightHi = { SHEET, 0.87304688, 0.89648438, 0.02246094, 0.05761719 },
    PaperLeft = { SHEET, 0.81445313, 0.93945313, 0.39453125, 0.42285156 },
    PaperRight = { SHEET, 0.34570313, 0.47070313, 0.49316406, 0.52148438 },
    AbilityTextBG = { SHEET, 0.00195313, 0.50195313, 0.02246094, 0.10058594 },
    AbilityIconBorder = { SHEET, 0.00195313, 0.04101563, 0.00097656, 0.02050781 },
    GridTabMid = { TILE, 0, 1, 0.67382813, 0.74414063 },
    GridTabMidOn = { TILE, 0, 1, 0.59960938, 0.66992188 },
    GridTabMidHi = { TILE, 0, 1, 0.52539063, 0.59570313 },
    PaperMid = { TILE, 0, 1, 0.22265625, 0.27929688 },
    FilterMid = { TILE, 0, 1, 0.00195313, 0.05273438 },
    FilterMidDown = { TILE, 0, 1, 0.05664063, 0.10742188 },
    -- The standard window frame (Interface\FrameGeneral).
    Portrait = { "UI-Frame", 0.00781250, 0.61718750, 0.00781250, 0.61718750 },
    TopCornerRight = { "UI-Frame", 0.63281250, 0.89062500, 0.00781250, 0.26562500 },
    BotCornerLeft = { "UI-Frame", 0.00781250, 0.11718750, 0.63281250, 0.74218750 },
    BotCornerRight = { "UI-Frame", 0.13281250, 0.21875000, 0.89843750, 0.98437500 },
    InnerTopLeft = { "UI-Frame", 0.63281250, 0.67968750, 0.54687500, 0.59375000 },
    InnerTopRight = { "UI-Frame", 0.90625000, 0.95312500, 0.21875000, 0.26562500 },
    InnerBotLeft = { "UI-Frame", 0.69531250, 0.74218750, 0.54687500, 0.59375000 },
    InnerBotRight = { "UI-Frame", 0.75781250, 0.80468750, 0.54687500, 0.59375000 },
    TitleTile = { "_UI-Frame", 0, 1, 0.43750000, 0.65625000 },
    TitleTileBg = { "_UI-Frame", 0, 1, 0.28906250, 0.42187500 },
    TopTileStreaks = { "_UI-Frame", 0, 1, 0.67187500, 0.96093750 },
    Bot = { "_UI-Frame", 0, 1, 0.20312500, 0.27343750 },
    InnerTop = { "_UI-Frame", 0, 1, 0.08593750, 0.10937500 },
    InnerBot = { "_UI-Frame", 0, 1, 0.00781250, 0.03125000 },
    LeftTile = { "!UI-Frame", 0.35937500, 0.60937500, 0, 1 },
    RightTile = { "!UI-Frame", 0.17187500, 0.32812500, 0, 1 },
    InnerLeft = { "!UI-Frame", 0.09375000, 0.14062500, 0, 1 },
    InnerRight = { "!UI-Frame", 0.01562500, 0.06250000, 0, 1 },
}
local FILE_SIZE = { [SHEET] = { 512, 1024 }, [TILE] = { 64, 512 }, ["UI-Frame"] = { 128, 128 },
    ["_UI-Frame"] = { 256, 128 }, ["!UI-Frame"] = { 64, 256 } }

-- A texture of a piece, at its own size (its coordinates times its file's size) unless sized after.
local function Tex(parent, layer, name, blend)
    local piece = A[name]
    local texture = parent:CreateTexture(nil, layer)
    texture:SetTexture(JOURNAL_ART .. piece[1])
    texture:SetTexCoord(piece[2], piece[3], piece[4], piece[5])
    local size = FILE_SIZE[piece[1]]
    texture:SetWidth(math.abs(piece[3] - piece[2]) * size[1])
    texture:SetHeight(math.abs(piece[5] - piece[4]) * size[2])
    if blend then
        texture:SetBlendMode(blend)
    end
    return texture
end

-- A piece made a button's normal, pushed or highlight texture, filling the button.
local function ButtonTex(button, setter, name)
    local texture = Tex(button, setter == "SetHighlightTexture" and "HIGHLIGHT" or "ARTWORK", name)
    texture:SetAllPoints()
    button[setter](button, texture)
    return texture
end

local function Font(text, size, color, shadow)
    text:SetFont(FONT_MORPHEUS, size)
    if color then
        text:SetTextColor(unpack(color))
    end
    text:SetShadowOffset(shadow and 1 or 0, shadow and -1 or 0)
end

local data = { instances = {}, order = {}, bosses = {}, wings = {}, details = {}, loaded = false, loading = false }
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
        wipe(data.wings)
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
            boss = NAMES[boss] or boss
            local key = map .. ":" .. difficulty
            data.bosses[key] = data.bosses[key] or {}
            local list = data.bosses[key]
            -- index: its place in its map's list, as the server counts (its details come by it).
            tinsert(list, { name = boss, creature = tonumber(creature) or 0, index = #list + 1 })
        end
    elseif kind == "W" then
        -- A wing (a dungeon of the finder that is part of a map): its bosses, by their indexes.
        for entry in message:sub(3):gmatch("[^;]+") do
            local id, first, last = strsplit("~", entry)
            data.wings[tonumber(id)] = { first = tonumber(first), last = tonumber(last) }
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
-- A creature's model in a frame (PlayerModel:SetCreature); portrait: the face camera.
-- A humanoid's skin (its baked texture) can load after its model: drawn at once, it is white. So the model is set
-- once more a moment later (setting it again restarts its animation: only once).
local RELOADS = { 0.5 }
local pending = {}
local function SetModel(model, creature, portrait)
    pcall(model.SetCreature, model, creature)
    if portrait and model.SetCamera then
        pcall(model.SetCamera, model, 0)
    end
end
local reloader = CreateFrame("Frame")
reloader:SetScript("OnUpdate", function(_, elapsed)
    for model, state in pairs(pending) do
        state.time = state.time + elapsed
        if state.time >= RELOADS[state.step] then
            if model:IsVisible() then
                SetModel(model, state.creature, state.portrait)
            end
            state.step = state.step + 1
            if not RELOADS[state.step] then
                pending[model] = nil
            end
        end
    end
end)

-- Set only when it changes: setting a model again restarts it.
local function ShowCreature(model, creature, portrait)
    if model.shown == creature then
        return
    end
    model.shown = creature
    model:ClearModel()
    pending[model] = nil
    if creature and creature > 0 and model.SetCreature then
        SetModel(model, creature, portrait)
        pending[model] = { creature = creature, portrait = portrait, time = 0, step = 1 }
    end
end
-- The Journal's data (Data.lua) by the server's names: a boss's key is its name, lowercase, letters and digits only;
-- a name that is not a key matches one that holds it (or that it holds).
local function Key(name)
    return ((name or ""):lower():gsub("[^%w]", ""))
end

local function JournalBoss(map, name)
    local list = EmberforgeJournalBosses and EmberforgeJournalBosses[map]
    if not list then
        return nil
    end
    local key = Key(name)
    if list[key] then
        return list[key]
    end
    if #key >= 5 then
        for other, boss in pairs(list) do
            if #other >= 5 and (other:find(key, 1, true) or key:find(other, 1, true)) then
                return boss
            end
        end
    end
end

-- A boss's portrait: the Journal's own, else our head shot of it (Headshots.lua, by tools/journal/portraits.py).
local function Portrait(map, name)
    local boss = JournalBoss(map, name)
    local file = boss and boss.p or (EmberforgeJournalPortraits and EmberforgeJournalPortraits[Key(name)])
        or (EmberforgeJournalHeadshots and EmberforgeJournalHeadshots[Key(name)])
    return file and (JOURNAL_ART .. file)
end

local function InstanceArt(map)
    return EmberforgeJournalArt and EmberforgeJournalArt[map]
end

-- An instance with no Journal art: its loading screen (the game's own), else the game's plain dungeon or raid one.
local function LoadingScreen(instance, art)
    local name = art and art.screen or (instance.type == 2 and "LoadScreenRaid" or "LoadScreenDungeon")
    return LOADING_SCREENS .. name
end
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

-- The chosen difficulty's bosses; a wing's own only (Scarlet Monastery's Cathedral: its three).
local function Bosses(instance)
    local chosen = Chosen(instance)
    local bosses = chosen and data.bosses[instance.map .. ":" .. chosen.difficulty]
    if not bosses then
        for _, difficulty in ipairs(instance.difficulties) do
            bosses = bosses or data.bosses[instance.map .. ":" .. difficulty.difficulty]
        end
    end
    local wing = chosen and data.wings[chosen.id]
    if not (bosses and wing) then
        return bosses or {}
    end
    local own = {}
    for _, boss in ipairs(bosses) do
        if boss.index >= wing.first and boss.index <= wing.last then
            own[#own + 1] = boss
        end
    end
    return own
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
-- Scrolling lists and texts --------------------------------------------------------------------------------------
-- A scroll frame's bar shows only when its child is taller than it.
local function FitScrollBar(scroll, child)
    local bar = _G[scroll:GetName() .. "ScrollBar"]
    if not bar then
        return
    end
    if child:GetHeight() > scroll:GetHeight() + 2 then
        bar:Show()
    else
        scroll:SetVerticalScroll(0)
        bar:Hide()
    end
end

-- A scrolling list (rows stacked in its child, gap pixels apart, the first `top` below its top), placed by `place`.
local lists = 0
local function List(parent, place, gap, top)
    lists = lists + 1
    local scroll = CreateFrame("ScrollFrame", "EmberforgeJournalList" .. lists, parent, "UIPanelScrollFrameTemplate")
    scroll.scrollBarHideable = 1
    place(scroll)
    local child = CreateFrame("Frame", nil, scroll)
    child:SetWidth(10)
    child:SetHeight(10)
    scroll:SetScrollChild(child)
    scroll:SetScript("OnSizeChanged", function(self, width) child:SetWidth(width) end)
    return { scroll = scroll, child = child, rows = {}, gap = gap or 4, top = top or 0 }
end

local function ListShown(list, shown)
    if shown then
        list.scroll:Show()
    else
        list.scroll:Hide()
    end
end

local function Stack(list, count)
    local y = list.top
    for index, row in ipairs(list.rows) do
        if index <= count then
            row:ClearAllPoints()
            row:SetPoint("TOPLEFT", list.child, "TOPLEFT", row.indent or 0, -y)
            if not row.fixedWidth then
                row:SetPoint("RIGHT", list.child, "RIGHT", 0, 0)
            end
            row:Show()
            y = y + row:GetHeight() + list.gap
        else
            row:Hide()
        end
    end
    list.child:SetHeight(math.max(1, y))
    list.scroll:UpdateScrollChildRect()
    FitScrollBar(list.scroll, list.child)
end

-- A scrolling text in ink: its SetBody sets the text.
local function TextScroll(parent, place, color)
    lists = lists + 1
    local scroll = CreateFrame("ScrollFrame", "EmberforgeJournalText" .. lists, parent, "UIPanelScrollFrameTemplate")
    scroll.scrollBarHideable = 1
    place(scroll)
    local child = CreateFrame("Frame", nil, scroll)
    child:SetWidth(10)
    child:SetHeight(10)
    scroll:SetScrollChild(child)
    local text = child:CreateFontString(nil, "ARTWORK", "GameFontBlack")
    text:SetPoint("TOPLEFT", 5, -5)
    text:SetJustifyH("LEFT")
    text:SetJustifyV("TOP")
    text:SetSpacing(2)
    text:SetTextColor(unpack(color or INK))
    local function Fit()
        local width = scroll:GetWidth()
        if width and width > 0 then
            child:SetWidth(width)
            text:SetWidth(width - 10)
        end
        child:SetHeight(math.max(1, text:GetStringHeight() + 12))
        scroll:UpdateScrollChildRect()
        FitScrollBar(scroll, child)
    end
    scroll:SetScript("OnSizeChanged", Fit)
    scroll.SetBody = function(self, body)
        text:SetText(body)
        Fit()
        self:SetVerticalScroll(0)
    end
    scroll.text = text
    return scroll
end

-- The window: the standard portrait frame (SharedUIPanelTemplates' PortraitFrameTemplate) ---------------------------
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
frame:Hide()
tinsert(UISpecialFrames, "EmberforgeJournal")

local ui = {}       -- the window's parts, in a table (the main chunk's locals are limited)

do
    local bg = frame:CreateTexture(nil, "BACKGROUND")
    bg:SetTexture(JOURNAL_ART .. "UI-Background-Rock")
    bg:SetPoint("TOPLEFT", 2, -21)
    bg:SetPoint("BOTTOMRIGHT", -2, 2)
    local titleBg = Tex(frame, "BACKGROUND", "TitleTileBg")
    titleBg:SetPoint("TOPLEFT", 2, -3)
    titleBg:SetPoint("TOPRIGHT", -25, -3)

    local portrait = frame:CreateTexture(nil, "OVERLAY")
    portrait:SetWidth(60)
    portrait:SetHeight(60)
    portrait:SetPoint("TOPLEFT", -6, 7)
    SetPortraitToTexture(portrait, JOURNAL_ART .. "UI-EJ-PortraitIcon")
    local portraitFrame = Tex(frame, "OVERLAY", "Portrait")
    portraitFrame:SetPoint("TOPLEFT", -14, 11)
    local topRight = Tex(frame, "OVERLAY", "TopCornerRight")
    topRight:SetPoint("TOPRIGHT", 0, 1)
    local top = Tex(frame, "OVERLAY", "TitleTile")
    top:SetPoint("TOPLEFT", portraitFrame, "TOPRIGHT", 0, -10)
    top:SetPoint("TOPRIGHT", topRight, "TOPLEFT")
    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", 0, -4)
    title:SetPoint("LEFT", 60, 0)
    title:SetPoint("RIGHT", -60, 0)
    title:SetText("Dungeon Journal")

    local streaks = Tex(frame, "BORDER", "TopTileStreaks")
    streaks:SetPoint("TOPLEFT", 0, -21)
    streaks:SetPoint("TOPRIGHT", -2, -21)
    local botLeft = Tex(frame, "BORDER", "BotCornerLeft")
    botLeft:SetPoint("BOTTOMLEFT", -6, -5)
    local botRight = Tex(frame, "BORDER", "BotCornerRight")
    botRight:SetPoint("BOTTOMRIGHT", 0, -5)
    local bottom = Tex(frame, "BORDER", "Bot")
    bottom:SetPoint("BOTTOMLEFT", botLeft, "BOTTOMRIGHT")
    bottom:SetPoint("BOTTOMRIGHT", botRight, "BOTTOMLEFT")
    local left = Tex(frame, "BORDER", "LeftTile")
    left:SetPoint("TOPLEFT", portraitFrame, "BOTTOMLEFT", 8, 0)
    left:SetPoint("BOTTOMLEFT", botLeft, "TOPLEFT")
    local right = Tex(frame, "BORDER", "RightTile")
    right:SetPoint("TOPRIGHT", topRight, "BOTTOMRIGHT", 1, 0)
    right:SetPoint("BOTTOMRIGHT", botRight, "TOPRIGHT")

    local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", 4, 5)

    -- The inset: marble, its thin inner border (InsetFrameTemplate).
    local inset = CreateFrame("Frame", nil, frame)
    inset:SetPoint("TOPRIGHT", -4, -60)
    inset:SetPoint("BOTTOMLEFT", 4, 5)
    local marble = inset:CreateTexture(nil, "BACKGROUND")
    marble:SetTexture(JOURNAL_ART .. "UI-Background-Marble")
    marble:SetAllPoints()
    local corners = {}
    for name, point in pairs({ InnerTopLeft = "TOPLEFT", InnerTopRight = "TOPRIGHT", InnerBotLeft = "BOTTOMLEFT",
        InnerBotRight = "BOTTOMRIGHT" }) do
        corners[name] = Tex(inset, "BORDER", name)
        corners[name]:SetPoint(point)
    end
    -- Each edge: { piece, its first point, the corner and point it starts at, its second point, the corner and
    -- point it ends at }.
    local edges = {
        { "InnerTop", "TOPLEFT", "InnerTopLeft", "TOPRIGHT", "TOPRIGHT", "InnerTopRight", "TOPLEFT" },
        { "InnerBot", "BOTTOMLEFT", "InnerBotLeft", "BOTTOMRIGHT", "BOTTOMRIGHT", "InnerBotRight", "BOTTOMLEFT" },
        { "InnerLeft", "TOPLEFT", "InnerTopLeft", "BOTTOMLEFT", "BOTTOMLEFT", "InnerBotLeft", "TOPLEFT" },
        { "InnerRight", "TOPRIGHT", "InnerTopRight", "BOTTOMRIGHT", "BOTTOMRIGHT", "InnerBotRight", "TOPRIGHT" },
    }
    for _, edge in ipairs(edges) do
        local texture = Tex(inset, "BORDER", edge[1])
        texture:SetPoint(edge[2], corners[edge[3]], edge[4])
        texture:SetPoint(edge[5], corners[edge[6]], edge[7])
    end
    ui.inset = inset

    -- The breadcrumbs (where the Journal's navigation bar is): Home, the instance, the boss; each a button in the
    -- Journal's filter button art.
    ui.crumbs = {}
    for index = 1, 3 do
        local crumb = CreateFrame("Button", nil, frame)
        crumb:SetHeight(26)
        local leftCap, rightCap = Tex(crumb, "BACKGROUND", "FilterUp"), Tex(crumb, "BACKGROUND", "FilterUp")
        local piece = A.FilterUp
        leftCap:SetTexCoord(piece[2], (piece[2] + piece[3]) / 2, piece[4], piece[5])
        leftCap:SetWidth(13)
        leftCap:SetPoint("LEFT")
        rightCap:SetTexCoord((piece[2] + piece[3]) / 2, piece[3], piece[4], piece[5])
        rightCap:SetWidth(13)
        rightCap:SetPoint("RIGHT")
        local mid = Tex(crumb, "BACKGROUND", "FilterMid")
        mid:SetPoint("TOPLEFT", leftCap, "TOPRIGHT")
        mid:SetPoint("BOTTOMRIGHT", rightCap, "BOTTOMLEFT")
        local hover = Tex(crumb, "HIGHLIGHT", "FilterHighlight", "ADD")
        hover:SetAllPoints()
        crumb.text = crumb:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
        crumb.text:SetPoint("CENTER", 0, 0)
        crumb.text:SetTextColor(unpack(BUTTON_TEXT))
        crumb:SetPoint("TOPLEFT", index == 1 and frame or ui.crumbs[index - 1], index == 1 and "TOPLEFT" or "TOPRIGHT",
            index == 1 and 61 or 4, index == 1 and -27 or 0)
        crumb.level = index
        crumb:SetScript("OnClick", function(self)
            if self.level == 1 then
                view.page, view.instance, view.boss = "home", nil, nil
            elseif self.level == 2 then
                view.page, view.boss, view.tab = "instance", nil, "bosses"
            end
            Refresh()
        end)
        ui.crumbs[index] = crumb
    end

    local search = CreateFrame("EditBox", "EmberforgeJournalSearch", frame, "InputBoxTemplate")
    search:SetWidth(200)
    search:SetHeight(20)
    search:SetPoint("TOPRIGHT", -20, -30)
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

-- Home (the Journal's instance select) ------------------------------------------------------------------------------
do
    local home = CreateFrame("Frame", nil, frame)
    home:SetPoint("TOPLEFT", ui.inset, "TOPLEFT", 0, -2)
    home:SetPoint("BOTTOMRIGHT", ui.inset, "BOTTOMRIGHT", -3, 0)
    home.glow = home:CreateTexture(nil, "BACKGROUND")
    home.glow:SetTexCoord(0, 496 / 512, 0, 1)
    home.glow:SetAllPoints()
    ui.home = home

    -- The expansion's name, and the arrow opening the choice of expansions.
    local title = CreateFrame("Button", nil, home)
    title:SetHeight(34)
    title:SetWidth(360)
    title:SetPoint("TOPLEFT", 10, -10)
    title.text = title:CreateFontString(nil, "ARTWORK")
    Font(title.text, 28, TIER_TEXT, true)
    title.text:SetPoint("LEFT")
    local arrow = title:CreateTexture(nil, "ARTWORK")
    arrow:SetTexture("Interface\\ChatFrame\\UI-ChatIcon-ScrollDown-Up")
    arrow:SetWidth(28)
    arrow:SetHeight(28)
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

    -- The Dungeons and Raids tabs (EncounterTierTabTemplate: the grid tab's three pieces, chosen, under the mouse).
    ui.typeTabs = {}
    for index, kind in ipairs({ 2, 1 }) do
        local tab = CreateFrame("Button", nil, home)
        tab:SetWidth(116)
        tab:SetHeight(36)
        if index == 1 then
            tab:SetPoint("BOTTOMRIGHT", home, "TOPRIGHT", -35, -45)
        else
            tab:SetPoint("BOTTOMRIGHT", ui.typeTabs[1], "BOTTOMLEFT", -20, 0)
        end
        tab.parts = {}
        for _, state in ipairs({ "", "On" }) do
            local leftPiece, rightPiece = Tex(tab, "BACKGROUND", "GridTabLeft" .. state),
                Tex(tab, "BACKGROUND", "GridTabRight" .. state)
            leftPiece:SetPoint("LEFT")
            rightPiece:SetPoint("RIGHT")
            local mid = Tex(tab, "BACKGROUND", "GridTabMid" .. state)
            mid:SetPoint("TOPLEFT", leftPiece, "TOPRIGHT")
            mid:SetPoint("BOTTOMRIGHT", rightPiece, "BOTTOMLEFT")
            tab.parts[state] = { leftPiece, mid, rightPiece }
        end
        local hiLeft = Tex(tab, "HIGHLIGHT", "GridTabLeftHi", "ADD")
        local hiRight = Tex(tab, "HIGHLIGHT", "GridTabRightHi", "ADD")
        hiLeft:SetPoint("LEFT")
        hiRight:SetPoint("RIGHT")
        local hiMid = Tex(tab, "HIGHLIGHT", "GridTabMidHi", "ADD")
        hiMid:SetPoint("TOPLEFT", hiLeft, "TOPRIGHT")
        hiMid:SetPoint("BOTTOMRIGHT", hiRight, "BOTTOMLEFT")
        tab.label = tab:CreateFontString(nil, "ARTWORK", "GameFontNormal")
        tab.label:SetPoint("CENTER", 0, -1)
        tab.label:SetText(TYPES[kind] .. "s")
        tab.kind = kind
        tab:SetScript("OnClick", function(self)
            view.type = self.kind
            Refresh()
        end)
        ui.typeTabs[index] = tab
    end

    local scroll = CreateFrame("ScrollFrame", "EmberforgeJournalTiles", home, "UIPanelScrollFrameTemplate")
    scroll.scrollBarHideable = 1
    scroll:SetPoint("TOPLEFT", 5, -49)
    scroll:SetPoint("BOTTOMRIGHT", -26, 4)
    local grid = CreateFrame("Frame", nil, scroll)
    grid:SetWidth(COLUMNS * (TILE_WIDTH + TILE_GAP) + 12)
    grid:SetHeight(1)
    scroll:SetScrollChild(grid)
    ui.grid, ui.tiles, ui.tileScroll = grid, {}, scroll
end

-- An instance's button (EncounterInstanceButtonTemplate): its art, the Journal's frame over it, its name.
local function Tile(index)
    local tile = ui.tiles[index]
    if tile then
        return tile
    end
    tile = CreateFrame("Button", nil, ui.grid)
    tile:SetWidth(TILE_WIDTH)
    tile:SetHeight(TILE_HEIGHT)
    local column, row = (index - 1) % COLUMNS, math.floor((index - 1) / COLUMNS)
    tile:SetPoint("TOPLEFT", 12 + column * (TILE_WIDTH + TILE_GAP), -10 - row * (TILE_HEIGHT + TILE_GAP))
    tile.fill = tile:CreateTexture(nil, "BACKGROUND")
    tile.fill:SetAllPoints()
    ButtonTex(tile, "SetNormalTexture", "DungeonUp")
    ButtonTex(tile, "SetPushedTexture", "DungeonDown")
    ButtonTex(tile, "SetHighlightTexture", "DungeonHighlight")
    tile.name = tile:CreateFontString(nil, "OVERLAY")
    Font(tile.name, 17, { 1, 0.82, 0 }, true)
    tile.name:SetShadowColor(0, 0, 0, 1)
    tile.name:SetWidth(150)
    tile.name:SetPoint("TOP", 0, -15)
    tile:SetScript("OnClick", function(self)
        view.page, view.instance, view.boss, view.difficulty, view.tab = "instance", self.instance.key, nil, 1,
            "bosses"
        Refresh()
    end)
    ui.tiles[index] = tile
    return tile
end

local function RefreshHome()
    ui.title.text:SetText(EXPANSIONS[view.expansion])
    ui.title:SetWidth(ui.title.text:GetStringWidth() + 44)
    ui.home.glow:SetTexture(JOURNAL_ART .. EXPANSION_ART[view.expansion])
    for _, tab in ipairs(ui.typeTabs) do
        local on = tab.kind == view.type
        for state, pieces in pairs(tab.parts) do
            for _, piece in ipairs(pieces) do
                if (state == "On") == on then
                    piece:Show()
                else
                    piece:Hide()
                end
            end
        end
        tab.label:SetTextColor(on and 1 or 1, on and 1 or 0.82, on and 1 or 0)
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
        local art = InstanceArt(instance.map)
        if art and art.button then
            tile.fill:SetTexture(JOURNAL_ART .. art.button)
            tile.fill:SetTexCoord(0, 0.68359375, 0, 0.7421875)
        else
            tile.fill:SetTexture(LoadingScreen(instance, art))
            tile.fill:SetTexCoord(0.1, 0.9, SCREEN_TOP, SCREEN_BOTTOM)     -- cut to the button's shape
        end
        tile.name:SetText(instance.name)
        tile.instance = instance
        tile:Show()
    end
    for index = #shown + 1, #ui.tiles do
        ui.tiles[index]:Hide()
    end
    ui.grid:SetHeight(math.max(1, 10 + math.ceil(#shown / COLUMNS) * (TILE_HEIGHT + TILE_GAP)))
    ui.tileScroll:UpdateScrollChildRect()
    FitScrollBar(ui.tileScroll, ui.grid)
end

-- The book (the Journal's encounter frame) --------------------------------------------------------------------------
do
    local book = CreateFrame("Frame", nil, frame)
    book:SetWidth(785)
    book:SetHeight(425)
    book:SetPoint("BOTTOMRIGHT", ui.inset, "BOTTOMRIGHT", -4, 2)
    local pages = book:CreateTexture(nil, "BACKGROUND")
    pages:SetTexture(JOURNAL_ART .. "UI-EJ-JournalBG")
    pages:SetTexCoord(0, 0.766601562, 0, 0.830078125)
    pages:SetAllPoints()
    ui.book = book

    -- The pages' header shadows and titles: the instance's on the left, the boss's on the right.
    local leftShadow = Tex(book, "BORDER", "LeftPageHeader")
    leftShadow:SetPoint("TOPLEFT", 0, -11)
    local instanceTitle = book:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
    instanceTitle:SetWidth(340)
    instanceTitle:SetHeight(14)
    instanceTitle:SetJustifyH("LEFT")
    instanceTitle:SetPoint("TOPLEFT", 20, -20)
    instanceTitle:SetTextColor(unpack(HEADER_TEXT))
    local rightShadow = Tex(book, "BORDER", "RightPageHeader")
    rightShadow:SetPoint("TOPRIGHT", 0, -11)
    local encounterTitle = book:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
    encounterTitle:SetWidth(227)
    encounterTitle:SetHeight(14)
    encounterTitle:SetJustifyH("LEFT")
    encounterTitle:SetPoint("BOTTOMLEFT", book, "TOPRIGHT", -350, -36)
    encounterTitle:SetTextColor(unpack(HEADER_TEXT))
    ui.instanceTitle, ui.rightShadow, ui.encounterTitle = instanceTitle, rightShadow, encounterTitle

    -- Left page: the bosses' buttons, or the boss.
    ui.bossList = List(book, function(scroll)
        scroll:SetWidth(338)
        scroll:SetHeight(382)
        scroll:SetPoint("BOTTOMLEFT", 25, 1)
    end, BOSS_GAP, 14)

    -- Right page, the instance: its framed picture (the Journal's own size and place), its name, its story.
    local plate = CreateFrame("Frame", nil, book)
    plate:SetWidth(390)
    plate:SetHeight(425)
    plate:SetPoint("BOTTOMRIGHT", -1, 2)
    plate.fill = plate:CreateTexture(nil, "ARTWORK")
    plate.fill:SetWidth(390)
    plate.fill:SetHeight(336)
    plate.fill:SetPoint("TOP", 3, -9)
    -- A loading screen in the Journal's empty picture frame (the frame's hole: 25..364 by 37..295 of its 512).
    plate.screen = plate:CreateTexture(nil, "BORDER")
    plate.screen:SetPoint("TOPLEFT", plate.fill, "TOPLEFT", 25, -37)
    plate.screen:SetPoint("BOTTOMRIGHT", plate.fill, "TOPLEFT", 364, -295)
    local nameBg = Tex(plate, "OVERLAY", "DungeonNameBg")
    nameBg:SetPoint("TOP", plate.fill, "TOP", 0, -32)
    plate.name = plate:CreateFontString(nil, "OVERLAY")
    Font(plate.name, 24, { 1, 0.82, 0 }, true)
    plate.name:SetWidth(320)
    plate.name:SetPoint("CENTER", nameBg, "CENTER", 0, 2)
    ui.plate = plate
    ui.about = TextScroll(plate, function(scroll)
        scroll:SetWidth(315)
        scroll:SetHeight(92)
        scroll:SetPoint("BOTTOM", 0, 5)
    end, INK)

    -- Right page, a list: abilities, or loot.
    local function RightList(gap)
        return List(book, function(scroll)
            scroll:SetWidth(330)
            scroll:SetHeight(383)
            scroll:SetPoint("BOTTOMRIGHT", -26, 1)
        end, gap, 8)
    end
    ui.abilityList = RightList(4)
    ui.lootList = RightList(3)

    ui.empty = book:CreateFontString(nil, "ARTWORK", "GameFontBlack")
    ui.empty:SetPoint("TOP", book, "TOP", 195, -90)
    ui.empty:SetTextColor(unpack(DARK))

    -- The difficulty button, top right of the book (the Journal's filter button): a menu of the difficulties.
    local difficulty = CreateFrame("Button", nil, book)
    difficulty:SetWidth(101)
    difficulty:SetHeight(26)
    difficulty:SetPoint("TOPRIGHT", -19, -13)
    local piece = A.FilterUp
    local leftCap, rightCap = Tex(difficulty, "BACKGROUND", "FilterUp"), Tex(difficulty, "BACKGROUND", "FilterUp")
    leftCap:SetTexCoord(piece[2], (piece[2] + piece[3]) / 2, piece[4], piece[5])
    leftCap:SetWidth(13)
    leftCap:SetPoint("LEFT")
    rightCap:SetTexCoord((piece[2] + piece[3]) / 2, piece[3], piece[4], piece[5])
    rightCap:SetWidth(13)
    rightCap:SetPoint("RIGHT")
    local mid = Tex(difficulty, "BACKGROUND", "FilterMid")
    mid:SetPoint("TOPLEFT", leftCap, "TOPRIGHT")
    mid:SetPoint("BOTTOMRIGHT", rightCap, "BOTTOMLEFT")
    local hover = Tex(difficulty, "HIGHLIGHT", "FilterHighlight", "ADD")
    hover:SetAllPoints()
    difficulty.text = difficulty:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    difficulty.text:SetPoint("CENTER", 0, -1)
    difficulty.text:SetTextColor(unpack(BUTTON_TEXT))
    local menu = CreateFrame("Frame", "EmberforgeJournalDifficultyMenu", UIParent, "UIDropDownMenuTemplate")
    difficulty:SetScript("OnClick", function(self)
        local instance = data.instances[view.instance]
        if not instance then
            return
        end
        local items = {}
        for index, entry in ipairs(instance.difficulties) do
            items[#items + 1] = { text = DifficultyName(instance, entry), checked = view.difficulty == index,
                func = function()
                    view.difficulty = index
                    Refresh()
                end }
        end
        EasyMenu(items, menu, self, 0, 0, "MENU")
    end)
    ui.difficulty = difficulty

    -- The tabs on the book's right edge (EncounterTabTemplate): the overview (abilities on a boss's page), loot.
    ui.sideTabs = {}
    for index, info in ipairs({ { "bosses", "BossIcon", "Overview" }, { "loot", "LootIcon", "Loot" } }) do
        local tab = CreateFrame("Button", nil, book)
        tab:SetWidth(63)
        tab:SetHeight(57)
        if index == 1 then
            tab:SetPoint("TOPLEFT", book, "TOPRIGHT", -12, -35)
        else
            tab:SetPoint("TOP", ui.sideTabs[index - 1], "BOTTOM", 0, 2)
        end
        tab.off = Tex(tab, "BACKGROUND", "TabUnselected")
        tab.off:SetAllPoints()
        tab.on = Tex(tab, "BACKGROUND", "TabSelected")
        tab.on:SetAllPoints()
        tab.iconOff = Tex(tab, "ARTWORK", info[2] .. "Off")
        tab.iconOff:SetPoint("RIGHT", -6, 0)
        tab.iconOn = Tex(tab, "ARTWORK", info[2] .. "On")
        tab.iconOn:SetPoint("CENTER", tab.iconOff, "CENTER")
        local glow = Tex(tab, "HIGHLIGHT", "TabHighlight", "ADD")
        glow:SetAllPoints()
        tab.key, tab.tip = info[1], info[3]
        tab:SetScript("OnClick", function(self)
            view.tab = self.key
            Refresh()
        end)
        tab:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            local tip = self.tip
            if self.key == "bosses" then
                tip = view.page == "boss" and "Abilities" or "Overview"
            end
            GameTooltip:SetText(tip)
            GameTooltip:Show()
        end)
        tab:SetScript("OnLeave", function() GameTooltip:Hide() end)
        ui.sideTabs[index] = tab
    end
end

-- The bosses' models, one a visible row with no portrait: over the list, placed by hand at their row's socket (the
-- list's scroll taken off), hidden when their row is scrolled out of sight (models in a scrolling list draw out of
-- place).
local function BossModel(index)
    ui.bossModels = ui.bossModels or {}
    local model = ui.bossModels[index]
    if model then
        return model
    end
    model = CreateFrame("PlayerModel", nil, ui.book)
    model:SetWidth(BOSS_HEIGHT - 10)
    model:SetHeight(BOSS_HEIGHT - 10)
    model:SetFrameLevel(ui.bossList.child:GetFrameLevel() + 10)
    ui.bossModels[index] = model
    return model
end

local function PlaceBossModels()
    local list = ui.bossList
    local models = ui.bossModels or {}
    for _, model in pairs(models) do
        model.wanted = false
    end
    if list.scroll:IsShown() then
        local offset = list.scroll:GetVerticalScroll() or 0
        local height = list.scroll:GetHeight() or 0
        for index, row in ipairs(list.rows) do
            local top = list.top + (index - 1) * (BOSS_HEIGHT + list.gap) - offset
            if row:IsShown() and row.creature and top >= -1 and top + BOSS_HEIGHT <= height + 1 then
                local model = BossModel(index)
                model:ClearAllPoints()
                model:SetPoint("TOPLEFT", list.scroll, "TOPLEFT", 9, -(top + 5))
                model.wanted = true
                if not model:IsShown() or model.creature ~= row.creature then
                    model:Show()
                    model.creature = row.creature
                    ShowCreature(model, row.creature, true)
                end
            end
        end
    end
    for _, model in pairs(models) do
        if not model.wanted then
            model:Hide()
            model.creature, model.shown = nil, nil
        end
    end
end

ui.bossList.scroll:HookScript("OnVerticalScroll", PlaceBossModels)

-- Rows -----------------------------------------------------------------------------------------------------------
-- A boss's button (EncounterBossButtonTemplate): the Journal's bar, its portrait standing over its left end.
local function BossRow(index)
    local list = ui.bossList
    local row = list.rows[index]
    if row then
        return row
    end
    row = CreateFrame("Button", nil, list.child)
    row:SetWidth(325)
    row:SetHeight(BOSS_HEIGHT)
    row.fixedWidth = true
    ButtonTex(row, "SetNormalTexture", "BossUp")
    ButtonTex(row, "SetPushedTexture", "BossDown")
    ButtonTex(row, "SetHighlightTexture", "BossHighlight")
    row.art = row:CreateTexture(nil, "OVERLAY")
    row.art:SetWidth(128)
    row.art:SetHeight(64)
    row.art:SetPoint("TOPLEFT", -4, 13)
    row.name = row:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    row.name:SetFont(STANDARD_TEXT_FONT or "Fonts\\FRIZQT__.TTF", 14)
    row.name:SetWidth(200)
    row.name:SetHeight(40)
    row.name:SetJustifyH("LEFT")
    row.name:SetPoint("LEFT", 105, -3)
    row.name:SetTextColor(unpack(BOSS_TEXT))
    row:SetScript("OnEnter", function(self) self.name:SetTextColor(unpack(BOSS_TEXT_HOVER)) end)
    row:SetScript("OnLeave", function(self) self.name:SetTextColor(unpack(BOSS_TEXT)) end)
    row:SetScript("OnClick", function(self)
        if view.page == "boss" and view.boss == self.index then
            view.page, view.boss = "instance", nil        -- the chosen one again: back to the instance
        else
            view.page, view.boss = "boss", self.index
        end
        view.tab = "bosses"
        Refresh()
    end)
    list.rows[index] = row
    return row
end

-- A loot row (EncounterItemTemplate): the Journal's loot frame, the icon in its square, the name in its quality's
-- color over the frame's dark band, the slot and type in ink below.
local function LootRow(index)
    local list = ui.lootList
    local row = list.rows[index]
    if row then
        return row
    end
    row = CreateFrame("Button", nil, list.child)
    row:SetWidth(321)
    row:SetHeight(45)
    row.fixedWidth = true
    local back = Tex(row, "BACKGROUND", "LootFrame")
    back:SetPoint("CENTER")
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(42)
    row.icon:SetHeight(42)
    row.icon:SetPoint("TOPLEFT", 2, -2)
    row.name = row:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    row.name:SetFont(STANDARD_TEXT_FONT or "Fonts\\FRIZQT__.TTF", 13)
    row.name:SetWidth(250)
    row.name:SetHeight(14)
    row.name:SetJustifyH("LEFT")
    row.name:SetPoint("TOPLEFT", row.icon, "TOPRIGHT", 7, -6)
    row.kind = row:CreateFontString(nil, "ARTWORK", "GameFontBlack")
    row.kind:SetJustifyH("RIGHT")
    row.kind:SetPoint("BOTTOMRIGHT", -10, 6)
    row.kind:SetTextColor(unpack(DARK))
    row.slot = row:CreateFontString(nil, "ARTWORK", "GameFontBlack")
    row.slot:SetJustifyH("LEFT")
    row.slot:SetPoint("BOTTOMLEFT", row.icon, "BOTTOMRIGHT", 7, 5)
    row.slot:SetTextColor(unpack(DARK))
    local hover = row:CreateTexture(nil, "HIGHLIGHT")
    hover:SetTexture(1, 0.85, 0.5, 0.12)
    hover:SetAllPoints()
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

-- An ability (EncounterInfoTemplate): its paper header (the Journal's), its icon, name and flags on it, its text
-- below on the ability text background.
local function AbilityRow(index)
    local list = ui.abilityList
    local row = list.rows[index]
    if row then
        return row
    end
    row = CreateFrame("Button", nil, list.child)
    row.bar = CreateFrame("Frame", nil, row)
    row.bar:SetHeight(24)
    row.bar:SetPoint("TOPLEFT")
    row.bar:SetPoint("RIGHT")
    local leftPiece, rightPiece = Tex(row.bar, "BACKGROUND", "PaperLeft"), Tex(row.bar, "BACKGROUND", "PaperRight")
    leftPiece:SetPoint("LEFT", -1, -1)
    rightPiece:SetPoint("RIGHT", 3, -1)
    local mid = Tex(row.bar, "BACKGROUND", "PaperMid")
    mid:SetPoint("LEFT", leftPiece, "RIGHT", -32, 0)
    mid:SetPoint("RIGHT", rightPiece, "LEFT", 32, 0)
    row.icon = row.bar:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(18)
    row.icon:SetHeight(18)
    row.icon:SetPoint("LEFT", 8, 0)
    row.name = row.bar:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 5, -1)
    row.name:SetJustifyH("LEFT")
    row.flags = {}
    for index = 1, 4 do
        local flag = CreateFrame("Frame", nil, row.bar)
        flag:SetWidth(18)
        flag:SetHeight(18)
        flag:SetPoint("RIGHT", index == 1 and row.bar or row.flags[index - 1], index == 1 and "RIGHT" or "LEFT",
            index == 1 and -6 or -1, 0)
        flag.icon = flag:CreateTexture(nil, "ARTWORK")
        flag.icon:SetAllPoints()
        flag.icon:SetTexture(FLAG_ICONS)
        flag:EnableMouse(true)
        flag:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(self.label)
            GameTooltip:Show()
        end)
        flag:SetScript("OnLeave", function() GameTooltip:Hide() end)
        row.flags[index] = flag
    end
    row.textBg = Tex(row, "BACKGROUND", "AbilityTextBG")
    row.text = row:CreateFontString(nil, "ARTWORK", "GameFontBlack")
    row.text:SetJustifyH("LEFT")
    row.text:SetJustifyV("TOP")
    row.text:SetTextColor(unpack(INK))
    row:SetScript("OnEnter", function(self)
        if self.spell then
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetHyperlink("spell:" .. self.spell)
            GameTooltip:Show()
        end
    end)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    list.rows[index] = row
    return row
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

-- An ability: indented by its depth, its icon (none: its name moves in), its flags, its text (none: a heading).
local function SetAbility(row, depth, icon, name, flags, text)
    row.indent = depth * 12
    row.icon:SetTexture(icon)
    row.icon:SetWidth(icon and 18 or 1)
    row.icon:SetAlpha(icon and 1 or 0)
    row.name:SetText(name)
    local shown = 0
    for bit = 0, #FLAG_NAMES - 1 do
        local mask = 2 ^ bit
        if flags and flags % (mask * 2) >= mask and shown < #row.flags then
            shown = shown + 1
            local flag = row.flags[shown]
            local column, line = bit % 8, math.floor(bit / 8)
            flag.icon:SetTexCoord(column / 8, (column + 1) / 8, line / 2, (line + 1) / 2)
            flag.label = FLAG_NAMES[bit + 1]
            flag:Show()
        end
    end
    for index = shown + 1, #row.flags do
        row.flags[index]:Hide()
    end
    row.name:SetPoint("RIGHT", row.bar, "RIGHT", -8 - shown * 19, 0)
    local width = 322 - row.indent
    row:SetWidth(width)
    row.fixedWidth = true
    row.text:ClearAllPoints()
    row.text:SetPoint("TOPLEFT", 10, -30)
    row.text:SetWidth(width - 20)
    row.text:SetText(text or "")
    local heading = not text or text == ""
    if heading then
        row.textBg:Hide()
        row:SetHeight(26)
    else
        local height = row.text:GetStringHeight()
        row.textBg:ClearAllPoints()
        row.textBg:SetPoint("TOPLEFT", 2, -24)
        row.textBg:SetPoint("TOPRIGHT", -2, -24)
        row.textBg:SetHeight(height + 14)
        row.textBg:Show()
        row:SetHeight(40 + height)
    end
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
            SetAbility(row, 0, icon, name, 0, text)
        end
    end
    Stack(ui.abilityList, shown)
    return shown
end

-- The Journal's abilities of a boss: its sections, in order, each at its depth.
local function FillJournal(sections)
    for index, section in ipairs(sections) do
        local row = AbilityRow(index)
        row.spell = nil
        SetAbility(row, section.l or 0, section.i and ("Interface\\Icons\\" .. section.i), section.t, section.f,
            section.b)
    end
    Stack(ui.abilityList, #sections)
    return #sections
end

local function Shown(part, shown)
    if shown then
        part:Show()
    else
        part:Hide()
    end
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
    local journal = bossPage and JournalBoss(instance.map, bossPage.name)

    -- The titles on the pages' shadows.
    ui.instanceTitle:SetText(instance.name)
    ui.encounterTitle:SetText(bossPage and bossPage.name or "")
    Shown(ui.rightShadow, bossPage)

    -- The difficulty button.
    local chosen = Chosen(instance)
    ui.difficulty.text:SetText(chosen and DifficultyName(instance, chosen) or "")
    ui.difficulty:SetWidth(math.max(90, ui.difficulty.text:GetStringWidth() + 34))
    Shown(ui.difficulty, #instance.difficulties > 1)

    -- The tabs.
    for _, tab in ipairs(ui.sideTabs) do
        local on = tab.key == view.tab
        Shown(tab.on, on)
        Shown(tab.off, not on)
        Shown(tab.iconOn, on)
        Shown(tab.iconOff, not on)
    end

    -- The left page: the bosses, the chosen one lit (the right page is its).
    for index, boss in ipairs(bosses) do
        local row = BossRow(index)
        row.index = index
        row.name:SetText(boss.name)
        local portrait = Portrait(instance.map, boss.name)
        row.creature = not portrait and boss.creature > 0 and boss.creature or nil
        row.art:SetTexture(portrait or (JOURNAL_ART .. "UI-EJ-BOSS-Default"))
        Shown(row.art, not row.creature)
        if bossPage and index == view.boss then
            row:LockHighlight()
        else
            row:UnlockHighlight()
        end
    end
    Stack(ui.bossList, #bosses)

    -- The right page: the instance's picture and story; else the tab's list.
    local overview = view.tab == "bosses" and not bossPage
    Shown(ui.plate, overview)
    if overview then
        local art = InstanceArt(instance.map)
        local own = art and art.lore
        ui.plate.fill:SetTexture(JOURNAL_ART .. (own or "UI-EJ-LOREBG-Frame"))
        ui.plate.fill:SetTexCoord(0, 0.7617187, 0, 0.65625)
        Shown(ui.plate.screen, not own)
        if not own then
            ui.plate.screen:SetTexture(LoadingScreen(instance, art))
            ui.plate.screen:SetTexCoord(0.21, 0.79, SCREEN_TOP, SCREEN_BOTTOM)    -- cut to the frame's shape
        end
        ui.plate.name:SetText(instance.name)
        local lore = EmberforgeJournalLore and EmberforgeJournalLore[instance.map]
        local lines = {}
        if lore then
            lines[1] = lore .. "\n"
        end
        for _, entry in ipairs(instance.difficulties) do
            lines[#lines + 1] = DifficultyName(instance, entry)
        end
        ui.about:SetBody(table.concat(lines, "\n"))
    end
    ListShown(ui.abilityList, bossPage and view.tab == "bosses")
    ListShown(ui.lootList, view.tab == "loot")
    ui.empty:SetText("")

    if bossPage and view.tab == "bosses" and journal and #journal.s > 0 then
        FillJournal(journal.s)
    elseif (view.tab == "loot" or (bossPage and view.tab == "bosses")) and not (details and details.ready) then
        ui.empty:SetText("Loading...")
        Stack(ui.lootList, 0)
        Stack(ui.abilityList, 0)
    elseif view.tab == "loot" then
        local items, own = {}, {}
        for _, boss in ipairs(bosses) do
            own[boss.index] = true
        end
        for _, item in ipairs(details.loot) do
            if (bossPage and item.boss == bossPage.index) or (not bossPage and own[item.boss]) then
                items[#items + 1] = item
            end
        end
        if FillLoot(items) == 0 then
            ui.empty:SetText("No loot listed")
        end
    elseif bossPage and view.tab == "bosses" then
        if FillAbilities(details.abilities[bossPage.index] or {}) == 0 then
            ui.empty:SetText("No abilities listed")
        end
    end
    PlaceBossModels()
end

local function RefreshCrumbs()
    local instance = data.instances[view.instance]
    local bosses = instance and Bosses(instance)
    local texts = { "Home", instance and instance.name, view.page == "boss" and bosses and bosses[view.boss]
        and bosses[view.boss].name }
    for index, crumb in ipairs(ui.crumbs) do
        if texts[index] then
            crumb.text:SetText(texts[index])
            crumb:SetWidth(crumb.text:GetStringWidth() + 30)
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
        -- The models are set again when the book shows (a hidden model can lose its own).
        for _, model in pairs(ui.bossModels or {}) do
            model:Hide()
            model.creature, model.shown = nil, nil
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

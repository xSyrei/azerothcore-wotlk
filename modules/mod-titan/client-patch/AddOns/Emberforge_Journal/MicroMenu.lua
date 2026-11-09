-- Emberforge Journal: its place in the game's interface.
--
-- - The micro menu: the Dungeon Journal's button takes the Player vs. Player button's place (the game's blank micro
--   button, the character button's, with the Journal's own icon on it). Player vs. Player is opened from the
--   dungeon finder's tabs (or its key, H).
-- - The dungeon finder and Player vs. Player windows are one: both carry the same two tabs on their right edge
--   (as the spellbook's), Dungeons and Player vs. Player, each opening the other in its place.
-- - The dungeon finder opens from level 10 (its micro button, its key I), as the server queues from there, and lists
--   what the server lets the Hero queue for, whatever the client's own level ranges.
-- - Its key: Shift-J, set once if nothing else uses it (Key Bindings: Emberforge).

BINDING_HEADER_EMBERFORGE = "Project Emberforge"
BINDING_NAME_EMBERFORGE_JOURNAL_TOGGLE = "Dungeon Journal"
local JOURNAL_ICON = "Interface\\AddOns\\Emberforge_Journal\\Art\\UI-EJ-PortraitIcon"

function EmberforgeJournal_Toggle()
    if EmberforgeJournal:IsShown() then
        EmberforgeJournal:Hide()
    else
        EmberforgeJournal:Show()
    end
end

-- The micro button ------------------------------------------------------------------------------------------------
local button = CreateFrame("Button", "EmberforgeJournalMicroButton", PVPMicroButton:GetParent(),
    "MainMenuBarMicroButton")
button:SetNormalTexture("Interface\\Buttons\\UI-MicroButtonCharacter-Up")
button:SetPushedTexture("Interface\\Buttons\\UI-MicroButtonCharacter-Down")
button:SetHighlightTexture("Interface\\Buttons\\UI-MicroButton-Hilight")
-- The icon where the character button has its portrait (and moving with it when pressed).
local icon = button:CreateTexture(nil, "OVERLAY")
icon:SetWidth(18)
icon:SetHeight(25)
icon:SetTexture(JOURNAL_ICON)
icon:SetTexCoord(0.2, 0.8, 0.0666, 0.9)
local function Pressed(pressed)
    icon:ClearAllPoints()
    icon:SetPoint("TOP", button, "TOP", pressed and 1 or 0, pressed and -29 or -28)
    icon:SetAlpha(pressed and 0.5 or 1)
end
Pressed(false)
button:SetScript("OnMouseDown", function() Pressed(true) end)
button:SetScript("OnMouseUp", function() Pressed(EmberforgeJournal:IsShown()) end)
button:SetScript("OnClick", EmberforgeJournal_Toggle)
button:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetText(MicroButtonTooltipText and MicroButtonTooltipText("Dungeon Journal",
        "EMBERFORGE_JOURNAL_TOGGLE") or "Dungeon Journal", 1, 1, 1)
    GameTooltip:AddLine("Every dungeon and raid: their bosses, abilities and loot.", nil, nil, nil, true)
    GameTooltip:Show()
end)
button:SetScript("OnLeave", function() GameTooltip:Hide() end)

-- In the Player vs. Player button's place (the buttons after it are anchored to it, so they stay put).
local function PlaceButton()
    button:ClearAllPoints()
    button:SetAllPoints(PVPMicroButton)
    PVPMicroButton:Hide()
    button:Show()
    if EmberforgeJournal:IsShown() then
        button:SetButtonState("PUSHED", 1)
    else
        button:SetButtonState("NORMAL")
    end
    Pressed(EmberforgeJournal:IsShown())
end
hooksecurefunc("UpdateMicroButtons", PlaceButton)
EmberforgeJournal:HookScript("OnShow", PlaceButton)
EmberforgeJournal:HookScript("OnHide", PlaceButton)
-- With the micro buttons (the vehicle bar moves them to its own bar).
if MICRO_BUTTONS then
    tinsert(MICRO_BUTTONS, "EmberforgeJournalMicroButton")
end
if MoveMicroButtons then
    hooksecurefunc("MoveMicroButtons", function()
        button:SetParent(PVPMicroButton:GetParent())
        PlaceButton()
    end)
end
PlaceButton()

-- One window for the dungeon finder and Player vs. Player ---------------------------------------------------------
-- The dungeon finder from level 10 (the server queues every normal dungeon from there: DungeonFinder.MinLevel): the
-- stock client keeps it shut below level 15, its key, its micro button and its toggle alike.
local FINDER_LEVEL = 10
SHOW_LFD_LEVEL = FINDER_LEVEL
local function OpenFinder()
    if not LFDParentFrame then
        return
    end
    -- Not before level 10, by its key (I) either: the window holds the PvP tab too.
    if not LFDParentFrame:IsShown() and UnitLevel("player") < FINDER_LEVEL then
        UIErrorsFrame:AddMessage(format(FEATURE_BECOMES_AVAILABLE_AT_LEVEL or "Available at level %d.", FINDER_LEVEL),
            1.0, 0.1, 0.1, 1.0)
        return
    end
    if LFDParentFrame:IsShown() then
        HideUIPanel(LFDParentFrame)
    else
        ShowUIPanel(LFDParentFrame)
    end
end
ToggleLFDParentFrame = OpenFinder
local function EnableFinderButton()
    if LFDMicroButton and UnitLevel("player") >= FINDER_LEVEL then
        LFDMicroButton:Enable()
        LFDMicroButton:SetScript("OnClick", OpenFinder)
        if MicroButtonTooltipText then
            LFDMicroButton.tooltipText = MicroButtonTooltipText(DUNGEONS_BUTTON or "Dungeon Finder", "TOGGLELFGPARENT")
        end
        LFDMicroButton.newbieText = NEWBIE_TOOLTIP_LFGPARENT or LFDMicroButton.newbieText
    end
end
hooksecurefunc("UpdateMicroButtons", EnableFinderButton)
-- Its lists: the stock client shows only the dungeons, specific and random, whose level range in its own
-- LFGDungeons.dbc holds the Hero's level (LFDFrame.lua). The server is the judge here: what it does not lock (by
-- level, gear or a closed dungeon) is listed.
local function FinderFilter(dungeonID)
    local info = LFGGetDungeonInfoByID and LFGGetDungeonInfoByID(dungeonID)
    if not info or not LFG_RETURN_VALUES then
        return false
    end
    return info[LFG_RETURN_VALUES.groupID] ~= 0 and EXPANSION_LEVEL >= info[LFG_RETURN_VALUES.expansionLevel]
        and LFGLockList ~= nil and not LFGLockList[dungeonID]
end
LFDList_DefaultFilterFunction = FinderFilter
LFD_CURRENT_FILTER = FinderFilter

-- The random dungeon kept for each difficulty (0 normal, 1 heroic): the earliest expansion's, as the server keeps.
local function ChosenRandoms()
    local chosen = {}
    for i = 1, GetNumRandomDungeons() do
        local id = GetLFGRandomDungeonInfo(i)
        local _, _, _, _, _, _, _, expansionLevel, _, _, difficulty, _, _, isHoliday = GetLFGDungeonInfo(id)
        if not isHoliday then
            local best = chosen[difficulty or 0]
            if not best or (expansionLevel or 0) < best.expansion then
                chosen[difficulty or 0] = { id = id, expansion = expansionLevel or 0 }
            end
        end
    end
    return chosen
end

-- The random dungeons the server offers (the stock dropdown drops those outside the client's level ranges): one for
-- every normal dungeon, one for every heroic (DungeonFinder.OneRandom), named for what they are.
function LFDQueueFrameTypeDropDown_Initialize()
    local info = UIDropDownMenu_CreateInfo()
    info.text = SPECIFIC_DUNGEONS
    info.value = "specific"
    info.func = LFDQueueFrameTypeDropDownButton_OnClick
    info.checked = LFDQueueFrame.type == info.value
    UIDropDownMenu_AddButton(info)
    -- One of each difficulty, and the holiday bosses.
    local chosen = ChosenRandoms()
    for i = 1, GetNumRandomDungeons() do
        local id, name = GetLFGRandomDungeonInfo(i)
        local _, _, _, _, _, _, _, expansionLevel, _, _, difficulty, _, _, isHoliday = GetLFGDungeonInfo(id)
        local shown = isHoliday or (chosen[difficulty or 0] and chosen[difficulty or 0].id == id)
        if not isHoliday then
            name = (difficulty or 0) > 0 and "Random Heroic Dungeon" or "Random Dungeon"
        end
        if shown and EXPANSION_LEVEL >= (expansionLevel or 0) then
            local available = IsLFGDungeonJoinable(id)
            info.text = name
            info.value = id
            info.isTitle = nil
            info.func = available and LFDQueueFrameTypeDropDownButton_OnClick or nil
            info.disabled = not available and 1 or nil
            info.checked = available and LFDQueueFrame.type == id or nil
            info.tooltipWhileDisabled = not available and 1 or nil
            info.tooltipOnButton = not available and 1 or nil
            info.tooltipTitle = not available and YOU_MAY_NOT_QUEUE_FOR_THIS or nil
            info.tooltipText = not available and LFDConstructDeclinedMessage(id) or nil
            UIDropDownMenu_AddButton(info)
        end
    end
end

-- The random dungeon's rewards: the Emberspark it gives too (src/dungeon_rewards.cpp), under the experience.
local SPARK_ICON = "|TInterface\\Icons\\INV_Emberforge_Emberspark:14|t"
hooksecurefunc("LFDQueueFrameRandom_UpdateFrame", function()
    local parent = LFDQueueFrameRandomScrollFrameChildFrame
    local dungeonID = LFDQueueFrame and LFDQueueFrame.type
    if not parent or type(dungeonID) ~= "number" then
        return
    end
    local _, _, _, _, _, _, _, _, _, _, difficulty, _, _, isHoliday = GetLFGDungeonInfo(dungeonID)
    if not parent.sparkLabel then
        -- As the money and experience lines.
        parent.sparkLabel = parent:CreateFontString(nil, "ARTWORK", "QuestFont")
        parent.sparkLabel:SetText("Emberspark:")
        parent.sparkLabel:SetTextColor(1, 1, 1)
        parent.sparkAmount = parent:CreateFontString(nil, "ARTWORK", "NumberFontNormalLarge")
        parent.sparkAmount:SetPoint("LEFT", parent.sparkLabel, "RIGHT", 6, 0)
    end
    if isHoliday then
        parent.sparkLabel:Hide()
        parent.sparkAmount:Hide()
        return
    end
    local last = parent.xpLabel:IsShown() and parent.xpLabel or parent.moneyLabel:IsShown() and parent.moneyLabel
        or parent.rewardsDescription
    parent.sparkLabel:ClearAllPoints()
    if last == parent.rewardsDescription then
        parent.sparkLabel:SetPoint("TOPLEFT", last, "BOTTOMLEFT", 20, -10)
    else
        parent.sparkLabel:SetPoint("TOPLEFT", last, "BOTTOMLEFT", 0, -5)
    end
    parent.sparkAmount:SetText(((difficulty or 0) > 0 and "500" or "100") .. " " .. SPARK_ICON)
    parent.sparkLabel:Show()
    parent.sparkAmount:Show()
    parent.spacer:SetPoint("TOPLEFT", parent.sparkLabel, "BOTTOMLEFT", 0, -10)
end)

-- Random Dungeon is the default choice: when the finder opens, or the server's list comes, a type that is not one
-- of those offered (the stock client picks its own "best" random dungeon, often one not offered) becomes it.
local function DefaultToRandom()
    if not LFDQueueFrame or not LFDQueueFrame_SetType then
        return
    end
    local chosen = ChosenRandoms()
    local normal, heroic = chosen[0] and chosen[0].id, chosen[1] and chosen[1].id
    local current = LFDQueueFrame.type
    local offered = current == "specific" or (current and (current == normal or current == heroic))
    if type(current) == "number" and not offered then
        local _, _, _, _, _, _, _, _, _, _, _, _, _, isHoliday = GetLFGDungeonInfo(current)
        offered = isHoliday and IsLFGDungeonJoinable(current)
    end
    if not LFDParentFrame.emberforgeChosen or not offered then
        if normal and IsLFGDungeonJoinable(normal) then
            LFDQueueFrame_SetType(normal)
            LFDParentFrame.emberforgeChosen = true
        end
    end
end
-- Once a session the finder starts on Random Dungeon; after that the player's own choice stays.
if LFDParentFrame then
    LFDParentFrame:HookScript("OnShow", DefaultToRandom)
end
local randomEvents = CreateFrame("Frame")
randomEvents:RegisterEvent("LFG_UPDATE_RANDOM_INFO")
randomEvents:SetScript("OnEvent", function()
    if LFDParentFrame and LFDParentFrame:IsShown() then
        DefaultToRandom()
    end
end)

local finderEvents = CreateFrame("Frame")
finderEvents:RegisterEvent("PLAYER_LOGIN")
finderEvents:RegisterEvent("PLAYER_LEVEL_UP")
finderEvents:SetScript("OnEvent", function()
    SHOW_LFD_LEVEL = FINDER_LEVEL
    EnableFinderButton()
end)

local faction = UnitFactionGroup("player") == "Horde" and "INV_BannerPVP_01" or "INV_BannerPVP_02"
-- edge: how far in from the frame's right side its art ends (the Player vs. Player frame has a transparent margin).
local WINDOWS = {
    { frame = "LFDParentFrame", name = "Dungeons", icon = "Interface\\Icons\\INV_Misc_GroupNeedMore", edge = 0,
        top = -40 },
    { frame = "PVPParentFrame", name = "Player vs. Player", icon = "Interface\\Icons\\" .. faction, edge = -32,
        top = -65 },
}
local tabs = {}
local function Window(index)
    return _G[WINDOWS[index].frame]
end

-- The tabs onto the window shown, the shown one checked.
local function PlaceTabs(shown)
    local place = WINDOWS[2]
    for _, window in ipairs(WINDOWS) do
        if _G[window.frame] == shown then
            place = window
        end
    end
    for index, tab in ipairs(tabs) do
        tab:SetParent(shown)
        tab:SetFrameStrata(shown:GetFrameStrata())
        tab:SetFrameLevel(shown:GetFrameLevel() + 20)
        tab:ClearAllPoints()
        if index == 1 then
            tab:SetPoint("TOPLEFT", shown, "TOPRIGHT", place.edge, place.top)
        else
            tab:SetPoint("TOPLEFT", tabs[index - 1], "BOTTOMLEFT", 0, -17)
        end
        tab:SetChecked(Window(index) == shown)
        tab:Show()
    end
end

for index, window in ipairs(WINDOWS) do
    local tab = CreateFrame("CheckButton", "EmberforgeFinderTab" .. index, UIParent, "SpellBookSkillLineTabTemplate")
    tab:SetNormalTexture(window.icon)
    tab.tooltip = window.name
    tab:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(self.tooltip)
        GameTooltip:Show()
    end)
    tab:SetScript("OnLeave", function() GameTooltip:Hide() end)
    tab:SetScript("OnClick", function(self)
        -- Both are left-hand panels: showing one puts it in the other's place.
        local target = Window(index)
        self:SetChecked(true)
        if target and not target:IsShown() then
            ShowUIPanel(target)
            if not target:IsShown() then
                for other = 1, #WINDOWS do
                    local frame = Window(other)
                    if frame and frame ~= target and frame:IsShown() then
                        HideUIPanel(frame)
                    end
                end
                ShowUIPanel(target)
            end
        end
    end)
    tab:Hide()
    tabs[index] = tab
end

for index = 1, #WINDOWS do
    local frame = Window(index)
    if frame then
        frame:HookScript("OnShow", function(self) PlaceTabs(self) end)
    end
end

-- The key --------------------------------------------------------------------------------------------------------
local keys = CreateFrame("Frame")
keys:RegisterEvent("PLAYER_LOGIN")
keys:SetScript("OnEvent", function()
    EmberforgeJournalDB = EmberforgeJournalDB or {}
    local free = (GetBindingAction("SHIFT-J") or "") == ""
    if not EmberforgeJournalDB.boundKey and not GetBindingKey("EMBERFORGE_JOURNAL_TOGGLE") and free then
        SetBinding("SHIFT-J", "EMBERFORGE_JOURNAL_TOGGLE")
        SaveBindings(GetCurrentBindingSet())
    end
    EmberforgeJournalDB.boundKey = true
end)

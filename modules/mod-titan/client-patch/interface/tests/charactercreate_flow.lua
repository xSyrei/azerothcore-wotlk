-- Mock 3.3.5 GlueXML environment for CharacterCreate.
local I, H, STRINGS = arg[1], arg[2], arg[3]
local log = {}
local function note(s) log[#log + 1] = s end

-- Widgets -------------------------------------------------------------------------------------
local W = {}
local METHODS = {
  Show=1, Hide=1, IsShown=1, SetPoint=1, ClearAllPoints=1, SetText=1, GetText=1, SetTextColor=1,
  SetJustifyH=1, SetChecked=1, GetChecked=1, Enable=1, Disable=1, IsEnabled=1, SetTexCoord=1,
  SetDesaturated=1, SetVertexColor=1, GetNormalTexture=1, SetBackdropBorderColor=1,
  SetBackdropColor=1, SetFocus=1, ClearFocus=1, GetID=1, GetName=1, GetParent=1, SetValue=1,
  GetButtonState=1, RegisterForClicks=1, SetSequence=1, SetCamera=1, AdvanceTime=1, SetSize=1,
  SetWidth=1, GetTextWidth=1,
}
local Widget = {}
Widget.__index = function(t, k)
  if Widget[k] then return Widget[k] end
  if METHODS[k] then return function() end end
  if type(k) == 'string' and k:match('^[A-Z]') then
    error(('%s has no method %s'):format(rawget(t, '_name') or '?', k), 2)
  end
  return nil
end
function Widget:Show() self._shown = true end
function Widget:Hide() self._shown = false end
function Widget:IsShown() return self._shown end
function Widget:SetText(s) self._text = s end
function Widget:GetText() return self._text or '' end
function Widget:SetChecked(v) self._checked = (v == 1 or v == true) and 1 or nil end
function Widget:GetChecked() return self._checked end
function Widget:GetID() return self._id end
function Widget:GetName() return self._name end
function Widget:GetParent() return W[self._owner] end
function Widget:Enable() self._enabled = true end
function Widget:Disable() self._enabled = false end
function Widget:IsEnabled() return self._enabled and 1 or 0 end
function Widget:SetDesaturated() return 1 end
function Widget:GetNormalTexture() return _G[self._name .. 'NormalTexture'] end
function Widget:GetButtonState() return 'NORMAL' end
function Widget:SetFocus() note('focus ' .. self._name) end

function make(name, kind, owner, id, text)
  local w = setmetatable({ _name = name, _kind = kind, _owner = owner, _id = tonumber(id), _shown = true, _enabled = true }, Widget)
  if name then W[name] = w; _G[name] = w end
  if text ~= '' then w._text = text end
  return w
end
-- Created by templates defined outside CharacterCreate.xml
for _, n in ipairs({'CharacterCreateRaceScrollFrameScrollBar', 'CharacterCreateClassScrollFrameScrollBar'}) do make(n, 'Slider', nil, 0, '') end

-- API -----------------------------------------------------------------------------------------
SEX_MALE, SEX_FEMALE = 2, 3
strupper, strlower, strlen = string.upper, string.lower, string.len
CHARACTER_ROTATION_CONSTANT = 0.6
ALLOW_RANDOM_NAME_BUTTON = true
local races = { {'Human','Human'}, {'Dwarf','Dwarf'}, {'Night Elf','NightElf'}, {'Gnome','Gnome'}, {'Draenei','Draenei'},
                {'Orc','Orc'}, {'Undead','Scourge'}, {'Tauren','Tauren'}, {'Troll','Troll'}, {'Blood Elf','BloodElf'} }
local classes = { {'Warrior','WARRIOR'}, {'Hero','PALADIN'}, {'Hunter','HUNTER'}, {'Rogue','ROGUE'}, {'Priest','PRIEST'},
                  {'Death Knight','DEATHKNIGHT'}, {'Shaman','SHAMAN'}, {'Mage','MAGE'}, {'Warlock','WARLOCK'}, {'Druid','DRUID'} }
local state = { race = 1, class = 1, sex = SEX_MALE, facing = 0 }
function GetAvailableRaces() local t = {} for _, r in ipairs(races) do t[#t+1] = r[1]; t[#t+1] = r[2]; t[#t+1] = 1 end return unpack(t) end
function GetAvailableClasses() local t = {} for i, c in ipairs(classes) do t[#t+1] = c[1]; t[#t+1] = c[2]; t[#t+1] = (i == 2) and 1 or 0 end return unpack(t) end
function GetFactionForRace(i) local f = (i <= 5) and 'Alliance' or 'Horde' return f, f end
function GetSelectedRace() return state.race end
function SetSelectedRace(i) state.race = i end
function GetNameForRace() return races[state.race][1], races[state.race][2] end
function IsRaceClassValid(r, c) return c == 2 end
function GetSelectedClass() local c = classes[state.class] return c[1], c[2], state.class end
function SetSelectedClass(i) state.class = i end
function GetSelectedSex() return state.sex end
function SetSelectedSex(s) state.sex = s end
function GetHairCustomization() return 'NORMAL' end
function GetFacialHairCustomization() return 'NORMAL' end
function GetCreateBackgroundModel() return races[state.race][2] end
function SetBackgroundModel(model, name) state.background = name end
function GetCharacterCreateFacing() return state.facing end
function SetCharacterCreateFacing(f) state.facing = f end
function CreateCharacter(name) note('CreateCharacter(' .. name .. ')') end
function SetGlueScreen(s) note('SetGlueScreen(' .. s .. ')') end
function CycleCharCustomization(id, d) note(('Cycle %d %d'):format(id, d)) end
function RandomizeCharCustomization() note('Randomize') end
function GetRandomName() return 'Emberling' end
function PaidChange_GetName() return 'Oldname' end
function PaidChange_GetCurrentClassIndex() return 2 end
function PaidChange_GetCurrentRaceIndex() return 1 end
for _, f in ipairs({'PlaySound', 'SetCharCustomizeFrame', 'ResetCharCustomize', 'CustomizeExistingCharacter',
  'GlueTooltip_SetOwner', 'GlueTooltip_SetText', 'GlueTooltip_SetFont', 'GlueScrollFrame_OnScrollRangeChanged',
  'UpdateCustomizationScene', 'GlueDialog_Show', 'message', 'Screenshot'}) do _G[f] = function() end end
function GetCursorPosition() return 0, 0 end
CharacterCreateTooltipFont = {}

-- Load ----------------------------------------------------------------------------------------
dofile(STRINGS)
dofile(H .. '/widgets.lua')
dofile(I .. '/CharacterCreate.lua')

local function fire(owner, kind, ...)
  local n = 0
  for _, s in ipairs(SCRIPTS) do
    if s.owner == owner and s.kind == kind then
      local f = assert(loadstring('local self, button, key, yrange, elapsed = ...; ' .. s.body, owner .. ':' .. kind))
      f(W[owner], ...)
      n = n + 1
    end
  end
  return n
end
for _, s in ipairs(SCRIPTS) do if s.kind == 'OnLoad' then fire(s.owner, 'OnLoad') end end

local function click(name, ...)
  local w = W[name]
  if w._kind == 'CheckButton' then w:SetChecked(w:GetChecked() and nil or 1) end
  assert(fire(name, 'OnClick', ...) > 0, 'no OnClick on ' .. name)
end
local function check(cond, msg) if not cond then error('FAILED: ' .. msg, 2) end print('ok  ' .. msg) end

-- Scenario ------------------------------------------------------------------------------------
CharacterCreate:Show(); fire('CharacterCreate', 'OnShow')
check(CharacterCreateRaceStep:IsShown() and not CharacterCreateCustomizeStep:IsShown(), 'opens on step 1 (race)')
check(CharacterCreateTitle:GetText() == EMBERFORGE_CREATE_TITLE, 'title: ' .. CharacterCreateTitle:GetText())
check(state.class == 2 and CharacterCreateHeroCardName:GetText() == 'Hero', 'invalid class 1 replaced by the Hero; card says Hero')
check(CharacterCreateClassRolesText:GetText():find('Wildcard'), 'Hero panel shows the Wildcard text')
check(CharacterCreateRaceButton1Text:GetText() == 'Human' and CharacterCreateRaceButton6Text:GetText() == 'Orc', 'race names shown')
check(CharacterCreateRaceButton1:GetChecked() == 1, 'Human selected')
click('CharacterCreateRaceButton7')
check(state.race == 7 and CharacterCreateRaceLabel:GetText() == 'Undead', 'clicking Undead selects it')
check(CharacterCreateRaceFaction:GetText() == 'Horde' and state.background == 'Scourge', 'faction label and background follow the race')
check(CharacterCreateRaceText:GetText():len() > 40, 'race lore filled in')
click('CharacterCreateGenderButtonFemale')
check(state.sex == SEX_FEMALE and CharacterCreateGender:GetText() == FEMALE, 'female selected')
click('CharCreateNextButton')
check(not CharacterCreateRaceStep:IsShown() and CharacterCreateCustomizeStep:IsShown(), 'Next goes to step 2')
check(CharacterCreateSummaryText:GetText() == 'Undead Hero', 'summary: ' .. CharacterCreateSummaryText:GetText())
check(CharacterCustomizationButtonFrame1Text:GetText() == CHAR_CUSTOMIZATION1_DESC, 'option labels set')
click('CharacterCustomizationButtonFrame2RightButton')
click('CharCreateRandomizeButton')
click('CharacterCreateRandomName')
check(CharacterCreateNameEdit:GetText() == 'Emberling', 'random name')
fire('CharacterCreateNameEdit', 'OnTextChanged')
check(not CharacterCreateNamePlaceholder:IsShown(), 'placeholder hidden once a name is typed')
CharacterCreateNameEdit:SetText(''); fire('CharacterCreateNameEdit', 'OnTextChanged')
check(CharacterCreateNamePlaceholder:IsShown() and CharacterCreateNamePlaceholder:GetText() == EMBERFORGE_NAME_PLACEHOLDER, 'placeholder shown when empty')
CharacterCreateNameEdit:SetText('Emberling')
click('CharCreateRandomizeDice')
click('CharacterCustomizationButtonFrame5LeftButton')
fire('CharacterCreate', 'OnKeyDown', nil, 'ESCAPE')
check(CharacterCreateRaceStep:IsShown(), 'Escape on step 2 goes back to step 1')
fire('CharacterCreate', 'OnKeyDown', nil, 'ENTER')
check(CharacterCreateCustomizeStep:IsShown(), 'Enter on step 1 goes to step 2')
fire('CharacterCreateNameEdit', 'OnEnterPressed')
click('CharCreateOkayButton')
fire('CharacterCreate', 'OnKeyDown', nil, 'ESCAPE'); fire('CharacterCreate', 'OnKeyDown', nil, 'ESCAPE')
PAID_SERVICE_TYPE = PAID_CHARACTER_CUSTOMIZATION; PAID_SERVICE_CHARACTER_ID = 1
fire('CharacterCreate', 'OnShow')
check(CharacterCreateCustomizeStep:IsShown() and CharacterCreateNameEdit:GetText() == 'Oldname', 'paid recustomization opens on step 2')
click('CharCreateCustomizeBackButton')
fire('CharacterCreate', 'OnHide')
print('calls: ' .. table.concat(log, ', '))

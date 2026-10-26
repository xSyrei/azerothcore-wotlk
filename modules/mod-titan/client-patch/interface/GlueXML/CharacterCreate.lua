-- Project Emberforge character creation, retail style, in two steps:
--   1. Race: Alliance races on the left, Horde on the right, gender on top, the Hero card at the
--      bottom, race and class details in side panels.
--   2. Customize: appearance options on the right, name and Create at the bottom.
-- Every character is a Hero (the only class the client patch allows), so there is no class grid.
-- Based on Blizzard's 3.3.5a CharacterCreate.lua; the game API calls are unchanged.

CHARACTER_FACING_INCREMENT = 2;
MAX_RACES = 10;
MAX_CLASSES_PER_RACE = 10;
NUM_CHAR_CUSTOMIZATIONS = 5;
MIN_CHAR_NAME_LENGTH = 2;
CHARACTER_CREATE_ROTATION_START_X = nil;
CHARACTER_CREATE_INITIAL_FACING = nil;

PAID_CHARACTER_CUSTOMIZATION = 1;
PAID_RACE_CHANGE = 2;
PAID_FACTION_CHANGE = 3;
PAID_SERVICE_CHARACTER_ID = nil;
PAID_SERVICE_TYPE = nil;

EMBERFORGE_CREATE_TITLE = "Forge Your Hero";
EMBERFORGE_CUSTOMIZE_TITLE = "Customize";
EMBERFORGE_NEXT = "Next";
EMBERFORGE_CREATE = "Create Hero";
EMBERFORGE_HERO_SUBTITLE = "Classless  -  Wildcard";
EMBERFORGE_SECTION_BODY = "Body";
EMBERFORGE_SECTION_FACE = "Face";
EMBERFORGE_SECTION_HAIR = "Hair";
EMBERFORGE_NAME_PLACEHOLDER = "Enter a name";
EMBERFORGE_RANDOM_NAME = "Random name";
PANEL_PIECES = { "TopLeft", "TopRight", "BottomLeft", "BottomRight", "Top", "Bottom", "Left", "Right" };

RACE_ROW_SPACING = 74;
RACE_COLUMN_TOP = -150;
RACE_COLUMN_INSET = 70;

FACTION_BACKDROP_COLOR_TABLE = {
	["Alliance"] = {0.35, 0.5, 1.0, 0.04, 0.05, 0.12},
	["Horde"] = {1.0, 0.3, 0.2, 0.12, 0.04, 0.03},
};
RACE_ICON_TCOORDS = {
	["HUMAN_MALE"]		= {0, 0.125, 0, 0.25},
	["DWARF_MALE"]		= {0.125, 0.25, 0, 0.25},
	["GNOME_MALE"]		= {0.25, 0.375, 0, 0.25},
	["NIGHTELF_MALE"]	= {0.375, 0.5, 0, 0.25},

	["TAUREN_MALE"]		= {0, 0.125, 0.25, 0.5},
	["SCOURGE_MALE"]	= {0.125, 0.25, 0.25, 0.5},
	["TROLL_MALE"]		= {0.25, 0.375, 0.25, 0.5},
	["ORC_MALE"]		= {0.375, 0.5, 0.25, 0.5},

	["HUMAN_FEMALE"]	= {0, 0.125, 0.5, 0.75},
	["DWARF_FEMALE"]	= {0.125, 0.25, 0.5, 0.75},
	["GNOME_FEMALE"]	= {0.25, 0.375, 0.5, 0.75},
	["NIGHTELF_FEMALE"]	= {0.375, 0.5, 0.5, 0.75},

	["TAUREN_FEMALE"]	= {0, 0.125, 0.75, 1.0},
	["SCOURGE_FEMALE"]	= {0.125, 0.25, 0.75, 1.0},
	["TROLL_FEMALE"]	= {0.25, 0.375, 0.75, 1.0},
	["ORC_FEMALE"]		= {0.375, 0.5, 0.75, 1.0},

	["BLOODELF_MALE"]	= {0.5, 0.625, 0.25, 0.5},
	["BLOODELF_FEMALE"]	= {0.5, 0.625, 0.75, 1.0},

	["DRAENEI_MALE"]	= {0.5, 0.625, 0, 0.25},
	["DRAENEI_FEMALE"]	= {0.5, 0.625, 0.5, 0.75},
};

-- Steps ---------------------------------------------------------------------------------------

function CharacterCreate_SetStep(step)
	CharacterCreate.step = step;
	if ( step == 1 ) then
		CharacterCreateRaceStep:Show();
		CharacterCreateCustomizeStep:Hide();
		CharacterCreateTitle:SetText(EMBERFORGE_CREATE_TITLE);
		CharacterCreateNameEdit:ClearFocus();
	else
		CharacterCreateRaceStep:Hide();
		CharacterCreateCustomizeStep:Show();
		CharacterCreateTitle:SetText(EMBERFORGE_CUSTOMIZE_TITLE);
		CharacterCreate_UpdateSummary();
		CharacterCreateNameEdit:SetFocus();
	end
end

function CharacterCreate_Next()
	PlaySound("gsCharacterCreationClass");
	CharacterCreate_SetStep(2);
end

function CharacterCreate_Previous()
	if ( PAID_SERVICE_TYPE == PAID_CHARACTER_CUSTOMIZATION ) then
		CharacterCreate_Back();
		return;
	end
	PlaySound("gsCharacterCreationCancel");
	CharacterCreate_SetStep(1);
end

-- Frame scripts -------------------------------------------------------------------------------

function CharacterCreate_OnLoad(self)
	self:SetSequence(0);
	self:SetCamera(0);

	CharacterCreate.numRaces = 0;
	CharacterCreate.selectedRace = 0;
	CharacterCreate.numClasses = 0;
	CharacterCreate.selectedClass = 0;
	CharacterCreate.selectedGender = 0;
	CharacterCreate.step = 1;

	SetCharCustomizeFrame("CharacterCreate");

	for i=1, NUM_CHAR_CUSTOMIZATIONS, 1 do
		_G["CharacterCustomizationButtonFrame"..i.."Text"]:SetText(_G["CHAR_CUSTOMIZATION"..i.."_DESC"]);
	end

	CharacterCreateNamePlaceholder:SetText(EMBERFORGE_NAME_PLACEHOLDER);
end

-- Tint a CharacterCreatePanelTemplate's edge (the race panel takes its faction's color).
function CharacterCreate_SetPanelColor(frame, r, g, b)
	local name = frame:GetName();
	for _, piece in pairs(PANEL_PIECES) do
		_G[name..piece]:SetVertexColor(r, g, b);
	end
end

function CharacterCreate_UpdateNamePlaceholder()
	if ( CharacterCreateNameEdit:GetText() == "" ) then
		CharacterCreateNamePlaceholder:Show();
	else
		CharacterCreateNamePlaceholder:Hide();
	end
end

function CharacterCreate_OnShow()
	for i=1, MAX_RACES, 1 do
		local button = _G["CharacterCreateRaceButton"..i];
		button:Enable();
		SetButtonDesaturated(button, false)
	end

	if ( PAID_SERVICE_TYPE ) then
		CustomizeExistingCharacter( PAID_SERVICE_CHARACTER_ID );
		CharacterCreateNameEdit:SetText( PaidChange_GetName() );
	else
		--randomly selects a combination
		ResetCharCustomize();
		CharacterCreateNameEdit:SetText("");
		CharCreateRandomizeButton:Show();
	end

	CharacterCreateEnumerateRaces(GetAvailableRaces());
	SetCharacterRace(GetSelectedRace());

	CharacterCreateEnumerateClasses(GetAvailableClasses());
	CharacterCreate_SelectValidClass();

	SetCharacterGender(GetSelectedSex())

	CharacterCreate_UpdateHairCustomization();

	SetCharacterCreateFacing(-15);

	if ( ALLOW_RANDOM_NAME_BUTTON ) then
		CharacterCreateRandomName:Show();
	end

	CharacterChangeFixup();

	if ( PAID_SERVICE_TYPE == PAID_CHARACTER_CUSTOMIZATION ) then
		CharacterCreate_SetStep(2);
	else
		CharacterCreate_SetStep(1);
	end
end

function CharacterCreate_OnHide()
	PAID_SERVICE_CHARACTER_ID = nil;
	PAID_SERVICE_TYPE = nil;
end

function CharacterCreateFrame_OnMouseDown(button)
	if ( button == "LeftButton" ) then
		CHARACTER_CREATE_ROTATION_START_X = GetCursorPosition();
		CHARACTER_CREATE_INITIAL_FACING = GetCharacterCreateFacing();
	end
end

function CharacterCreateFrame_OnMouseUp(button)
	if ( button == "LeftButton" ) then
		CHARACTER_CREATE_ROTATION_START_X = nil
	end
end

function CharacterCreateFrame_OnUpdate()
	if ( CHARACTER_CREATE_ROTATION_START_X ) then
		local x = GetCursorPosition();
		local diff = (x - CHARACTER_CREATE_ROTATION_START_X) * CHARACTER_ROTATION_CONSTANT;
		CHARACTER_CREATE_ROTATION_START_X = GetCursorPosition();
		SetCharacterCreateFacing(GetCharacterCreateFacing() + diff);
	end
end

function CharacterCreate_OnChar()
end

function CharacterCreate_OnKeyDown(key)
	if ( key == "ESCAPE" ) then
		if ( CharacterCreate.step == 2 ) then
			CharacterCreate_Previous();
		else
			CharacterCreate_Back();
		end
	elseif ( key == "ENTER" ) then
		if ( CharacterCreate.step == 1 ) then
			CharacterCreate_Next();
		else
			CharacterCreate_Okay();
		end
	elseif ( key == "PRINTSCREEN" ) then
		Screenshot();
	end
end

function CharacterCreate_UpdateModel(self)
	UpdateCustomizationScene();
	self:AdvanceTime();
end

function CharacterCreate_Okay()
	if ( PAID_SERVICE_TYPE ) then
		GlueDialog_Show("CONFIRM_PAID_SERVICE");
	else
		CreateCharacter(CharacterCreateNameEdit:GetText());
	end
	PlaySound("gsCharacterCreationCreateChar");
end

function CharacterCreate_Back()
	PlaySound("gsCharacterCreationCancel");
	SetGlueScreen("charselect");
end

-- Races ---------------------------------------------------------------------------------------

local function GenderSuffix()
	if ( GetSelectedSex() == SEX_MALE ) then
		return "MALE";
	end
	return "FEMALE";
end

function CharacterCreateEnumerateRaces(...)
	CharacterCreate.numRaces = select("#", ...)/3;
	if ( CharacterCreate.numRaces > MAX_RACES ) then
		message("Too many races!  Update MAX_RACES");
		return;
	end
	local gender = GenderSuffix();
	local rows = { ["Alliance"] = 0, ["Horde"] = 0 };
	local index = 1;
	for i=1, select("#", ...), 3 do
		local button = _G["CharacterCreateRaceButton"..index];
		local text = _G["CharacterCreateRaceButton"..index.."Text"];
		local coords = RACE_ICON_TCOORDS[strupper(select(i+1, ...).."_"..gender)];
		_G["CharacterCreateRaceButton"..index.."NormalTexture"]:SetTexCoord(coords[1], coords[2], coords[3], coords[4]);
		_G["CharacterCreateRaceButton"..index.."PushedTexture"]:SetTexCoord(coords[1], coords[2], coords[3], coords[4]);

		-- Alliance races stack down the left side, Horde races down the right side.
		local _, faction = GetFactionForRace(index);
		if ( faction ~= "Horde" ) then
			faction = "Alliance";
		end
		local y = RACE_COLUMN_TOP - rows[faction] * RACE_ROW_SPACING;
		rows[faction] = rows[faction] + 1;
		button:ClearAllPoints();
		text:ClearAllPoints();
		if ( faction == "Alliance" ) then
			button:SetPoint("TOPLEFT", CharacterCreateRaceStep, "TOPLEFT", RACE_COLUMN_INSET, y);
			text:SetPoint("LEFT", button, "RIGHT", 16, 0);
			text:SetJustifyH("LEFT");
		else
			button:SetPoint("TOPRIGHT", CharacterCreateRaceStep, "TOPRIGHT", -RACE_COLUMN_INSET, y);
			text:SetPoint("RIGHT", button, "LEFT", -16, 0);
			text:SetJustifyH("RIGHT");
		end

		button:Show();
		button.name = select(i, ...);
		button.faction = faction;
		text:SetText(button.name);
		if ( select(i+2, ...) == 1 ) then
			button.enable = true;
			SetButtonDesaturated(button);
			button.tooltip = select(i, ...);
		else
			button.enable = false;
			SetButtonDesaturated(button, 1);
			button.tooltip = _G[strupper(select(i+1, ...).."_".."DISABLED")];
		end
		index = index + 1;
	end
	for i=CharacterCreate.numRaces + 1, MAX_RACES, 1 do
		_G["CharacterCreateRaceButton"..i]:Hide();
	end
end

function SetCharacterRace(id)
	CharacterCreate.selectedRace = id;
	for i=1, CharacterCreate.numRaces, 1 do
		local button = _G["CharacterCreateRaceButton"..i];
		local text = _G["CharacterCreateRaceButton"..i.."Text"];
		if ( i == id ) then
			button:SetChecked(1);
			text:SetTextColor(1.0, 0.82, 0.0);
		else
			button:SetChecked(0);
			text:SetTextColor(0.8, 0.8, 0.8);
		end
	end

	local _, faction = GetFactionForRace(CharacterCreate.selectedRace);
	local race, fileString = GetNameForRace();
	fileString = strupper(fileString);

	CharacterCreateRaceLabel:SetText(race);
	local coords = RACE_ICON_TCOORDS[fileString.."_"..GenderSuffix()];
	CharacterCreateRaceIcon:SetTexCoord(coords[1], coords[2], coords[3], coords[4]);

	local abilityIndex = 1;
	local tempText = _G["ABILITY_INFO_"..fileString..abilityIndex];
	local abilityText = "";
	while ( tempText ) do
		abilityText = abilityText..tempText.."\n\n";
		abilityIndex = abilityIndex + 1;
		tempText = _G["ABILITY_INFO_"..fileString..abilityIndex];
	end
	CharacterCreateRaceScrollFrameScrollBar:SetValue(0);
	CharacterCreateRaceText:SetText(GetFlavorText("RACE_INFO_"..fileString, GetSelectedSex()).."|n|n");
	CharacterCreateRaceAbilityText:SetText(abilityText);

	-- The race panel takes the faction's colors.
	local color = FACTION_BACKDROP_COLOR_TABLE[faction] or FACTION_BACKDROP_COLOR_TABLE["Alliance"];
	CharacterCreate_SetPanelColor(CharacterCreateRaceInfo, color[1], color[2], color[3]);
	CharacterCreateRaceFaction:SetText(_G[strupper(faction or "")] or "");
	CharacterCreateRaceFaction:SetTextColor(color[1], color[2], color[3]);

	SetBackgroundModel(CharacterCreate, GetCreateBackgroundModel());
end

function CharacterRace_OnClick(self, id)
	PlaySound("gsCharacterCreationClass");
	if ( not self:GetChecked() ) then
		self:SetChecked(1);
		return;
	end
	if ( GetSelectedRace() ~= id ) then
		SetSelectedRace(id);
		SetCharacterRace(id);
		SetSelectedSex(GetSelectedSex());
		SetCharacterCreateFacing(-15);
		CharacterCreateEnumerateClasses(GetAvailableClasses());
		CharacterCreate_SelectValidClass();
		CharacterCreate_UpdateHairCustomization();
		CharacterChangeFixup();
	end
end

-- Class (the Hero) ----------------------------------------------------------------------------

function CharacterCreateEnumerateClasses(...)
	CharacterCreate.numClasses = select("#", ...)/3;
end

-- Keep the selected class valid for the selected race. With the Emberforge client patch the
-- Hero is the only valid class, so this always lands on it.
function CharacterCreate_SelectValidClass()
	local _, _, current = GetSelectedClass();
	if ( PAID_SERVICE_TYPE ) then
		current = PaidChange_GetCurrentClassIndex();
	elseif ( not (current and IsRaceClassValid(GetSelectedRace(), current)) ) then
		for i=1, CharacterCreate.numClasses, 1 do
			if ( IsRaceClassValid(GetSelectedRace(), i) ) then
				current = i;
				SetSelectedClass(i);
				break;
			end
		end
	end
	SetCharacterClass(current);
end

function SetCharacterClass(id)
	CharacterCreate.selectedClass = id;
	local className, classFileName = GetSelectedClass();
	CharacterCreateHeroCardName:SetText(className);
	CharacterCreateClassLabel:SetText(className);

	local abilityIndex = 0;
	local tempText = _G["CLASS_INFO_"..classFileName..abilityIndex];
	local abilityText = "";
	while ( tempText ) do
		abilityText = abilityText..tempText.."\n\n";
		abilityIndex = abilityIndex + 1;
		tempText = _G["CLASS_INFO_"..classFileName..abilityIndex];
	end
	CharacterCreateClassRolesText:SetText(abilityText);
	CharacterCreateClassText:SetText(GetFlavorText("CLASS_"..strupper(classFileName), GetSelectedSex()).."|n|n");
	CharacterCreateClassScrollFrameScrollBar:SetValue(0);
end

-- Gender --------------------------------------------------------------------------------------

function SetCharacterGender(sex)
	SetSelectedSex(sex);
	if ( sex == SEX_MALE ) then
		CharacterCreateGender:SetText(MALE);
		CharacterCreateGenderButtonMale:SetChecked(1);
		CharacterCreateGenderButtonFemale:SetChecked(nil);
	else
		CharacterCreateGender:SetText(FEMALE);
		CharacterCreateGenderButtonMale:SetChecked(nil);
		CharacterCreateGenderButtonFemale:SetChecked(1);
	end

	-- Race portraits follow the gender.
	CharacterCreateEnumerateRaces(GetAvailableRaces());
	SetCharacterRace(GetSelectedRace());
	CharacterCreateEnumerateClasses(GetAvailableClasses());
	CharacterCreate_SelectValidClass();
	CharacterCreate_UpdateHairCustomization();
	CharacterChangeFixup();
end

-- Customization -------------------------------------------------------------------------------

function CharacterCustomization_Left(id)
	PlaySound("gsCharacterCreationLook");
	CycleCharCustomization(id, -1);
end

function CharacterCustomization_Right(id)
	PlaySound("gsCharacterCreationLook");
	CycleCharCustomization(id, 1);
end

function CharacterCreate_Randomize()
	PlaySound("gsCharacterCreationLook");
	RandomizeCharCustomization();
end

function CharacterCreate_UpdateHairCustomization()
	CharacterCustomizationButtonFrame3Text:SetText(_G["HAIR_"..GetHairCustomization().."_STYLE"]);
	CharacterCustomizationButtonFrame4Text:SetText(_G["HAIR_"..GetHairCustomization().."_COLOR"]);
	CharacterCustomizationButtonFrame5Text:SetText(_G["FACIAL_HAIR_"..GetFacialHairCustomization()]);
end

-- Step 2 summary card: "<race icon> Human Hero".
function CharacterCreate_UpdateSummary()
	local race, fileString = GetNameForRace();
	local className = GetSelectedClass();
	local coords = RACE_ICON_TCOORDS[strupper(fileString).."_"..GenderSuffix()];
	CharacterCreateSummaryIcon:SetTexCoord(coords[1], coords[2], coords[3], coords[4]);
	CharacterCreateSummaryText:SetText(race.." "..className);
end

-- Helpers -------------------------------------------------------------------------------------

function SetButtonDesaturated(button, desaturated, r, g, b)
	if ( not button ) then
		return;
	end
	local icon = button:GetNormalTexture();
	if ( not icon ) then
		return;
	end
	local shaderSupported = icon:SetDesaturated(desaturated);

	if ( not desaturated ) then
		r = 1.0;
		g = 1.0;
		b = 1.0;
	elseif ( not r or not shaderSupported ) then
		r = 0.5;
		g = 0.5;
		b = 0.5;
	end

	icon:SetVertexColor(r, g, b);
end

function GetFlavorText(tagname, sex)
	local primary, secondary;
	if ( sex == SEX_MALE ) then
		primary = "";
		secondary = "_FEMALE";
	else
		primary = "_FEMALE";
		secondary = "";
	end
	local text = _G[tagname..primary];
	if ( (text == nil) or (text == "") ) then
		text = _G[tagname..secondary];
	end
	return text;
end

-- Paid race / faction change and recustomization only allow some races.
function CharacterChangeFixup()
	if ( not PAID_SERVICE_TYPE ) then
		return;
	end
	for i=1, MAX_RACES, 1 do
		local allow = false;
		if ( PAID_SERVICE_TYPE == PAID_FACTION_CHANGE ) then
			local faction = GetFactionForRace(PaidChange_GetCurrentRaceIndex());
			if ( (i == PaidChange_GetCurrentRaceIndex()) or ((GetFactionForRace(i) ~= faction) and (IsRaceClassValid(i,CharacterCreate.selectedClass))) ) then
				allow = true;
			end
		elseif ( PAID_SERVICE_TYPE == PAID_RACE_CHANGE ) then
			local faction = GetFactionForRace(PaidChange_GetCurrentRaceIndex());
			if ( (i == PaidChange_GetCurrentRaceIndex()) or ((GetFactionForRace(i) == faction) and (IsRaceClassValid(i,CharacterCreate.selectedClass))) ) then
				allow = true
			end
		elseif ( PAID_SERVICE_TYPE == PAID_CHARACTER_CUSTOMIZATION ) then
			if ( i == CharacterCreate.selectedRace ) then
				allow = true
			end
		end
		if (not allow) then
			local button = _G["CharacterCreateRaceButton"..i];
			button:Disable();
			SetButtonDesaturated(button, true)
		end
	end
end

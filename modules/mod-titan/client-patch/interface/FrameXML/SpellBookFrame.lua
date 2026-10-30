MAX_SPELLS = 1024;
MAX_SKILLLINE_TABS = 12;	-- Emberforge: General and up to 11 classes (below)
SPELLS_PER_PAGE = 12;
MAX_SPELL_PAGES = ceil(MAX_SPELLS / SPELLS_PER_PAGE);
BOOKTYPE_SPELL = "spell";
BOOKTYPE_PET = "pet";
SPELLBOOK_PAGENUMBERS = {};

local ceil = ceil;
local strlen = strlen;
local tinsert = tinsert;
local tremove = tremove;

-- Emberforge: a Hero learns every class's abilities, none of their skill lines, so the client files them all
-- under General. The tabs are General and one per class the Hero has abilities of instead: every spell of the
-- client's tabs, filed by EMBERFORGE_SPELL_CLASSES (spell id -> class token, EmberforgeSpellClasses.lua, made
-- from the game's class skill lines when the patch is built). A tab holds spellbook slots (the highest-rank
-- ones without "Show all spell ranks"); the spell buttons get a slot and its place in the tab
-- (SpellBook_GetSpellID), as they got the client tab's offset before.
local CLASS_TABS = {
	{ "WARRIOR", "Warrior" }, { "PALADIN", "Paladin" }, { "HUNTER", "Hunter" }, { "ROGUE", "Rogue" },
	{ "PRIEST", "Priest" }, { "DEATHKNIGHT", "Death Knight" }, { "SHAMAN", "Shaman" }, { "MAGE", "Mage" },
	{ "WARLOCK", "Warlock" }, { "MONK", "Monk", "Interface\\Icons\\Spell_Nature_GiftoftheWild" },
	{ "DRUID", "Druid" }, { "DEMONHUNTER", "Demon Hunter", "Interface\\Icons\\Ability_DemonHunter_Metamorphosis" },
};
local CLASS_ICONS = "Interface\\Glues\\CharacterCreate\\UI-CharacterCreate-Classes";
local FULL_TEXTURE = { 0, 1, 0, 1 };
local spellTabs = {};

function SpellBook_BuildTabs()
	local showAllRanks = GetCVarBool("ShowAllSpellRanks");
	local general, byClass = {}, {};
	for tab = 1, GetNumSpellTabs() do
		local _, _, offset, numSpells, highestRankOffset, highestRankNumSpells = GetSpellTabInfo(tab);
		if ( not showAllRanks ) then
			offset, numSpells = highestRankOffset, highestRankNumSpells;
		end
		for slot = offset + 1, offset + numSpells do
			local known = showAllRanks and slot or GetKnownSlotFromHighestRankSlot(slot);
			local link = GetSpellLink(known, BOOKTYPE_SPELL);
			local class = link and EMBERFORGE_SPELL_CLASSES[tonumber(link:match("spell:(%d+)"))];
			if ( class ) then
				byClass[class] = byClass[class] or {};
				tinsert(byClass[class], known);
			else
				tinsert(general, known);
			end
		end
	end

	wipe(spellTabs);
	local name, texture = GetSpellTabInfo(1);
	tinsert(spellTabs, { name = name, texture = texture, slots = general });
	for _, class in ipairs(CLASS_TABS) do
		local token, className, icon = unpack(class);
		if ( byClass[token] ) then
			tinsert(spellTabs, { name = className, texture = icon or CLASS_ICONS,
				coords = icon and FULL_TEXTURE or CLASS_ICON_TCOORDS[token], slots = byClass[token] });
		end
	end
	if ( not spellTabs[SpellBookFrame.selectedSkillLine or 1] ) then
		SpellBookFrame.selectedSkillLine = 1;
	end
end

-- The four tabs the frame lacks, and all of them closer together so that twelve fit down its side.
local function SpellBook_AddTabs()
	for i = 9, MAX_SKILLLINE_TABS do
		local tab = CreateFrame("CheckButton", "SpellBookSkillLineTab"..i, SpellBookFrame,
			"SpellBookSkillLineTabTemplate");
		tab:SetID(i);
		local flash = SpellBookTabFlashFrame:CreateTexture("SpellBookSkillLineTab"..i.."Flash", "OVERLAY");
		flash:SetTexture("Interface\\Buttons\\CheckButtonGlow");
		flash:SetBlendMode("ADD");
		flash:SetWidth(64);
		flash:SetHeight(64);
		flash:SetPoint("CENTER", tab);
		flash:Hide();
	end
	for i = 2, MAX_SKILLLINE_TABS do
		local tab = _G["SpellBookSkillLineTab"..i];
		tab:ClearAllPoints();
		tab:SetPoint("TOPLEFT", _G["SpellBookSkillLineTab"..(i - 1)], "BOTTOMLEFT", 0, -6);
	end
end

function ToggleSpellBook(bookType)
	if ( not HasPetSpells() and bookType == BOOKTYPE_PET ) then
		return;
	end
	
	local isShown = SpellBookFrame:IsShown();
	if ( isShown and (SpellBookFrame.bookType ~= bookType) ) then
		SpellBookFrame.suppressCloseSound = true;
	end
	
	HideUIPanel(SpellBookFrame);
	if ( (not isShown or (SpellBookFrame.bookType ~= bookType)) ) then
		SpellBookFrame.bookType = bookType;
		ShowUIPanel(SpellBookFrame);
	end
	SpellBookFrame_UpdatePages();

	SpellBookFrame.suppressCloseSound = nil;
end

function SpellBookFrame_OnLoad(self)
	self:RegisterEvent("SPELLS_CHANGED");
	self:RegisterEvent("LEARNED_SPELL_IN_TAB");

	SpellBookFrame.bookType = BOOKTYPE_SPELL;
	-- Init page nums
	SPELLBOOK_PAGENUMBERS[1] = 1;
	SPELLBOOK_PAGENUMBERS[2] = 1;
	SPELLBOOK_PAGENUMBERS[3] = 1;
	SPELLBOOK_PAGENUMBERS[4] = 1;
	SPELLBOOK_PAGENUMBERS[5] = 1;
	SPELLBOOK_PAGENUMBERS[6] = 1;
	SPELLBOOK_PAGENUMBERS[7] = 1;
	SPELLBOOK_PAGENUMBERS[8] = 1;
	SPELLBOOK_PAGENUMBERS[BOOKTYPE_PET] = 1;
	SpellBook_AddTabs();
	for i = 9, MAX_SKILLLINE_TABS do
		SPELLBOOK_PAGENUMBERS[i] = 1;
	end
	
	-- Set to the first tab by default
	SpellBookSkillLineTab_OnClick(nil, 1);

	-- Initialize tab flashing
	SpellBookFrame.flashTabs = nil;
end

function SpellBookFrame_OnEvent(self, event, ...)
	if ( event == "SPELLS_CHANGED" ) then
		if ( SpellBookFrame:IsVisible() ) then
			SpellBookFrame_Update();
		end
	elseif ( event == "LEARNED_SPELL_IN_TAB" ) then
		-- Emberforge: the client's tab is not ours; the book's own tab (General) flashes.
		local flashFrame = _G["SpellBookSkillLineTab1Flash"];
		if ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
			return;
		else
			if ( flashFrame ) then
				flashFrame:Show();
				SpellBookFrame.flashTabs = 1;
			end
		end
	end
end

function SpellBookFrame_OnShow(self)
	SpellBookFrame_Update(1);
	
	-- If there are tabs waiting to flash, then flash them... yeah..
	if ( self.flashTabs ) then
		UIFrameFlash(SpellBookTabFlashFrame, 0.5, 0.5, 30, nil);
	end

	-- Show multibar slots
	MultiActionBar_ShowAllGrids();
	UpdateMicroButtons();

	SpellBookFrame_PlayOpenSound();
end

function SpellBookFrame_Update(showing)
	-- Hide all tabs
	SpellBookFrameTabButton1:Hide();
	SpellBookFrameTabButton2:Hide();
	SpellBookFrameTabButton3:Hide();
	
	-- Setup skillline tabs
	SpellBook_BuildTabs();
	if ( showing ) then
		SpellBookSkillLineTab_OnClick(nil, SpellBookFrame.selectedSkillLine);
		UpdateSpells();
	end

	local numSkillLineTabs = #spellTabs;
	local name, texture, offset, numSpells;
	local skillLineTab;
	for i=1, MAX_SKILLLINE_TABS do
		skillLineTab = _G["SpellBookSkillLineTab"..i];
		if ( i <= numSkillLineTabs and SpellBookFrame.bookType == BOOKTYPE_SPELL ) then
			name, texture = spellTabs[i].name, spellTabs[i].texture;
			skillLineTab:SetNormalTexture(texture);
			skillLineTab:GetNormalTexture():SetTexCoord(unpack(spellTabs[i].coords or FULL_TEXTURE));
			skillLineTab.tooltip = name;
			skillLineTab:Show();

			-- Set the selected tab
			if ( SpellBookFrame.selectedSkillLine == i ) then
				skillLineTab:SetChecked(1);
			else
				skillLineTab:SetChecked(nil);
			end
		else
			_G["SpellBookSkillLineTab"..i.."Flash"]:Hide();
			skillLineTab:Hide();
		end
	end

	-- Setup tabs
	local hasPetSpells, petToken = HasPetSpells();
	SpellBookFrame.petTitle = nil;
	if ( hasPetSpells ) then
		SpellBookFrame_SetTabType(SpellBookFrameTabButton1, BOOKTYPE_SPELL);
		SpellBookFrame_SetTabType(SpellBookFrameTabButton2, BOOKTYPE_PET, petToken);
	else
		SpellBookFrame_SetTabType(SpellBookFrameTabButton1, BOOKTYPE_SPELL);

		if ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
			-- if has no pet spells but trying to show the pet spellbook close the window;
			HideUIPanel(SpellBookFrame);
			SpellBookFrame.bookType = BOOKTYPE_SPELL;
		end
	end

	if ( SpellBookFrame.bookType == BOOKTYPE_SPELL ) then
		SpellBookTitleText:SetText(SPELLBOOK);
		SpellBookFrame_ShowSpells();
		SpellBookFrame_UpdatePages();
	elseif ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
		SpellBookTitleText:SetText(SpellBookFrame.petTitle);
		SpellBookFrame_ShowSpells();
		SpellBookFrame_UpdatePages();
	end
end

function SpellBookFrame_HideSpells ()
	for i = 1, SPELLS_PER_PAGE do
		_G["SpellButton" .. i]:Hide();
	end
	
	for i = 1, MAX_SKILLLINE_TABS do
		_G["SpellBookSkillLineTab" .. i]:Hide();
	end
	
	SpellBookPrevPageButton:Hide();
	SpellBookNextPageButton:Hide();
	SpellBookPageText:Hide();
end

function SpellBookFrame_ShowSpells ()
	for i = 1, SPELLS_PER_PAGE do
		_G["SpellButton" .. i]:Show();
	end
	
	SpellBookPrevPageButton:Show();
	SpellBookNextPageButton:Show();
	SpellBookPageText:Show();
end

function SpellBookFrame_UpdatePages()
	local currentPage, maxPages = SpellBook_GetCurrentPage();
	if ( maxPages == 0 ) then
		return;
	end
	if ( currentPage > maxPages ) then
		if ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
			SPELLBOOK_PAGENUMBERS[BOOKTYPE_PET] = maxPages;
		else
			SPELLBOOK_PAGENUMBERS[SpellBookFrame.selectedSkillLine] = maxPages;
		end
		currentPage = maxPages;
		UpdateSpells();
		if ( currentPage == 1 ) then
			SpellBookPrevPageButton:Disable();
		else
			SpellBookPrevPageButton:Enable();
		end
		if ( currentPage == maxPages ) then
			SpellBookNextPageButton:Disable();
		else
			SpellBookNextPageButton:Enable();
		end
	end
	if ( currentPage == 1 ) then
		SpellBookPrevPageButton:Disable();
	else
		SpellBookPrevPageButton:Enable();
	end
	if ( currentPage == maxPages ) then
		SpellBookNextPageButton:Disable();
	else
		SpellBookNextPageButton:Enable();
	end
	SpellBookPageText:SetFormattedText(PAGE_NUMBER, currentPage);
	-- Hide spell rank checkbox if the player is a rogue or warrior
	local _, class = UnitClass("player");
	local showSpellRanks = true;
	if ( class == "ROGUE" or class == "WARRIOR" ) then
		showSpellRanks = false;
	end
	if ( SpellBookFrame.bookType == BOOKTYPE_SPELL and showSpellRanks ) then
		ShowAllSpellRanksCheckBox:Show();
	else
		ShowAllSpellRanksCheckBox:Hide();
	end
end

function SpellBookFrame_SetTabType(tabButton, bookType, token)
	if ( bookType == BOOKTYPE_SPELL ) then
		tabButton.bookType = BOOKTYPE_SPELL;
		tabButton:SetText(SPELLBOOK);
		tabButton:SetFrameLevel(SpellBookFrame:GetFrameLevel() + 1);
		tabButton.binding = "TOGGLESPELLBOOK";
	elseif ( bookType == BOOKTYPE_PET ) then
		tabButton.bookType = BOOKTYPE_PET;
		tabButton:SetText(_G["PET_TYPE_"..token]);
		tabButton:SetFrameLevel(SpellBookFrame:GetFrameLevel() + 1);
		tabButton.binding = "TOGGLEPETBOOK";
		SpellBookFrame.petTitle = _G["PET_TYPE_"..token];
	else
		tabButton.bookType = INSCRIPTION;
		tabButton:SetText(GLYPHS);
		tabButton:SetFrameLevel(SpellBookFrame:GetFrameLevel() + 2);
		tabButton.binding = "TOGGLEINSCRIPTION";
	end
	if ( SpellBookFrame.bookType == bookType ) then
		tabButton:Disable();
	else
		tabButton:Enable();
	end
	tabButton:Show();
end

function SpellBookFrame_PlayOpenSound()
	if ( SpellBookFrame.bookType == BOOKTYPE_SPELL ) then
		PlaySound("igSpellBookOpen");
	elseif ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
		-- Need to change to pet book open sound
		PlaySound("igAbilityOpen");
	else
		PlaySound("igSpellBookOpen");
	end
end

function SpellBookFrame_PlayCloseSound()
	if ( not SpellBookFrame.suppressCloseSound ) then
		if ( SpellBookFrame.bookType == BOOKTYPE_SPELL ) then
			PlaySound("igSpellBookClose");
		else
			-- Need to change to pet book close sound
			PlaySound("igAbilityClose");
		end
	end
end

function SpellBookFrame_OnHide(self)
	SpellBookFrame_PlayCloseSound();

	-- Stop the flash frame from flashing if its still flashing.. flash flash flash
	UIFrameFlashRemoveFrame(SpellBookTabFlashFrame);
	-- Hide all the flashing textures
	for i=1, MAX_SKILLLINE_TABS do
		_G["SpellBookSkillLineTab"..i.."Flash"]:Hide();
	end

	-- Hide multibar slots
	MultiActionBar_HideAllGrids();
	
	-- Do this last, it can cause taint.
	UpdateMicroButtons();
end

function SpellButton_OnLoad(self) 
	self:RegisterForDrag("LeftButton");
	self:RegisterForClicks("LeftButtonUp", "RightButtonUp");
end

function SpellButton_OnEvent(self, event, ...)
	if ( event == "SPELLS_CHANGED" or event == "SPELL_UPDATE_COOLDOWN" or event == "UPDATE_SHAPESHIFT_FORM" ) then
		-- need to listen for UPDATE_SHAPESHIFT_FORM because attack icons change when the shapeshift form changes
		SpellButton_UpdateButton(self);
	elseif ( event == "CURRENT_SPELL_CAST_CHANGED" ) then
		SpellButton_UpdateSelection(self);
	elseif ( event == "TRADE_SKILL_SHOW" or event == "TRADE_SKILL_CLOSE" ) then
		SpellButton_UpdateSelection(self);
	elseif ( event == "PET_BAR_UPDATE" ) then
		if ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
			SpellButton_UpdateButton(self);
		end
	end
end

function SpellButton_OnShow(self)
	self:RegisterEvent("SPELLS_CHANGED");
	self:RegisterEvent("SPELL_UPDATE_COOLDOWN");
	self:RegisterEvent("UPDATE_SHAPESHIFT_FORM");
	self:RegisterEvent("CURRENT_SPELL_CAST_CHANGED");
	self:RegisterEvent("TRADE_SKILL_SHOW");
	self:RegisterEvent("TRADE_SKILL_CLOSE");
	self:RegisterEvent("PET_BAR_UPDATE");

	SpellButton_UpdateButton(self);
end

function SpellButton_OnHide(self)
	self:UnregisterEvent("SPELLS_CHANGED");
	self:UnregisterEvent("SPELL_UPDATE_COOLDOWN");
	self:UnregisterEvent("UPDATE_SHAPESHIFT_FORM");
	self:UnregisterEvent("CURRENT_SPELL_CAST_CHANGED");
	self:UnregisterEvent("TRADE_SKILL_SHOW");
	self:UnregisterEvent("TRADE_SKILL_CLOSE");
	self:UnregisterEvent("PET_BAR_UPDATE");
end
 
function SpellButton_OnEnter(self)
	local id = SpellBook_GetSpellID(self:GetID());
	GameTooltip:SetOwner(self, "ANCHOR_RIGHT");
	if ( GameTooltip:SetSpell(id, SpellBookFrame.bookType) ) then
		self.UpdateTooltip = SpellButton_OnEnter;
	else
		self.UpdateTooltip = nil;
	end
end

function SpellButton_OnClick(self, button) 
	local id = SpellBook_GetSpellID(self:GetID());
	if ( id > MAX_SPELLS ) then
		return;
	end
	if ( button ~= "LeftButton" and SpellBookFrame.bookType == BOOKTYPE_PET ) then
		ToggleSpellAutocast(id, SpellBookFrame.bookType);
	else
		CastSpell(id, SpellBookFrame.bookType);
		SpellButton_UpdateSelection(self);
	end
end

function SpellButton_OnModifiedClick(self, button) 
	local id = SpellBook_GetSpellID(self:GetID());
	if ( id > MAX_SPELLS ) then
		return;
	end
	if ( IsModifiedClick("CHATLINK") ) then
		if ( MacroFrame and MacroFrame:IsShown() ) then
			local spellName, subSpellName = GetSpellName(id, SpellBookFrame.bookType);
			if ( spellName and not IsPassiveSpell(id, SpellBookFrame.bookType) ) then
				if ( subSpellName and (strlen(subSpellName) > 0) ) then
					ChatEdit_InsertLink(spellName.."("..subSpellName..")");
				else
					ChatEdit_InsertLink(spellName);
				end
			end
			return;
		else
			local spellLink, tradeSkillLink = GetSpellLink(id, SpellBookFrame.bookType);
			if ( tradeSkillLink ) then
				ChatEdit_InsertLink(tradeSkillLink);
			elseif ( spellLink ) then
				ChatEdit_InsertLink(spellLink);
			end
			return;
		end
	end
	if ( IsModifiedClick("PICKUPACTION") ) then
		PickupSpell(id, SpellBookFrame.bookType);
		return;
	end
	if ( IsModifiedClick("SELFCAST") ) then
		CastSpell(id, SpellBookFrame.bookType, true);
		return;
	end
end

function SpellButton_OnDrag(self) 
	local id = SpellBook_GetSpellID(self:GetID());
	if ( id > MAX_SPELLS or not _G[self:GetName().."IconTexture"]:IsShown() ) then
		return;
	end
	self:SetChecked(0);
	PickupSpell(id, SpellBookFrame.bookType);
end

function SpellButton_UpdateSelection(self)
	local temp, texture, offset, numSpells = SpellBook_GetTabInfo(SpellBookFrame.selectedSkillLine);
	
	local id, displayID = SpellBook_GetSpellID(self:GetID());
	if ( (SpellBookFrame.bookType ~= BOOKTYPE_PET) and (not displayID or displayID > (offset + numSpells)) ) then
		self:SetChecked("false");
		return;
	end

	if ( IsSelectedSpell(id, SpellBookFrame.bookType) ) then
		self:SetChecked("true");
	else
		self:SetChecked("false");
	end
end

function SpellButton_UpdateButton(self)
	if ( not SpellBookFrame.selectedSkillLine ) then
		SpellBookFrame.selectedSkillLine = 1;
	end
	local temp, texture, offset, numSpells = SpellBook_GetTabInfo(SpellBookFrame.selectedSkillLine);
	SpellBookFrame.selectedSkillLineOffset = offset;

	local id, displayID = SpellBook_GetSpellID(self:GetID());
	local name = self:GetName();
	local iconTexture = _G[name.."IconTexture"];
	local spellString = _G[name.."SpellName"];
	local subSpellString = _G[name.."SubSpellName"];
	local cooldown = _G[name.."Cooldown"];
	local autoCastableTexture = _G[name.."AutoCastable"];

	if ( (SpellBookFrame.bookType ~= BOOKTYPE_PET) and (not displayID or displayID > (offset + numSpells)) ) then
		self:Disable();
		iconTexture:Hide();
		spellString:Hide();
		subSpellString:Hide();
		cooldown:Hide();
		autoCastableTexture:Hide();
		SpellBook_ReleaseAutoCastShine(self.shine)
		self.shine = nil;
		self:SetChecked(0);
		_G[name.."NormalTexture"]:SetVertexColor(1.0, 1.0, 1.0);
		return;
	else
		self:Enable();
	end

	local texture = GetSpellTexture(id, SpellBookFrame.bookType);
	local highlightTexture = _G[name.."Highlight"];
	local normalTexture = _G[name.."NormalTexture"];
	-- If no spell, hide everything and return
	if ( not texture or (strlen(texture) == 0) ) then
		iconTexture:Hide();
		spellString:Hide();
		subSpellString:Hide();
		cooldown:Hide();
		autoCastableTexture:Hide();
		SpellBook_ReleaseAutoCastShine(self.shine);
		self.shine = nil;
		highlightTexture:SetTexture("Interface\\Buttons\\ButtonHilight-Square");
		self:SetChecked(0);
		normalTexture:SetVertexColor(1.0, 1.0, 1.0);
		return;
	end

	local start, duration, enable = GetSpellCooldown(id, SpellBookFrame.bookType);
	CooldownFrame_SetTimer(cooldown, start, duration, enable);
	if ( enable == 1 ) then
		iconTexture:SetVertexColor(1.0, 1.0, 1.0);
	else
		iconTexture:SetVertexColor(0.4, 0.4, 0.4);
	end

	local autoCastAllowed, autoCastEnabled = GetSpellAutocast(id, SpellBookFrame.bookType);
	if ( autoCastAllowed ) then
		autoCastableTexture:Show();
	else
		autoCastableTexture:Hide();
	end
	if ( autoCastEnabled and not self.shine ) then
		self.shine = SpellBook_GetAutoCastShine();
		self.shine:Show();
		self.shine:SetParent(self);
		self.shine:SetPoint("CENTER", self, "CENTER");
		AutoCastShine_AutoCastStart(self.shine);
	elseif ( autoCastEnabled ) then
		self.shine:Show();
		self.shine:SetParent(self);
		self.shine:SetPoint("CENTER", self, "CENTER");
		AutoCastShine_AutoCastStart(self.shine);
	elseif ( not autoCastEnabled ) then
		SpellBook_ReleaseAutoCastShine(self.shine);
		self.shine = nil;
	end

	local spellName, subSpellName = GetSpellName(id, SpellBookFrame.bookType);
	local isPassive = IsPassiveSpell(id, SpellBookFrame.bookType);
	if ( isPassive ) then
		normalTexture:SetVertexColor(0, 0, 0);
		highlightTexture:SetTexture("Interface\\Buttons\\UI-PassiveHighlight");
		--subSpellName = PASSIVE_PARENS;
		spellString:SetTextColor(PASSIVE_SPELL_FONT_COLOR.r, PASSIVE_SPELL_FONT_COLOR.g, PASSIVE_SPELL_FONT_COLOR.b);
	else
		normalTexture:SetVertexColor(1.0, 1.0, 1.0);
		highlightTexture:SetTexture("Interface\\Buttons\\ButtonHilight-Square");
		spellString:SetTextColor(NORMAL_FONT_COLOR.r, NORMAL_FONT_COLOR.g, NORMAL_FONT_COLOR.b);
	end
	iconTexture:SetTexture(texture);
	spellString:SetText(spellName);
	subSpellString:SetText(subSpellName);
	if ( subSpellName ~= "" ) then
		spellString:SetPoint("LEFT", self, "RIGHT", 4, 4);
	else
		spellString:SetPoint("LEFT", self, "RIGHT", 4, 2);
	end

	iconTexture:Show();
	spellString:Show();
	subSpellString:Show();
	SpellButton_UpdateSelection(self);
end

function SpellBookPrevPageButton_OnClick()
	local pageNum = SpellBook_GetCurrentPage() - 1;
	if ( SpellBookFrame.bookType == BOOKTYPE_SPELL ) then
		PlaySound("igAbiliityPageTurn");
		SPELLBOOK_PAGENUMBERS[SpellBookFrame.selectedSkillLine] = pageNum;
	else
		SpellBookTitleText:SetText(SpellBookFrame.petTitle);
		-- Need to change to pet book pageturn sound
		PlaySound("igAbiliityPageTurn");
		SPELLBOOK_PAGENUMBERS[BOOKTYPE_PET] = pageNum;
	end
	SpellBook_UpdatePageArrows();
	SpellBookPageText:SetFormattedText(PAGE_NUMBER, pageNum);
	UpdateSpells();
	
end

function SpellBookNextPageButton_OnClick()
	local pageNum = SpellBook_GetCurrentPage() + 1;
	if ( SpellBookFrame.bookType == BOOKTYPE_SPELL ) then
		PlaySound("igAbiliityPageTurn");
		SPELLBOOK_PAGENUMBERS[SpellBookFrame.selectedSkillLine] = pageNum;
	else
		SpellBookTitleText:SetText(SpellBookFrame.petTitle);
		-- Need to change to pet book pageturn sound
		PlaySound("igAbiliityPageTurn");
		SPELLBOOK_PAGENUMBERS[BOOKTYPE_PET] = pageNum;
	end
	SpellBook_UpdatePageArrows();
	SpellBookPageText:SetFormattedText(PAGE_NUMBER, pageNum);
	UpdateSpells();
	
end

function SpellBookSkillLineTab_OnClick(self, id)
	local update;
	if ( not id ) then
		update = 1;
		id = self:GetID();
	end
	if ( SpellBookFrame.selectedSkillLine ~= id ) then
		PlaySound("igAbiliityPageTurn");
	end
	SpellBookFrame.selectedSkillLine = id;
	local name, texture, offset, numSpells = SpellBook_GetTabInfo(SpellBookFrame.selectedSkillLine);
	SpellBookFrame.selectedSkillLineOffset = offset;
	SpellBookFrame.selectedSkillLineNumSpells = numSpells;
	SpellBook_UpdatePageArrows();
	SpellBookFrame_Update();
	SpellBookPageText:SetFormattedText(PAGE_NUMBER, SpellBook_GetCurrentPage());
	if ( update ) then
		UpdateSpells();
	end
	-- Stop tab flashing
	if ( self ) then
		local tabFlash = _G[self:GetName().."Flash"];
		if ( tabFlash ) then
			tabFlash:Hide();
		end
	end
end

function SpellBookFrameTabButton_OnClick(self)
	-- suppress the hiding sound so we don't play a hide and show sound simultaneously
	SpellBookFrame.suppressCloseSound = true;
	ToggleSpellBook(self.bookType, true);
	SpellBookFrame.suppressCloseSound = false;
end

function SpellBook_GetSpellID(id)
	if ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
		return id + (SPELLS_PER_PAGE * (SPELLBOOK_PAGENUMBERS[BOOKTYPE_PET] - 1));
	else
		-- Emberforge: the slot at that place in the tab (past its end, a slot that is none), and the place.
		local index = id + ( SPELLS_PER_PAGE * (SPELLBOOK_PAGENUMBERS[SpellBookFrame.selectedSkillLine] - 1));
		local tab = spellTabs[SpellBookFrame.selectedSkillLine];
		return tab and tab.slots[index] or MAX_SPELLS + 1, index;
	end
end

function SpellBook_UpdatePageArrows()
	local currentPage, maxPages = SpellBook_GetCurrentPage();
	if ( currentPage == 1 ) then
		SpellBookPrevPageButton:Disable();
	else
		SpellBookPrevPageButton:Enable();
	end
	if ( currentPage == maxPages ) then
		SpellBookNextPageButton:Disable();
	else
		SpellBookNextPageButton:Enable();
	end
end

function SpellBook_GetCurrentPage()
	local currentPage, maxPages;
	local numPetSpells = HasPetSpells();
	if ( SpellBookFrame.bookType == BOOKTYPE_PET ) then
		currentPage = SPELLBOOK_PAGENUMBERS[BOOKTYPE_PET];
		maxPages = ceil(numPetSpells/SPELLS_PER_PAGE);
	else
		currentPage = SPELLBOOK_PAGENUMBERS[SpellBookFrame.selectedSkillLine];
		local name, texture, offset, numSpells = SpellBook_GetTabInfo(SpellBookFrame.selectedSkillLine);
		maxPages = ceil(numSpells/SPELLS_PER_PAGE);
	end
	return currentPage, maxPages;
end

local maxShines = 1;
shineGet = {}
function SpellBook_GetAutoCastShine ()
	local shine = shineGet[1];
	
	if ( shine ) then
		tremove(shineGet, 1);
	else
		shine = CreateFrame("FRAME", "AutocastShine" .. maxShines, SpellBookFrame, "SpellBookShineTemplate");
		maxShines = maxShines + 1;
	end
	
	return shine;
end

function SpellBook_ReleaseAutoCastShine (shine)
	if ( not shine ) then
		return;
	end
	
	shine:Hide();
	AutoCastShine_AutoCastStop(shine);
	tinsert(shineGet, shine);
end

-- Emberforge: our tabs (SpellBook_BuildTabs), their places counted from 0.
function SpellBook_GetTabInfo(skillLine)
	local tab = spellTabs[skillLine];
	if ( not tab ) then
		return nil, nil, 0, 0;
	end
	return tab.name, tab.texture, 0, #tab.slots;
end

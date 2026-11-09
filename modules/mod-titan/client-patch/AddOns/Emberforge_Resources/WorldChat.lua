-- World chat: everyone joins the World channel at login, shown in the main chat window; talk in it with /world
-- (or its channel number). Both factions talk in it wherever they are (AllowTwoSide.Interaction.Channel,
-- docker-compose.yml).

local CHANNEL = "World"

local function JoinWorld()
    if GetChannelName(CHANNEL) == 0 then
        JoinChannelByName(CHANNEL)
    end
    ChatFrame_AddChannel(DEFAULT_CHAT_FRAME, CHANNEL)
end

-- A few seconds after entering the world: the zone channels join first and keep their numbers.
local frame = CreateFrame("Frame")
local wait
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:SetScript("OnEvent", function(self)
    self:UnregisterEvent("PLAYER_ENTERING_WORLD")
    wait = 3
end)
frame:SetScript("OnUpdate", function(_, elapsed)
    if not wait then
        return
    end
    wait = wait - elapsed
    if wait <= 0 then
        wait = nil
        JoinWorld()
    end
end)

-- /world <message>
SLASH_EMBERFORGEWORLD1 = "/world"
SlashCmdList.EMBERFORGEWORLD = function(message)
    local id = GetChannelName(CHANNEL)
    if id == 0 then
        JoinWorld()
        id = GetChannelName(CHANNEL)
    end
    if id > 0 and message and message ~= "" then
        SendChatMessage(message, "CHANNEL", nil, id)
    end
end

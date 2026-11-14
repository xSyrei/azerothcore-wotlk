-- Emberforge Runes' first file: /runes exists even if the rest fails to load, and says why.
--
-- The rest of the addon (Catalog.lua, Art.lua, Emberforge_Runes.lua) sets EmberforgeRunes_Loaded at its end and
-- replaces this /runes with its own. Until then, any error raised in this addon's files is kept here, and /runes
-- prints it (and asks the server for the rune state, which a loaded panel would show).

local PREFIX = "EFRN"
local errors = {}

local previous = geterrorhandler and geterrorhandler()
if seterrorhandler and previous then
    seterrorhandler(function(message, ...)
        if type(message) == "string" and message:find("Emberforge_Runes", 1, true) and #errors < 5 then
            errors[#errors + 1] = message
        end
        return previous(message, ...)
    end)
end

local function Say(text)
    if DEFAULT_CHAT_FRAME then
        DEFAULT_CHAT_FRAME:AddMessage("|cffa335eeLegendary Runes:|r " .. text)
    end
end

SLASH_EMBERFORGERUNES1 = "/runes"
SLASH_EMBERFORGERUNES2 = "/rune"
SlashCmdList["EMBERFORGERUNES"] = function()
    if EmberforgeRunes_Loaded then
        return
    end
    Say("the rune panel did not load. Please send this to the developers:")
    if #errors == 0 then
        Say("no error was recorded (is the addon up to date and enabled?)")
    end
    for _, message in ipairs(errors) do
        Say(message)
    end
    if SendAddonMessage and UnitName then
        SendAddonMessage(PREFIX, "STATE", "WHISPER", UnitName("player"))
    end
end

EmberforgeRunes_LoadErrors = errors

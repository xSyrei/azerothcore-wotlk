-- Emberforge Resources: rounded, gold-edged boxes (the Classless window's look), shared by the character window's
-- pieces (StatPanel.lua) through the addon's table.
--
-- Box(target, fill, edge): small rounded corners (8 px) and edges from Art\, the fill tinted by fill, the edge by
-- edge; the pieces sit on target's BACKGROUND and BORDER layers, so its own text stays on top. BoxColor recolors,
-- BoxShown shows or hides them.

local _, ns = ...
local ART = "Interface\\AddOns\\Emberforge_Resources\\Art\\"
local SIZE = 8
local FLIPS = { { 0, 1, 0, 1 }, { 1, 0, 0, 1 }, { 0, 1, 1, 0 }, { 1, 0, 1, 0 } }
local CORNERS = { "TOPLEFT", "TOPRIGHT", "BOTTOMLEFT", "BOTTOMRIGHT" }

function ns.BoxColor(target, fill, edge)
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

function ns.BoxShown(target, shown)
    for _, list in ipairs({ target.boxFill, target.boxEdge }) do
        for _, piece in ipairs(list) do
            if shown then
                piece:Show()
            else
                piece:Hide()
            end
        end
    end
end

function ns.Box(target, fill, edge)
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
            piece:SetWidth(SIZE)
            piece:SetHeight(SIZE)
            piece:SetPoint(corner)
        end
    end
    -- The flat fill stops half a pixel in, where the corner pieces' fill starts.
    local middle = Piece(target.boxFill, "BACKGROUND")
    middle:SetPoint("TOPLEFT", SIZE, -0.5)
    middle:SetPoint("BOTTOMRIGHT", -SIZE, 0.5)
    local left = Piece(target.boxFill, "BACKGROUND")
    left:SetPoint("TOPLEFT", 0.5, -SIZE)
    left:SetPoint("BOTTOMRIGHT", target, "BOTTOMLEFT", SIZE, SIZE)
    local right = Piece(target.boxFill, "BACKGROUND")
    right:SetPoint("TOPLEFT", target, "TOPRIGHT", -SIZE, -SIZE)
    right:SetPoint("BOTTOMRIGHT", -0.5, SIZE)
    local top = Piece(target.boxEdge, "BORDER", "LineEdge")
    top:SetPoint("TOPLEFT", SIZE, 0)
    top:SetPoint("BOTTOMRIGHT", target, "TOPRIGHT", -SIZE, -SIZE)
    local bottom = Piece(target.boxEdge, "BORDER", "LineEdge", FLIPS[3])
    bottom:SetPoint("TOPLEFT", target, "BOTTOMLEFT", SIZE, SIZE)
    bottom:SetPoint("BOTTOMRIGHT", -SIZE, 0)
    local leftEdge = Piece(target.boxEdge, "BORDER", "LineEdgeV")
    leftEdge:SetPoint("TOPLEFT", 0, -SIZE)
    leftEdge:SetPoint("BOTTOMRIGHT", target, "BOTTOMLEFT", SIZE, SIZE)
    local rightEdge = Piece(target.boxEdge, "BORDER", "LineEdgeV", FLIPS[2])
    rightEdge:SetPoint("TOPLEFT", target, "TOPRIGHT", -SIZE, -SIZE)
    rightEdge:SetPoint("BOTTOMRIGHT", 0, SIZE)
    ns.BoxColor(target, fill, edge)
end

-- The window's colors.
ns.GOLD = { 1, 0.82, 0 }
ns.BANNER_FILL, ns.BANNER_EDGE = { 0.13, 0.075, 0.035, 0.97 }, { 0.86, 0.64, 0.32, 1 }
ns.ARROW = ART .. "ArrowDown"

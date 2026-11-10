/*
 * Shared by the Emberforge effects: whether the world is on screen.
 *
 * ReShade sees one finished picture: the world, loading screens, the login and character screens, the world map and
 * videos alike. Only while the world is on screen (in game, no map or video over it) does the Emberforge_Lighting
 * addon draw two 4x4 near-black blocks in the top-left corner, (2, 1, 3) then (1, 3, 2) out of 255; every effect
 * runs only while it finds them. Loading and login screens run no addons, so they never carry it. Each effect leaves
 * those 8x4 pixels as they are, so the blocks reach every effect after it untouched. (The game's own interface over
 * the world, bars and chat, is in the same picture and still gets the colour grade.)
 */

#pragma once

float EmberforgeInWorld()
{
    float3 a = floor(tex2Dfetch(ReShade::BackBuffer, int2(1, 1)).rgb * 255.0 + 0.5);
    float3 b = floor(tex2Dfetch(ReShade::BackBuffer, int2(5, 2)).rgb * 255.0 + 0.5);
    return all(a == float3(2.0, 1.0, 3.0)) && all(b == float3(1.0, 3.0, 2.0)) ? 1.0 : 0.0;
}

// The marker's pixels, which no effect changes.
bool EmberforgeMarkerPixel(float4 pos)
{
    return pos.x < 8.0 && pos.y < 4.0;
}

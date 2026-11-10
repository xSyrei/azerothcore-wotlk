/*
 * Shared by the Emberforge effects.
 *
 * The world map (and any full-screen window) covers the world, but the depth buffer still holds the world behind it,
 * so effects that read depth would shade the window ("burning" onto the map). While such a window is open, the
 * Emberforge_Lighting addon draws two 8x8 blocks in the screen's top-left corner, magenta then green; the depth effects
 * pause while they see them. Read before any effect has changed those pixels: the effects that change colours skip
 * themselves while the blocks show, so the blocks reach every later effect untouched.
 */

#pragma once

float EmberforgeUiCovers()
{
    float3 a = tex2Dfetch(ReShade::BackBuffer, int2(3, 3)).rgb;
    float3 b = tex2Dfetch(ReShade::BackBuffer, int2(11, 3)).rgb;
    return all(abs(a - float3(1.0, 0.0, 1.0)) < 0.08) && all(abs(b - float3(0.0, 1.0, 0.0)) < 0.08) ? 1.0 : 0.0;
}

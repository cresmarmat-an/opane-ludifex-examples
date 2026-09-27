// Chooses the colour and keeps the built-in lighting.
//
// The stripes are computed in the mesh's own space, so they stay on the object
// as it turns instead of sliding across it.
//
//   Params[0]    ColorA
//   Params[1]    ColorB
//   Params[2].x  Frequency   stripes per unit of height
//   Params[2].y  Speed       how fast they scroll

#include "world_material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float coordinate = input.LocalPosition.y * Params[2].x + Seconds() * Params[2].y;
    float band = frac(coordinate);

    // Signed distance to the nearest stripe edge. AntiAlias smooths both edges
    // across one pixel, where step() would leave them jagged.
    float stripe = AntiAlias(abs(band - 0.5) - 0.25);

    float3 base = lerp(Params[0].rgb, Params[1].rgb, stripe);

    // Shade applies the same light, ambient, and rim as every default actor.
    return float4(Shade(input, base), 1.0);
}

// Ignores the built-in lighting and draws its own look: a glowing rim and
// world-space scan lines that move upward.
//
//   Params[0]    Tint
//   Params[1].x  ScanDensity   scan lines per metre

#include "world_material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float rim = Fresnel(input, 2.0);

    // World space rather than local, so every hologram in the scene scans in
    // step with the others.
    float scan = frac(input.WorldPosition.y * Params[1].x - Seconds() * 1.5);
    float scanLine = AntiAlias(abs(scan - 0.5) - 0.40);

    float3 color = Params[0].rgb * (0.20 + rim * 1.7) + Params[0].rgb * scanLine * 0.5;
    return float4(color, 1.0);
}

// A dissolve driven by 3D value noise, with a glowing edge.
//
// With multisampling, alpha below 1 becomes coverage (alpha to coverage), so
// the ragged edge is smoothed like any other edge, without sorting or blending,
// and depth is still written.
//
//   Params[0]    EdgeColor
//   Params[1].x  Amount      0 solid, 1 gone

#include "world_material.hlsli"

float Hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float Noise(float3 x)
{
    float3 cell = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(lerp(Hash(cell + float3(0, 0, 0)), Hash(cell + float3(1, 0, 0)), f.x),
                     lerp(Hash(cell + float3(0, 1, 0)), Hash(cell + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(Hash(cell + float3(0, 0, 1)), Hash(cell + float3(1, 0, 1)), f.x),
                     lerp(Hash(cell + float3(0, 1, 1)), Hash(cell + float3(1, 1, 1)), f.x), f.y),
                f.z);
}

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float noise = Noise(input.LocalPosition * 5.0) * 0.7 + Noise(input.LocalPosition * 11.0) * 0.3;

    // Positive where the surface survives, negative where it has dissolved.
    float edge = noise - Params[1].x;

    // One pixel of coverage across the cut, measured from the derivative.
    float alpha = saturate(edge / max(fwidth(edge), 1e-4) + 0.5);

    // Discard only what is entirely gone, so the partial-coverage band at the
    // edge still reaches alpha-to-coverage. With multisampling off this is what
    // keeps the dissolve working at all.
    clip(alpha - 0.001);

    float glow = 1.0 - saturate(edge / 0.07);
    float3 color = Shade(input, input.Color.rgb) + Params[0].rgb * glow * 2.2;

    return float4(color, alpha);
}

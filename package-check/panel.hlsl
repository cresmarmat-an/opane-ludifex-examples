#include "material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float2 uv = LocalUv(input);
    float3 color = lerp(Params[0].rgb, Params[1].rgb, uv.y);
    float alpha = ShapeCoverage(input) * input.Color.a;
    return Premultiply(color, alpha);
}

#include "world_material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float light = saturate(dot(SurfaceNormal(input), ToLight()));
    float band = light > 0.5 ? 1.0 : 0.45;
    return float4(ApplyFog(input, BaseColor(input).rgb * band), 1.0);
}

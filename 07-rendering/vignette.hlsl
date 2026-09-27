// A full-screen material that runs after tone mapping. It darkens the corners
// and warms them slightly.

#include "world_post.hlsli"

float4 FragmentMain(PostInput input) : SV_Target
{
    float3 color = SampleScene(input.UV).rgb;

    float2 centred = input.UV - 0.5;
    float distance = length(centred * float2(TexelSize.z / TexelSize.w, 1.0));
    float strength = Param(0).x;

    float vignette = 1.0 - strength * smoothstep(0.35, 0.95, distance);
    float3 warm = color * float3(1.03, 0.99, 0.94);

    return float4(lerp(color, warm, distance) * vignette, 1.0);
}

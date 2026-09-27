// A surface material whose colour and glow are set per actor.
//
// Every sphere in the grid uses this material with its own Tint and Glow, set
// with Actor3D::SetUniform. Param() reads the actor's value, so all the
// spheres are still drawn in one call.

#include "world_material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float3 tint = Param(input, 0).rgb;
    float glow = Param(input, 1).x;

    // A soft pulse, offset by where the sphere is so the grid ripples.
    float pulse = 0.5 + 0.5 * sin(Seconds() * 2.0 + input.WorldPosition.x * 0.8 + input.WorldPosition.z * 0.8);

    float3 lit = Shade(input, tint);
    return float4(lit + tint * glow * pulse, 1.0);
}

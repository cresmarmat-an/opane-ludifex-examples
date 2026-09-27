// One material shared by a thousand actors, each with its own values.
//
// Params[] holds the material's values, shared by every actor. Param() reads
// the actor's own value if it set one, and the material's otherwise. The
// per-actor values are stored with the instance data, so the actors are still
// drawn in one call.

#include "world_material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float4 tint = Param(input, 0);      // per actor
    float phase = Param(input, 1).x;    // per actor
    float speed = Params[2].x;          // shared by all of them

    // A band travelling up each object, offset so no two are in step.
    float wave = sin(input.LocalPosition.y * 9.0 + Seconds() * speed + phase * 6.2831853) * 0.5 + 0.5;
    float3 color = lerp(tint.rgb * 0.35, tint.rgb, wave);

    return float4(Shade(input, color), 1.0);
}

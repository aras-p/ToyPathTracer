#include "ComputeShader.hlsl"

[numthreads(kCSGroupSizeX, kCSGroupSizeY, 1)]
void main(uint3 gid : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
{
    if (tid.x == 0)
    {
        s_GroupRayCounter = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    Params params = g_Params[0];
    uint rngState = (gid.x * 1973 + gid.y * 9277 + params.frames * 26699) | 1;

    float3 col = 0;
    for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
    {
        float u = float(gid.x + RandomFloat01(rngState)) * params.invWidth;
        float v = float(gid.y + RandomFloat01(rngState)) * params.invHeight;
        Ray r = CameraGetRay(params.cam, u, v, rngState);

        // Do a ray cast against the world
        Hit rec;
        int id = HitWorld(g_Spheres, params.sphereCount, r, kMinT, kMaxT, rec);
        // Does not hit anything?
        if (id < 0)
        {
            // evaluate and add sky
            col += SkyHit(r);
        }
        else
        {
            // Hit something; evaluate material response (this can queue new rays for next bounce)
            col += SurfaceHit(g_Spheres, g_Materials, params.sphereCount, g_Emissives, params.emissiveCount,
                r, float3(1,1,1), (gid.x<<11)|gid.y, false, rec, id,
                rngState);
        }
    }
    dstImage[gid.xy] = float4(col, 0);

    GroupMemoryBarrierWithGroupSync();

    // debugging; add red tint to any places where we didn't have enough space for new rays
    //if (s_GroupRayCounter > kMaxGroupRays)
    //    dstImage[gid.xy] += float4(2,0,0,0);

    if (tid.x == 0 && tid.y == 0)
    {
        g_OutCounts.InterlockedAdd(0, DO_SAMPLES_PER_PIXEL * kCSGroupSizeX * kCSGroupSizeY);
        uint rayCount = min(s_GroupRayCounter, kMaxGroupRays);
        uint rayBufferStart;
        g_OutCounts.InterlockedAdd(4, rayCount, rayBufferStart);
        for (uint i = 0; i < rayCount; ++i)
        {
            g_RayBufferDst[rayBufferStart + i] = s_GroupRays[i];
        }
    }
}

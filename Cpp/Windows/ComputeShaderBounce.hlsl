#include "ComputeShader.hlsl"

// Since the rays we process during bounce CS invocations can be in arbitrary image locations,
// we can't "easily" blend their trace results into the resulting image (different CS threads
// could be processing rays for the same image location at once). So instead, we emit
// "point splats" (color + pixel index), that later on are rasterized as point primitives
// using regular GPU functionality; blending unit takes care of same-location splats just fine.
//
// The splat infos are emitted into group shared buffer, and later on blasted into global one.

struct SplatData
{
    float3 color;
    uint pixelIndex;
};
SplatData MakeSplatData(float3 color, uint pixelIndex)
{
    SplatData sd;
    sd.color = color;
    sd.pixelIndex = pixelIndex;
    return sd;
}

groupshared uint s_GroupSplatCounter;
groupshared SplatData s_GroupSplats[kCSRayBatchSize];

void PushSplat(float3 col, uint pixelIndex)
{
    uint splatIndex;
    InterlockedAdd(s_GroupSplatCounter, 1, splatIndex);
    s_GroupSplats[splatIndex] = MakeSplatData(col, pixelIndex);
}

RWStructuredBuffer<SplatData> g_SplatBufferDst : register(u3);


[numthreads(kCSRayBatchSize, 1, 1)]
void main(uint3 gid : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
{
    uint threadID = tid.x;
    if (threadID == 0)
    {
        s_GroupRayCounter = 0;
        s_GroupSplatCounter = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    Params params = g_Params[0];
    uint rngState = (gid.x * 9781 + params.frames * 6271) | 1;

    RayData rd = g_RayBufferSrc[gid.x];
    Ray rdRay = RayDataGetRay(rd);
    uint pixelIndex = RayDataGetPixelIndex(rd);
    float3 rdAtten = RayDataGetAtten(rd);

    Hit rec;
    int id = HitWorld(g_Spheres, params.sphereCount, rdRay, kMinT, kMaxT, rec);
    // Does not hit anything?
    if (id < 0)
    {
        if (!RayDataIsShadow(rd))
        {
            // for non-shadow rays, evaluate and add sky
            float3 col = SkyHit(rdRay) * rdAtten;
            PushSplat(col, pixelIndex);
        }
    }
    else
    {
        if (!RayDataIsShadow(rd))
        {
            // A non-shadow ray hit something; evaluate material response (this can queue new rays for next bounce)
            float3 col = SurfaceHit(g_Spheres, g_Materials, params.sphereCount, g_Emissives, params.emissiveCount,
                rdRay, rdAtten, pixelIndex, RayDataIsSkipEmission(rd), rec, id,
                rngState) * rdAtten;
            PushSplat(col, pixelIndex);
        }
        else
        {
            // A shadow ray; add illumination if we hit the needed light
            if (id == RayDataGetLightID(rd))
            {
                float3 col = rdAtten;
                PushSplat(col, pixelIndex);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // debugging; add green tint to any places where we didn't have enough space for new rays
    //if (s_GroupRayCounter > kMaxGroupRays)
    //    PushSplat(float3(0, 2, 0), pixelIndex);

    uint rayCount = min(s_GroupRayCounter, kMaxGroupRays);
    if (threadID == 0)
    {
        // total ray counts (for perf indicator)
        g_OutCounts.InterlockedAdd(0, kCSRayBatchSize);

        GetGlobalRayDataOffset(rayCount);

        // append new splats into global buffer
        uint splatBufferStart;
        g_OutCounts.InterlockedAdd(8, s_GroupSplatCounter, splatBufferStart);
        for (uint is = 0; is < s_GroupSplatCounter; ++is)
        {
            g_SplatBufferDst[splatBufferStart + is] = s_GroupSplats[is];
        }
    }
    GroupMemoryBarrierWithGroupSync();
    PushGlobalRayData(threadID, rayCount, kCSRayBatchSize);
}

#include "ComputeShader.hlsl"

groupshared uint s_RayCounter;
groupshared uint s_GroupSplatCounter;

groupshared SplatData s_GroupSplats[kCSRayBatchSize];

void PushSplat(float3 col, uint pixelIndex)
{
    uint splatIndex;
    InterlockedAdd(s_GroupSplatCounter, 1, splatIndex);
    s_GroupSplats[splatIndex] = MakeSplatData(col, pixelIndex);
}


[numthreads(kCSRayBatchSize, 1, 1)]
void main(uint3 gid : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
{
    if (tid.x == 0)
    {
        s_RayCounter = 0;
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
    //uint2 pixelCoord = uint2(pixelIndex>>11, pixelIndex & 0x7FF);

    Hit rec;
    int id = HitWorld(g_Spheres, params.sphereCount, rdRay, kMinT, kMaxT, rec);
    // Does not hit anything?
    if (id < 0)
    {
        if (!RayDataIsShadow(rd))
        {
            // for non-shadow rays, evaluate and add sky
            float3 col = SkyHit(rdRay) * rdAtten;
            //dstImage[pixelCoord] += float4(col, 0);
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
            //dstImage[pixelCoord] += float4(col, 0);
            PushSplat(col, pixelIndex);
        }
        else
        {
            // A shadow ray; add illumination if we hit the needed light
            if (id == RayDataGetLightID(rd))
            {
                float3 col = rdAtten;
                //dstImage[pixelCoord] += float4(rdAtten, 0);
                PushSplat(col, pixelIndex);
            }
        }
    }

    InterlockedAdd(s_RayCounter, 1);
    GroupMemoryBarrierWithGroupSync();

    // debugging; add green tint to any places where we didn't have enough space for new rays
    //if (s_GroupRayCounter > kMaxGroupRays)
    //    PushSplat(float3(0, 2, 0), pixelIndex);

    if (tid.x == 0)
    {
        g_OutCounts.InterlockedAdd(0, s_RayCounter);
        s_GroupRayCounter = min(s_GroupRayCounter, kMaxGroupRays);

        uint rayBufferStart;
        g_OutCounts.InterlockedAdd(4, s_GroupRayCounter, rayBufferStart);
        for (uint ir = 0; ir < s_GroupRayCounter; ++ir)
        {
            g_RayBufferDst[rayBufferStart + ir] = s_GroupRays[ir];
        }

        uint splatBufferStart;
        g_OutCounts.InterlockedAdd(8, s_GroupSplatCounter, splatBufferStart);
        for (uint is = 0; is < s_GroupSplatCounter; ++is)
        {
            g_SplatBufferDst[splatBufferStart + is] = s_GroupSplats[is];
        }
    }
}

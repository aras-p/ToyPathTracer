#include "ComputeShader.hlsl"

[numthreads(kCSRayBatchSize, 1, 1)]
void main(uint3 gid : SV_DispatchThreadID)
{
    Params params = g_Params[0];
    uint width;
    uint height;
    dstImage.GetDimensions(width, height);
    uint rngState = (gid.x * 9781 + params.frames * 6271) | 1;

    RayData rd = g_RayBufferSrc[gid.x];
    Ray rdRay = RayDataGetRay(rd);
    uint pixelIndex = RayDataGetPixelIndex(rd);
    float3 rdAtten = RayDataGetAtten(rd);
    uint2 pixelCoord = uint2(pixelIndex % width, pixelIndex / width);

    uint prevRayCount;
    g_OutCounts.InterlockedAdd(0, 1, prevRayCount);

    Hit rec;
    int id = HitWorld(g_Spheres, params.sphereCount, rdRay, kMinT, kMaxT, rec);
    float3 col;
    // Does not hit anything?
    if (id < 0)
    {
        if (!RayDataIsShadow(rd))
        {
            // for non-shadow rays, evaluate and add sky
            float3 col = SkyHit(rdRay) * rdAtten;
            dstImage[pixelCoord] += float4(col, 0);
        }
    }
    else
    {
        if (!RayDataIsShadow(rd))
        {
            // A non-shadow ray hit something; evaluate material response (this can queue new rays for next bounce)
            col = SurfaceHit(g_Spheres, g_Materials, params.sphereCount, g_Emissives, params.emissiveCount,
                rdRay, rdAtten, pixelIndex, RayDataIsSkipEmission(rd), rec, id,
                g_RayBufferDst, rngState) * rdAtten;
            dstImage[pixelCoord] += float4(col, 0);
        }
        else
        {
            // A shadow ray; add illumination if we hit the needed light
            if (id == RayDataGetLightID(rd))
            {
                dstImage[pixelCoord] += float4(rdAtten, 0);
            }
        }
    }
}

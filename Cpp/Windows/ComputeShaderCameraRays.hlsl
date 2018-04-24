#include "ComputeShader.hlsl"

[numthreads(kCSGroupSizeX, kCSGroupSizeY, 1)]
void main(uint3 gid : SV_DispatchThreadID)
{
    float3 col = 0;
    Params params = g_Params[0];
    uint rngState = (gid.x * 1973 + gid.y * 9277 + params.frames * 26699) | 1;

    uint width;
    uint height;
    dstImage.GetDimensions(width, height);

    for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
    {
        float u = float(gid.x + RandomFloat01(rngState)) * params.invWidth;
        float v = float(gid.y + RandomFloat01(rngState)) * params.invHeight;
        Ray r = CameraGetRay(params.cam, u, v, rngState);

        // Do a ray cast against the world
        Hit rec;
        int id = HitWorld(g_Spheres, params.sphereCount, r, kMinT, kMaxT, rec);
        float3 col;
        // Does not hit anything?
        if (id < 0)
        {
            // evaluate and add sky
            col = SkyHit(r);
        }
        else
        {
            // Hit something; evaluate material response (this can queue new rays for next bounce)
            col = SurfaceHit(g_Spheres, g_Materials, params.sphereCount, g_Emissives, params.emissiveCount,
                r, float3(1,1,1), gid.y*width+gid.x, false, rec, id,
                g_RayBufferDst, rngState);
        }
        dstImage[gid.xy] += float4(col, 0);
    }

    uint prevRayCount;
    g_OutCounts.InterlockedAdd(0, DO_SAMPLES_PER_PIXEL, prevRayCount);
}

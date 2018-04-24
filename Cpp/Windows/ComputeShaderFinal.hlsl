#include "ComputeShader.hlsl"

[numthreads(kCSGroupSizeX, kCSGroupSizeY, 1)]
void main(uint3 gid : SV_DispatchThreadID)
{
    Params params = g_Params[0];

    float3 col = tmpImage[gid.xy].rgb;
    col *= 1.0f / float(DO_SAMPLES_PER_PIXEL);
    float3 prev = prevFrameImage.Load(int3(gid.xy, 0)).rgb;
    col = lerp(col, prev, params.lerpFac);
    dstImage[gid.xy] = float4(col, 1);
}

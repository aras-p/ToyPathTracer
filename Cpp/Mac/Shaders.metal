#include <metal_stdlib>

using namespace metal;

struct vs2ps
{
    float4 pos [[position]];
    float2 uv;
};

vertex vs2ps vertexShader(ushort vid [[vertex_id]])
{
    vs2ps o;
    o.uv = float2((vid << 1) & 2, vid & 2);
    o.pos = float4(o.uv * float2(2, 2) + float2(-1, -1), 0, 1);
    return o;
}

// http://chilliant.blogspot.com.au/2012/08/srgb-approximations-for-hlsl.html
float3 LinearToSRGB (float3 rgb)
{
    rgb = max(rgb, float3(0,0,0));
    return max(1.055 * pow(rgb, 0.416666667) - 0.055, 0.0);
}


fragment float4 fragmentShader(vs2ps i [[stage_in]], texture2d<float> tex [[texture(0)]])
{
    constexpr sampler smp(mip_filter::nearest, mag_filter::linear, min_filter::linear);
    return float4(LinearToSRGB(tex.sample(smp, i.uv).rgb), 1);
}

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

fragment float4 fragmentShader(vs2ps i [[stage_in]], texture2d<float> tex [[texture(0)]])
{
    constexpr sampler smp(mip_filter::nearest, mag_filter::linear, min_filter::linear);
    return tex.sample(smp, i.uv);
}

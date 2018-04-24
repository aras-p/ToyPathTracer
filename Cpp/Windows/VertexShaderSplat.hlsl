
struct SplatData
{
    float3 color;
    uint pixelIndex;
};

StructuredBuffer<SplatData> g_SplatBuffer : register(t0);

struct vs2ps
{
    float3 color : TEXCOORD0;
    float4 pos : SV_Position;
};

vs2ps main(uint vid : SV_VertexID)
{
    vs2ps o;
    SplatData s = g_SplatBuffer[vid];
    o.color = s.color;
    float x = ((s.pixelIndex >> 11) + 0.5) / 1280.0;
    float y = 1.0 - ((s.pixelIndex & 0x7FF) + 0.5) / 720.0;

    //x = 0 + vid * 0.01f;
    //y = 0 + vid * 0.01f;    
    //o.color = float3(1, 0.5, 0.2);

    o.pos = float4(x * 2 - 1, y * 2 - 1, 0, 1);
    return o;
}

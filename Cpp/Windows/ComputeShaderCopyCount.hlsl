#include "../Source/Config.h"

ByteAddressBuffer g_InCounts : register(t0);
RWByteAddressBuffer g_OutIndirectCounts : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 gid : SV_DispatchThreadID)
{
    uint rayCount = g_InCounts.Load(4);
    g_OutIndirectCounts.Store3(0, uint3((rayCount + kCSRayBatchSize-1) / kCSRayBatchSize, 1, 1));
}

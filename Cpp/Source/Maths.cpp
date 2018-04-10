#include "Maths.h"
#include <stdlib.h>
#include <stdint.h>

static uint32_t XorShift32(uint32_t& state)
{
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 15;
    state = x;
    return x;
}

float RandomFloat01(uint32_t& state)
{
    return (XorShift32(state) & 0xFFFFFF) / 16777216.0f;
}

float3 RandomInUnitDisk(uint32_t& state)
{
    float3 p;
    do
    {
        p = 2.0 * float3(RandomFloat01(state),RandomFloat01(state),0) - float3(1,1,0);
    } while (dot(p,p) >= 1.0);
    return p;
}

float3 RandomInUnitSphere(uint32_t& state)
{
    float3 p;
    do {
        p = 2.0*float3(RandomFloat01(state),RandomFloat01(state),RandomFloat01(state)) - float3(1,1,1);
    } while (sqLength(p) >= 1.0);
    return p;
}

float3 RandomUnitVector(uint32_t& state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float a = RandomFloat01(state) * 2.0f * kPI;
    float r = sqrtf(1.0f - z * z);
    float x = r * cosf(a);
    float y = r * sinf(a);
    return float3(x, y, z);
}


int HitSpheres(const Ray& r, const SpheresSoA& spheres, float tMin, float tMax, Hit& outHit)
{
    float4 hitPosX, hitPosY, hitPosZ;
    float4 hitNorX, hitNorY, hitNorZ;
    float4 hitT = float4(tMax);
    __m128i id = _mm_set1_epi32(-1);

    float4 rOrigX = SHUFFLE4(r.orig, 0, 0, 0, 0);
    float4 rOrigY = SHUFFLE4(r.orig, 1, 1, 1, 1);
    float4 rOrigZ = SHUFFLE4(r.orig, 2, 2, 2, 2);
    float4 rDirX = SHUFFLE4(r.dir, 0, 0, 0, 0);
    float4 rDirY = SHUFFLE4(r.dir, 1, 1, 1, 1);
    float4 rDirZ = SHUFFLE4(r.dir, 2, 2, 2, 2);
    float4 tMin4 = float4(tMin);
    float4 tMax4 = float4(tMax);
    for (int i = 0; i < spheres.simdCount; i += kSimdWidth)
    {
        float4 sCenterX = float4(spheres.centerX + i);
        float4 sCenterY = float4(spheres.centerY + i);
        float4 sCenterZ = float4(spheres.centerZ + i);
        float4 sSqRadius = float4(spheres.sqRadius + i);
        float4 ocX = rOrigX - sCenterX;
        float4 ocY = rOrigY - sCenterY;
        float4 ocZ = rOrigZ - sCenterZ;
        float4 b = ocX * rDirX + ocY * rDirY + ocZ * rDirZ;
        float4 c = ocX * ocX + ocY * ocY + ocZ * ocZ - sSqRadius;
        float4 discr = b * b - c;
        bool4 discrPos = discr > float4(0.0f);
        if (any(discrPos))
        {
            float4 discrSq = sqrtf(discr);

            float4 t = (-b - discrSq);
            bool4 maskMin = t > tMin4;
            bool4 maskMax = t < tMax4;
            bool4 maskComb = discrPos & maskMin & maskMax;
            if (mask(maskComb) != 0)
            {
                id = select(id, _mm_set_epi32(i + 3, i + 2, i + 1, i + 0), maskComb);
                tMax4 = select(tMax4, t, maskComb);
                float4 posX = rOrigX + rDirX * t;
                float4 posY = rOrigY + rDirY * t;
                float4 posZ = rOrigZ + rDirZ * t;
                hitPosX = select(hitPosX, posX, maskComb);
                hitPosY = select(hitPosY, posY, maskComb);
                hitPosZ = select(hitPosZ, posZ, maskComb);
                float4 sInvRadius = float4(spheres.invRadius + i);
                hitNorX = select(hitNorX, (posX - sCenterX) * sInvRadius, maskComb);
                hitNorY = select(hitNorY, (posY - sCenterY) * sInvRadius, maskComb);
                hitNorZ = select(hitNorZ, (posZ - sCenterZ) * sInvRadius, maskComb);
                hitT = select(hitT, t, maskComb);
            }
            t = (-b + discrSq);
            maskMin = t > tMin4;
            maskMax = t < tMax4;
            bool4 negatedPrevMask = bool4(_mm_xor_ps(maskComb.m, _mm_cmpeq_ps(maskComb.m, maskComb.m)));
            maskComb = discrPos & maskMin & maskMax & negatedPrevMask;
            if (mask(maskComb) != 0)
            {
                id = select(id, _mm_set_epi32(i + 3, i + 2, i + 1, i + 0), maskComb);
                tMax4 = select(tMax4, t, maskComb);
                float4 posX = rOrigX + rDirX * t;
                float4 posY = rOrigY + rDirY * t;
                float4 posZ = rOrigZ + rDirZ * t;
                hitPosX = select(hitPosX, posX, maskComb);
                hitPosY = select(hitPosY, posY, maskComb);
                hitPosZ = select(hitPosZ, posZ, maskComb);
                float4 sInvRadius = float4(spheres.invRadius + i);
                hitNorX = select(hitNorX, (posX - sCenterX) * sInvRadius, maskComb);
                hitNorY = select(hitNorY, (posY - sCenterY) * sInvRadius, maskComb);
                hitNorZ = select(hitNorZ, (posZ - sCenterZ) * sInvRadius, maskComb);
                hitT = select(hitT, t, maskComb);
            }
        }
    }
    // now we have up to 4 hits, find and return closest one
    //@TODO: pretty sure this is super sub-optimal :)
    float minT = hmin(hitT);
    if (hitT.getX() == minT)
    {
        outHit.pos = float3(hitPosX.getX(), hitPosY.getX(), hitPosZ.getX());
        outHit.normal = float3(hitNorX.getX(), hitNorY.getX(), hitNorZ.getX());
        outHit.t = hitT.getX();
        return (int16_t)_mm_extract_epi16(id, 0);
    }
    if (hitT.getY() == minT)
    {
        outHit.pos = float3(hitPosX.getY(), hitPosY.getY(), hitPosZ.getY());
        outHit.normal = float3(hitNorX.getY(), hitNorY.getY(), hitNorZ.getY());
        outHit.t = hitT.getY();
        return (int16_t)_mm_extract_epi16(id, 2);
    }
    if (hitT.getZ() == minT)
    {
        outHit.pos = float3(hitPosX.getZ(), hitPosY.getZ(), hitPosZ.getZ());
        outHit.normal = float3(hitNorX.getZ(), hitNorY.getZ(), hitNorZ.getZ());
        outHit.t = hitT.getZ();
        return (int16_t)_mm_extract_epi16(id, 4);
    }
    if (hitT.getW() == minT)
    {
        outHit.pos = float3(hitPosX.getW(), hitPosY.getW(), hitPosZ.getW());
        outHit.normal = float3(hitNorX.getW(), hitNorY.getW(), hitNorZ.getW());
        outHit.t = hitT.getW();
        return (int16_t)_mm_extract_epi16(id, 6);
    }

    return -1;
}

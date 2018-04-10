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
    Hit tmpHit;
    int id = -1;
    float closest = tMax;
    for (int i = 0; i < spheres.count; ++i)
    {
        float ocX = r.orig.getX() - spheres.centerX[i];
        float ocY = r.orig.getY() - spheres.centerY[i];
        float ocZ = r.orig.getZ() - spheres.centerZ[i];
        float b = ocX * r.dir.getX() + ocY * r.dir.getY() + ocZ * r.dir.getZ();
        float c = ocX * ocX + ocY * ocY + ocZ * ocZ - spheres.radius[i] * spheres.radius[i];
        float discr = b * b - c;
        if (discr > 0)
        {
            float discrSq = sqrtf(discr);

            float t = (-b - discrSq);
            if (t > tMin && t < tMax && t < closest)
            {
                id = i;
                closest = t;
                tmpHit.pos = r.pointAt(t);
                tmpHit.normal = (tmpHit.pos - float3(spheres.centerX[i], spheres.centerY[i], spheres.centerZ[i])) * spheres.invRadius[i];
                tmpHit.t = t;
            }
            else
            {
                t = (-b + discrSq);
                if (t > tMin && t < tMax && t < closest)
                {
                    id = i;
                    closest = t;
                    tmpHit.pos = r.pointAt(t);
                    tmpHit.normal = (tmpHit.pos - float3(spheres.centerX[i], spheres.centerY[i], spheres.centerZ[i])) * spheres.invRadius[i];
                    tmpHit.t = t;
                }
            }
        }
    }
    outHit = tmpHit;
    return id;
}

#include "Maths.h"
#include <stdlib.h>
#include <stdint.h>

static uint32_t s_RndState = 1;

static uint32_t XorShift32()
{
    uint32_t x = s_RndState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 15;
    s_RndState = x;
    return x;
}

float RandomFloat01()
{
    return (XorShift32() & 0xFFFFFF) / 16777216.0f;
}

float3 RandomInUnitDisk()
{
    float3 p;
    do
    {
        p = 2.0 * float3(RandomFloat01(),RandomFloat01(),0) - float3(1,1,0);
    } while (dot(p,p) >= 1.0);
    return p;
}

float3 RandomInUnitSphere()
{
    float3 p;
    do {
        p = 2.0*float3(RandomFloat01(),RandomFloat01(),RandomFloat01()) - float3(1,1,1);
    } while (p.sqLength() >= 1.0);
    return p;
}


bool HitSphere(const Ray& r, const Sphere& s, float tMin, float tMax, Hit& outHit)
{
    float3 oc = r.orig - s.center;
    float a = dot(r.dir, r.dir);
    float b = dot(oc, r.dir);
    float c = dot(oc, oc) - s.radius*s.radius;
    float discr = b*b - a*c;
    if (discr > 0)
    {
        float discrSq = sqrtf(discr);
        
        float t = (-b - discrSq) / a;
        if (t < tMax && t > tMin)
        {
            outHit.pos = r.pointAt(t);
            outHit.normal = (outHit.pos - s.center) * (1.f / s.radius);
            outHit.t = t;
            return true;
        }
        t = (-b + discrSq) / a;
        if (t < tMax && t > tMin)
        {
            outHit.pos = r.pointAt(t);
            outHit.normal = (outHit.pos - s.center) * (1.f / s.radius);
            outHit.t = t;
            return true;
        }
    }
    return false;
}

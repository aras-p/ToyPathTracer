#include "../Source/Config.h"

inline uint RNG(inout uint state)
{
    uint x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 15;
    state = x;
    return x;
}

float RandomFloat01(inout uint state)
{
    return (RNG(state) & 0xFFFFFF) / 16777216.0f;
}

float3 RandomInUnitDisk(inout uint state)
{
    float a = RandomFloat01(state) * 2.0f * 3.1415926f;
    float2 xy = float2(cos(a), sin(a));
    xy *= sqrt(RandomFloat01(state));
    return float3(xy, 0);
}
float3 RandomInUnitSphere(inout uint state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float t = RandomFloat01(state) * 2.0f * 3.1415926f;
    float r = sqrt(max(0.0, 1.0f - z * z));
    float x = r * cos(t);
    float y = r * sin(t);
    float3 res = float3(x, y, z);
    res *= pow(RandomFloat01(state), 1.0 / 3.0);
    return res;
}
float3 RandomUnitVector(inout uint state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float a = RandomFloat01(state) * 2.0f * 3.1415926f;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}



struct Ray
{
    float3 orig;
    float3 dir;
};
Ray MakeRay(float3 orig_, float3 dir_) { Ray r; r.orig = orig_; r.dir = dir_; return r; }
float3 RayPointAt(Ray r, float t) { return r.orig + r.dir * t; }


inline bool refract(float3 v, float3 n, float nint, out float3 outRefracted)
{
    float dt = dot(v, n);
    float discr = 1.0f - nint * nint*(1 - dt * dt);
    if (discr > 0)
    {
        outRefracted = nint * (v - n * dt) - n * sqrt(discr);
        return true;
    }
    return false;
}
inline float schlick(float cosine, float ri)
{
    float r0 = (1 - ri) / (1 + ri);
    r0 = r0 * r0;
    // note: saturate to guard against possible tiny negative numbers
    return r0 + (1 - r0)*pow(saturate(1 - cosine), 5);
}

struct Hit
{
    float3 pos;
    float3 normal;
    float t;
};

struct Sphere
{
    float3 center;
    float radius;
    float invRadius;
};

#define MatLambert 0
#define MatMetal 1
#define MatDielectric 2

struct Material
{
    int type;
    float3 albedo;
    float3 emissive;
    float roughness;
    float ri;
};

struct Camera
{
    float3 origin;
    float3 lowerLeftCorner;
    float3 horizontal;
    float3 vertical;
    float3 u, v, w;
    float lensRadius;
};

Ray CameraGetRay(Camera cam, float s, float t, inout uint state)
{
    float3 rd = cam.lensRadius * RandomInUnitDisk(state);
    float3 offset = cam.u * rd.x + cam.v * rd.y;
    return MakeRay(cam.origin + offset, normalize(cam.lowerLeftCorner + s * cam.horizontal + t * cam.vertical - cam.origin - offset));
}

struct RayData
{
    float origX, origY, origZ;
    uint dirXY, dirZattenX, attenYZ;
    uint flags; // 22b index, 8b lightID, 1b shadow, 1b emission
};

RayData MakeRayData(Ray r, float3 atten, uint pixelIndex, uint lightID, bool shadow, bool skipEmission)
{
    RayData rd;
    rd.origX = r.orig.x;
    rd.origY = r.orig.y;
    rd.origZ = r.orig.z;
    uint3 dir16 = f32tof16(r.dir);
    uint3 atten16 = f32tof16(atten);
    rd.dirXY = (dir16.x << 16) | (dir16.y);
    rd.dirZattenX = (dir16.z << 16) | (atten16.x);
    rd.attenYZ = (atten16.y << 16) | (atten16.z);
    rd.flags = (pixelIndex & 0x3FFFFF) | ((lightID & 0xFF) << 22) | (shadow ? (1U << 30) : 0) | (skipEmission ? (1U << 31) : 0);
    return rd;
}

Ray RayDataGetRay(RayData rd)
{
    float3 orig = float3(rd.origX, rd.origY, rd.origZ);
    float3 dir = f16tof32(uint3(rd.dirXY >> 16, rd.dirXY & 0xFFFF, rd.dirZattenX >> 16));
    return MakeRay(orig, normalize(dir));
}
float3 RayDataGetAtten(RayData rd)
{
    return f16tof32(uint3(rd.dirZattenX & 0xFFFF, rd.attenYZ >> 16, rd.attenYZ & 0xFFFF));
}
uint RayDataGetPixelIndex(RayData rd)
{
    return rd.flags & 0x3FFFFF;
}
uint RayDataGetLightID(RayData rd)
{
    return (rd.flags >> 22) & 0xFF;
}
bool RayDataIsShadow(RayData rd)
{
    return (rd.flags & (1 << 30)) != 0;
}
bool RayDataIsSkipEmission(RayData rd)
{
    return (rd.flags & (1 << 31)) != 0;
}


int HitSpheres(Ray r, StructuredBuffer<Sphere> spheres, int sphereCount, float tMin, float tMax, inout Hit outHit)
{
    float hitT = tMax;
    int id = -1;
    for (int i = 0; i < sphereCount; ++i)
    {
        Sphere s = spheres[i];
        float3 co = s.center - r.orig;
        float nb = dot(co, r.dir);
        float c = dot(co, co) - s.radius*s.radius;
        float discr = nb * nb - c;
        if (discr > 0)
        {
            float discrSq = sqrt(discr);

            // Try earlier t
            float t = nb - discrSq;
            if (t <= tMin) // before min, try later t!
                t = nb + discrSq;

            if (t > tMin && t < hitT)
            {
                id = i;
                hitT = t;
            }
        }
    }

    if (id != -1)
    {
        outHit.pos = RayPointAt(r, hitT);
        outHit.normal = (outHit.pos - spheres[id].center) * spheres[id].invRadius;
        outHit.t = hitT;
    }
    return id;
}

struct Params
{
    Camera cam;
    int sphereCount;
    int screenWidth;
    int screenHeight;
    int frames;
    float invWidth;
    float invHeight;
    float lerpFac;
    int emissiveCount;
};

Texture2D prevFrameImage : register(t0);
Texture2D tmpImage : register(t1);
StructuredBuffer<Sphere> g_Spheres : register(t2);
StructuredBuffer<Material> g_Materials : register(t3);
StructuredBuffer<Params> g_Params : register(t4);
StructuredBuffer<int> g_Emissives : register(t5);
StructuredBuffer<RayData> g_RayBufferSrc : register(t6);

RWTexture2D<float4> dstImage : register(u0);
RWByteAddressBuffer g_OutCounts : register(u1);
RWStructuredBuffer<RayData> g_RayBufferDst : register(u2);


#define kMinT 0.001f
#define kMaxT 1.0e7f
#define kMaxDepth 10


static int HitWorld(StructuredBuffer<Sphere> spheres, int sphereCount, Ray r, float tMin, float tMax, inout Hit outHit)
{
    return HitSpheres(r, spheres, sphereCount, tMin, tMax, outHit);
}


static bool Scatter(StructuredBuffer<Material> materials, int matID, Ray r_in, Hit rec, out float3 attenuation, out Ray scattered, inout uint state)
{
    Material mat = materials[matID];
    if (mat.type == MatLambert)
    {
        // random point on unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomUnitVector(state);
        scattered = MakeRay(rec.pos, normalize(target - rec.pos));
        attenuation = mat.albedo;
        return true;
    }
    else if (mat.type == MatMetal)
    {
        float3 refl = reflect(r_in.dir, rec.normal);
        // reflected ray, and random inside of sphere based on roughness
        float roughness = mat.roughness;
#if DO_MITSUBA_COMPARE
        roughness = 0; // until we get better BRDF for metals
#endif
        scattered = MakeRay(rec.pos, normalize(refl + roughness*RandomInUnitSphere(state)));
        attenuation = mat.albedo;
        return dot(scattered.dir, rec.normal) > 0;
    }
    else if (mat.type == MatDielectric)
    {
        float3 outwardN;
        float3 rdir = r_in.dir;
        float3 refl = reflect(rdir, rec.normal);
        float nint;
        attenuation = float3(1, 1, 1);
        float3 refr;
        float reflProb;
        float cosine;
        if (dot(rdir, rec.normal) > 0)
        {
            outwardN = -rec.normal;
            nint = mat.ri;
            cosine = mat.ri * dot(rdir, rec.normal);
        }
        else
        {
            outwardN = rec.normal;
            nint = 1.0f / mat.ri;
            cosine = -dot(rdir, rec.normal);
        }
        if (refract(rdir, outwardN, nint, refr))
        {
            reflProb = schlick(cosine, mat.ri);
        }
        else
        {
            reflProb = 1;
        }
        if (RandomFloat01(state) < reflProb)
            scattered = MakeRay(rec.pos, normalize(refl));
        else
            scattered = MakeRay(rec.pos, normalize(refr));
    }
    else
    {
        attenuation = float3(1, 0, 1);
        scattered = MakeRay(float3(0,0,0), float3(0, 0, 1));
        return false;
    }
    return true;
}

static float3 SurfaceHit(
    StructuredBuffer<Sphere> spheres, StructuredBuffer<Material> materials, int sphereCount,
    StructuredBuffer<int> emissives, int emissiveCount,
    Ray r, float3 rayAtten,
    uint pixelIndex, bool raySkipEmission, Hit hit, int id,
    RWStructuredBuffer<RayData> buffer, inout uint state)
{
    Material mat = materials[id];
    Ray scattered;
    float3 atten;
    if (Scatter(materials, id, r, hit, atten, scattered, state))
    {
        // Queue the scattered ray for next bounce iteration
        bool skipEmission = false;
#if DO_LIGHT_SAMPLING
        // for Lambert materials, we are doing explicit light (emissive) sampling
        // for their contribution, so if the scattered ray hits the light again, don't add emission
        if (mat.type == MatLambert)
            skipEmission = true;
#endif

        uint rayIdx;
        g_OutCounts.InterlockedAdd(4, 1, rayIdx);
        buffer[rayIdx] = MakeRayData(scattered, atten * rayAtten, pixelIndex, 0, false, skipEmission);

        // sample lights
#if DO_LIGHT_SAMPLING
        if (mat.type == MatLambert)
        {
            for (int j = 0; j < emissiveCount; ++j)
            {
                int i = emissives[j];
                if (id == i)
                    continue; // skip self
                Material smat = materials[i];
                Sphere s = spheres[i];

                // create a random direction towards sphere
                // coord system for sampling: sw, su, sv
                float3 sw = normalize(s.center - hit.pos);
                float3 su = normalize(cross(abs(sw.x) > 0.01f ? float3(0, 1, 0) : float3(1, 0, 0), sw));
                float3 sv = cross(sw, su);
                // sample sphere by solid angle
                float cosAMax = sqrt(1.0f - s.radius*s.radius / dot(hit.pos - s.center, hit.pos - s.center));
                float eps1 = RandomFloat01(state), eps2 = RandomFloat01(state);
                float cosA = 1.0f - eps1 + eps1 * cosAMax;
                float sinA = sqrt(1.0f - cosA * cosA);
                float phi = 2 * 3.1415926 * eps2;
                float3 l = su * cos(phi) * sinA + sv * sin(phi) * sinA + sw * cosA;

                // Queue a shadow ray for next bounce iteration
                float omega = 2 * 3.1415926 * (1 - cosAMax);
                float3 nl = dot(hit.normal, r.dir) < 0 ? hit.normal : -hit.normal;

                uint rayIdx;
                g_OutCounts.InterlockedAdd(4, 1, rayIdx);
                float3 shadowAtt = (mat.albedo * smat.emissive) * (max(0.0f, dot(l, nl)) * omega / 3.1415926);
                buffer[rayIdx] = MakeRayData(MakeRay(hit.pos,l), shadowAtt * rayAtten, pixelIndex, i, true, false);
            }
        }
#endif
    }
    return raySkipEmission ? float3(0, 0, 0) : mat.emissive; // don't add material emission if told so
}


float3 SkyHit(Ray r)
{
#if DO_MITSUBA_COMPARE
    col += float3(0.15f, 0.21f, 0.3f); // easier compare with Mitsuba's constant environment light
#else
    float3 unitDir = r.dir;
    float t = 0.5f*(unitDir.y + 1.0f);
    return ((1.0f - t)*float3(1.0f, 1.0f, 1.0f) + t * float3(0.5f, 0.7f, 1.0f)) * 0.3f;
#endif
}

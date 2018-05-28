#include <metal_stdlib>
using namespace metal;

// -----------------------------------------------------------------
// regular blit / display

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

// -----------------------------------------------------------------
// GPU path tracing

#include "../Source/Config.h"

inline uint32_t RNG(thread uint32_t& state)
{
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 15;
    state = x;
    return x;
}

float RandomFloat01(thread uint32_t& state)
{
    return (RNG(state) & 0xFFFFFF) / 16777216.0f;
}
float3 RandomInUnitDisk(thread uint32_t& state)
{
    float a = RandomFloat01(state) * 2.0f * 3.1415926f;
    float2 xy = float2(cos(a), sin(a));
    xy *= sqrt(RandomFloat01(state));
    return float3(xy,0);
}
float3 RandomInUnitSphere(thread uint32_t& state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float t = RandomFloat01(state) * 2.0f * 3.1415926f;
    float r = sqrt(max(0.0, 1.0f - z*z));
    float x = r * cos(t);
    float y = r * sin(t);
    float3 res = float3(x,y,z);
    res *= pow(RandomFloat01(state), 1.0/3.0);
    return res;
}
float3 RandomUnitVector(thread uint32_t& state)
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
    
    Ray() {}
    Ray(float3 orig_, float3 dir_) : orig(orig_), dir(dir_) {}
    float3 pointAt(float t) const { return orig + dir * t; }
};

inline bool refract(float3 v, float3 n, float nint, thread float3& outRefracted)
{
    float dt = dot(v, n);
    float discr = 1.0f - nint*nint*(1-dt*dt);
    if (discr > 0)
    {
        outRefracted = nint * (v - n*dt) - n*sqrt(discr);
        return true;
    }
    return false;
}
inline float schlick(float cosine, float ri)
{
    float r0 = (1-ri) / (1+ri);
    r0 = r0*r0;
    // note: saturate to guard against possible tiny negative numbers
    return r0 + (1-r0)*pow(saturate(1-cosine), 5);
}

struct Hit
{
    float3 pos;
    float3 normal;
    float t;
};

struct Sphere
{
    packed_float3 center;
    float radius;
    float invRadius;
};

struct Material
{
    enum Type { Lambert, Metal, Dielectric };
    int type;
    packed_float3 albedo;
    packed_float3 emissive;
    float roughness;
    float ri;
};

struct Camera
{
    packed_float3 origin;
    packed_float3 lowerLeftCorner;
    packed_float3 horizontal;
    packed_float3 vertical;
    packed_float3 u, v, w;
    float lensRadius;
};

Ray CameraGetRay(constant const Camera& cam, float s, float t, thread uint32_t& state)
{
    float3 rd = cam.lensRadius * RandomInUnitDisk(state);
    float3 offset = cam.u * rd.x + cam.v * rd.y;
    return Ray(cam.origin + offset, normalize(cam.lowerLeftCorner + s*cam.horizontal + t*cam.vertical - cam.origin - offset));
}

int HitSpheres(Ray r, threadgroup const Sphere* spheres, int sphereCount, float tMin, float tMax, thread Hit& outHit)
{
    float hitT = tMax;
    int id = -1;
    for (int i = 0; i < sphereCount; ++i)
    {
        Sphere s = spheres[i];
        float3 co = s.center - r.orig;
        float nb = dot(co, r.dir);
        float c = dot(co, co) - s.radius*s.radius;
        float discr = nb*nb - c;
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
        outHit.pos = r.pointAt(hitT);
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
    uint emissiveCount;
};

#define kMinT 0.001f
#define kMaxT 1.0e7f
#define kMaxDepth 10

static int HitWorld(threadgroup const Sphere* spheres, int sphereCount, Ray r, float tMin, float tMax, thread Hit& outHit)
{
    return HitSpheres(r, spheres, sphereCount, tMin, tMax, outHit);
}

static bool Scatter(threadgroup const Sphere* spheres, threadgroup const Material* materials, int sphereCount, threadgroup int* emissives, int emissiveCount, int matID, Ray r_in, Hit rec, thread float3& attenuation, thread Ray& scattered, thread float3& outLightE, thread int& inoutRayCount, thread uint32_t& state)
{
    outLightE = float3(0,0,0);
    Material mat = materials[matID];
    if (mat.type == Material::Lambert)
    {
        // random point on unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomUnitVector(state);
        scattered = Ray(rec.pos, normalize(target - rec.pos));
        attenuation = mat.albedo;
        
        // sample lights
#if DO_LIGHT_SAMPLING
        for (int j = 0; j < emissiveCount; ++j)
        {
            int i = emissives[j];
            if (matID == i)
                continue; // skip self
            Material smat = materials[i];
            Sphere s = spheres[i];
            
            // create a random direction towards sphere
            // coord system for sampling: sw, su, sv
            float3 sw = normalize(s.center - rec.pos);
            float3 su = normalize(cross(fabs(sw.x)>0.01f ? float3(0,1,0):float3(1,0,0), sw));
            float3 sv = cross(sw, su);
            // sample sphere by solid angle
            float cosAMax = sqrt(1.0f - s.radius*s.radius / length_squared(rec.pos-s.center));
            float eps1 = RandomFloat01(state), eps2 = RandomFloat01(state);
            float cosA = 1.0f - eps1 + eps1 * cosAMax;
            float sinA = sqrt(1.0f - cosA*cosA);
            float phi = 2 * 3.1415926 * eps2;
            float3 l = su * cos(phi) * sinA + sv * sin(phi) * sinA + sw * cosA;
            
            // shoot shadow ray
            Hit lightHit;
            ++inoutRayCount;
            int hitID = HitWorld(spheres, sphereCount, Ray(rec.pos, l), kMinT, kMaxT, lightHit);
            if (hitID == i)
            {
                float omega = 2 * 3.1415926 * (1-cosAMax);
                float3 rdir = r_in.dir;
                float3 nl = dot(rec.normal, rdir) < 0 ? rec.normal : -rec.normal;
                outLightE += (mat.albedo * smat.emissive) * (max(0.0f, dot(l, nl)) * omega / 3.1415926);
            }
        }
#endif
        return true;
    }
    else if (mat.type == Material::Metal)
    {
        float3 refl = reflect(r_in.dir, rec.normal);
        // reflected ray, and random inside of sphere based on roughness
        scattered = Ray(rec.pos, normalize(refl + mat.roughness*RandomInUnitSphere(state)));
        attenuation = mat.albedo;
        return dot(scattered.dir, rec.normal) > 0;
    }
    else if (mat.type == Material::Dielectric)
    {
        float3 outwardN;
        float3 rdir = r_in.dir;
        float3 refl = reflect(rdir, rec.normal);
        float nint;
        attenuation = float3(1,1,1);
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
            scattered = Ray(rec.pos, normalize(refl));
        else
            scattered = Ray(rec.pos, normalize(refr));
    }
    else
    {
        attenuation = float3(1,0,1);
        return false;
    }
    return true;
}

static float3 Trace(threadgroup const Sphere* spheres, threadgroup const Material* materials, int sphereCount, threadgroup int* emissives, int emissiveCount, Ray r, thread int& inoutRayCount, thread uint32_t& state)
{
    float3 col = 0;
    float3 curAtten = 1;
    bool doMaterialE = true;

    // Metal (and most/all other current shading languages) don't support recursion, so to tracing
    // iterations in a loop up to max depth. For each iteration, track "current attenuation";
    // or the amount of ligth that should contribute to the eye / final pixel at this point;
    // it achieves the same as "return matE + lightE + attenuation * Trace(...)" in
    // the regular recursive C++ version does.
    for (int depth = 0; depth < kMaxDepth; ++depth)
    {
        Hit rec;
        ++inoutRayCount;
        int id = HitWorld(spheres, sphereCount, r, kMinT, kMaxT, rec);
        if (id >= 0)
        {
            Ray scattered;
            float3 attenuation;
            float3 lightE;
            Material mat = materials[id];
            float3 matE = mat.emissive;
            if (Scatter(spheres, materials, sphereCount, emissives, emissiveCount, id, r, rec, attenuation, scattered, lightE, inoutRayCount, state))
            {
#if DO_LIGHT_SAMPLING
                if (!doMaterialE) matE = 0;
                doMaterialE = (mat.type != Material::Lambert);
#endif
                col += curAtten * (matE + lightE);
                curAtten *= attenuation;
                r = scattered;
            }
            else
            {
                col += curAtten * matE;
                break;
            }
        }
        else
        {
            // sky
#if DO_MITSUBA_COMPARE
            col += curAtten * float3(0.15f, 0.21f, 0.3f); // easier compare with Mitsuba's constant environment light
#else
            float3 unitDir = r.dir;
            float t = 0.5f*(unitDir.y + 1.0f);
            float3 skyCol = ((1.0f-t)*float3(1.0f, 1.0f, 1.0f) + t*float3(0.5f, 0.7f, 1.0f)) * 0.3f;
            col += curAtten * skyCol;
#endif
            break;
        }
    }
    return col;
}


kernel void TraceGPU(
                     texture2d<float,access::read> srcImage [[texture(0)]],
                     texture2d<float,access::write> dstImage [[texture(1)]],
                     uint2 gid [[thread_position_in_grid]],
                     uint tid [[thread_index_in_threadgroup]],
                     constant Sphere* spheres [[buffer(0)]],
                     constant Material* materials [[buffer(1)]],
                     constant Params* params [[buffer(2)]],
                     constant int* emissives [[buffer(3)]],
                     device atomic_int* outRayCount [[buffer(4)]])
{
    // First, move scene data (spheres, materials, emissive indices) into thread group shared
    // memory. Do this in parallel; each thread in group copies its own chunk of data.
    threadgroup Sphere groupSpheres[kCSMaxObjects];
    threadgroup Material groupMaterials[kCSMaxObjects];
    threadgroup int groupEmissives[kCSMaxObjects];
    uint groupSize = kCSGroupSizeX * kCSGroupSizeY;
    uint objCount = params->sphereCount;
    uint myObjCount = (objCount + groupSize - 1) / groupSize;
    uint myObjStart = tid * myObjCount;
    for (uint io = myObjStart; io < myObjStart + myObjCount; ++io)
    {
        if (io < objCount)
        {
            groupSpheres[io] = spheres[io];
            groupMaterials[io] = materials[io];
        }
        if (io < params->emissiveCount)
        {
            groupEmissives[io] = emissives[io];
        }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    
    int rayCount = 0;
    float3 col(0, 0, 0);
    uint32_t rngState = (gid.x * 1973 + gid.y * 9277 + params->frames * 26699) | 1;
    for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
    {
        float u = float(gid.x + RandomFloat01(rngState)) * params->invWidth;
        float v = float(gid.y + RandomFloat01(rngState)) * params->invHeight;
        Ray r = CameraGetRay(params->cam, u, v, rngState);
        col += Trace(groupSpheres, groupMaterials, params->sphereCount, groupEmissives, params->emissiveCount, r, rayCount, rngState);
    }
    col *= 1.0f / float(DO_SAMPLES_PER_PIXEL);
    
    float3 prev = srcImage.read(gid).rgb;
    // initially "prev" might contain NaNs; guard against that. @TODO: clear to black on app side
    if (!isfinite(length_squared(prev)))
        prev = float3(0.0);
    
    col = mix(col, prev, params->lerpFac);
    dstImage.write(float4(col,1), gid);

    atomic_fetch_add_explicit(outRayCount, rayCount, memory_order_relaxed);
}

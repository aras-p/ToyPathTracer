#include "Config.h"

typedef float<3> float3;
typedef unsigned int uint32_t;

inline float3 f3(float x, float y, float z)
{
    float3 v = {x,y,z};
    return v;
}

inline float dot(float3 a, float3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float length(float3 v) { return sqrt(dot(v,v)); }
inline float invLength(float3 v) { return rsqrt(dot(v,v)); }
inline float sqLength(float3 v) { return dot(v,v); }
inline float3 cross(float3 a, float3 b)
{
    float3 ret;
    ret.x = a.y * b.z - a.z * b.y;
    ret.y = a.z * b.x - a.x * b.z;
    ret.z = a.x * b.y - a.y * b.x;
    return ret;
}
inline float3 normalize(float3 v)
{
    return v * invLength(v);
}
inline float3 reflect(const float3& v, const float3& n)
{
    return v - 2*dot(v,n)*n;
}
inline bool refract(const float3& v, const float3& n, float nint, float3& outRefracted)
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
    return r0 + (1-r0)*pow(1-cosine, 5);
}


struct Ray
{
    float3 orig;
    float3 dir;
};

struct Hit
{
    float3 pos;
    float3 normal;
    float t;
};

struct SpheresSoA
{
    uniform float* centerX;
    uniform float* centerY;
    uniform float* centerZ;
    uniform float* sqRadius;
    uniform float* invRadius;
    int simdCount;
    int count;
};

struct Camera
{
    float originX, originY, originZ;
    float llcX, llcY, llcZ;
    float horX, horY, horZ;
    float verX, verY, verZ;
    float ux, uy, uz, vx, vy, vz, wx, wy, wz;
    float lensRadius;
};

uint32_t XorShift32(uint32_t& state)
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
    return (int)(XorShift32(state) & 0xFFFFFF) / 16777216.0f;
}

float3 RandomInUnitDisk(uint32_t& state)
{
    float a = RandomFloat01(state) * 2.0f * 3.1415926f;
    float x, y;
    sincos(a, &x, &y);
    float d = sqrt(RandomFloat01(state));
    x *= d;
    y *= d;
    return f3(x,y,0);
}

float3 RandomInUnitSphere(uint32_t& state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float t = RandomFloat01(state) * 2.0f * 3.1415926f;
    float r = sqrt(max(0.0, 1.0f - z*z));
    float x, y;
    sincos(t, &x, &y);
    x *= r;
    y *= r;
    float3 res = {x,y,z};
    res *= pow(RandomFloat01(state), 1.0/3.0);
    return res;
}

float3 RandomUnitVector(uint32_t& state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float t = RandomFloat01(state) * 2.0f * 3.1415926f;
    float r = sqrt(max(0.0, 1.0f - z * z));
    float x, y;
    sincos(t, &x, &y);
    x *= r;
    y *= r;
    float3 res = {x,y,z};
    return res;
}



float3 RayPointAt(const Ray& r, float t) { return r.orig + r.dir * t; }

Ray CameraGetRay(const uniform Camera& cam, float s, float t, uint32_t& state)
{
    float3 rd = cam.lensRadius * RandomInUnitDisk(state);
    float3 offset = f3(cam.ux,cam.uy,cam.uz) * rd.x + f3(cam.vx,cam.vy,cam.vz) * rd.y;
    uniform float3 corigin = {cam.originX,cam.originY,cam.originZ};
    uniform float3 cllc = {cam.llcX,cam.llcY,cam.llcZ};
    uniform float3 chor = {cam.horX,cam.horY,cam.horZ};
    uniform float3 cver = {cam.verX,cam.verY,cam.verZ};
    Ray ret = {corigin + offset, normalize(cllc + s*chor + t*cver - corigin - offset)};
    return ret;
}



enum Type { MatLambert, MatMetal, MatDielectric };
struct Material
{
    Type type;
    float albedoX,albedoY,albedoZ;
    float emissiveX,emissiveY,emissiveZ;
    float roughness;
    float ri;
};

const float kMinT = 0.001f;
const float kMaxT = 1.0e7f;
const int kMaxDepth = 10;


static int HitWorld(const uniform SpheresSoA& spheres, int sphereCount, const Ray& r, float tMin, float tMax, Hit& outHit)
{
    float hitT = tMax;
    int id = -1;
    for (uniform int i = 0; i < sphereCount; ++i)
    {
        float coX = spheres.centerX[i] - r.orig.x;
        float coY = spheres.centerY[i] - r.orig.y;
        float coZ = spheres.centerZ[i] - r.orig.z;
        float nb = coX * r.dir.x + coY * r.dir.y + coZ * r.dir.z;
        float c = coX * coX + coY * coY + coZ * coZ - spheres.sqRadius[i];
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
        outHit.normal = (outHit.pos - f3(spheres.centerX[id], spheres.centerY[id], spheres.centerZ[id])) * spheres.invRadius[id];
        outHit.t = hitT;
        return id;
    }
    else
        return -1;
}

Ray MakeRay(float3 pos, float3 dir)
{
    Ray r = {pos, dir};
    return r;
}


static bool Scatter(int matIndex, const Ray& r_in, const Hit& rec, float3& attenuation, Ray& scattered, float3& outLightE, int& inoutRayCount, uint32_t& randomState,
    const uniform uniform SpheresSoA& spheres, const uniform Material materials[], uniform int sphereCount,
    const uniform int emissiveSpheres[], uniform int emissiveSphereCount)
{
    const Material* mat = &materials[matIndex];
    outLightE.x = outLightE.y = outLightE.z = 0;
    if (mat->type == MatLambert)
    {
        // random point inside unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomUnitVector(randomState);
        scattered = MakeRay(rec.pos, normalize(target - rec.pos));
        attenuation = f3(mat->albedoX,mat->albedoY,mat->albedoZ);

        // sample lights
#if DO_LIGHT_SAMPLING
        for (uniform int j = 0; j < emissiveSphereCount; ++j)
        {
            uniform int i = emissiveSpheres[j];
            if (i == matIndex)
                continue; // skip self
            const uniform Material& smat = materials[i];

            // create a random direction towards sphere
            // coord system for sampling: sw, su, sv
            uniform float3 scenter = {spheres.centerX[i],spheres.centerY[i],spheres.centerZ[i]};
            float3 sw = normalize(scenter - rec.pos);
            float3 su = normalize(cross(abs(sw.x)>0.01f ? f3(0,1,0):f3(1,0,0), sw));
            float3 sv = cross(sw, su);
            // sample sphere by solid angle
            float cosAMax = sqrt(1.0f - spheres.sqRadius[i] / sqLength(rec.pos-scenter));
            float eps1 = RandomFloat01(randomState), eps2 = RandomFloat01(randomState);
            float cosA = 1.0f - eps1 + eps1 * cosAMax;
            float sinA = sqrt(1.0f - cosA*cosA);
            float phi = 2 * 3.14159265 * eps2;
            float3 l = su * cos(phi) * sinA + sv * sin(phi) * sinA + sw * cosA;
            l = normalize(l);

            // shoot shadow ray
            Hit lightHit;
            ++inoutRayCount;
            int hitID = HitWorld(spheres, sphereCount, MakeRay(rec.pos, l), kMinT, kMaxT, lightHit);
            if (hitID == i)
            {
                float omega = 2 * 3.1415926 * (1-cosAMax);

                float3 rdir = r_in.dir;
                float3 nl = dot(rec.normal, rdir) < 0 ? rec.normal : -rec.normal;
                outLightE += (attenuation * f3(smat.emissiveX,smat.emissiveY,smat.emissiveZ)) * (max(0.0f, dot(l, nl)) * omega / 3.1415926);
            }
        }
#endif
        return true;
    }
    else if (mat->type == MatMetal)
    {
        float3 refl = reflect(r_in.dir, rec.normal);
        // reflected ray, and random inside of sphere based on roughness
        scattered = MakeRay(rec.pos, normalize(refl + mat->roughness*RandomInUnitSphere(randomState)));
        attenuation = f3(mat->albedoX,mat->albedoY,mat->albedoZ);
        return dot(scattered.dir, rec.normal) > 0;
    }
    else if (mat->type == MatDielectric)
    {
        float3 outwardN;
        float3 rdir = r_in.dir;
        float3 refl = reflect(rdir, rec.normal);
        float nint;
        attenuation = f3(1,1,1);
        float3 refr;
        float reflProb;
        float cosine;
        if (dot(rdir, rec.normal) > 0)
        {
            outwardN = -rec.normal;
            nint = mat->ri;
            cosine = mat->ri * dot(rdir, rec.normal);
        }
        else
        {
            outwardN = rec.normal;
            nint = 1.0f / mat->ri;
            cosine = -dot(rdir, rec.normal);
        }
        if (refract(rdir, outwardN, nint, refr))
        {
            reflProb = schlick(cosine, mat->ri);
        }
        else
        {
            reflProb = 1;
        }
        if (RandomFloat01(randomState) < reflProb)
            scattered = MakeRay(rec.pos, normalize(refl));
        else
            scattered = MakeRay(rec.pos, normalize(refr));
    }
    else
    {
        attenuation = f3(1,0,1);
        return false;
    }
    return true;
}

static float3 Trace(const uniform SpheresSoA& spheres, const uniform Material materials[], uniform int sphereCount, const Ray& r, int depth, int& inoutRayCount, uint32_t& randomState, const uniform int emissiveSpheres[], uniform int emissiveSphereCount, bool doMaterialE = true)
{
    Hit rec;
    ++inoutRayCount;
    int id = HitWorld(spheres, sphereCount, r, kMinT, kMaxT, rec);
    if (id != -1)
    {
        Ray scattered;
        float3 attenuation;
        float3 lightE;
        const Material* mat = materials + id;
        float3 matE = f3(mat->emissiveX,mat->emissiveY,mat->emissiveZ);
        if (depth < kMaxDepth && Scatter(id, r, rec, attenuation, scattered, lightE, inoutRayCount, randomState, spheres, materials, sphereCount, emissiveSpheres, emissiveSphereCount))
        {
#if DO_LIGHT_SAMPLING
            if (!doMaterialE) matE = f3(0,0,0); // don't add material emission if told so
            // for Lambert materials, we just did explicit light (emissive) sampling and already
            // for their contribution, so if next ray bounce hits the light again, don't add
            // emission
            doMaterialE = (mat->type != MatLambert);
#endif

            return matE + lightE + attenuation * Trace(spheres, materials, sphereCount, scattered, depth+1, inoutRayCount, randomState, emissiveSpheres, emissiveSphereCount, doMaterialE);
        }
        else
        {
            return matE;
        }
    }
    else
    {
        // sky
        float3 unitDir = r.dir;
        float t = 0.5f*(unitDir.y + 1.0f);
        return ((1.0f-t)*f3(1.0f, 1.0f, 1.0f) + t*f3(0.5f, 0.7f, 1.0f)) * 0.3f;
    }
}


export void TraceRowJobJspc(
    uniform int screenWidth,
    uniform int screenHeight,
    uniform int startRow,
    uniform int endRow,
    uniform uint32_t randomState,
    uniform float backbuffer[],
    uniform float lerpFac,
    uniform Camera& dataCam,
    uniform SpheresSoA& spheres,
    uniform Material materials[],
    uniform int sphereCount,
    uniform int emissiveSpheres[],
    uniform int emissiveSphereCount,
    uniform int& rayCounter)
{
    uniform float invWidth = 1.0f / screenWidth;
    uniform float invHeight = 1.0f / screenHeight;
    int rayCount = 0;
    uint32_t rndState = randomState;
    foreach_tiled(y = startRow ... endRow, x = 0 ... screenWidth)
    {
        rndState += programIndex;
        rndState |= 1;
        int pixelIndex = (y * screenWidth + x) * 4;
        float3 col = {0, 0, 0};
        for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
        {
            float u = (x + RandomFloat01(rndState)) * invWidth;
            float v = (y + RandomFloat01(rndState)) * invHeight;
            Ray r = CameraGetRay(dataCam, u, v, rndState);
            col += Trace(spheres, materials, sphereCount, r, 0, rayCount, rndState, emissiveSpheres, emissiveSphereCount);
        }
        col *= 1.0f / (float)DO_SAMPLES_PER_PIXEL;

        float3 prev = {backbuffer[pixelIndex+0], backbuffer[pixelIndex+1], backbuffer[pixelIndex+2]};
        col = prev * lerpFac + col * (1-lerpFac);
        backbuffer[pixelIndex+0] = col.x;
        backbuffer[pixelIndex+1] = col.y;
        backbuffer[pixelIndex+2] = col.z;
    }
    atomic_add_global(&rayCounter, rayCount);
}


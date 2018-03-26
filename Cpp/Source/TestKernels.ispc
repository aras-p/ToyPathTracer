#define DO_SAMPLES_PER_PIXEL 4
#define DO_LIGHT_SAMPLING 1

typedef float<3> float3;
typedef unsigned int uint32_t;

float3 f3(float x, float y, float z)
{
    float3 v = {x,y,z};
    return v;
}

inline float dot(float3 a, float3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float length(float3 v) { return sqrt(dot(v,v)); }
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
    float k = 1.0f / length(v);
    float3 ret;
    ret.x = v.x*k;
    ret.y = v.y*k;
    ret.z = v.z*k;
    return ret;
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
struct RayPacked
{
    float origX, origY, origZ;
    float dirX, dirY, dirZ;
};
Ray RayFromPacked(RayPacked rp)
{
    Ray r = {f3(rp.origX, rp.origY, rp.origZ), f3(rp.dirX, rp.dirY, rp.dirZ)};
    return r;
}
RayPacked PackedFromRay(Ray r)
{
    RayPacked rp = {r.orig.x, r.orig.y, r.orig.z, r.dir.x, r.dir.y, r.dir.z};
    return rp;
}

struct RayPayload
{
    float attenX, attenY, attenZ;
    uint32_t info; // 24 bit pixel index, 7 bit light ID, 1 bit shadow
};
uint32_t EncodeInfo(uint32_t pixelIndex, uint32_t lightID, uint32_t shadow)
{
    return pixelIndex | (lightID << 24) | (shadow << 31);
}
void DecodeInfo(uint32_t info, uint32_t& pixelIndex, uint32_t& lightID, uint32_t& shadow)
{
    pixelIndex = info & 0xFFFFFF;
    lightID = (info >> 24) & 0x7F;
    shadow = info >> 31;
}


struct Hit
{
    float3 pos;
    float3 normal;
    float t;
};

struct Sphere
{
    float centerX, centerY, centerZ;
    float radius;
    float invRadius;
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
    uint32_t x = state | 0x80000000;
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
    float3 p;
    do
    {
        p.x = 2.0 * RandomFloat01(state) - 1;
        p.y = 2.0 * RandomFloat01(state) - 1;
        p.z = 0;
    } while (dot(p,p) >= 1.0);
    return p;
}

float3 RandomInUnitSphere(uint32_t& state)
{
    float3 p;
    do {
        p.x = 2.0 * RandomFloat01(state) - 1;
        p.y = 2.0 * RandomFloat01(state) - 1;
        p.z = 2.0 * RandomFloat01(state) - 1;
    } while (sqLength(p) >= 1.0);
    return p;
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



bool HitSphere(const Ray& r, const uniform Sphere &s, float tMin, float tMax, Hit& outHit)
{
    float3 scenter = f3(s.centerX, s.centerY, s.centerZ);
    float3 oc = r.orig - scenter;
    float b = dot(oc, r.dir);
    float c = dot(oc, oc) - s.radius*s.radius;
    float discr = b*b - c;
    if (discr > 0)
    {
        float discrSq = sqrt(discr);

        float t = (-b - discrSq);
        if (t < tMax && t > tMin)
        {
            outHit.pos = RayPointAt(r, t);
            outHit.normal = (outHit.pos - scenter) * s.invRadius;
            outHit.t = t;
            return true;
        }
        t = (-b + discrSq);
        if (t < tMax && t > tMin)
        {
            outHit.pos = RayPointAt(r, t);
            outHit.normal = (outHit.pos - scenter) * s.invRadius;
            outHit.t = t;
            return true;
        }
    }
    return false;
}


enum MatType { MatLambert, MatMetal, MatDielectric };
struct Material
{
    MatType type;
    float albedoX,albedoY,albedoZ;
    float emissiveX,emissiveY,emissiveZ;
    float roughness;
    float ri;
};

const float kMinT = 0.001f;
const float kMaxT = 1.0e7f;
const int kMaxDepth = 10;


static int HitWorld(const uniform Sphere spheres[], int sphereCount, const Ray& r, float tMin, float tMax, Hit& outHit)
{
    Hit tmpHit;
    int id = -1;
    float closest = tMax;
    for (uniform int i = 0; i < sphereCount; ++i)
    {
        if (HitSphere(r, spheres[i], tMin, closest, tmpHit))
        {
            closest = tmpHit.t;
            outHit = tmpHit;
            id = i;
        }
    }
    return id;
}

Ray MakeRay(float3 pos, float3 dir)
{
    Ray r = {pos, dir};
    return r;
}


static bool Scatter(int matIndex, const Ray& r_in, const Hit& rec, float3& attenuation, Ray& scattered, uint32_t& randomState,
    const uniform Sphere spheres[], const uniform Material materials[], int sphereCount)
{
    const Material* mat = &materials[matIndex];
    MatType matType = mat->type;
    float3 matAlbedo = {mat->albedoX,mat->albedoY,mat->albedoZ};
    if (matType == MatLambert)
    {
        // random point inside unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomInUnitSphere(randomState);
        scattered = MakeRay(rec.pos, normalize(target - rec.pos));
        attenuation = matAlbedo;
        return true;
    }
    else if (matType == MatMetal)
    {
        float3 refl = reflect(r_in.dir, rec.normal);
        // reflected ray, and random inside of sphere based on roughness
        scattered = MakeRay(rec.pos, normalize(refl + mat->roughness*RandomInUnitSphere(randomState)));
        attenuation = matAlbedo;
        return dot(scattered.dir, rec.normal) > 0;
    }
    else if (matType == MatDielectric)
    {
        float3 outwardN;
        float3 rdir = r_in.dir;
        float3 refl = reflect(rdir, rec.normal);
        float nint;
        attenuation = f3(1,1,1);
        float3 refr;
        float reflProb;
        float cosine;
        float matRI = mat->ri;
        if (dot(rdir, rec.normal) > 0)
        {
            outwardN = -rec.normal;
            nint = matRI;
            cosine = matRI * dot(rdir, rec.normal);
        }
        else
        {
            outwardN = rec.normal;
            nint = 1.0f / matRI;
            cosine = -dot(rdir, rec.normal);
        }
        if (refract(rdir, outwardN, nint, refr))
        {
            reflProb = schlick(cosine, matRI);
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

static void PushRay(
    uniform RayPacked dstRays[],
    uniform RayPayload dstPayloads[],
    uniform int* dstCount,
    uniform int dstCapacity,
    const Ray& ray, float3 atten, uint32_t rpInfo, uint32_t lightId, uint32_t shadow)
{
    cif (*dstCount < dstCapacity - programCount)
    {
        int dstIndex = atomic_add_global(dstCount, 1);
        cif (dstIndex < dstCapacity)
        {
            dstRays[dstIndex] = PackedFromRay(ray);
            uint32_t rpPixelIndex, rpLightId, rpShadow;
            DecodeInfo(rpInfo, rpPixelIndex, rpLightId, rpShadow);
            RayPayload newRP = {atten.x, atten.y, atten.z, EncodeInfo(rpPixelIndex, lightId, shadow)};
            dstPayloads[dstIndex] = newRP;
        }
    }    
}

static float3 TraceHit(const uniform Sphere spheres[], const uniform Material materials[], int sphereCount, const Ray& r, const RayPayload& rp, const Hit& hit, int id, uint32_t& randomState,
    uniform RayPacked dstRays[],
    uniform RayPayload dstPayloads[],
    uniform int* dstCount,
    uniform int dstCapacity
)
{
    const Material* mat = materials + id;
    Ray scattered;
    float3 atten;
    if (Scatter(id, r, hit, atten, scattered, randomState, spheres, materials, sphereCount))
    {
        PushRay(dstRays, dstPayloads, dstCount, dstCapacity, scattered, atten * f3(rp.attenX, rp.attenY, rp.attenZ), rp.info, 0, 0);

#if DO_LIGHT_SAMPLING
        if (mat->type == MatLambert)
        {
            for (uniform int i = 0; i < sphereCount; ++i)
            {
                const uniform Material& smat = materials[i];
                if (smat.emissiveX <= 0 && smat.emissiveY <= 0 && smat.emissiveZ <= 0)
                    continue; // skip non-emissive
                if (i == id)
                    continue; // skip self
                const uniform Sphere& s = spheres[i];

                // create a random direction towards sphere
                // coord system for sampling: sw, su, sv
                uniform float3 scenter = {s.centerX,s.centerY,s.centerZ};
                float3 sw = normalize(scenter - hit.pos);
                float3 su = normalize(cross(abs(sw.x)>0.01f ? f3(0,1,0):f3(1,0,0), sw));
                float3 sv = cross(sw, su);
                // sample sphere by solid angle
                float cosAMax = sqrt(1.0f - s.radius*s.radius / sqLength(hit.pos-scenter));
                float eps1 = RandomFloat01(randomState), eps2 = RandomFloat01(randomState);
                float cosA = 1.0f - eps1 + eps1 * cosAMax;
                float sinA = sqrt(1.0f - cosA*cosA);
                float phi = 2 * 3.14159265 * eps2;
                float3 l = su * cos(phi) * sinA + sv * sin(phi) * sinA + sw * cosA;
                l = normalize(l);

                // queue a shadow ray
                float omega = 2 * 3.1415926 * (1-cosAMax);

                float3 nl = dot(hit.normal, r.dir) < 0 ? hit.normal : -hit.normal;
                float3 matAlbedo = {mat->albedoX,mat->albedoY,mat->albedoZ};
                float3 shadowAtt = (matAlbedo * f3(smat.emissiveX,smat.emissiveY,smat.emissiveZ)) * (max(0.0f, dot(l, nl)) * omega / 3.1415926);
                float3 newAtten = shadowAtt * f3(rp.attenX, rp.attenY, rp.attenZ);
                PushRay(dstRays, dstPayloads, dstCount, dstCapacity, MakeRay(hit.pos, l), newAtten, rp.info, i, 1);
            }
        }
#endif
    }
    return f3(mat->emissiveX,mat->emissiveY,mat->emissiveZ);
}

static float3 SkyHit(const Ray& r)
{
    float3 unitDir = r.dir;
    float t = 0.5f*(unitDir.y + 1.0f);
    return ((1.0f-t)*f3(1.0f, 1.0f, 1.0f) + t*f3(0.5f, 0.7f, 1.0f)) * 0.3f;
}


export void TracePrimaryRaysIspc(
    uniform int screenWidth,
    uniform int screenHeight,
    uniform int startRow,
    uniform int endRow,
    uniform uint32_t randomState,
    uniform float tmpbuffer[],
    uniform Camera& dataCam,
    uniform Sphere spheres[],
    uniform Material materials[],
    uniform int sphereCount,
    uniform RayPacked dstRays[],
    uniform RayPayload dstPayloads[],
    uniform int& dstCount,
    uniform int dstCapacity)
{
    uniform float invWidth = 1.0f / screenWidth;
    uniform float invHeight = 1.0f / screenHeight;
    uint32_t rndState = randomState;
    foreach(y = startRow ... endRow, x = 0 ... screenWidth)
    {
        rndState += programIndex;
        int pixelIndex = (y * screenWidth + x) * 4;
        for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
        {
            float u = (x + RandomFloat01(rndState)) * invWidth;
            float v = (y + RandomFloat01(rndState)) * invHeight;
            Ray r = CameraGetRay(dataCam, u, v, rndState);
            Hit hit;
            int hitID = HitWorld(spheres, sphereCount, r, kMinT, kMaxT, hit);
            float3 col;
            if (hitID < 0)
            {
                col = SkyHit(r);
            }
            else
            {
                RayPayload rp = {1,1,1, EncodeInfo(pixelIndex, 0, 0)};
                col = TraceHit(spheres, materials, sphereCount, r, rp, hit, hitID, rndState, dstRays, dstPayloads, &dstCount, dstCapacity);
            }
            tmpbuffer[pixelIndex+0] += col.x;
            tmpbuffer[pixelIndex+1] += col.y;
            tmpbuffer[pixelIndex+2] += col.z;
        }
    }
}

export void TraceSecondaryRaysIspc(
    uniform uint32_t randomState,
    uniform float tmpbuffer[],
    uniform Sphere spheres[],
    uniform Material materials[],
    uniform int sphereCount,
    uniform const RayPacked srcRays[],
    uniform const RayPayload srcPayloads[],
    uniform int srcCount,
    uniform RayPacked dstRays[],
    uniform RayPayload dstPayloads[],
    uniform int& dstCount,
    uniform int dstCapacity)
{
    uint32_t rndState = randomState;
    foreach(i = 0 ... srcCount)
    {
        rndState += programIndex;
        Ray r = RayFromPacked(srcRays[i]);
        RayPayload rp = srcPayloads[i];
        Hit hit;
        int hitID = HitWorld(spheres, sphereCount, r, kMinT, kMaxT, hit);
        float3 rpAtten = {rp.attenX,rp.attenY,rp.attenZ};
        uint32_t rpPix, rpLightID, rpShadow;
        DecodeInfo(rp.info, rpPix, rpLightID, rpShadow);

        if (hitID < 0 && rpShadow)
            continue;

        float3 col;
        if (hitID < 0)
        {
            col = SkyHit(r) * rpAtten;
        }
        else if (!rpShadow)
        {
            col = TraceHit(spheres, materials, sphereCount, r, rp, hit, hitID, rndState, dstRays, dstPayloads, &dstCount, dstCapacity) * rpAtten;
        }
        else if (rpLightID != hitID)
        {
            continue;
        }
        else
        {
            col = rpAtten;
        }
        atomic_add_local(&tmpbuffer[rpPix+0], col.x);
        atomic_add_local(&tmpbuffer[rpPix+1], col.y);
        atomic_add_local(&tmpbuffer[rpPix+2], col.z);
    }
}

export void TraceBlendIspc(
    uniform int screenWidth,
    uniform int startRow,
    uniform int endRow,
    uniform float backbuffer[],
    uniform float tmpbuffer[],
    uniform float lerpFac)
{
    foreach(y = startRow ... endRow, x = 0 ... screenWidth)
    {
        int pixelIndex = (y * screenWidth + x) * 4;
        float3 col = {tmpbuffer[pixelIndex+0], tmpbuffer[pixelIndex+1], tmpbuffer[pixelIndex+2]};
        col *= 1.0f / (float)DO_SAMPLES_PER_PIXEL;
        col = f3(sqrt(col.x), sqrt(col.y), sqrt(col.z));
        
        float3 prev = {backbuffer[pixelIndex+0], backbuffer[pixelIndex+1], backbuffer[pixelIndex+2]};
        col = prev * lerpFac + col * (1-lerpFac);
        backbuffer[pixelIndex+0] = col.x;
        backbuffer[pixelIndex+1] = col.y;
        backbuffer[pixelIndex+2] = col.z;
    }
}

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


static bool HitWorld(const uniform Sphere spheres[], int sphereCount, const Ray& r, float tMin, float tMax, Hit& outHit, int& outID)
{
    Hit tmpHit;
    bool anything = false;
    float closest = tMax;
    for (uniform int i = 0; i < sphereCount; ++i)
    {
        if (HitSphere(r, spheres[i], tMin, closest, tmpHit))
        {
            anything = true;
            closest = tmpHit.t;
            outHit = tmpHit;
            outID = i;
        }
    }
    return anything;
}

Ray MakeRay(float3 pos, float3 dir)
{
    Ray r = {pos, dir};
    return r;
}


static bool Scatter(int matIndex, const Ray& r_in, const Hit& rec, float3& attenuation, Ray& scattered, float3& outLightE, int& inoutRayCount, uint32_t& randomState,
    const uniform Sphere spheres[], const uniform Material materials[], int sphereCount)
{
    const Material* mat = &materials[matIndex];
    outLightE.x = outLightE.y = outLightE.z = 0;
    if (mat->type == MatLambert)
    {
        // random point inside unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomInUnitSphere(randomState);
        scattered = MakeRay(rec.pos, normalize(target - rec.pos));
        attenuation = f3(mat->albedoX,mat->albedoY,mat->albedoZ);

        // sample lights
#if DO_LIGHT_SAMPLING
        for (uniform int i = 0; i < sphereCount; ++i)
        {
            const uniform Material& smat = materials[i];
            if (smat.emissiveX <= 0 && smat.emissiveY <= 0 && smat.emissiveZ <= 0)
                continue; // skip non-emissive
            if (i == matIndex)
                continue; // skip self
            const uniform Sphere& s = spheres[i];

            // create a random direction towards sphere
            // coord system for sampling: sw, su, sv
            uniform float3 scenter = {s.centerX,s.centerY,s.centerZ};
            float3 sw = normalize(scenter - rec.pos);
            float3 su = normalize(cross(abs(sw.x)>0.01f ? f3(0,1,0):f3(1,0,0), sw));
            float3 sv = cross(sw, su);
            // sample sphere by solid angle
            float cosAMax = sqrt(1.0f - s.radius*s.radius / sqLength(rec.pos-scenter));
            float eps1 = RandomFloat01(randomState), eps2 = RandomFloat01(randomState);
            float cosA = 1.0f - eps1 + eps1 * cosAMax;
            float sinA = sqrt(1.0f - cosA*cosA);
            float phi = 2 * 3.14159265 * eps2;
            float3 l = su * cos(phi) * sinA + sv * sin(phi) * sinA + sw * cosA;
            l = normalize(l);

            // shoot shadow ray
            Hit lightHit;
            int hitID;
            ++inoutRayCount;
            if (HitWorld(spheres, sphereCount, MakeRay(rec.pos, l), kMinT, kMaxT, lightHit, hitID) && hitID == i)
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

static float3 Trace(const uniform Sphere spheres[], const uniform Material materials[], int sphereCount, const Ray& r, int depth, int& inoutRayCount, uint32_t& randomState)
{
    Hit rec;
    int id = 0;
    ++inoutRayCount;
    if (HitWorld(spheres, sphereCount, r, kMinT, kMaxT, rec, id))
    {
        Ray scattered;
        float3 attenuation;
        float3 lightE;
        const Material* mat = materials + id;
        if (depth < kMaxDepth && Scatter(id, r, rec, attenuation, scattered, lightE, inoutRayCount, randomState, spheres, materials, sphereCount))
        {
            return f3(mat->emissiveX,mat->emissiveY,mat->emissiveZ) + lightE + attenuation * Trace(spheres, materials, sphereCount, scattered, depth+1, inoutRayCount, randomState);
        }
        else
        {
            return f3(mat->emissiveX,mat->emissiveY,mat->emissiveZ);
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
    uniform Sphere spheres[],
    uniform Material materials[],
    uniform int sphereCount,
    uniform int& rayCounter)
{
    uniform float invWidth = 1.0f / screenWidth;
    uniform float invHeight = 1.0f / screenHeight;
    int rayCount = 0;
    uint32_t rndState = randomState;
    foreach_tiled(y = startRow ... endRow, x = 0 ... screenWidth)
    {
        rndState += programIndex;
        int pixelIndex = (y * screenWidth + x) * 4;
        float3 col = {0, 0, 0};
        for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
        {
            float u = (x + RandomFloat01(rndState)) * invWidth;
            float v = (y + RandomFloat01(rndState)) * invHeight;
            Ray r = CameraGetRay(dataCam, u, v, rndState);
            col += Trace(spheres, materials, sphereCount, r, 0, rayCount, rndState);
        }
        col *= 1.0f / (float)DO_SAMPLES_PER_PIXEL;
        col = f3(sqrt(col.x), sqrt(col.y), sqrt(col.z));

        float3 prev = {backbuffer[pixelIndex+0], backbuffer[pixelIndex+1], backbuffer[pixelIndex+2]};
        col = prev * lerpFac + col * (1-lerpFac);
        backbuffer[pixelIndex+0] = col.x;
        backbuffer[pixelIndex+1] = col.y;
        backbuffer[pixelIndex+2] = col.z;
    }
    atomic_add_global(&rayCounter, rayCount);
}


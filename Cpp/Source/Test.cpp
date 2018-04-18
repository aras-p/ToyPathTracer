#include "Config.h"
#include "Test.h"
#include "Maths.h"
#include <algorithm>
#include "enkiTS/TaskScheduler_c.h"
#include <atomic>

// 46 spheres (2 emissive) when enabled; 9 spheres (1 emissive) when disabled
#define DO_BIG_SCENE 1

static Sphere s_Spheres[] =
{
    {float3(0,-100.5,-1), 100},
    {float3(2,0,-1), 0.5f},
    {float3(0,0,-1), 0.5f},
    {float3(-2,0,-1), 0.5f},
    {float3(2,0,1), 0.5f},
    {float3(0,0,1), 0.5f},
    {float3(-2,0,1), 0.5f},
    {float3(0.5f,1,0.5f), 0.5f},
    {float3(-1.5f,1.5f,0.f), 0.3f},
#if DO_BIG_SCENE
    {float3(4,0,-3), 0.5f}, {float3(3,0,-3), 0.5f}, {float3(2,0,-3), 0.5f}, {float3(1,0,-3), 0.5f}, {float3(0,0,-3), 0.5f}, {float3(-1,0,-3), 0.5f}, {float3(-2,0,-3), 0.5f}, {float3(-3,0,-3), 0.5f}, {float3(-4,0,-3), 0.5f},
    {float3(4,0,-4), 0.5f}, {float3(3,0,-4), 0.5f}, {float3(2,0,-4), 0.5f}, {float3(1,0,-4), 0.5f}, {float3(0,0,-4), 0.5f}, {float3(-1,0,-4), 0.5f}, {float3(-2,0,-4), 0.5f}, {float3(-3,0,-4), 0.5f}, {float3(-4,0,-4), 0.5f},
    {float3(4,0,-5), 0.5f}, {float3(3,0,-5), 0.5f}, {float3(2,0,-5), 0.5f}, {float3(1,0,-5), 0.5f}, {float3(0,0,-5), 0.5f}, {float3(-1,0,-5), 0.5f}, {float3(-2,0,-5), 0.5f}, {float3(-3,0,-5), 0.5f}, {float3(-4,0,-5), 0.5f},
    {float3(4,0,-6), 0.5f}, {float3(3,0,-6), 0.5f}, {float3(2,0,-6), 0.5f}, {float3(1,0,-6), 0.5f}, {float3(0,0,-6), 0.5f}, {float3(-1,0,-6), 0.5f}, {float3(-2,0,-6), 0.5f}, {float3(-3,0,-6), 0.5f}, {float3(-4,0,-6), 0.5f},
    {float3(1.5f,1.5f,-2), 0.3f},
#endif // #if DO_BIG_SCENE
};
const int kSphereCount = sizeof(s_Spheres) / sizeof(s_Spheres[0]);

static SpheresSoA s_SpheresSoA(kSphereCount);

struct Material
{
    enum Type { Lambert, Metal, Dielectric };
    Type type;
    float3 albedo;
    float3 emissive;
    float roughness;
    float ri;
};

static Material s_SphereMats[kSphereCount] =
{
    { Material::Lambert, float3(0.8f, 0.8f, 0.8f), float3(0,0,0), 0, 0, },
    { Material::Lambert, float3(0.8f, 0.4f, 0.4f), float3(0,0,0), 0, 0, },
    { Material::Lambert, float3(0.4f, 0.8f, 0.4f), float3(0,0,0), 0, 0, },
    { Material::Metal, float3(0.4f, 0.4f, 0.8f), float3(0,0,0), 0, 0 },
    { Material::Metal, float3(0.4f, 0.8f, 0.4f), float3(0,0,0), 0, 0 },
    { Material::Metal, float3(0.4f, 0.8f, 0.4f), float3(0,0,0), 0.2f, 0 },
    { Material::Metal, float3(0.4f, 0.8f, 0.4f), float3(0,0,0), 0.6f, 0 },
    { Material::Dielectric, float3(0.4f, 0.4f, 0.4f), float3(0,0,0), 0, 1.5f },
    { Material::Lambert, float3(0.8f, 0.6f, 0.2f), float3(30,25,15), 0, 0 },
#if DO_BIG_SCENE
    { Material::Lambert, float3(0.1f, 0.1f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.2f, 0.2f, 0.2f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.3f, 0.3f, 0.3f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.4f, 0.4f, 0.4f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.5f, 0.5f, 0.5f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.6f, 0.6f, 0.6f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.7f, 0.7f, 0.7f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.8f, 0.8f, 0.8f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.9f, 0.9f, 0.9f), float3(0,0,0), 0, 0, },
    { Material::Metal, float3(0.1f, 0.1f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.2f, 0.2f, 0.2f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.3f, 0.3f, 0.3f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.4f, 0.4f, 0.4f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.5f, 0.5f, 0.5f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.6f, 0.6f, 0.6f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.7f, 0.7f, 0.7f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.8f, 0.8f, 0.8f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.9f, 0.9f, 0.9f), float3(0,0,0), 0, 0, },
    { Material::Metal, float3(0.8f, 0.1f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.8f, 0.5f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.8f, 0.8f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.4f, 0.8f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.1f, 0.8f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.1f, 0.8f, 0.5f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.1f, 0.8f, 0.8f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.1f, 0.1f, 0.8f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.5f, 0.1f, 0.8f), float3(0,0,0), 0, 0, },
    { Material::Lambert, float3(0.8f, 0.1f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.8f, 0.5f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.8f, 0.8f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.4f, 0.8f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.1f, 0.8f, 0.1f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.1f, 0.8f, 0.5f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.1f, 0.8f, 0.8f), float3(0,0,0), 0, 0, }, { Material::Lambert, float3(0.1f, 0.1f, 0.8f), float3(0,0,0), 0, 0, }, { Material::Metal, float3(0.5f, 0.1f, 0.8f), float3(0,0,0), 0, 0, },
    { Material::Lambert, float3(0.1f, 0.2f, 0.5f), float3(3,10,20), 0, 0 },
#endif
};

static int s_EmissiveSpheres[kSphereCount];
static int s_EmissiveSphereCount;

static Camera s_Cam;
static int s_ScreenWidth, s_ScreenHeight;
static float* s_TempPixels;

// For each bounce, iteration, this is everything we need to know about "the ray" (direction, current attenuation along it,
// pixel location, etc.)
struct RayData
{
    RayData() {}
    RayData(const Ray& r, const float3& atten, uint32_t pixelIndex_, uint32_t lightID_, bool shadow_, bool skipEmission_)
    : origX(r.orig.getX()), origY(r.orig.getY()), origZ(r.orig.getZ())
    , dirX(r.dir.getX()), dirY(r.dir.getY()), dirZ(r.dir.getZ())
    , attenX(atten.getX()), attenY(atten.getY()), attenZ(atten.getZ())
    , pixelIndex(pixelIndex_), lightID(lightID_), shadow(shadow_), skipEmission(skipEmission_) {}

    Ray GetRay() const { return Ray(float3(origX,origY,origZ), float3(dirX,dirY,dirZ)); }
    float3 GetAtten() const { return float3(attenX,attenY,attenZ); }
    float origX, origY, origZ;
    float dirX, dirY, dirZ;
    float attenX, attenY, attenZ;
    uint32_t pixelIndex;
    uint32_t lightID;
    bool shadow;
    bool skipEmission;
};

// Each bounce iteration will read rays to process from one buffer, and add next bounce into another buffer.
struct RayBuffer
{
    RayData* data;
    uint32_t size;
    uint32_t capacity;
    
    void AddRay(const RayData& r)
    {
        if (size < capacity)
        {
            data[size++] = r;
        }
    }
};

// Buffer to hold all the possible rays for whole image (all pixels, samples per pixel, double buffered, etc.)
static RayData* s_RayDataBuffer;
static uint32_t s_RayDataCapacityPerRow;


const float kMinT = 0.001f;
const float kMaxT = 1.0e7f;
const int kMaxDepth = 10;


static int HitWorld(const Ray& r, float tMin, float tMax, Hit& outHit)
{
    return HitSpheres(r, s_SpheresSoA, tMin, tMax, outHit);
}


static bool Scatter(const Material& mat, const Ray& r_in, const Hit& rec, float3& attenuation, Ray& scattered, /*float3& outLightE, int& inoutRayCount,*/ uint32_t& state)
{
    if (mat.type == Material::Lambert)
    {
        // random point on unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomUnitVector(state);
        scattered = Ray(rec.pos, normalize(target - rec.pos));
        attenuation = mat.albedo;
        return true;
    }
    else if (mat.type == Material::Metal)
    {
        AssertUnit(r_in.dir); AssertUnit(rec.normal);
        float3 refl = reflect(r_in.dir, rec.normal);
        // reflected ray, and random inside of sphere based on roughness
        float roughness = mat.roughness;
#if DO_MITSUBA_COMPARE
        roughness = 0; // until we get better BRDF for metals
#endif
        scattered = Ray(rec.pos, normalize(refl + roughness*RandomInUnitSphere(state)));
        attenuation = mat.albedo;
        return dot(scattered.dir, rec.normal) > 0;
    }
    else if (mat.type == Material::Dielectric)
    {
        AssertUnit(r_in.dir); AssertUnit(rec.normal);
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

static float3 SurfaceHit(const Ray& r, float3 rayAtten, uint32_t pixelIndex, bool raySkipEmission, const Hit& hit, int id, RayBuffer& buffer, uint32_t& state)
{
    assert(id >= 0 && id < kSphereCount);
    const Material& mat = s_SphereMats[id];
    Ray scattered;
    float3 atten;
    float3 lightE;
    if (Scatter(mat, r, hit, atten, scattered, state))
    {
        // Queue the scattered ray for next bounce iteration
        bool skipEmission = false;
#if DO_LIGHT_SAMPLING
        // dor Lambert materials, we are doing explicit light (emissive) sampling
        // for their contribution, so if the scattered ray hits the light again, don't add emission
        if (mat.type == Material::Lambert)
            skipEmission = true;
#endif
        buffer.AddRay(RayData(scattered, atten * rayAtten, pixelIndex, 0, false, skipEmission));
        
#if DO_LIGHT_SAMPLING
        // sample lights
        if (mat.type == Material::Lambert)
        {
            for (int j = 0; j < s_EmissiveSphereCount; ++j)
            {
                int i = s_EmissiveSpheres[j];
                const Material& smat = s_SphereMats[i];
                if (&mat == &smat)
                    continue; // skip self
                const Sphere& s = s_Spheres[i];

                // create a random direction towards sphere
                // coord system for sampling: sw, su, sv
                float3 sw = normalize(s.center - hit.pos);
                float3 su = normalize(cross(fabs(sw.getX())>0.01f ? float3(0,1,0):float3(1,0,0), sw));
                float3 sv = cross(sw, su);
                // sample sphere by solid angle
                float cosAMax = sqrtf(1.0f - s.radius*s.radius / sqLength(hit.pos-s.center));
                float eps1 = RandomFloat01(state), eps2 = RandomFloat01(state);
                float cosA = 1.0f - eps1 + eps1 * cosAMax;
                float sinA = sqrtf(1.0f - cosA*cosA);
                float phi = 2 * kPI * eps2;
                float3 l = su * (cosf(phi) * sinA) + sv * (sinf(phi) * sinA) + sw * cosA;

                // Queue a shadow ray for next bounce iteration
                float omega = 2 * kPI * (1-cosAMax);
                AssertUnit(r.dir);
                float3 nl = dot(hit.normal, r.dir) < 0 ? hit.normal : -hit.normal;
                float3 shadowAtt = (mat.albedo * smat.emissive) * (std::max(0.0f, dot(l, nl)) * omega / kPI);
                buffer.AddRay(RayData(Ray(hit.pos, l), shadowAtt * rayAtten, pixelIndex, i, true, false));
            }
        }
#endif // #if DO_LIGHT_SAMPLING
    }
    return raySkipEmission ? float3(0,0,0) : mat.emissive; // don't add material emission if told so
}


static float3 SkyHit(const Ray& r)
{
    #if DO_MITSUBA_COMPARE
    return float3(0.15f,0.21f,0.3f); // easier compare with Mitsuba's constant environment light
    #else
    float3 unitDir = r.dir;
    float t = 0.5f*(unitDir.getY() + 1.0f);
    return ((1.0f-t)*float3(1.0f, 1.0f, 1.0f) + t*float3(0.5f, 0.7f, 1.0f)) * 0.3f;
    #endif
}


static enkiTaskScheduler* g_TS;

void InitializeTest(int screenWidth, int screenHeight)
{
    s_ScreenWidth = screenWidth;
    s_ScreenHeight = screenHeight;
    s_TempPixels = new float[screenWidth * screenHeight * 4];

    // for every bounce iteration, each pixel/sample potentially scatters one ray, plus
    // max amount of shadow rays
    const int kMaxShadowRays = 3;
    s_RayDataCapacityPerRow = screenWidth * DO_SAMPLES_PER_PIXEL * (1 + kMaxShadowRays);
    // and we need two of those buffers (bounce input, bounce output)
    size_t rayDataCapacity = s_RayDataCapacityPerRow * 2 * screenHeight;
    printf("Total ray buffers: %.1fMB (%.1fMrays)\n", rayDataCapacity * sizeof(RayData) / 1024.0 / 1024.0, rayDataCapacity / 1000.0 / 1000.0);
    s_RayDataBuffer = new RayData[rayDataCapacity];
    
    g_TS = enkiNewTaskScheduler();
    enkiInitTaskScheduler(g_TS);
}

void ShutdownTest()
{
    enkiDeleteTaskScheduler(g_TS);
    delete[] s_TempPixels;
}

struct JobData
{
    float time;
    int frameCount;
    int screenWidth, screenHeight;
    float* backbuffer;
    Camera* cam;
    std::atomic<int> rayCount;
};

static void TraceRowJob(uint32_t start, uint32_t end, uint32_t threadnum, void* data_)
{
    JobData& data = *(JobData*)data_;
    float* backbuffer = data.backbuffer + start * data.screenWidth * 4;
    float invWidth = 1.0f / data.screenWidth;
    float invHeight = 1.0f / data.screenHeight;
    float lerpFac = float(data.frameCount) / float(data.frameCount+1);
#if DO_ANIMATE
    lerpFac *= DO_ANIMATE_SMOOTHING;
#endif
#if !DO_PROGRESSIVE
    lerpFac = 0;
#endif
    int rayCount = 0;
    
    // clear temp buffer to zero
    float* tmpPixels = s_TempPixels + start * data.screenWidth * 4;
    memset(tmpPixels, 0, (end-start) * data.screenWidth * 4 * sizeof(s_TempPixels[0]));

    // two ray buffers
    RayBuffer buffer1, buffer2;
    buffer1.capacity = buffer2.capacity = (end-start) * s_RayDataCapacityPerRow;
    buffer1.size = buffer2.size = 0;
    buffer1.data = s_RayDataBuffer + start * s_RayDataCapacityPerRow * 2;
    buffer2.data = buffer1.data + buffer1.capacity;

    // queue up initial eye rays
    uint32_t state = (start * 9781 + data.frameCount * 6271) | 1;
    uint32_t pixelIndex = 0;
    for (uint32_t y = start; y < end; ++y)
    {
        for (int x = 0; x < data.screenWidth; ++x)
        {
            float3 col(0, 0, 0);
            for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
            {
                float u = float(x + RandomFloat01(state)) * invWidth;
                float v = float(y + RandomFloat01(state)) * invHeight;
                Ray r = data.cam->GetRay(u, v, state);
                buffer1.AddRay(RayData(r, float3(1,1,1), pixelIndex, 0, false, false));
            }
            pixelIndex += 4;
        }
    }

    // process rays from one buffer into another buffer, for all bounce iterations
    for (int depth = 0; depth < kMaxDepth; ++depth)
    {
        buffer2.size = 0;
        for (int i = 0, n = buffer1.size; i != n; ++i)
        {
            // Do a ray cast against the world
            const RayData& rd = buffer1.data[i];
            Ray rdRay = rd.GetRay();
            float3 rdAtten = rd.GetAtten();
            Hit rec;
            int id = HitWorld(rdRay, kMinT, kMaxT, rec);
            
            // Does not hit anything?
            if (id < 0)
            {
                if (!rd.shadow)
                {
                    // for non-shadow rays, evaluate and add sky
                    float3 col = SkyHit(rdRay) * rdAtten;
                    tmpPixels[rd.pixelIndex+0] += col.getX();
                    tmpPixels[rd.pixelIndex+1] += col.getY();
                    tmpPixels[rd.pixelIndex+2] += col.getZ();
                }
                continue;
            }

            // A non-shadow ray hit something; evaluate material response (this can queue new rays for next bounce)
            if (!rd.shadow)
            {
                float3 col = SurfaceHit(rdRay, rdAtten, rd.pixelIndex, rd.skipEmission, rec, id, buffer2, state) * rdAtten;
                tmpPixels[rd.pixelIndex+0] += col.getX();
                tmpPixels[rd.pixelIndex+1] += col.getY();
                tmpPixels[rd.pixelIndex+2] += col.getZ();
                continue;
            }

            // A shadow ray; add illumination if we hit the needed light
            if (rd.lightID == id)
            {
                assert(rd.shadow);
                float3 col = rdAtten;
                tmpPixels[rd.pixelIndex+0] += col.getX();
                tmpPixels[rd.pixelIndex+1] += col.getY();
                tmpPixels[rd.pixelIndex+2] += col.getZ();
                continue;
            }
        }

        rayCount += buffer1.size;
        // Swap ray buffers and will go to next bounce iteration
        std::swap(buffer1, buffer2);
    }
    
    // Blend results into backbuffer
    for (uint32_t y = start; y < end; ++y)
    {
        for (int x = 0; x < data.screenWidth; ++x)
        {
            float3 col(tmpPixels[0], tmpPixels[1], tmpPixels[2]);
            col *= 1.0f / float(DO_SAMPLES_PER_PIXEL);
            
            float3 prev(backbuffer[0], backbuffer[1], backbuffer[2]);
            col = prev * lerpFac + col * (1-lerpFac);
            col.store(backbuffer);
            backbuffer += 4;
            tmpPixels += 4;
        }
    }
    
    data.rayCount += rayCount;
}

void UpdateTest(float time, int frameCount)
{
#if DO_ANIMATE
    s_Spheres[1].center.y = cosf(time) + 1.0f;
    s_Spheres[8].center.z = sinf(time)*0.3f;
#endif
    float3 lookfrom(0, 2, 3);
    float3 lookat(0, 0, 0);
    float distToFocus = 3;
#if DO_MITSUBA_COMPARE
    float aperture = 0.0f;
#else
    float aperture = 0.1f;
#endif
#if DO_BIG_SCENE
    aperture *= 0.2f;
#endif

    s_EmissiveSphereCount = 0;
    for (int i = 0; i < kSphereCount; ++i)
    {
        Sphere& s = s_Spheres[i];
        s.UpdateDerivedData();
        s_SpheresSoA.centerX[i] = s.center.getX();
        s_SpheresSoA.centerY[i] = s.center.getY();
        s_SpheresSoA.centerZ[i] = s.center.getZ();
        s_SpheresSoA.sqRadius[i] = s.radius * s.radius;
        s_SpheresSoA.invRadius[i] = s.invRadius;

        // Remember IDs of emissive spheres (light sources)
        const Material& smat = s_SphereMats[i];
        if (smat.emissive.getX() > 0 || smat.emissive.getY() > 0 || smat.emissive.getZ() > 0)
        {
            s_EmissiveSpheres[s_EmissiveSphereCount] = i;
            s_EmissiveSphereCount++;
        }
    }

    s_Cam = Camera(lookfrom, lookat, float3(0, 1, 0), 60, float(s_ScreenWidth) / float(s_ScreenHeight), aperture, distToFocus);
}

void DrawTest(float time, int frameCount, float* backbuffer, int& outRayCount)
{
    JobData args;
    args.time = time;
    args.frameCount = frameCount;
    args.screenWidth = s_ScreenWidth;
    args.screenHeight = s_ScreenHeight;
    args.backbuffer = backbuffer;
    args.cam = &s_Cam;
    args.rayCount = 0;
    enkiTaskSet* task = enkiCreateTaskSet(g_TS, TraceRowJob);
    bool threaded = true;
    enkiAddTaskSetToPipeMinRange(g_TS, task, &args, s_ScreenHeight, threaded ? 4 : s_ScreenHeight);
    enkiWaitForTaskSet(g_TS, task);
    enkiDeleteTaskSet(task);
    outRayCount = args.rayCount;
}

void GetObjectCount(int& outCount, int& outObjectSize, int& outMaterialSize, int& outCamSize)
{
    outCount = kSphereCount;
    outObjectSize = sizeof(Sphere);
    outMaterialSize = sizeof(Material);
    outCamSize = sizeof(Camera);
}

void GetSceneDesc(void* outObjects, void* outMaterials, void* outCam, void* outEmissives, int* outEmissiveCount)
{
    memcpy(outObjects, s_Spheres, kSphereCount * sizeof(s_Spheres[0]));
    memcpy(outMaterials, s_SphereMats, kSphereCount * sizeof(s_SphereMats[0]));
    memcpy(outCam, &s_Cam, sizeof(s_Cam));
    memcpy(outEmissives, s_EmissiveSpheres, s_EmissiveSphereCount * sizeof(s_EmissiveSpheres[0]));
    *outEmissiveCount = s_EmissiveSphereCount;
}

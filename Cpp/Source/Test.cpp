#include "Test.h"
#include "Maths.h"
#include <algorithm>
#include "enkiTS/TaskScheduler_c.h"
#include <atomic>
#include <vector>

#define DO_SAMPLES_PER_PIXEL 4
#define DO_ANIMATE 0
#define DO_ANIMATE_SMOOTHING 0.5f
#define DO_LIGHT_SAMPLING 1

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
};
const int kSphereCount = sizeof(s_Spheres) / sizeof(s_Spheres[0]);

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
};

const float kMinT = 0.001f;
const float kMaxT = 1.0e7f;
const int kMaxDepth = 10;


struct RayPayload
{
    RayPayload() {}
    RayPayload(const float3& atten_, uint32_t pixelIndex_, uint32_t depth_, uint32_t lightID_, uint32_t shadow_)
        : atten(atten_), pixelIndex(pixelIndex_), depth(depth_), lightID(lightID_), shadow(shadow_) {}
    float3 atten;
    uint32_t pixelIndex : 22;
    uint32_t depth : 4;
    uint32_t lightID : 5;
    uint32_t shadow : 1;
};

struct TraceContext
{
    std::vector<Ray> queries;
    std::vector<RayPayload> payloads;
};

int HitWorld(const Ray& r, float tMin, float tMax, Hit& outHit)
{
    Hit tmpHit;
    int id = -1;
    float closest = tMax;
    for (int i = 0; i < kSphereCount; ++i)
    {
        if (HitSphere(r, s_Spheres[i], tMin, closest, tmpHit))
        {
            closest = tmpHit.t;
            outHit = tmpHit;
            id = i;
        }
    }
    return id;
}


static bool Scatter(const Material& mat, const Ray& r_in, const Hit& rec, float3& attenuation, Ray& scattered)
{
    if (mat.type == Material::Lambert)
    {
        // random point inside unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomInUnitSphere();
        scattered = Ray(rec.pos, normalize(target - rec.pos));
        attenuation = mat.albedo;
        return true;
    }
    else if (mat.type == Material::Metal)
    {
        AssertUnit(r_in.dir); AssertUnit(rec.normal);
        float3 refl = reflect(r_in.dir, rec.normal);
        // reflected ray, and random inside of sphere based on roughness
        scattered = Ray(rec.pos, normalize(refl + mat.roughness*RandomInUnitSphere()));
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
        if (RandomFloat01() < reflProb)
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

static float3 SkyHit(const Ray& r)
{
    float t = 0.5f*(r.dir.y + 1.0f);
    return ((1.0f-t)*float3(1.0f, 1.0f, 1.0f) + t*float3(0.5f, 0.7f, 1.0f)) * 0.3f;
}

static float3 TraceHit(const Ray& r, const RayPayload& rp, const Hit& hit, int id, TraceContext& ctx)
{
    assert(id >= 0 && id < kSphereCount);
    const Material& mat = s_SphereMats[id];
    Ray scattered;
    float3 atten;
    if (rp.depth < kMaxDepth)
    {
        if (Scatter(mat, r, hit, atten, scattered))
        {
            ctx.queries.push_back(scattered);
            ctx.payloads.push_back(RayPayload(atten * rp.atten, rp.pixelIndex, (uint32_t)(rp.depth+1), 0, 0));
            
#if DO_LIGHT_SAMPLING
            if (mat.type == Material::Lambert)
            {
                for (int i = 0; i < kSphereCount; ++i)
                {
                    const Material& smat = s_SphereMats[i];
                    if (smat.emissive.x <= 0 && smat.emissive.y <= 0 && smat.emissive.z <= 0)
                        continue; // skip non-emissive
                    if (&mat == &smat)
                        continue; // skip self
                    const Sphere& s = s_Spheres[i];
                    
                    // create a random direction towards sphere
                    // coord system for sampling: sw, su, sv
                    float3 sw = normalize(s.center - hit.pos);
                    float3 su = normalize(cross(fabs(sw.x)>0.01f ? float3(0,1,0):float3(1,0,0), sw));
                    float3 sv = cross(sw, su);
                    // sample sphere by solid angle
                    float cosAMax = sqrtf(1.0f - s.radius*s.radius / (hit.pos-s.center).sqLength());
                    float eps1 = RandomFloat01(), eps2 = RandomFloat01();
                    float cosA = 1.0f - eps1 + eps1 * cosAMax;
                    float sinA = sqrtf(1.0f - cosA*cosA);
                    float phi = 2 * kPI * eps2;
                    float3 l = su * cosf(phi) * sinA + sv * sin(phi) * sinA + sw * cosA;
                    l.normalize();
                    
                    // queue a shadow ray
                    float omega = 2 * kPI * (1-cosAMax);
                    AssertUnit(r.dir);
                    float3 nl = dot(hit.normal, r.dir) < 0 ? hit.normal : -hit.normal;
                    float3 shadowAtt = (mat.albedo * smat.emissive) * (std::max(0.0f, dot(l, nl)) * omega / kPI);
                    
                    ctx.queries.push_back(Ray(hit.pos, l));
                    ctx.payloads.push_back(RayPayload(shadowAtt * rp.atten, rp.pixelIndex, (uint32_t)(rp.depth+1), (uint32_t)i, 1));
                }
            }
#endif // #if DO_LIGHT_SAMPLING

            return mat.emissive;
        }
    }
    return mat.emissive;
}

static enkiTaskScheduler* g_TS;

void InitializeTest()
{
    g_TS = enkiNewTaskScheduler();
    enkiInitTaskScheduler(g_TS);
}

void ShutdownTest()
{
    enkiDeleteTaskScheduler(g_TS);
}

struct JobData
{
    float time;
    int frameCount;
    int screenWidth, screenHeight;
    float* backbuffer;
    float* tmpbuffer;
    Camera* cam;
    std::atomic<int> rayCount;
};

static void TraceRowJob(uint32_t start, uint32_t end, uint32_t threadnum, void* data_)
{
    JobData& data = *(JobData*)data_;
    float* backbuffer = data.backbuffer + start * data.screenWidth * 4;
    float* tmpbuffer = data.tmpbuffer + start * data.screenWidth * 4;
    float invWidth = 1.0f / data.screenWidth;
    float invHeight = 1.0f / data.screenHeight;
    float lerpFac = float(data.frameCount) / float(data.frameCount+1);
#if DO_ANIMATE
    lerpFac *= DO_ANIMATE_SMOOTHING;
#endif
    int rayCount = 0;
    TraceContext ctx;
    auto space = (end-start)*data.screenWidth*DO_SAMPLES_PER_PIXEL;
    ctx.queries.reserve(space);
    ctx.payloads.reserve(space);
    
    // clear temp buffer to zero
    memset(tmpbuffer, 0, (end-start)*data.screenWidth*4*sizeof(tmpbuffer[0]));
    
    // initial eye rays
    int pixelIndex = 0;
    for (uint32_t y = start; y < end; ++y)
    {
        for (int x = 0; x < data.screenWidth; ++x)
        {
            for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
            {
                float u = float(x + RandomFloat01()) * invWidth;
                float v = float(y + RandomFloat01()) * invHeight;
                Ray r = data.cam->GetRay(u, v);
                ctx.queries.push_back(r);
                ctx.payloads.push_back(RayPayload(float3(1,1,1), (uint32_t)(pixelIndex*4), 0, 0, 0));
            }
            ++pixelIndex;
        }
    }
    
    // process ray requests
    while (!ctx.queries.empty())
    {
        TraceContext newctx;
        int rcount = (int)ctx.queries.size();
        newctx.queries.reserve(rcount);
        newctx.payloads.reserve(rcount);
        
        for (int i = 0; i < rcount; ++i)
        {
            const Ray& rq = ctx.queries[i];
            const RayPayload& rp = ctx.payloads[i];
            int pix = rp.pixelIndex;
            Hit hit;
            int hitID = HitWorld(rq, kMinT, kMaxT, hit);
            if (hitID < 0)
            {
                if (!rp.shadow)
                {
                    float3 col = SkyHit(rq) * rp.atten;
                    tmpbuffer[pix+0] += col.x;
                    tmpbuffer[pix+1] += col.y;
                    tmpbuffer[pix+2] += col.z;
                }
                continue;
            }
            if (!rp.shadow)
            {
                float3 col = TraceHit(rq, rp, hit, hitID, newctx) * rp.atten;
                tmpbuffer[pix+0] += col.x;
                tmpbuffer[pix+1] += col.y;
                tmpbuffer[pix+2] += col.z;
                continue;
            }
            if (rp.lightID == hitID)
            {
                assert(rp.shadow);
                float3 col = rp.atten;
                tmpbuffer[pix+0] += col.x;
                tmpbuffer[pix+1] += col.y;
                tmpbuffer[pix+2] += col.z;
                continue;
            }
        }
        
        rayCount += rcount;
        
        ctx.queries.swap(newctx.queries);
        ctx.payloads.swap(newctx.payloads);
    }

    // blend results into backbuffer
    for (uint32_t y = start; y < end; ++y)
    {
        for (int x = 0; x < data.screenWidth; ++x)
        {
            float3 col(tmpbuffer[0], tmpbuffer[1], tmpbuffer[2]);
            col *= 1.0f / float(DO_SAMPLES_PER_PIXEL);
            col = float3(sqrtf(col.x), sqrtf(col.y), sqrtf(col.z));
            
            float3 prev(backbuffer[0], backbuffer[1], backbuffer[2]);
            col = prev * lerpFac + col * (1-lerpFac);
            backbuffer[0] = col.x;
            backbuffer[1] = col.y;
            backbuffer[2] = col.z;
            backbuffer += 4;
            tmpbuffer += 4;
        }
    }
    data.rayCount += rayCount;
}

void DrawTest(float time, int frameCount, int screenWidth, int screenHeight, float* backbuffer, float* tmpbuffer, int& outRayCount)
{
#if DO_ANIMATE
    s_Spheres[1].center.y = cosf(time)+1.0f;
    s_Spheres[8].center.z = sinf(time)*0.3f;
#endif
    float3 lookfrom(0,2,3);
    float3 lookat(0,0,0);
    float distToFocus = 3;
    float aperture = 0.1f;
    
    for (int i = 0; i < kSphereCount; ++i)
        s_Spheres[i].UpdateDerivedData();
    
    Camera cam(lookfrom, lookat, float3(0,1,0), 60, float(screenWidth)/float(screenHeight), aperture, distToFocus);

    JobData args;
    args.time = time;
    args.frameCount = frameCount;
    args.screenWidth = screenWidth;
    args.screenHeight = screenHeight;
    args.backbuffer = backbuffer;
    args.tmpbuffer = tmpbuffer;
    args.cam = &cam;
    args.rayCount = 0;
    enkiTaskSet* task = enkiCreateTaskSet(g_TS, TraceRowJob);
    bool threaded = true;
    enkiAddTaskSetToPipeMinRange(g_TS, task, &args, screenHeight, threaded ? 4 : screenHeight);
    enkiWaitForTaskSet(g_TS, task);
    enkiDeleteTaskSet(task);
    outRayCount = args.rayCount;
}


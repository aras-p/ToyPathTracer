#include "Test.h"
#include "Maths.h"
#include <algorithm>
#include "enkiTS/TaskScheduler_c.h"

const int kSphereCount = 8;
static Sphere s_Spheres[kSphereCount] =
{
    {float3(0,-100.5,-1), 100},
    {float3(2,0,-1), 0.5f},
    {float3(0,0,-1), 0.5f},
    {float3(-2,0,-1), 0.5f},
    {float3(2,0,1), 0.5f},
    {float3(0,0,1), 0.5f},
    {float3(-2,0,1), 0.5f},
    {float3(0.5f,1,0.5f), 0.5f},
};

struct Material
{
    enum Type { Lambert, Metal, Dielectric };
    float3 albedo;
    float roughness;
    float ri;
    Type type;
};

static Material s_SphereMats[kSphereCount] =
{
    { float3(0.8f, 0.8f, 0.8f), 0, 0, Material::Lambert },
    { float3(0.8f, 0.4f, 0.4f), 0, 0, Material::Lambert },
    { float3(0.4f, 0.8f, 0.4f), 0, 0, Material::Lambert },
    { float3(0.4f, 0.4f, 0.8f), 0, 0, Material::Metal },
    { float3(0.4f, 0.8f, 0.4f), 0, 0, Material::Metal },
    { float3(0.4f, 0.8f, 0.4f), 0.2f, 0, Material::Metal },
    { float3(0.4f, 0.8f, 0.4f), 0.6f, 0, Material::Metal },
    { float3(0.4f, 0.8f, 0.4f), 0, 1.5f, Material::Dielectric },
};

const float kMinT = 0.001f;
const float kMaxT = 1.0e7f;
const int kMaxDepth = 10;

static bool Scatter(const Material& mat, const Ray& r_in, const Hit& rec, float3& attenuation, Ray& scattered)
{
    if (mat.type == Material::Lambert)
    {
        // random point inside unit sphere that is tangent to the hit point
        float3 target = rec.pos + rec.normal + RandomInUnitSphere();
        scattered = Ray(rec.pos, target - rec.pos);
        attenuation = mat.albedo;
    }
    else if (mat.type == Material::Metal)
    {
        float3 refl = reflect(normalize(r_in.dir), rec.normal);
        // reflected ray, and random inside of sphere based on roughness
        scattered = Ray(rec.pos, refl + mat.roughness*RandomInUnitSphere());
        attenuation = mat.albedo;
        return dot(scattered.dir, rec.normal) > 0;
    }
    else if (mat.type == Material::Dielectric)
    {
        float3 outwardN;
        float3 rdir = normalize(r_in.dir);
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
            nint = 1.0 / mat.ri;
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
            scattered = Ray(rec.pos, refl);
        else
            scattered = Ray(rec.pos, refr);
    }
    else
    {
        attenuation = float3(1,0,1);
        return false;
    }
    return true;
}

bool HitWorld(const Ray& r, float tMin, float tMax, Hit& outHit, int& outID)
{
    Hit tmpHit;
    bool anything = false;
    double closest = tMax;
    for (int i = 0; i < kSphereCount; ++i)
    {
        if (HitSphere(r, s_Spheres[i], tMin, closest, tmpHit))
        {
            anything = true;
            closest = tmpHit.t;
            outHit = tmpHit;
            outID = i;
        }
    }
    return anything;
}

static float3 Trace(const Ray& r, int depth)
{
    Hit rec;
    int id = 0;
    if (HitWorld(r, kMinT, kMaxT, rec, id))
    {
        Ray scattered;
        float3 attenuation;
        if (depth < kMaxDepth && Scatter(s_SphereMats[id], r, rec, attenuation, scattered))
        {
            return attenuation * Trace(scattered, depth+1);
        }
        else
        {
            return float3(0,0,0);
        }
    }
    else
    {
        // sky
        float3 unitDir = normalize(r.dir);
        float t = 0.5*(unitDir.y + 1.0);
        return (1.0-t)*float3(1.0, 1.0, 1.0) + t*float3(0.5, 0.7, 1.0);
    }
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
    Camera* cam;
};

static void TraceRowJob(uint32_t start, uint32_t end, uint32_t threadnum, void* data_)
{
    JobData& data = *(JobData*)data_;
    float* backbuffer = data.backbuffer + start * data.screenWidth * 4;
    for (int y = start; y < end; ++y)
    {
        for (int x = 0; x < data.screenWidth; ++x)
        {
            float3 col(0, 0, 0);
            const int kSamples = 1;
            for (int s = 0; s < kSamples; s++)
            {
                float u = float(x + RandomFloat01()) / float(data.screenWidth);
                float v = float(y + RandomFloat01()) / float(data.screenHeight);
                Ray r = data.cam->GetRay(u, v);
                col += Trace(r, 0);
            }
            col *= 1.0f / float(kSamples);
            col = float3(sqrtf(col.x), sqrtf(col.y), sqrtf(col.z));
            
            float3 prev(backbuffer[0], backbuffer[1], backbuffer[2]);
            float lerpFac = float(data.frameCount) / float(data.frameCount+1);
            //lerpFac *= 0.9f;
            //lerpFac = 0.9f;
            //lerpFac = 0;
            col = prev * lerpFac + col * (1-lerpFac);
            backbuffer[0] = col.x;
            backbuffer[1] = col.y;
            backbuffer[2] = col.z;
            backbuffer += 4;
        }
    }
}

void DrawTest(float time, int frameCount, int screenWidth, int screenHeight, float* backbuffer)
{
    //s_Spheres[1].center.y = cosf(time)+1.0f;
    //float3 lookfrom(13,2,3); lookfrom *= 0.25f;
    float3 lookfrom(0,2,3);
    float3 lookat(0,0,0);
    float distToFocus = 3;
    float aperture = 0.05f;
    
    Camera cam(lookfrom, lookat, float3(0,1,0), 60, float(screenWidth)/float(screenHeight), aperture, distToFocus);

    JobData args;
    args.time = time;
    args.frameCount = frameCount;
    args.screenWidth = screenWidth;
    args.screenHeight = screenHeight;
    args.backbuffer = backbuffer;
    args.cam = &cam;
    enkiTaskSet* task = enkiCreateTaskSet(g_TS, TraceRowJob);
    bool threaded = true;
    enkiAddTaskSetToPipeMinRange(g_TS, task, &args, screenHeight, threaded ? 4 : screenHeight);
    enkiWaitForTaskSet(g_TS, task);
    enkiDeleteTaskSet(task);
}


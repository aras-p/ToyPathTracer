//#define DO_ANIMATE
#define DO_LIGHT_SAMPLING
#define DO_THREADED

using static MathUtil;
using Unity.Mathematics;
using static Unity.Mathematics.math;
using Unity.Collections;
using Unity.Jobs;

struct Material
{
    public enum Type { Lambert, Metal, Dielectric };
    public Type type;
    public float3 albedo;
    public float3 emissive;
    public float roughness;
    public float ri;
    public Material(Type t, float3 a, float3 e, float r, float i)
    {
        type = t; albedo = a; emissive = e; roughness = r; ri = i;
    }
    public bool HasEmission => emissive.x > 0 || emissive.y > 0 || emissive.z > 0;
};

class Test
{
    const int DO_SAMPLES_PER_PIXEL = 4;
    const float DO_ANIMATE_SMOOTHING = 0.5f;

    static Sphere[] s_SpheresData = {
        new Sphere(new float3(0,-100.5f,-1), 100),
        new Sphere(new float3(2,0,-1), 0.5f),
        new Sphere(new float3(0,0,-1), 0.5f),
        new Sphere(new float3(-2,0,-1), 0.5f),
        new Sphere(new float3(2,0,1), 0.5f),
        new Sphere(new float3(0,0,1), 0.5f),
        new Sphere(new float3(-2,0,1), 0.5f),
        new Sphere(new float3(0.5f,1,0.5f), 0.5f),
        new Sphere(new float3(-1.5f,1.5f,0f), 0.3f),
    };

    static Material[] s_SphereMatsData = {
        new Material(Material.Type.Lambert,     new float3(0.8f, 0.8f, 0.8f), new float3(0,0,0), 0, 0),
        new Material(Material.Type.Lambert,     new float3(0.8f, 0.4f, 0.4f), new float3(0,0,0), 0, 0),
        new Material(Material.Type.Lambert,     new float3(0.4f, 0.8f, 0.4f), new float3(0,0,0), 0, 0),
        new Material(Material.Type.Metal,       new float3(0.4f, 0.4f, 0.8f), new float3(0,0,0), 0, 0),
        new Material(Material.Type.Metal,       new float3(0.4f, 0.8f, 0.4f), new float3(0,0,0), 0, 0),
        new Material(Material.Type.Metal,       new float3(0.4f, 0.8f, 0.4f), new float3(0,0,0), 0.2f, 0),
        new Material(Material.Type.Metal,       new float3(0.4f, 0.8f, 0.4f), new float3(0,0,0), 0.6f, 0),
        new Material(Material.Type.Dielectric,  new float3(0.4f, 0.4f, 0.4f), new float3(0,0,0), 0, 1.5f),
        new Material(Material.Type.Lambert,     new float3(0.8f, 0.6f, 0.2f), new float3(30,25,15), 0, 0),
    };

    const float kMinT = 0.001f;
    const float kMaxT = 1.0e7f;
    const int kMaxDepth = 10;


    static bool HitWorld(Ray r, float tMin, float tMax, ref Hit outHit, ref int outID, NativeArray<Sphere> spheres)
    {
        Hit tmpHit = default(Hit);
        bool anything = false;
        float closest = tMax;
        for (int i = 0; i < spheres.Length; ++i)
        {
            if (spheres[i].HitSphere(r, tMin, closest, ref tmpHit))
            {
                anything = true;
                closest = tmpHit.t;
                outHit = tmpHit;
                outID = i;
            }
        }
        return anything;
    }

    static bool Scatter(Material mat, Ray r_in, Hit rec, out float3 attenuation, out Ray scattered, out float3 outLightE, ref int inoutRayCount, NativeArray<Sphere> spheres, NativeArray<Material> materials, ref uint randState)
    {
        outLightE = new float3(0, 0, 0);
        if (mat.type == Material.Type.Lambert)
        {
            // random point inside unit sphere that is tangent to the hit point
            float3 target = rec.pos + rec.normal + MathUtil.RandomUnitVector(ref randState);
            scattered = new Ray(rec.pos, normalize(target - rec.pos));
            attenuation = mat.albedo;

            // sample lights
#if DO_LIGHT_SAMPLING
            for (int i = 0; i < spheres.Length; ++i)
            {
                if (!materials[i].HasEmission)
                    continue; // skip non-emissive
                //@TODO if (&mat == &smat)
                //    continue; // skip self
                var s = spheres[i];

                // create a random direction towards sphere
                // coord system for sampling: sw, su, sv
                float3 sw = normalize(s.center - rec.pos);
                float3 su = normalize(cross(abs(sw.x) > 0.01f ? new float3(0, 1, 0) : new float3(1, 0, 0), sw));
                float3 sv = cross(sw, su);
                // sample sphere by solid angle
                float cosAMax = sqrt(max(0.0f, 1.0f - s.radius * s.radius / lengthSquared(rec.pos - s.center)));
                float eps1 = RandomFloat01(ref randState), eps2 = RandomFloat01(ref randState);
                float cosA = 1.0f - eps1 + eps1 * cosAMax;
                float sinA = sqrt(1.0f - cosA * cosA);
                float phi = 2 * PI * eps2;
                float3 l = su * cos(phi) * sinA + sv * sin(phi) * sinA + sw * cosA;
                l = normalize(l);

                // shoot shadow ray
                Hit lightHit = default(Hit);
                int hitID = 0;
                ++inoutRayCount;
                if (HitWorld(new Ray(rec.pos, l), kMinT, kMaxT, ref lightHit, ref hitID, spheres) && hitID == i)
                {
                    float omega = 2 * PI * (1 - cosAMax);

                    float3 rdir = r_in.dir;
                    float3 nl = dot(rec.normal, rdir) < 0 ? rec.normal : -rec.normal;
                    outLightE += (mat.albedo * materials[i].emissive) * (max(0.0f, dot(l, nl)) * omega / PI);
                }
            }
#endif
            return true;
        }
        else if (mat.type == Material.Type.Metal)
        {
            float3 refl = reflect(r_in.dir, rec.normal);
            // reflected ray, and random inside of sphere based on roughness
            scattered = new Ray(rec.pos, normalize(refl + mat.roughness * RandomInUnitSphere(ref randState)));
            attenuation = mat.albedo;
            return dot(scattered.dir, rec.normal) > 0;
        }
        else if (mat.type == Material.Type.Dielectric)
        {
            float3 outwardN;
            float3 rdir = r_in.dir;
            float3 refl = reflect(rdir, rec.normal);
            float nint;
            attenuation = new float3(1, 1, 1);
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
            if (Refract(rdir, outwardN, nint, out refr))
            {
                reflProb = Schlick(cosine, mat.ri);
            }
            else
            {
                reflProb = 1;
            }
            if (RandomFloat01(ref randState) < reflProb)
                scattered = new Ray(rec.pos, normalize(refl));
            else
                scattered = new Ray(rec.pos, normalize(refr));
        }
        else
        {
            attenuation = new float3(1, 0, 1);
            scattered = default(Ray);
            return false;
        }
        return true;
    }

    static float3 Trace(Ray r, int depth, ref int inoutRayCount, NativeArray<Sphere> spheres, NativeArray<Material> materials, ref uint randState, bool doMaterialE = true)
    {
        Hit rec = default(Hit);
        int id = 0;
        ++inoutRayCount;
        if (HitWorld(r, kMinT, kMaxT, ref rec, ref id, spheres))
        {
            Ray scattered;
            float3 attenuation;
            float3 lightE;
            var mat = materials[id];
            var matE = mat.emissive;
            if (depth < kMaxDepth && Scatter(mat, r, rec, out attenuation, out scattered, out lightE, ref inoutRayCount, spheres, materials, ref randState))
            {
                #if DO_LIGHT_SAMPLING
                if (!doMaterialE) matE = new float3(0, 0, 0);
                doMaterialE = (mat.type != Material.Type.Lambert);
                #endif
                return matE + lightE + attenuation * Trace(scattered, depth + 1, ref inoutRayCount, spheres, materials, ref randState, doMaterialE);
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
            float t = 0.5f * (unitDir.y + 1.0f);
            return ((1.0f - t) * new float3(1.0f, 1.0f, 1.0f) + t * new float3(0.5f, 0.7f, 1.0f)) * 0.3f;
        }
    }

    [ComputeJobOptimization]
    struct TraceRowJob : IJobParallelFor
    {
        public int screenWidth, screenHeight, frameCount;
        [NativeDisableParallelForRestriction] public NativeArray<UnityEngine.Color> backbuffer;
        public Camera cam;

        [NativeDisableParallelForRestriction] public NativeArray<int> rayCounter;
        [NativeDisableParallelForRestriction] public NativeArray<Sphere> spheres;
        [NativeDisableParallelForRestriction] public NativeArray<Material> materials;

        public void Execute(int y)
        {
            int backbufferIdx = y * screenWidth;
            float invWidth = 1.0f / screenWidth;
            float invHeight = 1.0f / screenHeight;
            float lerpFac = (float)frameCount / (float)(frameCount + 1);
#if DO_ANIMATE
            lerpFac *= DO_ANIMATE_SMOOTHING;
#endif
            uint state = (uint)(y * 9781 + frameCount * 6271) | 1;
            int rayCount = 0;
            for (int x = 0; x < screenWidth; ++x)
            {
                float3 col = new float3(0, 0, 0);
                for (int s = 0; s < DO_SAMPLES_PER_PIXEL; s++)
                {
                    float u = (x + RandomFloat01(ref state)) * invWidth;
                    float v = (y + RandomFloat01(ref state)) * invHeight;
                    Ray r = cam.GetRay(u, v, ref state);
                    col += Trace(r, 0, ref rayCount, spheres, materials, ref state);
                }
                col *= 1.0f / (float)DO_SAMPLES_PER_PIXEL;

                UnityEngine.Color prev = backbuffer[backbufferIdx];
                col = new float3(prev.r, prev.g, prev.b) * lerpFac + col * (1 - lerpFac);
                backbuffer[backbufferIdx] = new UnityEngine.Color(col.x, col.y, col.z, 1);
                backbufferIdx++;
            }
            rayCounter[0] += rayCount; //@TODO: how to do atomics add?
        }
    }


    public void DrawTest(float time, int frameCount, int screenWidth, int screenHeight, NativeArray<UnityEngine.Color> backbuffer, out int outRayCount)
    {
        int rayCount = 0;
#if DO_ANIMATE
        s_SpheresData[1].center.y = cos(time)+1.0f;
        s_SpheresData[8].center.z = sin(time)*0.3f;
#endif
        float3 lookfrom = new float3(0, 2, 3);
        float3 lookat = new float3(0, 0, 0);
        float distToFocus = 3;
        float aperture = 0.1f;

        for (int i = 0; i < s_SpheresData.Length; ++i)
            s_SpheresData[i].UpdateDerivedData();

        Camera cam = new Camera(lookfrom, lookat, new float3(0, 1, 0), 60, (float)screenWidth / (float)screenHeight, aperture, distToFocus);

#if DO_THREADED
        TraceRowJob job;
        job.screenWidth = screenWidth;
        job.screenHeight = screenHeight;
        job.frameCount = frameCount;
        job.backbuffer = backbuffer;
        job.cam = cam;
        job.rayCounter = new NativeArray<int>(1, Allocator.Temp);
        job.spheres = new NativeArray<Sphere>(s_SpheresData, Allocator.Temp);
        job.materials = new NativeArray<Material>(s_SphereMatsData, Allocator.Temp);
        var fence = job.Schedule(screenHeight, 4);
        fence.Complete();
        rayCount = job.rayCounter[0];
        job.rayCounter.Dispose();
        job.spheres.Dispose();
        job.materials.Dispose();
#else
        for (int y = 0; y < screenHeight; ++y)
            rayCount += TraceRowJob(y, screenWidth, screenHeight, frameCount, backbuffer, ref cam);
#endif
        outRayCount = rayCount;
    }
}

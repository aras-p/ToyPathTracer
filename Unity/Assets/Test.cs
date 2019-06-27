using static MathUtil;
using static Unity.Mathematics.math;
using Unity.Collections;
using Unity.Jobs;
using float3 = Unity.Mathematics.float3;
using Unity.Mathematics;

class Test
{
    TraceRowJob job;

    public Test()
    {
        job.Init();
    }

    public void Dispose()
    {
        job.Dispose();
    }
    
    const float kMinT = 0.001f;
    const float kMaxT = 1.0e7f;
    const int kMaxDepth = 10;
    
    static float3 GetAlbedo(int id)
    {
        return float3(
            (id & 1) != 0 ? 0.8f : 0.4f,
            (id & 2) != 0 ? 0.8f : 0.4f,
            (id & 4) != 0 ? 0.8f : 0.4f
        );
    }
    
    [Unity.Burst.BurstCompileAttribute]
    struct TraceRowJob : IJobParallelFor
    {
        public void Init()
        {
            spheresLength = 8;
            
            centerX = new NativeArray<float>(new float[]{0,2,0,-2,2,0,-2,0}, Allocator.Persistent);
            centerY = new NativeArray<float>(new float[]{-100.5f,0,0,0,0,0,0,0}, Allocator.Persistent);
            centerZ = new NativeArray<float>(new float[]{-1,-1,-1,-1,1,1,1,-3}, Allocator.Persistent);
            sqRadius = new NativeArray<float>(new float[]{100f*100f,0.25f,0.25f,0.25f,0.25f,0.25f,0.25f,0.25f}, Allocator.Persistent);
            invRadius = new NativeArray<float>(new float[]{1f/100f,2,2,2,2,2,2,2}, Allocator.Persistent);

            backbuffer = default;
            cam = default;
            frameCount = 0;
            screenWidth = 0;
            screenHeight = 0;
        }

        public void Dispose()
        {
            centerX.Dispose();
            centerY.Dispose();
            centerZ.Dispose();
            sqRadius.Dispose();
            invRadius.Dispose();
        }

        public int screenWidth, screenHeight, frameCount;
        public Camera cam;
        int spheresLength;

        [NativeDisableParallelForRestriction] public NativeArray<float> centerX;
        [NativeDisableParallelForRestriction] public NativeArray<float> centerY;
        [NativeDisableParallelForRestriction] public NativeArray<float> centerZ;
        [NativeDisableParallelForRestriction] public NativeArray<float> sqRadius;
        [NativeDisableParallelForRestriction] public NativeArray<float> invRadius;
        
        public NativeArray<UnityEngine.Color> backbuffer;

        public void Execute(int backbufferIdx)
        {
            float invWidth = 1.0f / screenWidth;
            float invHeight = 1.0f / screenHeight;

            uint state = (uint)(backbufferIdx * 9781 + frameCount * 6271) | 1;
            {
                int y = backbufferIdx / screenWidth;
                int x = backbufferIdx % screenWidth;
                float u = (x + RandomFloat01(ref state)) * invWidth;
                float v = (y + RandomFloat01(ref state)) * invHeight;
                Ray r = cam.GetRay(u, v, ref state);
                float3 col = Trace(r, 0, ref state);

                backbuffer[backbufferIdx] = new UnityEngine.Color(col.x, col.y, col.z, 1);
            }
        }
        
        float3 Trace(Ray r, int depth, ref uint randState)
        {
            Hit rec = default;
            int id = HitSpheres(ref r, kMinT, kMaxT, ref rec);
            if (id != -1)
            {
                if (depth < kMaxDepth)
                {
                    Scatter(GetAlbedo(id), rec, out var attenuation, out var scattered, ref randState);
                    return attenuation * Trace(scattered, depth + 1, ref randState);
                }
                else
                {
                    return float3(0,0,0);
                }
            }
            else
            {
                // sky
                float3 unitDir = r.dir;
                float t = 0.5f * (unitDir.y + 1.0f);
                return ((1.0f - t) * new float3(1.0f, 1.0f, 1.0f) + t * new float3(0.5f, 0.7f, 1.0f)) * 0.7f;
            }
        }
        
        static void Scatter(float3 albedo, Hit rec, out float3 attenuation, out Ray scattered, ref uint randState)
        {
            // random point inside unit sphere that is tangent to the hit point
            float3 target = rec.pos + rec.normal + RandomUnitVector(ref randState);
            scattered = new Ray(rec.pos, normalize(target - rec.pos));
            attenuation = albedo;
        }
        
        int HitSpheres(ref Ray r, float tMin, float tMax, ref Hit outHit)
        {
            float hitT = tMax;
            int id = -1;
            for (int i = 0; i < spheresLength; ++i)
            {
                float coX = centerX[i] - r.orig.x;
                float coY = centerY[i] - r.orig.y;
                float coZ = centerZ[i] - r.orig.z;
                float nb = coX * r.dir.x + coY * r.dir.y + coZ * r.dir.z;
                float c = coX * coX + coY * coY + coZ * coZ - sqRadius[i];
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
                outHit.pos = r.PointAt(hitT);
                outHit.normal = (outHit.pos - new float3(centerX[id], centerY[id], centerZ[id])) * invRadius[id];
                return id;        
            }
            return -1;
        }        
    }


    public void DrawTest(float time, int frameCount, int screenWidth, int screenHeight, NativeArray<UnityEngine.Color> backbuffer)
    {
        float3 lookfrom = new float3(0, 2, 3);
        float3 lookat = new float3(0, 0, 0);
        float distToFocus = 3;
        float aperture = 0.1f;

        Camera cam = new Camera(lookfrom, lookat, new float3(0, 1, 0), 60, (float)screenWidth / (float)screenHeight, aperture, distToFocus);

        job.screenWidth = screenWidth;
        job.screenHeight = screenHeight;
        job.frameCount = frameCount;
        job.backbuffer = backbuffer;
        job.cam = cam;
        var fence = job.Schedule(screenWidth * screenHeight, 64);
        fence.Complete();
    }
}




public class MathUtil
{
    public static float kPI => 3.1415926f;

    static uint XorShift32(ref uint state)
    {
        uint x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 15;
        state = x;
        return x;
    }

    public static float RandomFloat01(ref uint state)
    {
        return (XorShift32(ref state) & 0xFFFFFF) / 16777216.0f;
    }

    public static float3 RandomInUnitDisk(ref uint state)
    {
        float a = RandomFloat01(ref state) * 2.0f * 3.1415926f;
        float2 xy = float2(cos(a), sin(a));
        xy *= sqrt(RandomFloat01(ref state));
        return float3(xy,0);
    }

    public static float3 RandomUnitVector(ref uint state)
    {
        float z = RandomFloat01(ref state) * 2.0f - 1.0f;
        float a = RandomFloat01(ref state) * 2.0f * kPI;
        float r = sqrt(1.0f - z * z);
        float x, y;
        sincos(a, out x, out y);
        return new float3(r * x, r* y, z);
    }

}

public struct Ray
{
    public float3 orig;
    public float3 dir;

    public Ray(float3 orig_, float3 dir_)
    {
        orig = orig_;
        dir = dir_;        
    }

    public float3 PointAt(float t) { return orig + dir * t; }
}

public struct Hit
{
    public float3 pos;
    public float3 normal;
}

struct Camera
{
    public Camera(float3 lookFrom, float3 lookAt, float3 vup, float vfov, float aspect, float aperture, float focusDist)
    {
        lensRadius = aperture / 2;
        float theta = vfov * MathUtil.kPI / 180;
        float halfHeight = tan(theta / 2);
        float halfWidth = aspect * halfHeight;
        origin = lookFrom;
        w = normalize(lookFrom - lookAt);
        u = normalize(cross(vup, w));
        v = cross(w, u);
        lowerLeftCorner = origin - halfWidth*focusDist*u - halfHeight*focusDist*v - focusDist*w;
        horizontal = 2*halfWidth * focusDist*u;
        vertical = 2*halfHeight * focusDist*v;
    }

    public Ray GetRay(float s, float t, ref uint state)
    {
        float3 rd = lensRadius * MathUtil.RandomInUnitDisk(ref state);
        float3 offset = u * rd.x + v * rd.y;
        return new Ray(origin + offset, normalize(lowerLeftCorner + s*horizontal + t*vertical - origin - offset));
    }
    
    float3 origin;
    float3 lowerLeftCorner;
    float3 horizontal;
    float3 vertical;
    float3 u, v, w;
    float lensRadius;
}

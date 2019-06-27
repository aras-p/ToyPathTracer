using Unity.Collections;
using Unity.Jobs;
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

        public int screenWidth, screenHeight;
        public Camera cam;
        int spheresLength;

        [NativeDisableParallelForRestriction] public NativeArray<float> centerX;
        [NativeDisableParallelForRestriction] public NativeArray<float> centerY;
        [NativeDisableParallelForRestriction] public NativeArray<float> centerZ;
        [NativeDisableParallelForRestriction] public NativeArray<float> sqRadius;
        [NativeDisableParallelForRestriction] public NativeArray<float> invRadius;
        
        public NativeArray<uint> backbuffer;

        public void Execute(int backbufferIdx)
        {
            float invWidth = 1.0f / screenWidth;
            float invHeight = 1.0f / screenHeight;

            int y = backbufferIdx / screenWidth;
            int x = backbufferIdx % screenWidth;
            float u = x * invWidth;
            float v = y * invHeight;
            Ray r = cam.GetRay(u, v);
            int id = HitSpheres(ref r, 0.001f, 1000.0f);
            float colR, colG, colB;
            if (id != -1)
            {
                colR = (id & 1) != 0 ? 0.8f * 255.0f : 0.4f * 255.0f;
                colG = (id & 2) != 0 ? 0.8f * 255.0f : 0.4f * 255.0f;
                colB = (id & 4) != 0 ? 0.8f * 255.0f : 0.4f * 255.0f;
            }
            else
            {
                colR = 128;
                colG = 178;
                colB = 255;
            }
            uint colb =
                ((uint)colR) |
                ((uint)colG << 8) |
                ((uint)colB << 16) |
                0xFF000000;
            backbuffer[backbufferIdx] = colb;
        }

        int HitSpheres(ref Ray r, float tMin, float tMax)
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
                    float discrSq = math.sqrt(discr);
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
            return id;
        }        
    }

    public void DrawTest(int screenWidth, int screenHeight, NativeArray<uint> backbuffer)
    {
        float distToFocus = 3;
        Camera cam = new Camera(new float3(0, 2, 3), new float3(0, 0, 0), new float3(0, 1, 0), 60, (float)screenWidth / (float)screenHeight, distToFocus);

        job.screenWidth = screenWidth;
        job.screenHeight = screenHeight;
        job.backbuffer = backbuffer;
        job.cam = cam;
        var fence = job.Schedule(screenWidth * screenHeight, 64);
        fence.Complete();
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
}

struct Camera
{
    public Camera(float3 lookFrom, float3 lookAt, float3 vup, float vfov, float aspect, float focusDist)
    {
        float theta = vfov * 3.1415926f / 180;
        float halfHeight = math.tan(theta / 2);
        float halfWidth = aspect * halfHeight;
        origin = lookFrom;
        float3 w = math.normalize(lookFrom - lookAt);
        float3 u = math.normalize(math.cross(vup, w));
        float3 v = math.cross(w, u);
        lowerLeftCorner = origin - halfWidth * focusDist * u - halfHeight * focusDist * v - focusDist * w;
        horizontal = 2 * halfWidth * focusDist * u;
        vertical = 2 * halfHeight * focusDist * v;
    }

    public Ray GetRay(float s, float t)
    {
        return new Ray(origin, math.normalize(lowerLeftCorner + s * horizontal + t * vertical - origin));
    }

    float3 origin;
    float3 lowerLeftCorner;
    float3 horizontal;
    float3 vertical;
}

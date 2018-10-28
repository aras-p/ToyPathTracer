using Unity.Collections;
using Unity.Mathematics;
using static Unity.Mathematics.math;


public class MathUtil
{

    public static bool Refract(float3 v, float3 n, float nint, out float3 outRefracted)
    {
        float dt = dot(v, n);
        float discr = 1.0f - nint * nint * (1 - dt * dt);
        if (discr > 0)
        {
            outRefracted = nint * (v - n * dt) - n * sqrt(discr);
            return true;
        }
        outRefracted = new float3(0, 0, 0);
        return false;
    }

    public static float kPI => 3.1415926f;

    public static float Schlick(float cosine, float ri)
    {
        float r0 = (1 - ri) / (1 + ri);
        r0 = r0 * r0;
        return r0 + (1 - r0) * pow(1 - cosine, 5);
    }

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
        float3 p;
        do
        {
            p = 2.0f * new float3(RandomFloat01(ref state), RandomFloat01(ref state), 0) - new float3(1, 1, 0);
        } while (lengthsq(p) >= 1.0);
        return p;
    }

    public static float3 RandomInUnitSphere(ref uint state)
    {
        float3 p;
        do
        {
            p = 2.0f * new float3(RandomFloat01(ref state), RandomFloat01(ref state), RandomFloat01(ref state)) - new float3(1, 1, 1);
        } while (lengthsq(p) >= 1.0);
        return p;
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
    public float t;
}

public struct Sphere
{
    public float3 center;
    public float radius;

    public Sphere(float3 center_, float radius_) { center = center_; radius = radius_; }
}

struct SpheresSoA
{
    [ReadOnly] public NativeArray<float> centerX;
    [ReadOnly] public NativeArray<float> centerY;
    [ReadOnly] public NativeArray<float> centerZ;
    [ReadOnly] public NativeArray<float> sqRadius;
    [ReadOnly] public NativeArray<float> invRadius;
    [ReadOnly] public NativeArray<int> emissives;
    public int emissiveCount;

    public SpheresSoA(int len)
    {
        centerX = new NativeArray<float>(len, Allocator.Persistent);
        centerY = new NativeArray<float>(len, Allocator.Persistent);
        centerZ = new NativeArray<float>(len, Allocator.Persistent);
        sqRadius = new NativeArray<float>(len, Allocator.Persistent);
        invRadius = new NativeArray<float>(len, Allocator.Persistent);
        emissives = new NativeArray<int>(len, Allocator.Persistent);
        emissiveCount = 0;
    }

    public void Dispose()
    {
        centerX.Dispose();
        centerY.Dispose();
        centerZ.Dispose();
        sqRadius.Dispose();
        invRadius.Dispose();
        emissives.Dispose();
    }

    public void Update(Sphere[] src, Material[] mat)
    {
        emissiveCount = 0;
        for (var i = 0; i < src.Length; ++i)
        {
            Sphere s = src[i];
            centerX[i] = s.center.x;
            centerY[i] = s.center.y;
            centerZ[i] = s.center.z;
            sqRadius[i] = s.radius * s.radius;
            invRadius[i] = 1.0f / s.radius;
            if (mat[i].HasEmission)
            {
                emissives[emissiveCount++] = i;
            }
        }
    }

    public int HitSpheres(ref Ray r, float tMin, float tMax, ref Hit outHit)
    {
        float hitT = tMax;
        int id = -1;
        for (int i = 0; i < centerX.Length; ++i)
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
            outHit.t = hitT;
            return id;
        }
        else
            return -1;
    }
}

struct Camera
{
    // vfov is top to bottom in degrees
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

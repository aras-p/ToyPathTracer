using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
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
        var simdLen = (len + 3) / 4 * 4;
        centerX = new NativeArray<float>(simdLen, Allocator.Persistent);
        centerY = new NativeArray<float>(simdLen, Allocator.Persistent);
        centerZ = new NativeArray<float>(simdLen, Allocator.Persistent);
        sqRadius = new NativeArray<float>(simdLen, Allocator.Persistent);
        invRadius = new NativeArray<float>(simdLen, Allocator.Persistent);
        // set trailing data to "impossible sphere" state
        for (int i = len; i < simdLen; ++i)
        {
            centerX[i] = centerY[i] = centerZ[i] = 10000.0f;
            sqRadius[i] = 0.0f;
            invRadius[i] = 0.0f;
        }
        emissives = new NativeArray<int>(simdLen, Allocator.Persistent);
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

    public unsafe int HitSpheres(ref Ray r, float tMin, float tMax, ref Hit outHit)
    {
        float4 hitT = tMax;
        int4 id = -1;
        float4 rOrigX = r.orig.x;
        float4 rOrigY = r.orig.y;
        float4 rOrigZ = r.orig.z;
        float4 rDirX = r.dir.x;
        float4 rDirY = r.dir.y;
        float4 rDirZ = r.dir.z;
        float4 tMin4 = tMin;
        int4 curId = new int4(0,1,2,3);
        int simdLen = centerX.Length/4;
        float4* ptrCenterX = (float4*)centerX.GetUnsafeReadOnlyPtr();
        float4* ptrCenterY = (float4*)centerY.GetUnsafeReadOnlyPtr();
        float4* ptrCenterZ = (float4*)centerZ.GetUnsafeReadOnlyPtr();
        float4* ptrSqRadius = (float4*)sqRadius.GetUnsafeReadOnlyPtr();
        for (int i = 0; i < simdLen; ++i)
        {
            float4 sCenterX = *ptrCenterX;
            float4 sCenterY = *ptrCenterY;
            float4 sCenterZ = *ptrCenterZ;
            float4 sSqRadius = *ptrSqRadius;
            float4 coX = sCenterX - rOrigX;
            float4 coY = sCenterY - rOrigY;
            float4 coZ = sCenterZ - rOrigZ;
            float4 nb = coX * rDirX + coY * rDirY + coZ * rDirZ;
            float4 c = coX * coX + coY * coY + coZ * coZ - sSqRadius;
            float4 discr = nb * nb - c;
            bool4 discrPos = discr > float4(0.0f);
            // if ray hits any of the 4 spheres
            if (any(discrPos))
            {
                float4 discrSq = sqrt(discr);

                // ray could hit spheres at t0 & t1
                float4 t0 = nb - discrSq;
                float4 t1 = nb + discrSq;

                float4 t = select(t1, t0, t0 > tMin4); // if t0 is above min, take it (since it's the earlier hit); else try t1.
                bool4 msk = discrPos & (t > tMin4) & (t < hitT);
                // if hit, take it
                id = select(id, curId, msk);
                hitT = select(hitT, t, msk);
            }
            curId += int4(4);
            ptrCenterX++;
            ptrCenterY++;
            ptrCenterZ++;
            ptrSqRadius++;
        }
        // now we have up to 4 hits, find and return closest one
        float2 minT2 = min(hitT.xy, hitT.zw);
        float minT = min(minT2.x, minT2.y);
        if (minT < tMax) // any actual hits?
        {
            // get bitmask of which lanes are closest (rarely, but it can happen that more than one at once is closest)
            int laneMask = csum(int4(hitT == float4(minT)) * int4(1,2,4,8));
            // get index of first closest lane
            int lane = tzcnt(laneMask); //if (lane < 0 || lane > 3) Debug.LogError($"invalid lane {lane}");
            int hitId = id[lane]; //if (hitId < 0 || hitId >= centerX.Length) Debug.LogError($"invalid hitID {hitId}");
            float finalHitT = hitT[lane];
            outHit.pos = r.PointAt(finalHitT);
            outHit.normal = (outHit.pos - float3(centerX[hitId], centerY[hitId], centerZ[hitId])) * invRadius[hitId];
            outHit.t = finalHitT;
            return hitId;
        }
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

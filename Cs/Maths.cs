using System;
using System.Diagnostics;

#if DESKTOP
public static class MathF
{
    public static float Sqrt(float v)
    {
        return (float)Math.Sqrt(v);
    }

    public static float Abs(float v)
    {
        return (float)Math.Abs(v);
    }

    public static float Pow(float a, float b)
    {
        return (float)Math.Pow(a, b);
    }
    public static float Sin(float v)
    {
        return (float)Math.Sin(v);
    }
    public static float Cos(float v)
    {
        return (float)Math.Cos(v);
    }
    public static float Tan(float v)
    {
        return (float)Math.Tan(v);
    }
    public static float Max(float a, float b)
    {
        return (float) Math.Max(a, b);
    }
}
#endif

public struct float3
{
    public float x, y, z;

    public float3(float x_, float y_, float z_) { x = x_; y = y_; z = z_; }

    public float SqLength => x * x + y * y + z * z;
    public float Length => MathF.Sqrt(x * x + y * y + z * z);
    public void Normalize() { float k = 1.0f / Length; x *= k; y *= k; z *= k; }

    public static float3 operator +(float3 a, float3 b) { return new float3(a.x + b.x, a.y + b.y, a.z + b.z); }
    public static float3 operator -(float3 a, float3 b) { return new float3(a.x - b.x, a.y - b.y, a.z - b.z); }
    public static float3 operator *(float3 a, float3 b) { return new float3(a.x * b.x, a.y * b.y, a.z * b.z); }
    public static float3 operator *(float3 a, float b) { return new float3(a.x * b, a.y * b, a.z * b); }
    public static float3 operator *(float a, float3 b) { return new float3(a * b.x, a * b.y, a * b.z); }
    public static float3 operator -(float3 a) { return new float3(-a.x, -a.y, -a.z); }

    public static float Dot(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    public static float3 Cross(float3 a, float3 b) { return new float3(a.y * b.z - a.z * b.y, -(a.x * b.z - a.z * b.x), a.x * b.y - a.y * b.x); }
    public static float3 Normalize(float3 v) { float k = 1.0f / v.Length; return new float3(v.x * k, v.y * k, v.z * k); }

    public bool IsNormalized => MathF.Abs(SqLength - 1.0f) < 0.01f;

    public static float3 Reflect(float3 v, float3 n)
    {
        Debug.Assert(v.IsNormalized);
        return v - 2 * Dot(v, n) * n;
    }

    public static bool Refract(float3 v, float3 n, float nint, out float3 outRefracted)
    {
        Debug.Assert(v.IsNormalized);
        float dt = Dot(v, n);
        float discr = 1.0f - nint * nint * (1 - dt * dt);
        if (discr > 0)
        {
            outRefracted = nint * (v - n* dt) - n * MathF.Sqrt(discr);
            Debug.Assert(outRefracted.IsNormalized);
            return true;
        }
        outRefracted = new float3(0, 0, 0);
        return false;
    }
}

public class MathUtil
{
    public static float PI => 3.1415926f;

    public static float Schlick(float cosine, float ri)
    {
        float r0 = (1 - ri) / (1 + ri);
        r0 = r0 * r0;
        return r0 + (1 - r0) * MathF.Pow(1 - cosine, 5);
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
        } while (p.SqLength >= 1.0);
        return p;
    }

    public static float3 RandomInUnitSphere(ref uint state)
    {
        float3 p;
        do
        {
            p = 2.0f * new float3(RandomFloat01(ref state), RandomFloat01(ref state), RandomFloat01(ref state)) - new float3(1, 1, 1);
        } while (p.SqLength >= 1.0);
        return p;
    }

    public static float3 RandomUnitVector(ref uint state)
    {
        float z = RandomFloat01(ref state) * 2.0f - 1.0f;
        float a = RandomFloat01(ref state) * 2.0f * PI;
        float r = MathF.Sqrt(1.0f - z * z);
        float x = MathF.Sin(a);
        float y = MathF.Cos(a);
        return new float3(r * x, r * y, z);
    }

}

public struct Ray
{
    public float3 orig;
    public float3 dir;

    public Ray(float3 orig_, float3 dir_)
    {
        Debug.Assert(dir_.IsNormalized);
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
    public float invRadius;

    public Sphere(float3 center_, float radius_) { center = center_; radius = radius_; invRadius = 1.0f / radius_; }
    public void UpdateDerivedData() { invRadius = 1.0f / radius; }

    public bool HitSphere(Ray r, float tMin, float tMax, ref Hit outHit)
    {
        Debug.Assert(invRadius == 1.0f / radius);
        Debug.Assert(r.dir.IsNormalized);
        float3 oc = r.orig - center;
        float b = float3.Dot(oc, r.dir);
        float c = float3.Dot(oc, oc) - radius * radius;
        float discr = b * b - c;
        if (discr > 0)
        {
            float discrSq = MathF.Sqrt(discr);

            float t = (-b - discrSq);
            if (t < tMax && t > tMin)
            {
                outHit.pos = r.PointAt(t);
                outHit.normal = (outHit.pos - center) * invRadius;
                Debug.Assert(outHit.normal.IsNormalized);
                outHit.t = t;
                return true;
            }
            t = (-b + discrSq);
            if (t < tMax && t > tMin)
            {
                outHit.pos = r.PointAt(t);
                outHit.normal = (outHit.pos - center) * invRadius;
                Debug.Assert(outHit.normal.IsNormalized);
                outHit.t = t;
                return true;
            }
        }
        return false;
    }
}

struct Camera
{
    // vfov is top to bottom in degrees
    public Camera(float3 lookFrom, float3 lookAt, float3 vup, float vfov, float aspect, float aperture, float focusDist)
    {
        lensRadius = aperture / 2;
        float theta = vfov * MathUtil.PI / 180;
        float halfHeight = MathF.Tan(theta / 2);
        float halfWidth = aspect * halfHeight;
        origin = lookFrom;
        w = float3.Normalize(lookFrom - lookAt);
        u = float3.Normalize(float3.Cross(vup, w));
        v = float3.Cross(w, u);
        lowerLeftCorner = origin - halfWidth*focusDist*u - halfHeight*focusDist*v - focusDist*w;
        horizontal = 2*halfWidth * focusDist*u;
        vertical = 2*halfHeight * focusDist*v;
    }

    public Ray GetRay(float s, float t, ref uint state)
    {
        float3 rd = lensRadius * MathUtil.RandomInUnitDisk(ref state);
        float3 offset = u * rd.x + v * rd.y;
        return new Ray(origin + offset, float3.Normalize(lowerLeftCorner + s*horizontal + t*vertical - origin - offset));
    }
    
    float3 origin;
    float3 lowerLeftCorner;
    float3 horizontal;
    float3 vertical;
    float3 u, v, w;
    float lensRadius;
}

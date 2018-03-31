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

    public static float PI => 3.1415926f;

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
        } while (lengthSquared(p) >= 1.0);
        return p;
    }

    public static float3 RandomInUnitSphere(ref uint state)
    {
        float3 p;
        do
        {
            p = 2.0f * new float3(RandomFloat01(ref state), RandomFloat01(ref state), RandomFloat01(ref state)) - new float3(1, 1, 1);
        } while (lengthSquared(p) >= 1.0);
        return p;
    }

    public static float3 RandomUnitVector(ref uint state)
    {
        float z = RandomFloat01(ref state) * 2.0f - 1.0f;
        float a = RandomFloat01(ref state) * 2.0f * PI;
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
    public float invRadius;

    public Sphere(float3 center_, float radius_) { center = center_; radius = radius_; invRadius = 1.0f / radius_; }
    public void UpdateDerivedData() { invRadius = 1.0f / radius; }

    public bool HitSphere(Ray r, float tMin, float tMax, ref Hit outHit)
    {
        float3 oc = r.orig - center;
        float b = dot(oc, r.dir);
        float c = dot(oc, oc) - radius * radius;
        float discr = b * b - c;
        if (discr > 0)
        {
            float discrSq = sqrt(discr);

            float t = (-b - discrSq);
            if (t < tMax && t > tMin)
            {
                outHit.pos = r.PointAt(t);
                outHit.normal = (outHit.pos - center) * invRadius;
                outHit.t = t;
                return true;
            }
            t = (-b + discrSq);
            if (t < tMax && t > tMin)
            {
                outHit.pos = r.PointAt(t);
                outHit.normal = (outHit.pos - center) * invRadius;
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

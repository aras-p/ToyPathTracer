#pragma once

#include <math.h>

struct float3
{
    float3() : x(0), y(0), z(0) {}
    float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    float sqLength() const { return x*x+y*y+z*z; }
    float length() const { return sqrtf(x*x+y*y+z*z); }
    void normalize() { float k = 1.0f / length(); x *= k; y *= k; z *= k; }
    
    float3 operator-() const { return float3(-x, -y, -z); }
    float3& operator+=(const float3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    float3& operator-=(const float3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
    float3& operator*=(const float3& o) { x*=o.x; y*=o.y; z*=o.z; return *this; }
    float3& operator*=(float o) { x*=o; y*=o; z*=o; return *this; }

    float x, y, z;
};

inline float3 operator+(const float3& a, const float3& b) { return float3(a.x+b.x,a.y+b.y,a.z+b.z); }
inline float3 operator-(const float3& a, const float3& b) { return float3(a.x-b.x,a.y-b.y,a.z-b.z); }
inline float3 operator*(const float3& a, const float3& b) { return float3(a.x*b.x,a.y*b.y,a.z*b.z); }
inline float3 operator*(const float3& a, float b) { return float3(a.x*b,a.y*b,a.z*b); }
inline float3 operator*(float a, const float3& b) { return float3(a*b.x,a*b.y,a*b.z); }
inline float dot(const float3& a, const float3& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float3 cross(const float3& a, const float3& b)
{
    return float3(
                  a.y*b.z - a.z*b.y,
                  -(a.x*b.z - a.z*b.x),
                  a.x*b.y - a.y*b.x
                  );
}
inline float3 normalize(const float3& v) { float k = 1.0f / v.length(); return float3(v.x*k, v.y*k, v.z*k); }
inline float3 reflect(const float3& v, const float3& n)
{
    return v - 2*dot(v,n)*n;
}
inline bool refract(const float3& v, const float3& n, float nint, float3& outRefracted)
{
    float3 uv = normalize(v);
    float dt = dot(uv, n);
    float discr = 1.0f - nint*nint*(1-dt*dt);
    if (discr > 0)
    {
        outRefracted = nint * (uv - n*dt) - n*sqrt(discr);
        return true;
    }
    return false;
}
inline float schlick(float cosine, float ri)
{
    float r0 = (1-ri) / (1+ri);
    r0 = r0*r0;
    return r0 + (1-r0)*powf(1-cosine, 5);
}

struct Ray
{
    Ray() {}
    Ray(const float3& orig_, const float3& dir_) : orig(orig_), dir(dir_) {}

    float3 pointAt(float t) const { return orig + dir * t; }
    
    float3 orig;
    float3 dir;
};


struct Hit
{
    float3 pos;
    float3 normal;
    float t;
};


struct Sphere
{
    Sphere() : radius(1.0f) {}
    Sphere(float3 center_, float radius_) : center(center_), radius(radius_) {}
    
    float3 center;
    float radius;
};


bool HitSphere(const Ray& r, const Sphere& s, float tMin, float tMax, Hit& outHit);

float RandomFloat01();
float3 RandomInUnitDisk();
float3 RandomInUnitSphere();


struct Camera
{
    // vfov is top to bottom in degrees
    Camera(const float3& lookFrom, const float3& lookAt, const float3& vup, float vfov, float aspect, float aperture, float focusDist)
    {
        lensRadius = aperture / 2;
        float theta = vfov*M_PI/180;
        float halfHeight = tan(theta/2);
        float halfWidth = aspect * halfHeight;
        origin = lookFrom;
        w = normalize(lookFrom - lookAt);
        u = normalize(cross(vup, w));
        v = cross(w, u);
        lowerLeftCorner = origin - halfWidth*focusDist*u - halfHeight*focusDist*v - focusDist*w;
        horizontal = 2*halfWidth*focusDist*u;
        vertical = 2*halfHeight*focusDist*v;
    }
    
    Ray GetRay(float s, float t) const
    {
        float3 rd = lensRadius * RandomInUnitDisk();
        float3 offset = u * rd.x + v * rd.y;
        return Ray(origin + offset, lowerLeftCorner + s*horizontal + t*vertical - origin - offset);
    }
    
    float3 origin;
    float3 lowerLeftCorner;
    float3 horizontal;
    float3 vertical;
    float3 u, v, w;
    float lensRadius;
};


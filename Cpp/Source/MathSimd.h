#pragma once

#if defined(_MSC_VER)
#define VM_INLINE __forceinline
#else
#define VM_INLINE __attribute__((unused, always_inline, nodebug)) inline
#endif

#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#define kSimdWidth 4

#define SHUFFLE4(V, X,Y,Z,W) float4(_mm_shuffle_ps((V).m, (V).m, _MM_SHUFFLE(W,Z,Y,X)))

struct float4
{
    VM_INLINE float4() {}
    VM_INLINE explicit float4(const float *p) { m = _mm_loadu_ps(p); }
    VM_INLINE explicit float4(float x, float y, float z, float w) { m = _mm_set_ps(w, z, y, x); }
    VM_INLINE explicit float4(float v) { m = _mm_set_ps1(v); }
    VM_INLINE explicit float4(__m128 v) { m = v; }
    
    VM_INLINE float getX() const { return _mm_cvtss_f32(m); }
    VM_INLINE float getY() const { return _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(1, 1, 1, 1))); }
    VM_INLINE float getZ() const { return _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(2, 2, 2, 2))); }
    VM_INLINE float getW() const { return _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(3, 3, 3, 3))); }
    
    __m128 m;
};

typedef float4 bool4;

VM_INLINE float4 operator+ (float4 a, float4 b) { a.m = _mm_add_ps(a.m, b.m); return a; }
VM_INLINE float4 operator- (float4 a, float4 b) { a.m = _mm_sub_ps(a.m, b.m); return a; }
VM_INLINE float4 operator* (float4 a, float4 b) { a.m = _mm_mul_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator==(float4 a, float4 b) { a.m = _mm_cmpeq_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator!=(float4 a, float4 b) { a.m = _mm_cmpneq_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator< (float4 a, float4 b) { a.m = _mm_cmplt_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator> (float4 a, float4 b) { a.m = _mm_cmpgt_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator<=(float4 a, float4 b) { a.m = _mm_cmple_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator>=(float4 a, float4 b) { a.m = _mm_cmpge_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator&(bool4 a, bool4 b) { a.m = _mm_and_ps(a.m, b.m); return a; }
VM_INLINE bool4 operator|(bool4 a, bool4 b) { a.m = _mm_or_ps(a.m, b.m); return a; }
VM_INLINE float4 operator- (float4 a) { a.m = _mm_xor_ps(a.m, _mm_set1_ps(-0.0f)); return a; }
VM_INLINE float4 min(float4 a, float4 b) { a.m = _mm_min_ps(a.m, b.m); return a; }
VM_INLINE float4 max(float4 a, float4 b) { a.m = _mm_max_ps(a.m, b.m); return a; }

VM_INLINE float hmin(float4 v)
{
    v = min(v, SHUFFLE4(v, 2, 3, 0, 0));
    v = min(v, SHUFFLE4(v, 1, 0, 0, 0));
    return v.getX();
}

// Returns a 4-bit code where bit0..bit3 is X..W
VM_INLINE unsigned mask(float4 v) { return _mm_movemask_ps(v.m) & 15; }
// Once we have a comparison, we can branch based on its results:
VM_INLINE bool any(bool4 v) { return mask(v) != 0; }
VM_INLINE bool all(bool4 v) { return mask(v) == 15; }

// "select", i.e. hibit(cond) ? b : a
// on SSE4.1 and up this can be done easily via "blend" instruction;
// on older SSEs has to do a bunch of hoops, see
// https://fgiesen.wordpress.com/2016/04/03/sse-mind-the-gap/

VM_INLINE float4 select(float4 a, float4 b, bool4 cond)
{
#if defined(__SSE4_1__) || defined(_MSC_VER) // on windows assume we always have SSE4.1
    a.m = _mm_blendv_ps(a.m, b.m, cond.m);
#else
    __m128 d = _mm_castsi128_ps(_mm_srai_epi32(_mm_castps_si128(cond.m), 31));
    a.m = _mm_or_ps(_mm_and_ps(d, b.m), _mm_andnot_ps(d, a.m));
#endif
    return a;
}
VM_INLINE __m128i select(__m128i a, __m128i b, bool4 cond)
{
#if defined(__SSE4_1__) || defined(_MSC_VER) // on windows assume we always have SSE4.1
    return _mm_blendv_epi8(a, b, _mm_castps_si128(cond.m));
#else
    __m128i d = _mm_srai_epi32(_mm_castps_si128(cond.m), 31);
    return _mm_or_si128(_mm_and_si128(d, b), _mm_andnot_si128(d, a));
#endif
}

VM_INLINE float4 sqrtf(float4 v) { return float4(_mm_sqrt_ps(v.m)); }

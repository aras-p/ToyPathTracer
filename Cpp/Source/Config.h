
#if defined(__APPLE__) && !defined(__METAL_VERSION__)
#include <TargetConditionals.h>
#endif

#define kBackbufferWidth 1280
#define kBackbufferHeight 720


#define DO_SAMPLES_PER_PIXEL 4
#define DO_ANIMATE_SMOOTHING 0.9f
#define DO_LIGHT_SAMPLING 1
#define DO_MITSUBA_COMPARE 0

// Should path tracing be done on the GPU with a compute shader?
#define DO_COMPUTE_GPU 0
#define kCSGroupSizeX 8
#define kCSGroupSizeY 8
#define kCSMaxObjects 64

// Should float3 struct use SSE/NEON?
#define DO_FLOAT3_WITH_SIMD (!(DO_COMPUTE_GPU) && 1)

// Should HitSpheres function use SSE/NEON?
#define DO_HIT_SPHERES_SIMD 1

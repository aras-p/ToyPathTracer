
#define kBackbufferWidth 1280
#define kBackbufferHeight 720


#define DO_SAMPLES_PER_PIXEL 4
#define DO_ANIMATE 0
#define DO_ANIMATE_SMOOTHING 0.9f
#define DO_LIGHT_SAMPLING 1
#define DO_PROGRESSIVE 1
#define DO_MITSUBA_COMPARE 0


#define kMaxDepth 10

// Should path tracing be done on the GPU with a compute shader?
#define DO_COMPUTE_GPU 1
#define kCSGroupSizeX 16
#define kCSGroupSizeY 16
#define kCSRayBatchSize 64

// Should float3 struct use SSE?
#define DO_FLOAT3_WITH_SSE (!(DO_COMPUTE_GPU) && 1)

// Should HitSpheres function use SSE?
#define DO_HIT_SPHERES_SSE 1

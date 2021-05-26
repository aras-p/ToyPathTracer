#include <emscripten.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <algorithm>
#include <math.h>
#include "../Source/Test.h"

EMSCRIPTEN_KEEPALIVE
extern "C" void init()
{
    InitializeTest();
}

EMSCRIPTEN_KEEPALIVE
extern "C" void done()
{
    ShutdownTest();
}

EMSCRIPTEN_KEEPALIVE
extern "C" uint8_t* create_buffer(int width, int height)
{
    return (uint8_t*)malloc(width * height * 4);
}

EMSCRIPTEN_KEEPALIVE
extern "C" void destroy_buffer(uint8_t* p)
{
    free(p);
}

static float* backbuffer;
static int frameCount;
static int rayCount;
static unsigned flags = kFlagProgressive;

EMSCRIPTEN_KEEPALIVE
extern "C" int getRayCount()
{
    return rayCount;
}

EMSCRIPTEN_KEEPALIVE
extern "C" void setFlagAnimate(int value)
{
    flags = (flags & ~kFlagAnimate) | (value ? kFlagAnimate : 0);
}

EMSCRIPTEN_KEEPALIVE
extern "C" void setFlagProgressive(int value)
{
    flags = (flags & ~kFlagProgressive) | (value ? kFlagProgressive : 0);
    frameCount = 0;
}

EMSCRIPTEN_KEEPALIVE
extern "C" void render(uint8_t* screen, int width, int height, double time)
{
    if (!backbuffer)
    {
        backbuffer = new float[width * height * 4];
        memset(backbuffer, 0, width*height*4*4);
    }
    float timeS = (float)(time / 1000);

    // slow down animation time compared to C++/GPU versions, because single threaded
    // on the web is much slower
    timeS *= 0.2f;

    UpdateTest(timeS, frameCount, width, height, flags);
    DrawTest(timeS, frameCount, width, height, backbuffer, rayCount, flags);
    ++frameCount;

    // We get a floating point, linear color space buffer result.
    // Convert into 8bit/channel RGBA, and do a cheap sRGB approximation via sqrt.
    // Note that C++ versions don't do this since they feed the linear FP buffer
    // into the texture directly.
    for (int y = 0; y < height; ++y)
    {
        const float* bb = backbuffer + (height-y-1) * width * 4;
        for (int x = 0; x < width; ++x)
        {
            screen[0] = std::min(sqrtf(bb[0]) * 255, 255.0f);
            screen[1] = std::min(sqrtf(bb[1]) * 255, 255.0f);
            screen[2] = std::min(sqrtf(bb[2]) * 255, 255.0f);
            screen[3] = 255;
            screen += 4;
            bb += 4;
        }
    }
}

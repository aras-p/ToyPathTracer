#import <simd/simd.h>
#include <mach/mach_time.h>

#import "Renderer.h"
#include "../Source/Config.h"
#include "../Source/Maths.h"
#include "../Source/Test.h"

static const NSUInteger kMaxBuffersInFlight = 3;

#if TARGET_OS_IPHONE
#define kMetalBufferMode MTLResourceStorageModeShared
#else
#define kMetalBufferMode MTLResourceStorageModeManaged
#endif

// Metal on Mac needs buffer offsets to be 256-byte aligned
static int AlignedSize(int sz)
{
    return (sz + 0xFF) & ~0xFF;
}

struct ComputeParams
{
    Camera cam;
    int sphereCount;
    int screenWidth;
    int screenHeight;
    int frames;
    float invWidth;
    float invHeight;
    float lerpFac;
    int emissiveCount;
};


@implementation Renderer
{
    dispatch_semaphore_t _inFlightSemaphore;
    id <MTLDevice> _device;
    id <MTLCommandQueue> _commandQueue;

    id <MTLRenderPipelineState> _pipelineState;
    id <MTLDepthStencilState> _depthState;
    
    // GPU tracing things:
    id <MTLComputePipelineState> _computeState;
    // all the data in separate buffers
    id <MTLBuffer> _computeSpheres;
    id <MTLBuffer> _computeMaterials;
    id <MTLBuffer> _computeParams;
    id <MTLBuffer> _computeEmissives;
    id <MTLBuffer> _computeCounter;
    int _sphereCount;
    int _objSize;
    int _matSize;
    int _uniformBufferIndex;
    
    id <MTLTexture> _backbuffer, _backbuffer2;
    int _backbufferIndex;
    float* _backbufferPixels;

    mach_timebase_info_data_t _clock_timebase;
#if TARGET_OS_IPHONE
    UILabel* _label;
#else
    NSTextField* _label;
#endif
}

#if TARGET_OS_IPHONE
-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view withLabel:(nonnull UILabel*) label;
#else
-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view withLabel:(nonnull NSTextField*) label;
#endif
{
    self = [super init];
    if(self)
    {
        _label = label;
        _device = view.device;
        printf("GPU: %s\n", [[_device name] UTF8String]);
        _inFlightSemaphore = dispatch_semaphore_create(kMaxBuffersInFlight);
        mach_timebase_info(&_clock_timebase);
        [self _loadMetalWithView:view];
        [self _loadAssets];
    }

    return self;
}

- (void)_loadMetalWithView:(nonnull MTKView *)view;
{
    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    view.sampleCount = 1;

    NSError *error = NULL;
    id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];
    id <MTLFunction> vertexFunction = [defaultLibrary newFunctionWithName:@"vertexShader"];
    id <MTLFunction> fragmentFunction = [defaultLibrary newFunctionWithName:@"fragmentShader"];

    // GPU tracing things:
    id <MTLFunction> computeFunction = [defaultLibrary newFunctionWithName:@"TraceGPU"];
    _computeState = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    if (!_computeState)
        NSLog(@"Failed to created compute pipeline state, error %@", error);
    int camSize;
    GetObjectCount(_sphereCount, _objSize, _matSize, camSize);
    assert(_objSize == 20);
    assert(_matSize == 36);
    assert(camSize == 88);
    _computeSpheres = [_device newBufferWithLength:AlignedSize(_sphereCount*_objSize)*kMaxBuffersInFlight options:kMetalBufferMode];
    _computeMaterials = [_device newBufferWithLength:AlignedSize(_sphereCount*_matSize)*kMaxBuffersInFlight options:kMetalBufferMode];
    _computeParams = [_device newBufferWithLength:AlignedSize(sizeof(ComputeParams))*kMaxBuffersInFlight options:kMetalBufferMode];
    _computeEmissives = [_device newBufferWithLength:AlignedSize(_sphereCount*4)*kMaxBuffersInFlight options:kMetalBufferMode];
    _computeCounter = [_device newBufferWithLength:AlignedSize(4)*kMaxBuffersInFlight options:MTLStorageModeShared];
    _uniformBufferIndex = 0;

    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.sampleCount = view.sampleCount;
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    pipelineStateDescriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    pipelineStateDescriptor.stencilAttachmentPixelFormat = view.depthStencilPixelFormat;

    _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineState)
    {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

    MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthCompareFunction = MTLCompareFunctionLess;
    depthStateDesc.depthWriteEnabled = YES;
    _depthState = [_device newDepthStencilStateWithDescriptor:depthStateDesc];

    _commandQueue = [_device newCommandQueue];
    
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:kBackbufferWidth height:kBackbufferHeight mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.usage |= MTLTextureUsageShaderWrite;

    _backbuffer = [_device newTextureWithDescriptor:desc];
    _backbuffer2 = [_device newTextureWithDescriptor:desc];

    _backbufferPixels = new float[kBackbufferWidth * kBackbufferHeight * 4];
    memset(_backbufferPixels, 0, kBackbufferWidth * kBackbufferHeight * 4 * sizeof(_backbufferPixels[0]));
}


- (void)_loadAssets
{
    InitializeTest();
}

static uint64_t _computeStartTime;
static uint64_t _computeDur;
static size_t rayCounter = 0;
unsigned g_TestFlags = kFlagProgressive | kFlagAnimate;
static bool g_UseGPU = true;
static int totalCounter = 0;
static int frameCounter = 0;

- (void)_drawTestGpu:(id <MTLCommandBuffer>) cmd;
{
    _backbufferIndex = 1-_backbufferIndex;
    _uniformBufferIndex = (_uniformBufferIndex + 1) % kMaxBuffersInFlight;
    uint8_t* dataSpheres = (uint8_t*)[_computeSpheres contents];
    uint8_t* dataMaterials = (uint8_t*)[_computeMaterials contents];
    uint8_t* dataParams = (uint8_t*)[_computeParams contents];
    uint8_t* dataEmissives = (uint8_t*)[_computeEmissives contents];
    uint8_t* dataCounter = (uint8_t*)[_computeCounter contents];
    int spheresSize = AlignedSize(_sphereCount * _objSize);
    int matsSize = AlignedSize(_sphereCount * _matSize);
    int paramsSize = AlignedSize(sizeof(ComputeParams));
    int emissivesSize = AlignedSize(_sphereCount * 4);
    int counterSize = AlignedSize(4);
    ComputeParams* params = (ComputeParams*)(dataParams+_uniformBufferIndex*paramsSize);
    GetSceneDesc(dataSpheres+_uniformBufferIndex*spheresSize, dataMaterials+_uniformBufferIndex*matsSize, params, dataEmissives+_uniformBufferIndex*emissivesSize, &params->emissiveCount);
    params->sphereCount = _sphereCount;
    params->screenWidth = kBackbufferWidth;
    params->screenHeight = kBackbufferHeight;
    params->frames = totalCounter;
    params->invWidth = 1.0f / kBackbufferWidth;
    params->invHeight = 1.0f / kBackbufferHeight;
    params->lerpFac = float(totalCounter) / float(totalCounter+1);
    if (g_TestFlags & kFlagAnimate)
        params->lerpFac *= DO_ANIMATE_SMOOTHING;
    if (!(g_TestFlags & kFlagProgressive))
        params->lerpFac = 0;
    *(int*)(dataCounter+_uniformBufferIndex*counterSize) = 0;
#if !TARGET_OS_IPHONE
    [_computeSpheres didModifyRange:NSMakeRange(_uniformBufferIndex*spheresSize, spheresSize)];
    [_computeMaterials didModifyRange:NSMakeRange(_uniformBufferIndex*matsSize, matsSize)];
    [_computeEmissives didModifyRange:NSMakeRange(_uniformBufferIndex*emissivesSize, emissivesSize)];
    [_computeParams didModifyRange:NSMakeRange(_uniformBufferIndex*paramsSize, paramsSize)];
#endif

    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    [enc setComputePipelineState:_computeState];
    [enc setBuffer:_computeSpheres offset:_uniformBufferIndex*spheresSize atIndex:0];
    [enc setBuffer:_computeMaterials offset:_uniformBufferIndex*matsSize atIndex:1];
    [enc setBuffer:_computeParams offset:_uniformBufferIndex*paramsSize atIndex:2];
    [enc setBuffer:_computeEmissives offset:_uniformBufferIndex*emissivesSize atIndex:3];
    [enc setBuffer:_computeCounter offset:_uniformBufferIndex*counterSize atIndex:4];
    [enc setTexture:_backbufferIndex==0?_backbuffer2:_backbuffer atIndex: 0];
    [enc setTexture:_backbufferIndex==0?_backbuffer:_backbuffer2 atIndex: 1];
    MTLSize groupSize = {kCSGroupSizeX, kCSGroupSizeY, 1};
    MTLSize groupCount = {kBackbufferWidth/groupSize.width, kBackbufferHeight/groupSize.height, 1};
    [enc dispatchThreadgroups:groupCount threadsPerThreadgroup:groupSize];
    [enc endEncoding];
}

- (void)_doRenderingWith:(id <MTLCommandBuffer>) cmd;
{
    static uint64_t frameTime = 0;
    uint64_t time1 = mach_absolute_time();
    _computeStartTime = time1;
    
    uint64_t curNs = (time1 * _clock_timebase.numer) / _clock_timebase.denom;
    float curT = float(curNs * 1.0e-9f);

    UpdateTest(curT, totalCounter, kBackbufferWidth, kBackbufferHeight, g_TestFlags);
    
    if (g_UseGPU)
    {
        [self _drawTestGpu: cmd];
    }
    else
    {
        int rayCount;
        DrawTest(curT, totalCounter, kBackbufferWidth, kBackbufferHeight, _backbufferPixels, rayCount, g_TestFlags);
        rayCounter += rayCount;
    }
    
    uint64_t time2 = mach_absolute_time();
    (void)time2;
    ++frameCounter;
    ++totalCounter;
    if (g_UseGPU)
        frameTime += _computeDur;
    else
        frameTime += (time2-time1);
    if (frameCounter > 10)
    {
        uint64_t ns = (frameTime * _clock_timebase.numer) / _clock_timebase.denom;
        float s = (float)(ns * 1.0e-9) / frameCounter;
        char buffer[500];
        snprintf(buffer, 200, "%s: %.2fms (%.1f FPS) %.1fMrays/s %.2fMrays/frame frames %i",
                 g_UseGPU ? "GPU" : "CPU",
                 s * 1000.0f, 1.f / s, rayCounter / frameCounter / s * 1.0e-6f, rayCounter / frameCounter * 1.0e-6f, totalCounter);
        puts(buffer);
        NSString* str = [[NSString alloc] initWithUTF8String:buffer];
#if TARGET_OS_IPHONE
        _label.text = str;
#else
        _label.stringValue = str;
#endif
        frameCounter = 0;
        frameTime = 0;
        rayCounter = 0;
    }

    if (!g_UseGPU)
    {
        [_backbuffer replaceRegion:MTLRegionMake2D(0,0,kBackbufferWidth,kBackbufferHeight) mipmapLevel:0 withBytes:_backbufferPixels bytesPerRow:kBackbufferWidth*16];
    }
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_FOREVER);

    id <MTLCommandBuffer> cmd = [_commandQueue commandBuffer];

    __block dispatch_semaphore_t block_sema = _inFlightSemaphore;
    int counterIndex = (_uniformBufferIndex+1)%kMaxBuffersInFlight;
    id <MTLBuffer> counterBuffer = _computeCounter;
    [cmd addCompletedHandler:^(id<MTLCommandBuffer> buffer)
    {
        if (g_UseGPU)
        {
            // There's no easy/proper way to do GPU timing on Metal (or at least I couldn't find any),
            // so I'm timing CPU side, from beginning of command buffer invocation to when we get the
            // callback that the GPU is done with it. Not 100% proper, but gets similar results to
            // what Xcode reports for the GPU duration.
            uint64_t time2 = mach_absolute_time();
            _computeDur = (time2 - _computeStartTime);
            int rayCount = *(const int*)(((const uint8_t*)[counterBuffer contents]) + counterIndex*AlignedSize(4));
            rayCounter += rayCount;
        }

        dispatch_semaphore_signal(block_sema);
    }];

    [self _doRenderingWith:cmd];

    // Delay getting the currentRenderPassDescriptor until we absolutely need it to avoid
    //   holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;
    if(renderPassDescriptor != nil)
    {
        id <MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [enc setFrontFacingWinding:MTLWindingCounterClockwise];
        [enc setCullMode:MTLCullModeNone];
        [enc setRenderPipelineState:_pipelineState];
        [enc setDepthStencilState:_depthState];
        [enc setFragmentTexture:_backbuffer atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];
        [cmd presentDrawable:view.currentDrawable];
    }
    [cmd commit];
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    //printf("View size %ix%i\n", (int)size.width, (int)size.height);
}

-(void)toggleGPU
{
    g_UseGPU = !g_UseGPU;
    frameCounter = 0;
    totalCounter = 0;
}
-(void)toggleAnimation
{
    g_TestFlags ^= kFlagAnimate;
    frameCounter = 0;
    totalCounter = 0;
}
-(void)toggleProgressive
{
    g_TestFlags ^= kFlagProgressive;
    frameCounter = 0;
    totalCounter = 0;
}


@end

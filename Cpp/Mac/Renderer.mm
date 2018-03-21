#import <simd/simd.h>
#include <mach/mach_time.h>

const int kBackbufferWidth = 1280;
const int kBackbufferHeight = 720;

#import "Renderer.h"
#include "../Source/Test.h"

static const NSUInteger kMaxBuffersInFlight = 3;


@implementation Renderer
{
    dispatch_semaphore_t _inFlightSemaphore;
    id <MTLDevice> _device;
    id <MTLCommandQueue> _commandQueue;

    id <MTLRenderPipelineState> _pipelineState;
    id <MTLDepthStencilState> _depthState;
    
    id <MTLTexture> _backbuffer;
    float* _backbufferPixels;

    mach_timebase_info_data_t _clock_timebase;
}

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view;
{
    self = [super init];
    if(self)
    {
        _device = view.device;
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

    id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];
    id <MTLFunction> vertexFunction = [defaultLibrary newFunctionWithName:@"vertexShader"];
    id <MTLFunction> fragmentFunction = [defaultLibrary newFunctionWithName:@"fragmentShader"];

    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.sampleCount = view.sampleCount;
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    pipelineStateDescriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    pipelineStateDescriptor.stencilAttachmentPixelFormat = view.depthStencilPixelFormat;

    NSError *error = NULL;
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
    _backbuffer = [_device newTextureWithDescriptor:desc];
    _backbufferPixels = new float[kBackbufferWidth * kBackbufferHeight * 4];
    memset(_backbufferPixels, 0, kBackbufferWidth * kBackbufferHeight * 4 * sizeof(_backbufferPixels[0]));
}


- (void)_loadAssets
{
    InitializeTest();
}

- (void)_updateBackbufferState
{
    static int totalCounter = 0;
    static int frameCounter = 0;
    static uint64_t frameTime = 0;
    uint64_t time1 = mach_absolute_time();
    
    uint64_t curNs = (time1 * _clock_timebase.numer) / _clock_timebase.denom;
    float curT = float(curNs * 1.0e-9f);

    DrawTest(curT, totalCounter, kBackbufferWidth, kBackbufferHeight, _backbufferPixels);
    
    uint64_t time2 = mach_absolute_time();
    ++frameCounter;
    ++totalCounter;
    frameTime += (time2-time1);
    if (frameCounter > 10)
    {
        uint64_t ns = (frameTime * _clock_timebase.numer) / _clock_timebase.denom;
        float s = (float)(ns * 1.0e-9) / frameCounter;
        printf("%.2fms (%.1f FPS)\n", s * 1000.0f, 1.f / s);
        frameCounter = 0;
        frameTime = 0;
    }
    
    [_backbuffer replaceRegion:MTLRegionMake2D(0,0,kBackbufferWidth,kBackbufferHeight) mipmapLevel:0 withBytes:_backbufferPixels bytesPerRow:kBackbufferWidth*16];
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_FOREVER);

    id <MTLCommandBuffer> cmd = [_commandQueue commandBuffer];

    __block dispatch_semaphore_t block_sema = _inFlightSemaphore;
    [cmd addCompletedHandler:^(id<MTLCommandBuffer> buffer) { dispatch_semaphore_signal(block_sema); }];

    [self _updateBackbufferState];

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
}

@end

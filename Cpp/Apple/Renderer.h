#import <MetalKit/MetalKit.h>

@interface Renderer : NSObject <MTKViewDelegate>

#if TARGET_OS_IPHONE
-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view withLabel:(nonnull UILabel*) label;
#else
-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view withLabel:(nonnull NSTextField*) label;
#endif

-(void)toggleGPU;
-(void)toggleAnimation;
-(void)toggleProgressive;

@end

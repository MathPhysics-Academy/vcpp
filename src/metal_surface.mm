/*
 *  metal_surface.mm - Objective-C helper for Metal surface creation
 */

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

extern "C" void* vcpp_create_metal_layer(void* ns_window) {
  NSWindow* window = (__bridge NSWindow*)ns_window;
  NSView* view = [window contentView];

  [view setWantsLayer:YES];
  CAMetalLayer* layer = [CAMetalLayer layer];
  [view setLayer:layer];

  return (__bridge void*)layer;
}

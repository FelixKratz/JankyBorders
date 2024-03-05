#pragma once
#include <CoreGraphics/CoreGraphics.h>

struct gradient {
  enum { TL_TO_BR, TR_TO_BL } direction;
  uint32_t color1;
  uint32_t color2;
};

static inline void colors_from_hex(uint32_t hex, float* a, float* r, float* g, float* b) {
  *a = ((hex >> 24) & 0xff) / 255.f;
  *r = ((hex >> 16) & 0xff) / 255.f;
  *g = ((hex >> 8) & 0xff) / 255.f;
  *b = ((hex >> 0) & 0xff) / 255.f;
}

static inline void drawing_set_fill(CGContextRef context, uint32_t color) {
  float a,r,g,b;
  colors_from_hex(color, &a, &r, &g, &b);
  CGContextSetRGBFillColor(context, r, g, b, a);
}

static inline void drawing_set_stroke(CGContextRef context, uint32_t color) {
  float a,r,g,b;
  colors_from_hex(color, &a, &r, &g, &b);
  CGContextSetRGBStrokeColor(context, r, g, b, a);
}

static inline void drawing_set_stroke_and_fill(CGContextRef context, uint32_t color, bool glow) {
  float a,r,g,b;
  colors_from_hex(color, &a, &r, &g, &b);
  CGContextSetRGBFillColor(context, r, g, b, a);
  CGContextSetRGBStrokeColor(context, r, g, b, a);

  if (glow) {
    CGColorRef color_ref = CGColorCreateGenericRGB(r, g, b, 1.0);
    CGContextSetShadowWithColor(context, CGSizeZero, 10.0, color_ref);
    CGColorRelease(color_ref);
  }
}

static inline void drawing_clip_between_rect_and_path(CGContextRef context, CGRect frame, CGPathRef path) {
  CGMutablePathRef clip_path = CGPathCreateMutable();
  CGPathAddRect(clip_path, NULL, frame);
  CGPathAddPath(clip_path, NULL, path);
  CGContextAddPath(context, clip_path);
  CGContextEOClip(context);
  CFRelease(clip_path);
}

static inline void drawing_add_rect_with_inset(CGContextRef context, CGRect rect, float inset) {
  CGRect square_rect = CGRectInset(rect, inset, inset);
  CGPathRef square_path = CGPathCreateWithRect(square_rect, NULL);
  CGContextAddPath(context, square_path);
  CFRelease(square_path);
}

static inline void drawing_add_rounded_rect(CGContextRef context, CGRect rect, float border_radius) {
  CGPathRef stroke_path = CGPathCreateWithRoundedRect(rect,
                                                      border_radius,
                                                      border_radius,
                                                      NULL          );

  CGContextAddPath(context, stroke_path);
  CFRelease(stroke_path);
}

static inline void drawing_draw_square_with_inset(CGContextRef context, CGRect rect, float inset) {
  drawing_add_rect_with_inset(context, rect, inset);
  CGContextFillPath(context);
}

static inline void drawing_draw_square_gradient_with_inset(CGContextRef context,CGGradientRef gradient, CGPoint dir[2], CGRect rect, float inset) {
  drawing_add_rect_with_inset(context, rect, inset);
  CGContextClip(context);
  CGContextDrawLinearGradient(context, gradient, dir[0], dir[1], 0);
}

static inline void drawing_draw_rounded_rect_with_inset(CGContextRef context, CGRect rect, float border_radius) {
  drawing_add_rounded_rect(context, rect, border_radius);
  CGContextStrokePath(context);
}

static inline void drawing_draw_rounded_gradient_with_inset(CGContextRef context,CGGradientRef gradient, CGPoint dir[2], CGRect rect, float border_radius) {
  drawing_add_rounded_rect(context, rect, border_radius);
  CGContextReplacePathWithStrokedPath(context);
  CGContextClip(context);
  CGContextDrawLinearGradient(context, gradient, dir[0], dir[1], 0);
}

static inline void drawing_draw_filled_path(CGContextRef context, CGPathRef path, uint32_t color) {
  drawing_set_fill(context, color);
  drawing_set_stroke(context, 0);
  CGContextAddPath(context, path);
  CGContextFillPath(context);
}

static inline CGGradientRef drawing_create_gradient(struct gradient* gradient, CGAffineTransform trans, CGPoint direction[2]) {
  float a1, a2, r1, r2, g1, g2, b1, b2;
  colors_from_hex(gradient->color1, &a1, &r1, &g1, &b1);
  colors_from_hex(gradient->color2, &a2, &r2, &g2, &b2);
  CGColorRef c[] = { CGColorCreateSRGB(r1, g1, b1, a1),
                     CGColorCreateSRGB(r2, g2, b2, a2) };
  CFArrayRef cfc = CFArrayCreate(NULL,
                                 (const void **)c,
                                 2,
                                 &kCFTypeArrayCallBacks);
  CGGradientRef result = CGGradientCreateWithColors(NULL, cfc, NULL);
  CFRelease(cfc);
  CGColorRelease(c[0]);
  CGColorRelease(c[1]);
  if (gradient->direction == TR_TO_BL) {
    direction[0] = CGPointMake(1, 1);
    direction[1] = CGPointZero;
  } else if (gradient->direction == TL_TO_BR) {
    direction[0] = CGPointMake(0, 1);
    direction[1] = CGPointMake(1, 0);
  }
  direction[0] = CGPointApplyAffineTransform(direction[0], trans);
  direction[1] = CGPointApplyAffineTransform(direction[1], trans);
  return result;
}


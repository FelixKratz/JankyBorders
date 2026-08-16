#pragma once
#include <CoreGraphics/CoreGraphics.h>

// Overlap neighboring side regions slightly, so that their antialiased edges add
// up to full coverage instead of leaving a hairline seam where they meet
#define BORDER_SIDE_OVERLAP 0.25f

// Clockwise, as drawing_clip_to_side rotates by one quarter turn per side
enum border_side {
  BORDER_SIDE_TOP,
  BORDER_SIDE_RIGHT,
  BORDER_SIDE_BOTTOM,
  BORDER_SIDE_LEFT,
  BORDER_SIDE_COUNT
};

// The per side clip extends this far past `rect` to cover everything drawn
// outside of it. The value is unconstrained: the context is already clipped to
// the border frame, so only the part of the trapezoid inside the frame paints.
#define BORDER_SIDE_OUTSET 1000.0f

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

// Clips the context to the region of `rect` that belongs to `side`, i.e. the
// region closer to this side than to any other. Neighboring regions meet at the
// 45 degree lines through the corners, like the mitered joints of a picture
// frame.
static inline void drawing_clip_to_side(CGContextRef context, CGRect rect, enum border_side side) {
  bool horizontal = side == BORDER_SIDE_TOP || side == BORDER_SIDE_BOTTOM;

  // Distance from the center to the side (a) and half the length of the side (b)
  float a = 0.5f * (horizontal ? rect.size.height : rect.size.width);
  float b = 0.5f * (horizontal ? rect.size.width : rect.size.height)
            + BORDER_SIDE_OVERLAP;

  // The region ends where it meets the opposing side (a) or where the diagonals
  // of the two adjacent corners cross (b)
  float depth = fminf(a, b);

  // The top region, centered in the rect, u along the side and v towards it.
  // The outer edge sits on the same diagonal through the corner for any outset,
  // and the context is already clipped to the frame, so BORDER_SIDE_OUTSET only
  // needs to be large enough to reach past it.
  CGPoint trapezoid[] = { { -(b + BORDER_SIDE_OUTSET), a + BORDER_SIDE_OUTSET },
                          {  (b + BORDER_SIDE_OUTSET), a + BORDER_SIDE_OUTSET },
                          {  (b - depth),  a - depth  },
                          { -(b - depth),  a - depth  } };

  // The other sides are the same shape rotated a quarter turn each, which is why
  // enum border_side is ordered clockwise
  CGAffineTransform transform = CGAffineTransformRotate(
                                  CGAffineTransformMakeTranslation(
                                    CGRectGetMidX(rect),
                                    CGRectGetMidY(rect)            ),
                                  -M_PI_2 * side                    );

  CGMutablePathRef path = CGPathCreateMutable();
  CGPathAddLines(path, &transform, trapezoid, 4);
  CGPathCloseSubpath(path);

  CGContextAddPath(context, path);
  CGContextClip(context);
  CFRelease(path);
}

static inline CGPathRef drawing_create_rect_path(CGRect rect, float inset) {
  return CGPathCreateWithRect(CGRectInset(rect, inset, inset), NULL);
}

static inline CGPathRef drawing_create_rounded_rect_path(CGRect rect, float border_radius) {
  return CGPathCreateWithRoundedRect(rect, border_radius, border_radius, NULL);
}

static inline void drawing_add_rect_with_inset(CGContextRef context, CGRect rect, float inset) {
  CGPathRef square_path = drawing_create_rect_path(rect, inset);
  CGContextAddPath(context, square_path);
  CFRelease(square_path);
}

static inline void drawing_add_rounded_rect(CGContextRef context, CGRect rect, float border_radius) {
  CGPathRef stroke_path = drawing_create_rounded_rect_path(rect, border_radius);
  CGContextAddPath(context, stroke_path);
  CFRelease(stroke_path);
}

// Paints `path` with one color per side of `rect`, clipping each pass to the
// region belonging to that side. A border whose sides share one color is painted
// in a single pass, and so is a NULL `colors`, which keeps whichever color the
// context already carries.
static inline void drawing_paint_path(CGContextRef context, CGPathRef path, CGRect rect, uint32_t* colors, bool glow, bool fill) {
  bool per_side = colors
                  && !(colors[0] == colors[1]
                       && colors[0] == colors[2]
                       && colors[0] == colors[3]);

  for (int side = 0; side < (per_side ? BORDER_SIDE_COUNT : 1); side++) {
    if (per_side && !(colors[side] & 0xff000000)) continue;

    CGContextSaveGState(context);
    if (per_side) drawing_clip_to_side(context, rect, side);
    if (colors) drawing_set_stroke_and_fill(context, colors[side], glow);

    CGContextAddPath(context, path);
    if (fill) CGContextFillPath(context);
    else CGContextStrokePath(context);
    CGContextRestoreGState(context);
  }
}

static inline void drawing_draw_square_with_inset(CGContextRef context, CGRect rect, float inset, uint32_t* colors, bool glow) {
  CGPathRef square_path = drawing_create_rect_path(rect, inset);
  drawing_paint_path(context, square_path, rect, colors, glow, true);
  CFRelease(square_path);
}

static inline void drawing_draw_square_gradient_with_inset(CGContextRef context,CGGradientRef gradient, CGPoint dir[2], CGRect rect, float inset) {
  drawing_add_rect_with_inset(context, rect, inset);
  CGContextClip(context);
  CGContextDrawLinearGradient(context, gradient, dir[0], dir[1], 0);
}

static inline void drawing_draw_rounded_rect_with_inset(CGContextRef context, CGRect rect, float border_radius, bool fill, uint32_t* colors, bool glow) {
  CGPathRef stroke_path = drawing_create_rounded_rect_path(rect, border_radius);
  drawing_paint_path(context, stroke_path, rect, colors, glow, fill);
  CFRelease(stroke_path);
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


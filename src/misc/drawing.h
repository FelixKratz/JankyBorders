#pragma once
#include <CoreGraphics/CoreGraphics.h>
#include <math.h>

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

static inline void drawing_draw_rounded_rect_with_inset(CGContextRef context, CGRect rect, float border_radius, bool fill) {
  drawing_add_rounded_rect(context, rect, border_radius);
  if (fill) CGContextFillPath(context);
  else CGContextStrokePath(context);
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

// Nine slice: a solid rounded border is four cached corner rasterisations plus
// one row (or column) of each, stretched along the edge. The bands are sliced
// out of the corners instead of filled as rects because a stroke edge and a
// fill edge do not rasterise identically, and the corners are not mirrored
// because CoreGraphics resamples a mirrored arc.
struct nine_slice {
  uint32_t color;
  float border_width;
  float outer_offset;
  float corner_radius;
  float inner_radius;
  float scale;
  bool uniform;
};

static inline bool drawing_nine_slice_equal(const struct nine_slice* a, const struct nine_slice* b) {
  return a->color         == b->color
      && a->border_width  == b->border_width
      && a->outer_offset  == b->outer_offset
      && a->corner_radius == b->corner_radius
      && a->inner_radius  == b->inner_radius
      && a->scale         == b->scale
      && a->uniform       == b->uniform;
}

// Distance from a frame corner past which nothing is curved anymore
static inline float drawing_nine_slice_extent(const struct nine_slice* ns) {
  float curved = ns->corner_radius > ns->inner_radius + 1.f
                 ? ns->corner_radius
                 : ns->inner_radius + 1.f;
  return ceilf(ns->outer_offset + curved) + 1.f;
}

static inline void drawing_draw_round_solid(CGContextRef context, CGRect frame, CGRect bounds, const struct nine_slice* ns) {
  CGContextSaveGState(context);
  drawing_set_stroke_and_fill(context, ns->color, false);
  CGContextSetLineWidth(context, ns->border_width);

  CGMutablePathRef inner_clip_path = CGPathCreateMutable();
  CGPathAddRoundedRect(inner_clip_path,
                       NULL,
                       CGRectInset(bounds, 1.0, 1.0),
                       ns->inner_radius,
                       ns->inner_radius             );
  drawing_clip_between_rect_and_path(context, frame, inner_clip_path);
  CFRelease(inner_clip_path);

  if (ns->uniform) {
    drawing_draw_rounded_rect_with_inset(context,
                                         bounds,
                                         ns->corner_radius,
                                         true              );
  }
  drawing_draw_rounded_rect_with_inset(context,
                                       bounds,
                                       ns->corner_radius,
                                       false             );
  CGContextRestoreGState(context);
}

#define NINE_SLICE_BL 0
#define NINE_SLICE_BR 1
#define NINE_SLICE_TL 2
#define NINE_SLICE_TR 3

#define NINE_SLICE_LEFT   0
#define NINE_SLICE_RIGHT  1
#define NINE_SLICE_BOTTOM 2
#define NINE_SLICE_TOP    3

// Bands are one pixel along the edge, trimmed across it to the painted span:
// near is the distance from the frame edge to that span
struct nine_slice_cache {
  struct nine_slice key;
  CGImageRef corner[4];
  CGImageRef band[4];
  float near[4];
  float thickness[4];
};

static inline void drawing_nine_slice_release(struct nine_slice_cache* cache) {
  for (int i = 0; i < 4; ++i) {
    if (cache->band[i]) CGImageRelease(cache->band[i]);
    if (cache->corner[i]) CGImageRelease(cache->corner[i]);
    cache->band[i] = NULL;
    cache->corner[i] = NULL;
    cache->near[i] = 0.f;
    cache->thickness[i] = 0.f;
  }
}

// Renders a 2x2 corner square border, offset so that corner index lands in the
// bitmap. Device RGB keeps the colour conversion identical to the window's.
static inline CGContextRef drawing_nine_slice_corner(const struct nine_slice* ns, int index) {
  float extent = drawing_nine_slice_extent(ns);
  size_t pixels = (size_t)(extent * ns->scale + 0.5f);
  if (pixels == 0) return NULL;

  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(NULL,
                                               pixels,
                                               pixels,
                                               8,
                                               0,
                                               color_space,
                                               kCGImageAlphaPremultipliedFirst
                                               | kCGBitmapByteOrder32Host    );
  CGColorSpaceRelease(color_space);
  if (!context) return NULL;

  CGContextScaleCTM(context, ns->scale, ns->scale);
  CGContextTranslateCTM(context,
                        (index == NINE_SLICE_BR || index == NINE_SLICE_TR)
                        ? -extent : 0.f,
                        (index == NINE_SLICE_TL || index == NINE_SLICE_TR)
                        ? -extent : 0.f                                   );

  CGRect frame = { { 0.f, 0.f }, { 2.f * extent, 2.f * extent } };
  CGRect bounds = CGRectInset(frame, ns->outer_offset, ns->outer_offset);
  drawing_draw_round_solid(context, frame, bounds, ns);
  return context;
}

static inline void drawing_nine_slice_span(const uint8_t* pixels, size_t stride, size_t count, size_t* first, size_t* last) {
  *first = count;
  *last = 0;
  for (size_t i = 0; i < count; ++i) {
    const uint8_t* p = pixels + i * stride;
    if (p[0] || p[1] || p[2] || p[3]) {
      if (i < *first) *first = i;
      *last = i + 1;
    }
  }
  if (*first > *last) *first = *last;
}

static inline bool drawing_nine_slice_build(struct nine_slice_cache* cache, const struct nine_slice* ns) {
  drawing_nine_slice_release(cache);
  cache->key = *ns;

  float extent = drawing_nine_slice_extent(ns);
  float scale = ns->scale;
  size_t pixels = (size_t)(extent * scale + 0.5f);
  if (pixels == 0) return false;

  CGContextRef corner[4] = { NULL, NULL, NULL, NULL };
  bool ok = true;

  for (int i = 0; i < 4; ++i) {
    corner[i] = drawing_nine_slice_corner(ns, i);
    if (!corner[i]) ok = false;
  }

  for (int i = 0; ok && i < 4; ++i) {
    cache->corner[i] = CGBitmapContextCreateImage(corner[i]);
    if (!cache->corner[i]) ok = false;
  }

  for (int i = 0; ok && i < 4; ++i) {
    bool vertical = (i == NINE_SLICE_LEFT || i == NINE_SLICE_RIGHT);
    int source = i == NINE_SLICE_RIGHT ? NINE_SLICE_BR
               : i == NINE_SLICE_TOP   ? NINE_SLICE_TL
                                       : NINE_SLICE_BL;

    const uint8_t* data = CGBitmapContextGetData(corner[source]);
    size_t row_bytes = CGBitmapContextGetBytesPerRow(corner[source]);
    size_t first, last;

    // The row (or column) of the corner farthest from its edge is the profile
    // of the straight band; image row 0 is the top row
    if (vertical) {
      drawing_nine_slice_span(data, 4, pixels, &first, &last);
    } else {
      drawing_nine_slice_span(data + (pixels - 1) * 4, row_bytes, pixels,
                              &first, &last                             );
    }

    if (last <= first) {
      cache->thickness[i] = 0.f;
      continue;
    }

    CGRect slice = vertical
                   ? CGRectMake((float)first, 0.f, (float)(last - first), 1.f)
                   : CGRectMake((float)(pixels - 1), (float)first,
                                1.f, (float)(last - first));
    cache->band[i] = CGImageCreateWithImageInRect(cache->corner[source], slice);
    if (!cache->band[i]) { ok = false; break; }

    cache->thickness[i] = (float)(last - first) / scale;

    cache->near[i] = (i == NINE_SLICE_LEFT || i == NINE_SLICE_TOP)
                     ? (float)first / scale
                     : extent - (float)last / scale;
  }

  for (int i = 0; i < 4; ++i) if (corner[i]) CGContextRelease(corner[i]);
  if (!ok) drawing_nine_slice_release(cache);
  return ok;
}

static inline void drawing_draw_nine_slice(CGContextRef context, CGRect frame, const struct nine_slice_cache* cache) {
  float extent = drawing_nine_slice_extent(&cache->key);
  float width = frame.size.width;
  float height = frame.size.height;
  float span_x = width - 2.f * extent;
  float span_y = height - 2.f * extent;
  float far_x = width - extent;
  float far_y = height - extent;

  float x = frame.origin.x;
  float y = frame.origin.y;

  const float* near = cache->near;
  const float* thick = cache->thickness;

  CGRect corners[4] = {
    { { x,         y         }, { extent, extent } },
    { { x + far_x, y         }, { extent, extent } },
    { { x,         y + far_y }, { extent, extent } },
    { { x + far_x, y + far_y }, { extent, extent } },
  };

  CGRect bands[4] = {
    { { x + near[0], y + extent }, { thick[0], span_y } },
    { { x + width - near[1] - thick[1], y + extent }, { thick[1], span_y } },
    { { x + extent, y + near[2] }, { span_x, thick[2] } },
    { { x + extent, y + height - near[3] - thick[3] }, { span_x, thick[3] } },
  };

  CGContextSaveGState(context);
  CGContextSetInterpolationQuality(context, kCGInterpolationNone);

  for (int i = 0; i < 4; ++i)
    CGContextDrawImage(context, corners[i], cache->corner[i]);

  for (int i = 0; i < 4; ++i) {
    bool vertical = (i == NINE_SLICE_LEFT || i == NINE_SLICE_RIGHT);
    if (!cache->band[i]) continue;
    if ((vertical ? span_y : span_x) <= 0.f) continue;
    CGContextDrawImage(context, bands[i], cache->band[i]);
  }

  CGContextRestoreGState(context);
}


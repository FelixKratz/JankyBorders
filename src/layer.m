#include "layer.h"
#include "misc/extern.h"

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

#include <dlfcn.h>
#include <math.h>

// Resolved at runtime so that a system without them falls back to the
// CoreGraphics path instead of failing to launch
@interface CAContext : NSObject
+ (CAContext*)contextWithCGSConnection:(uint32_t)cid options:(NSDictionary*)options;
@property(readonly) uint32_t contextId;
@property(retain) CALayer* layer;
- (void)invalidate;
@end

typedef CGError (*set_window_layer_context_fn)(int cid, uint32_t wid, id context);
static set_window_layer_context_fn g_set_window_layer_context = NULL;

enum { LAYER_PASS_BASE, LAYER_PASS_MASK, LAYER_PASS_BACKGROUND };

// Distance from a frame corner past which nothing varies along the edge
static float layer_extent(const struct layer_style* style) {
  float curved = style->corner_radius > style->inner_radius + 1.f
                 ? style->corner_radius
                 : style->inner_radius + 1.f;

  float extent = ceilf(style->outer_offset + curved) + 2.f;
  if (style->color.stype == COLOR_STYLE_GLOW) extent += 14.f;
  return extent;
}

static bool layer_background_paintable(const struct layer_style* style) {
  return style->show_background
         && (style->background.stype == COLOR_STYLE_SOLID
             || style->background.stype == COLOR_STYLE_GLOW);
}

// Mirrors border_draw_slow. The mask pass is the ring in opaque white for a
// CAGradientLayer, the background pass the fill alone for the gradient styles.
static CGImageRef layer_render(const struct layer_style* style, CGSize size, int pass, float scale) {
  size_t width = (size_t)(size.width * scale + 0.5f);
  size_t height = (size_t)(size.height * scale + 0.5f);
  if (!width || !height) return NULL;

  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(NULL,
                                               width,
                                               height,
                                               8,
                                               0,
                                               color_space,
                                               kCGImageAlphaPremultipliedFirst
                                               | kCGBitmapByteOrder32Host    );
  CGColorSpaceRelease(color_space);
  if (!context) return NULL;

  CGContextScaleCTM(context, scale, scale);

  bool mask = pass == LAYER_PASS_MASK;
  bool background_only = pass == LAYER_PASS_BACKGROUND;
  bool gradient = style->color.stype == COLOR_STYLE_GRADIENT;
  bool glow = style->color.stype == COLOR_STYLE_GLOW;
  bool square = style->border_style == BORDER_STYLE_SQUARE;
  bool uniform = style->border_style == BORDER_STYLE_ROUND_UNIFORM;

  CGRect frame = { CGPointZero, size };
  CGRect path_rect = CGRectInset(frame, style->outer_offset, style->outer_offset);

  CGMutablePathRef inner_clip_path = CGPathCreateMutable();
  if (style->truly_square) {
    // Inset the frame to overlap the rounding of macOS windows to create a
    // truly square border
    path_rect = CGRectInset(path_rect, BORDER_TSMN, BORDER_TSMN);
    CGPathAddRect(inner_clip_path, NULL, path_rect);
  } else {
    CGPathAddRoundedRect(inner_clip_path,
                         NULL,
                         CGRectInset(path_rect, 1.0, 1.0),
                         style->inner_radius,
                         style->inner_radius             );
  }

  if (!background_only) {
    CGContextSaveGState(context);
    if (mask) drawing_set_stroke_and_fill(context, 0xffffffff, false);
    else if (!gradient) drawing_set_stroke_and_fill(context, style->color.color, glow);
    CGContextSetLineWidth(context, style->border_width);
    drawing_clip_between_rect_and_path(context, frame, inner_clip_path);

    if (square) {
      if (mask || !gradient) {
        drawing_draw_square_with_inset(context,
                                       path_rect,
                                       -style->border_width / 2.f);
      }
    } else {
      if (uniform && !mask) {
        drawing_draw_rounded_rect_with_inset(context,
                                             path_rect,
                                             style->corner_radius,
                                             true                 );
      }
      if (mask || !gradient) {
        drawing_draw_rounded_rect_with_inset(context,
                                             path_rect,
                                             style->corner_radius,
                                             false                );
      }
    }
    CGContextRestoreGState(context);
  }

  // The background composites over the ring, for a gradient that needs a layer
  // of its own above the gradient layer
  if (layer_background_paintable(style)
      && (background_only || (!mask && !gradient))) {
    CGContextSaveGState(context);
    drawing_draw_filled_path(context, inner_clip_path, style->background.color);
    CGContextRestoreGState(context);
  }

  CFRelease(inner_clip_path);

  CGImageRef image = CGBitmapContextCreateImage(context);
  CGContextRelease(context);
  return image;
}

// Two per border, so that a focus change is a contents swap
struct layer_art {
  bool valid;
  bool exact;
  struct layer_style key;
  float scale;
  CGSize image_size;
  CGRect contents_center;
  CGImageRef base;
  CGImageRef mask;
  CGImageRef background;
  uint64_t stamp;
};

struct layer_border {
  int cid;
  uint32_t wid;
  float scale;

  CAContext* context;
  CALayer* root;
  CALayer* base;
  CAGradientLayer* gradient;
  CALayer* gradient_mask;
  CALayer* background;

  struct layer_art art[2];
  int current;
  uint64_t clock;

  bool has_size;
  CGSize size;
};

static bool layer_color_equal(const struct color_style* a, const struct color_style* b) {
  if (a->stype != b->stype) return false;
  if (a->stype == COLOR_STYLE_GRADIENT) {
    return a->gradient.direction == b->gradient.direction
        && a->gradient.color1 == b->gradient.color1
        && a->gradient.color2 == b->gradient.color2;
  }
  return a->color == b->color;
}

static bool layer_style_equal(const struct layer_style* a, const struct layer_style* b) {
  if (!layer_color_equal(&a->color, &b->color)) return false;
  if (a->show_background != b->show_background) return false;
  if (a->show_background && !layer_color_equal(&a->background, &b->background))
    return false;

  return a->border_width  == b->border_width
      && a->corner_radius == b->corner_radius
      && a->inner_radius  == b->inner_radius
      && a->outer_offset  == b->outer_offset
      && a->border_style  == b->border_style
      && a->truly_square  == b->truly_square;
}

static void layer_art_release(struct layer_art* art) {
  if (art->base) CGImageRelease(art->base);
  if (art->mask) CGImageRelease(art->mask);
  if (art->background) CGImageRelease(art->background);
  art->base = NULL;
  art->mask = NULL;
  art->background = NULL;
  art->valid = false;
}

static bool layer_art_matches(const struct layer_art* art, const struct layer_style* style, bool exact, CGSize size, float scale) {
  if (!art->valid || art->exact != exact || art->scale != scale) return false;
  if (exact && !CGSizeEqualToSize(art->image_size, size)) return false;
  return layer_style_equal(&art->key, style);
}

static bool layer_art_build(struct layer_art* art, const struct layer_style* style, bool exact, CGSize size, float extent, float centre, float scale) {
  layer_art_release(art);

  CGSize image_size = exact
                      ? size
                      : CGSizeMake(2.f * extent + centre, 2.f * extent + centre);
  if (image_size.width <= 0.f || image_size.height <= 0.f) return false;

  bool gradient = style->color.stype == COLOR_STYLE_GRADIENT;
  bool uniform = style->border_style == BORDER_STYLE_ROUND_UNIFORM;
  bool background = gradient && layer_background_paintable(style);

  bool base = !gradient || uniform;

  if (base) art->base = layer_render(style, image_size, LAYER_PASS_BASE, scale);
  if (gradient) art->mask = layer_render(style, image_size, LAYER_PASS_MASK, scale);
  if (background)
    art->background = layer_render(style, image_size, LAYER_PASS_BACKGROUND, scale);

  if ((base && !art->base)
      || (gradient && !art->mask)
      || (background && !art->background)) {
    layer_art_release(art);
    return false;
  }

  art->key = *style;
  art->exact = exact;
  art->scale = scale;
  art->image_size = image_size;

  art->contents_center = exact
                         ? CGRectMake(0.f, 0.f, 1.f, 1.f)
                         : CGRectMake(extent / image_size.width,
                                      extent / image_size.height,
                                      centre / image_size.width,
                                      centre / image_size.height);
  art->valid = true;
  return true;
}

static void layer_gradient_apply(CAGradientLayer* layer, const struct gradient* spec) {
  float a1, r1, g1, b1, a2, r2, g2, b2;
  colors_from_hex(spec->color1, &a1, &r1, &g1, &b1);
  colors_from_hex(spec->color2, &a2, &r2, &g2, &b2);

  CGColorRef first = CGColorCreateSRGB(r1, g1, b1, a1);
  CGColorRef second = CGColorCreateSRGB(r2, g2, b2, a2);
  if (first && second) {
    layer.colors = [NSArray arrayWithObjects:(id)first, (id)second, nil];
  }
  if (first) CGColorRelease(first);
  if (second) CGColorRelease(second);

  // Unflipped like the window context, so the same corners as in
  // drawing_create_gradient
  if (spec->direction == TR_TO_BL) {
    layer.startPoint = CGPointMake(1.f, 1.f);
    layer.endPoint = CGPointMake(0.f, 0.f);
  } else {
    layer.startPoint = CGPointMake(0.f, 1.f);
    layer.endPoint = CGPointMake(1.f, 0.f);
  }
}

static void layer_configure_content(CALayer* layer, float scale) {
  layer.anchorPoint = CGPointZero;
  layer.position = CGPointZero;
  layer.bounds = CGRectZero;
  layer.masksToBounds = NO;
  layer.contentsScale = scale;
  layer.contentsGravity = kCAGravityResize;
  layer.magnificationFilter = kCAFilterNearest;
  layer.minificationFilter = kCAFilterNearest;
  layer.edgeAntialiasingMask = 0;
}

static void layer_ensure_gradient(struct layer_border* border) {
  if (border->gradient) return;

  CAGradientLayer* gradient = [CAGradientLayer layer];
  gradient.anchorPoint = CGPointZero;
  gradient.position = CGPointZero;
  gradient.bounds = CGRectZero;
  gradient.masksToBounds = NO;
  gradient.contentsScale = border->scale;

  CALayer* mask = [CALayer layer];
  layer_configure_content(mask, border->scale);
  gradient.mask = mask;

  [border->root addSublayer:gradient];
  border->gradient = [gradient retain];
  border->gradient_mask = [mask retain];
}

static void layer_ensure_background(struct layer_border* border) {
  if (border->background) return;
  layer_ensure_gradient(border);

  CALayer* background = [CALayer layer];
  layer_configure_content(background, border->scale);
  [border->root addSublayer:background];
  border->background = [background retain];
}

bool layer_supported(void) {
  static int supported = -1;
  if (supported >= 0) return supported != 0;
  supported = 0;

  set_window_layer_context_fn attach = (set_window_layer_context_fn)
                            dlsym(RTLD_DEFAULT, "SLSSetWindowLayerContext");
  if (!attach) {
    debug("Layer borders: SLSSetWindowLayerContext is unavailable\n");
    return false;
  }

  Class context_class = NSClassFromString(@"CAContext");
  if (!context_class
      || ![context_class respondsToSelector:
                         @selector(contextWithCGSConnection:options:)]) {
    debug("Layer borders: CAContext is unavailable\n");
    return false;
  }

  g_set_window_layer_context = attach;
  supported = 1;
  return true;
}

struct layer_border* layer_border_create(int cid, CGRect frame, bool unmanaged, bool hidpi, uint32_t* wid_out) {
  if (wid_out) *wid_out = 0;
  if (!layer_supported()) return NULL;

  float scale = hidpi ? 2.f : 1.f;
  uint32_t wid = 0;
  struct layer_border* border = NULL;

  @autoreleasepool {
    uint64_t set_tags = (1ULL << 1) | (1ULL << 9);
    uint64_t clear_tags = 0;

    CFTypeRef shape = NULL;
    CGSNewRegionWithRect(&frame, &shape);
    CFTypeRef opaque_shape = CGRegionCreateEmptyRegion();
    CGError error = SLSNewWindowWithOpaqueShapeAndContext(cid,
                                                          5,
                                                          shape,
                                                          opaque_shape,
                                                          13 | (1 << 18),
                                                          &set_tags,
                                                          -9999,
                                                          -9999,
                                                          64,
                                                          &wid,
                                                          NULL           );
    if (shape) CFRelease(shape);
    CFRelease(opaque_shape);

    if (error != kCGErrorSuccess || !wid) {
      debug("Layer borders: could not create a type 5 window (%d)\n", error);
      return NULL;
    }

    Class context_class = NSClassFromString(@"CAContext");
    CAContext* context = [(id)context_class contextWithCGSConnection:(uint32_t)cid
                                            options:nil                          ];
    if (!context) {
      debug("Layer borders: could not create a CAContext\n");
      SLSReleaseWindow(cid, wid);
      return NULL;
    }

    CALayer* root = [CALayer layer];
    root.anchorPoint = CGPointZero;
    root.position = CGPointZero;
    root.bounds = CGRectZero;
    root.masksToBounds = NO;
    root.backgroundColor = NULL;
    root.opaque = NO;
    root.geometryFlipped = NO;
    root.contentsScale = scale;

    CALayer* base = [CALayer layer];
    layer_configure_content(base, scale);
    [root addSublayer:base];

    context.layer = root;

    error = g_set_window_layer_context(cid, wid, context);
    if (error != kCGErrorSuccess) {
      debug("Layer borders: SLSSetWindowLayerContext failed (%d)\n", error);
      context.layer = nil;
      [context invalidate];
      SLSReleaseWindow(cid, wid);
      return NULL;
    }

    if (unmanaged) SLSSetWindowAlpha(cid, wid, 0.f);

    SLSSetWindowResolution(cid, wid, hidpi ? 2.0f : 1.0f);
    SLSSetWindowTags(cid, wid, &set_tags, 64);
    SLSClearWindowTags(cid, wid, &clear_tags, 64);
    SLSSetWindowOpacity(cid, wid, 0);
    window_disable_shadow(wid);

    border = malloc(sizeof(struct layer_border));
    memset(border, 0, sizeof(struct layer_border));
    border->cid = cid;
    border->wid = wid;
    border->scale = scale;
    border->context = [context retain];
    border->root = [root retain];
    border->base = [base retain];
    border->current = -1;
  }

  if (wid_out) *wid_out = wid;
  return border;
}

void layer_border_destroy(struct layer_border* border) {
  if (!border) return;

  @autoreleasepool {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    if (border->background) [border->background removeFromSuperlayer];
    if (border->gradient) {
      border->gradient.mask = nil;
      [border->gradient removeFromSuperlayer];
    }
    if (border->base) [border->base removeFromSuperlayer];
    if (border->context) border->context.layer = nil;
    [CATransaction commit];
    [CATransaction flush];

    if (border->context) [border->context invalidate];

    [border->background release];
    [border->gradient_mask release];
    [border->gradient release];
    [border->base release];
    [border->root release];
    [border->context release];
  }

  for (int i = 0; i < 2; ++i) layer_art_release(&border->art[i]);
  free(border);
}

bool layer_border_update(struct layer_border* border, const struct layer_style* style, CGSize size) {
  if (!border || !style) return false;
  if (!(size.width > 0.f) || !(size.height > 0.f)
      || !isfinite(size.width) || !isfinite(size.height)) {
    return true;
  }

  float scale = border->scale;
  float extent = layer_extent(style);

  // A two pixel centre keeps the template width even, an odd width makes
  // CoreGraphics round the antialiasing of a half covered stroke differently
  float centre = 2.f / scale;

  // Rendered at its own size when the corner slices would overlap
  bool exact = size.width < 2.f * extent + centre
            || size.height < 2.f * extent + centre;

  int slot = -1;
  for (int i = 0; i < 2; ++i) {
    if (layer_art_matches(&border->art[i], style, exact, size, scale)) {
      slot = i;
      break;
    }
  }

  bool art_changed = slot < 0 || slot != border->current;
  bool size_changed = !border->has_size
                      || !CGSizeEqualToSize(border->size, size);
  if (!art_changed && !size_changed) return true;

  if (slot < 0) {
    if (!border->art[0].valid) slot = 0;
    else if (!border->art[1].valid) slot = 1;
    else slot = border->art[0].stamp <= border->art[1].stamp ? 0 : 1;

    if (!layer_art_build(&border->art[slot], style, exact, size, extent,
                         centre, scale                                  )) {
      debug("Layer borders: could not rasterise the artwork for window %u\n",
            border->wid                                                     );
      return false;
    }
  }

  struct layer_art* art = &border->art[slot];
  bool gradient = style->color.stype == COLOR_STYLE_GRADIENT;

  @autoreleasepool {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (art_changed) {
      border->base.contents = (id)art->base;
      border->base.contentsCenter = art->contents_center;
      border->base.hidden = art->base == NULL;

      if (gradient) {
        layer_ensure_gradient(border);
        border->gradient_mask.contents = (id)art->mask;
        border->gradient_mask.contentsCenter = art->contents_center;
        layer_gradient_apply(border->gradient, &style->color.gradient);
        border->gradient.hidden = NO;
      } else if (border->gradient) {
        border->gradient.hidden = YES;
        border->gradient_mask.contents = nil;
      }

      if (art->background) {
        layer_ensure_background(border);
        border->background.contents = (id)art->background;
        border->background.contentsCenter = art->contents_center;
        border->background.hidden = NO;
      } else if (border->background) {
        border->background.hidden = YES;
        border->background.contents = nil;
      }
    }

    CGRect bounds = CGRectMake(0.f, 0.f, size.width, size.height);
    border->root.bounds = bounds;
    border->base.bounds = bounds;
    if (border->gradient) {
      border->gradient.bounds = bounds;
      border->gradient_mask.bounds = bounds;
    }
    if (border->background) border->background.bounds = bounds;

    [CATransaction commit];
    [CATransaction flush];
  }

  border->size = size;
  border->has_size = true;
  border->current = slot;
  art->stamp = ++border->clock;
  return true;
}

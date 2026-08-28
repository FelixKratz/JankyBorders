#include "border.h"
#include "hashtable.h"
#include "layer.h"
#include "misc/extern.h"
#include "windows.h"
#include <pthread.h>
#include <time.h>

extern struct settings g_settings;

struct settings* border_get_settings(struct border* border) {
  assert(pthread_main_np() != 0);
  return border->setting_override.enabled
         ? &border->setting_override
         : &g_settings;
}

static void border_destroy_window(struct border* border) {
  if (border->layer) layer_border_destroy(border->layer);
  if (border->context) CGContextRelease(border->context);
  if (border->wid) SLSReleaseWindow(border->cid, border->wid);
  drawing_nine_slice_release(&border->nine_slice);
  border->layer = NULL;
  border->use_layer = false;
  border->wid = 0;
  border->context = NULL;
  border->tags_applied = false;
  border->applied_sticky = false;
}

static bool border_check_too_small(struct border* border, CGRect window_frame) {
  CGRect smallest_rect = CGRectInset(window_frame, 1.0, 1.0);
  if (smallest_rect.size.width < 2.f * border->inner_radius
      || smallest_rect.size.height < 2.f * border->inner_radius) {
    return true;
  }
  return false;
}

static bool border_calculate_bounds(struct border* border, CGRect* frame, struct settings* settings) {
  CGRect window_frame;
  if (border->is_proxy) window_frame = border->target_bounds;
  else SLSGetWindowBounds(border->cid, border->target_wid, &window_frame);

  border->target_bounds = window_frame;
  border->too_small = border_check_too_small(border, window_frame);
  if (border->too_small) {
    border_hide(border);
    return false;
  }

  float border_offset = - settings->border_width - BORDER_PADDING;
  *frame = CGRectInset(window_frame, border_offset, border_offset);

  border->origin = frame->origin;
  frame->origin = CGPointZero;


  window_frame.origin = (CGPoint){ -border_offset, -border_offset };
  border->drawing_bounds = window_frame;

  return true;
}

static void border_clear_ring(CGContextRef context, CGRect frame, float thickness) {
  if (frame.size.width <= 2.f * thickness
      || frame.size.height <= 2.f * thickness) {
    CGContextClearRect(context, frame);
    return;
  }

  float inner_height = frame.size.height - 2.f * thickness;
  CGRect strips[4] = {
    { { frame.origin.x, frame.origin.y },
      { frame.size.width, thickness } },
    { { frame.origin.x, CGRectGetMaxY(frame) - thickness },
      { frame.size.width, thickness } },
    { { frame.origin.x, frame.origin.y + thickness },
      { thickness, inner_height } },
    { { CGRectGetMaxX(frame) - thickness, frame.origin.y + thickness },
      { thickness, inner_height } },
  };

  for (int i = 0; i < 4; ++i) CGContextClearRect(context, strips[i]);
}

static float border_clear_thickness(struct border* border, struct settings* settings) {
  // Stroke, padding, corner cutouts and the glow shadow all lie within this
  // distance of the frame edge.
  return settings->border_width + BORDER_PADDING + border->inner_radius + 12.f;
}

static void border_clear(struct border* border, CGRect frame, struct settings* settings) {
  // A fresh backing store is transparent already; a full clear would only
  // commit all of its interior pages.
  bool fresh = border->fresh_surface;
  bool dirty = border->interior_painted;
  border->fresh_surface = false;
  border->interior_painted = false;

  if ((settings->show_background || dirty) && !fresh) {
    CGContextClearRect(border->context, frame);
    return;
  }

  border_clear_ring(border->context,
                    frame,
                    border_clear_thickness(border, settings));
}

static void border_draw_slow(struct border* border, CGRect frame, struct settings* settings) {
  CGContextSaveGState(border->context);
  border->needs_redraw = false;
  struct color_style color_style = border->focused
                                   ? settings->active_window
                                   : settings->inactive_window;

  CGGradientRef gradient = NULL;
  CGPoint gradient_dir[2];
  if (color_style.stype == COLOR_STYLE_SOLID
     || color_style.stype == COLOR_STYLE_GLOW) {
    bool glow = color_style.stype == COLOR_STYLE_GLOW;
    drawing_set_stroke_and_fill(border->context, color_style.color, glow);
  } else if (color_style.stype == COLOR_STYLE_GRADIENT) {
    CGAffineTransform trans = CGAffineTransformMakeScale(frame.size.width,
                                                         frame.size.height);
    gradient = drawing_create_gradient(&color_style.gradient,
                                       trans,
                                       gradient_dir          );
  }

  CGContextSetLineWidth(border->context, settings->border_width);

  border_clear(border, frame, settings);

  CGRect path_rect = border->drawing_bounds;
  CGMutablePathRef inner_clip_path = CGPathCreateMutable();
  if (settings->border_style == BORDER_STYLE_SQUARE
      && settings->border_order == BORDER_ORDER_ABOVE
      && settings->border_width >= BORDER_TSMW) {
    // Inset the frame to overlap the rounding of macOS windows to create a
    // truly square border
    path_rect = CGRectInset(border->drawing_bounds,
                            BORDER_TSMN,
                            BORDER_TSMN            );

    CGPathAddRect(inner_clip_path, NULL, path_rect);
  } else {
    CGPathAddRoundedRect(inner_clip_path,
                         NULL,
                         CGRectInset(path_rect, 1.0, 1.0),
                         border->inner_radius,
                         border->inner_radius             );
  }
  drawing_clip_between_rect_and_path(border->context, frame, inner_clip_path);

  if (settings->border_style == BORDER_STYLE_SQUARE) {
    if (color_style.stype == COLOR_STYLE_SOLID
       || color_style.stype == COLOR_STYLE_GLOW) {
      drawing_draw_square_with_inset(border->context,
                                     path_rect,
                                     -settings->border_width / 2.f);
    }
    else if (color_style.stype == COLOR_STYLE_GRADIENT) {
      drawing_draw_square_gradient_with_inset(border->context,
                                              gradient,
                                              gradient_dir,
                                              path_rect,
                                              -settings->border_width / 2.f);
    }
  } else {
    float corner_radius = settings->border_style == BORDER_STYLE_ROUND_UNIFORM ? 9.0 : border->radius;

    if (settings->border_style == BORDER_STYLE_ROUND_UNIFORM) {
      drawing_draw_rounded_rect_with_inset(border->context,
                                           path_rect,
                                           corner_radius,
                                           true            );
    }

    if (color_style.stype == COLOR_STYLE_SOLID
       || color_style.stype == COLOR_STYLE_GLOW) {
      drawing_draw_rounded_rect_with_inset(border->context,
                                           path_rect,
                                           corner_radius,
                                           false           );
    } else if (color_style.stype == COLOR_STYLE_GRADIENT) {
      drawing_draw_rounded_gradient_with_inset(border->context,
                                               gradient,
                                               gradient_dir,
                                               path_rect,
                                               corner_radius  );
    }
  }
  CGGradientRelease(gradient);

  if (settings->show_background && settings->border_order != 1) {
    CGContextRestoreGState(border->context);
    CGContextSaveGState(border->context);
    color_style = settings->background;
    border->interior_painted = true;
    if (color_style.stype == COLOR_STYLE_SOLID
       || color_style.stype == COLOR_STYLE_GLOW) {
      drawing_draw_filled_path(border->context,
                               inner_clip_path,
                               color_style.color);
    }
  }
  CFRelease(inner_clip_path);
  CGContextFlush(border->context);
  CGContextRestoreGState(border->context);
  SLSFlushWindowContentRegion(border->cid, border->wid, NULL);
  SLSWindowThaw(border->cid, border->wid);
}

static bool border_nine_slice_style(struct border* border, struct settings* settings, struct nine_slice* ns) {
  if (settings->show_background) return false;
  if (settings->border_style != BORDER_STYLE_ROUND
      && settings->border_style != BORDER_STYLE_ROUND_UNIFORM) return false;

  struct color_style color_style = border->focused
                                   ? settings->active_window
                                   : settings->inactive_window;
  if (color_style.stype != COLOR_STYLE_SOLID) return false;

  ns->color = color_style.color;
  ns->border_width = settings->border_width;
  ns->outer_offset = settings->border_width + BORDER_PADDING;
  ns->uniform = settings->border_style == BORDER_STYLE_ROUND_UNIFORM;
  ns->corner_radius = ns->uniform ? 9.f : border->radius;
  ns->inner_radius = border->inner_radius;
  ns->scale = settings->hidpi ? 2.f : 1.f;
  return true;
}

static bool border_nine_slice_fits(CGRect frame, const struct nine_slice* ns) {
  // The blits are only pixel exact on an integral pixel grid
  float width_px = frame.size.width * ns->scale;
  float height_px = frame.size.height * ns->scale;
  if (width_px != floorf(width_px) || height_px != floorf(height_px))
    return false;

  float extent = drawing_nine_slice_extent(ns);
  return frame.size.width >= 2.f * extent
      && frame.size.height >= 2.f * extent;
}

static void border_draw(struct border* border, CGRect frame, struct settings* settings) {
  struct nine_slice ns;
  if (!border_nine_slice_style(border, settings, &ns)) {
    drawing_nine_slice_release(&border->nine_slice);
    border_draw_slow(border, frame, settings);
    return;
  }

  if (!border_nine_slice_fits(frame, &ns)) {
    border_draw_slow(border, frame, settings);
    return;
  }

  if (!border->nine_slice.corner[0]
      || !drawing_nine_slice_equal(&border->nine_slice.key, &ns)) {
    if (!drawing_nine_slice_build(&border->nine_slice, &ns)) {
      border_draw_slow(border, frame, settings);
      return;
    }
  }

  CGContextSaveGState(border->context);
  border->needs_redraw = false;

  border_clear(border, frame, settings);
  drawing_draw_nine_slice(border->context, frame, &border->nine_slice);

  CGContextFlush(border->context);
  CGContextRestoreGState(border->context);
  SLSFlushWindowContentRegion(border->cid, border->wid, NULL);
  SLSWindowThaw(border->cid, border->wid);
}

static struct layer_style border_layer_style(struct border* border, struct settings* settings) {
  struct layer_style style = { 0 };
  style.color = border->focused ? settings->active_window
                                : settings->inactive_window;
  style.background = settings->background;
  style.show_background = settings->show_background
                          && settings->border_order != BORDER_ORDER_ABOVE;
  style.border_width = settings->border_width;
  style.outer_offset = settings->border_width + BORDER_PADDING;
  style.inner_radius = border->inner_radius;
  style.border_style = settings->border_style;
  style.corner_radius = settings->border_style == BORDER_STYLE_ROUND_UNIFORM
                        ? 9.f
                        : border->radius;
  style.truly_square = settings->border_style == BORDER_STYLE_SQUARE
                       && settings->border_order == BORDER_ORDER_ABOVE
                       && settings->border_width >= BORDER_TSMW;
  return style;
}

static bool border_layer_draw(struct border* border, CGRect frame, struct settings* settings) {
  struct layer_style style = border_layer_style(border, settings);
  if (!layer_border_update(border->layer, &style, frame.size)) {
    border->layer_failed = true;
    return false;
  }
  border->needs_redraw = false;
  return true;
}

void border_create_window(struct border* border, CGRect frame, bool unmanaged, bool hidpi) {
  pthread_mutex_lock(&border->mutex);
  int cid = border->cid;

  border->use_layer = false;
  border->layer = NULL;
  border->wid = 0;

  if (!border_get_settings(border)->force_cg
      && !border->layer_failed
      && !unmanaged
      && !border->is_proxy) {
    uint32_t layer_wid = 0;
    border->layer = layer_border_create(cid, frame, unmanaged, hidpi, &layer_wid);
    if (border->layer) {
      border->wid = layer_wid;
      border->use_layer = true;
    }
  }

  if (!border->use_layer) {
    debug("Drawing this border into a backing store\n");
    border->wid = window_create(cid, frame, hidpi, unmanaged);
  }

  border->frame = frame;
  border->needs_redraw = true;
  border->fresh_surface = true;

  if (!border->use_layer) {
    border->context = SLWindowContextCreate(cid, border->wid, NULL);
    CGContextSetInterpolationQuality(border->context, kCGInterpolationNone);
  }

  if (!border->sid) border->sid = window_space_id(cid, border->target_wid);
  window_send_to_space(cid, border->wid, border->sid);
  pthread_mutex_unlock(&border->mutex);
}

void border_update_internal(struct border* border, struct settings* settings) {
  if (border->external_proxy_wid) {
    border->stale_props = true;
    return;
  }

  int cid = border->cid;
  CGRect frame;
  if (!border_calculate_bounds(border, &frame, settings)) return;

  // The sticky tag is not cached, no event announces a change of it
  bool refetch_props = border->stale_props || !border->wid;

  uint64_t tags = window_tags(cid, border->target_wid);
  border->sticky = tags & WINDOW_TAG_STICKY;
  if (!border->sticky && !is_space_visible(cid, border->sid)) return;


  bool shown = false;
  SLSWindowIsOrderedIn(cid, border->target_wid, &shown);
  if (!shown && !border->is_proxy) {
    border_hide(border);
    return;
  } 

  if (refetch_props) {
    border->level = window_level(cid, border->target_wid);
    border->sub_level = window_sub_level(cid, border->target_wid);
    border->stale_props = false;
  }

  int level = border->level;
  int sub_level = border->sub_level;

  if (!border->wid) {
    border_create_window(border,
                         frame,
                         border->is_proxy,
                         settings->hidpi  );
  }

  bool disabled_update = false;
  if (!CGRectEqualToRect(frame, border->frame)) {
    disabled_update = true;
    SLSDisableUpdate(cid);

    CFTypeRef frame_region;
    CGSNewRegionWithRect(&frame, &frame_region);

    if (!border->use_layer)
      SLSWindowFreezeWithOptions(border->cid, border->wid, NULL);

    SLSSetWindowShape(border->cid, border->wid, border->origin.x, border->origin.y, frame_region);
    CFRelease(frame_region);

    border->fresh_surface = true;
    border->needs_redraw = true;
    border->frame = frame;
  }

  if (border->use_layer && !border_layer_draw(border, frame, settings)) {
    border_destroy_window(border);
    border_create_window(border, frame, border->is_proxy, settings->hidpi);
  }

  if (!border->use_layer && border->needs_redraw) {
    border_draw(border, frame, settings);
  }

  CFTypeRef transaction = SLSTransactionCreate(cid);
  if (!transaction) {
    if (disabled_update) SLSReenableUpdate(cid);
    return;
  }
  SLSTransactionMoveWindowWithGroup(transaction, border->wid, border->origin);

  if (!border->is_proxy) {
    CGAffineTransform transform = CGAffineTransformIdentity;
    transform.tx = -border->origin.x;
    transform.ty = -border->origin.y;
    SLSTransactionSetWindowTransform(transaction,
                                     border->wid,
                                     0,
                                     0,
                                     transform   );
  }
  SLSTransactionSetWindowLevel(transaction, border->wid, level);
  SLSTransactionSetWindowSubLevel(transaction, border->wid, sub_level);
  SLSTransactionOrderWindow(transaction,
                            border->wid,
                            settings->border_order,
                            border->target_wid      );
  SLSTransactionCommit(transaction, 0);
  CFRelease(transaction);

  if (!border->tags_applied
      || refetch_props
      || border->applied_sticky != border->sticky) {
    uint64_t set_tags = (1ULL << 1) | (1ULL << 9);
    uint64_t clear_tags = 0;

    if (border->sticky) {
      set_tags |= WINDOW_TAG_STICKY;
      clear_tags |= (1ULL << 45);
    }

    SLSSetWindowTags(cid, border->wid, &set_tags, 0x40);
    SLSClearWindowTags(cid, border->wid, &clear_tags, 0x40);

    border->tags_applied = true;
    border->applied_sticky = border->sticky;
  }

  if (disabled_update) SLSReenableUpdate(cid);
}

void border_init(struct border* border, int cid) {
  memset(border, 0, sizeof(struct border));
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&border->mutex, &mattr);
  animation_init(&border->animation);
  border->stale_props = true;
  if (cid) border->cid = cid;
  else border->cid = SLSMainConnectionID();
}

struct border* border_create() {
  struct border* border = malloc(sizeof(struct border));
  int cid = 0;
  SLSNewConnection(0, &cid);
  border_init(border, cid);
  return border;
}

void border_destroy(struct border* border) {
  border_hide(border);
  dispatch_async(dispatch_get_main_queue(), ^{
    pthread_mutex_lock(&border->mutex);
    border_destroy_window(border);
    if (border->proxy) border_destroy(border->proxy);
    animation_stop(&border->animation);
    if (!border->is_proxy && border->cid != SLSMainConnectionID())
      SLSReleaseConnection(border->cid);
    pthread_mutex_unlock(&border->mutex);
    free(border);
  });
}

void border_move(struct border* border) {
  pthread_mutex_lock(&border->mutex);
  if (border->external_proxy_wid) {
    pthread_mutex_unlock(&border->mutex);
    return;
  }
  pthread_mutex_unlock(&border->mutex);

  struct settings* settings = border_get_settings(border);
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
    pthread_mutex_lock(&border->mutex);
    CGRect window_frame;
    SLSGetWindowBounds(border->cid, border->target_wid, &window_frame);
    CGPoint origin = { .x = window_frame.origin.x
                            - settings->border_width
                            - BORDER_PADDING,
                       .y = window_frame.origin.y
                            - settings->border_width
                            - BORDER_PADDING          };

    CFTypeRef transaction = SLSTransactionCreate(border->cid);
    if (transaction) {
      SLSTransactionMoveWindowWithGroup(transaction, border->wid, origin);
      SLSTransactionCommit(transaction, 0);
      CFRelease(transaction);
    }
    border->target_bounds = window_frame;
    border->origin = origin;
    pthread_mutex_unlock(&border->mutex);
  });
}

void border_update(struct border* border, bool try_async) {
  pthread_mutex_lock(&border->mutex);
  struct settings* settings = border_get_settings(border);
  border_update_internal(border, settings);
  pthread_mutex_unlock(&border->mutex);
}

void border_invalidate_props(struct border* border) {
  pthread_mutex_lock(&border->mutex);
  border->stale_props = true;
  pthread_mutex_unlock(&border->mutex);
}

void border_hide(struct border* border) {
  pthread_mutex_lock(&border->mutex);
  if (border->wid) {
    CFTypeRef transaction = SLSTransactionCreate(border->cid);
    if (transaction) {
      SLSTransactionOrderWindow(transaction,
                                border->wid,
                                0,
                                border->target_wid);
      SLSTransactionCommit(transaction, 0);
      CFRelease(transaction);
    }
  }
  pthread_mutex_unlock(&border->mutex);
}

void border_unhide(struct border* border) {
  pthread_mutex_lock(&border->mutex);
  if (border->too_small
      || border->external_proxy_wid
      || (!border->sticky && !is_space_visible(border->cid, border->sid))) {
    pthread_mutex_unlock(&border->mutex);
    return;
  }

  if (border->wid) {
    struct settings* settings = border_get_settings(border);
    CFTypeRef transaction = SLSTransactionCreate(border->cid);
    if (transaction) {
      SLSTransactionOrderWindow(transaction,
                                border->wid,
                                settings->border_order,
                                border->target_wid      );
      SLSTransactionCommit(transaction, 0);
      CFRelease(transaction);
    }
  }
  pthread_mutex_unlock(&border->mutex);
}

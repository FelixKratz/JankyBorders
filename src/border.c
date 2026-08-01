#include "border.h"
#include "hashtable.h"
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
  if (border->context) CGContextRelease(border->context);
  if (border->wid) SLSReleaseWindow(border->cid, border->wid);
  border->wid = 0;
  border->context = NULL;
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

static void border_add_gradient_glow_path(CGContextRef context,
                                          CGRect path_rect,
                                          float inset,
                                          float corner_radius,
                                          bool square) {
  if (square) drawing_add_rect_with_inset(context, path_rect, inset);
  else drawing_add_rounded_rect(context, path_rect, corner_radius);
}

static void border_draw_gradient_glow(CGContextRef context,
                                      const struct gradient* gradient,
                                      CGRect path_rect,
                                      float inset,
                                      float corner_radius,
                                      float blur_radius,
                                      bool square) {
  float a, r, g, b;
  colors_mix(gradient->color1, gradient->color2, &a, &r, &g, &b);
  CGColorRef glow_color = CGColorCreateGenericRGB(r, g, b, a);

  CGContextSaveGState(context);
  CGContextSetShadowWithColor(context, CGSizeZero, blur_radius, glow_color);
  CGColorRelease(glow_color);
  CGContextSetRGBFillColor(context, 1.0f, 1.0f, 1.0f, 1.0f);
  CGContextSetRGBStrokeColor(context, 1.0f, 1.0f, 1.0f, 1.0f);
  border_add_gradient_glow_path(context,
                                path_rect,
                                inset,
                                corner_radius,
                                square       );
  if (square) CGContextFillPath(context);
  else CGContextStrokePath(context);

  CGContextSetShadowWithColor(context, CGSizeZero, 0, NULL);
  CGContextSetBlendMode(context, kCGBlendModeDestinationOut);
  border_add_gradient_glow_path(context,
                                path_rect,
                                inset,
                                corner_radius,
                                square       );
  if (square) CGContextFillPath(context);
  else CGContextStrokePath(context);
  CGContextRestoreGState(context);
}

static void border_draw(struct border* border, CGRect frame, struct settings* settings) {
  CGContextSaveGState(border->context);
  border->needs_redraw = false;
  struct color_style color_style = border->focused
                                   ? settings->active_window
                                   : settings->inactive_window;

  CGGradientRef gradient = NULL;
  CGPoint gradient_dir[2];
  if (color_style.stype == COLOR_STYLE_SOLID) {
    drawing_set_stroke_and_fill(border->context, color_style.color, color_style.glow);
  } else if (color_style.stype == COLOR_STYLE_GRADIENT) {
    CGAffineTransform trans = CGAffineTransformMakeScale(frame.size.width,
                                                         frame.size.height);
    gradient = drawing_create_gradient(&color_style.gradient,
                                       trans,
                                       gradient_dir          );
  }

  CGContextSetLineWidth(border->context, settings->border_width);
  CGContextClearRect(border->context, frame);

  CGRect path_rect = border->drawing_bounds;
  CGMutablePathRef inner_clip_path = CGPathCreateMutable();
  bool square_thick_above = settings->border_style == BORDER_STYLE_SQUARE
                            && settings->border_order == BORDER_ORDER_ABOVE
                            && settings->border_width >= BORDER_TSMW;
  if (square_thick_above) {
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

  bool square = settings->border_style == BORDER_STYLE_SQUARE;
  float inset = -settings->border_width / 2.f;
  float corner_radius = settings->border_style == BORDER_STYLE_ROUND_UNIFORM
                        ? 9.0
                        : border->radius;

  if (settings->border_style == BORDER_STYLE_ROUND_UNIFORM) {
    drawing_draw_rounded_rect_with_inset(border->context,
                                         path_rect,
                                         corner_radius,
                                         true            );
  }

  if (color_style.stype == COLOR_STYLE_SOLID) {
    if (square) {
      drawing_draw_square_with_inset(border->context,
                                     path_rect,
                                     inset    );
    } else {
      drawing_draw_rounded_rect_with_inset(border->context,
                                           path_rect,
                                           corner_radius,
                                           false           );
    }
  } else if (color_style.stype == COLOR_STYLE_GRADIENT) {
    if (color_style.glow) {
      float blur_radius = square_thick_above ? BORDER_TSMN : 10.0f;
      border_draw_gradient_glow(border->context,
                                &color_style.gradient,
                                path_rect,
                                inset,
                                corner_radius,
                                blur_radius,
                                square               );
    }

    CGContextSaveGState(border->context);
    if (square) {
      drawing_draw_square_gradient_with_inset(border->context,
                                              gradient,
                                              gradient_dir,
                                              path_rect,
                                              inset       );
    } else {
      drawing_draw_rounded_gradient_with_inset(border->context,
                                               gradient,
                                               gradient_dir,
                                               path_rect,
                                               corner_radius  );
    }
    CGContextRestoreGState(border->context);
  }
  if (gradient) CGGradientRelease(gradient);

  if (settings->show_background && settings->border_order != 1) {
    CGContextRestoreGState(border->context);
    CGContextSaveGState(border->context);
    color_style = settings->background;
    if (color_style.stype == COLOR_STYLE_SOLID) {
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

void border_create_window(struct border* border, CGRect frame, bool unmanaged, bool hidpi) {
  pthread_mutex_lock(&border->mutex);
  int cid = border->cid;
  border->wid = window_create(cid, frame, hidpi, unmanaged);

  border->frame = frame;
  border->needs_redraw = true;
  border->context = SLWindowContextCreate(cid, border->wid, NULL);
  CGContextSetInterpolationQuality(border->context, kCGInterpolationNone);

  if (!border->sid) border->sid = window_space_id(cid, border->target_wid);
  window_send_to_space(cid, border->wid, border->sid);
  pthread_mutex_unlock(&border->mutex);
}

void border_update_internal(struct border* border, struct settings* settings) {
  if (border->external_proxy_wid) return;

  int cid = border->cid;
  CGRect frame;
  if (!border_calculate_bounds(border, &frame, settings)) return;

  uint64_t tags = window_tags(cid, border->target_wid);
  border->sticky = tags & WINDOW_TAG_STICKY;
  if (!border->sticky && !is_space_visible(cid, border->sid)) return;


  bool shown = false;
  SLSWindowIsOrderedIn(cid, border->target_wid, &shown);
  if (!shown && !border->is_proxy) {
    border_hide(border);
    return;
  } 

  int level = window_level(cid, border->target_wid);
  int sub_level = window_sub_level(cid, border->target_wid);

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

    SLSWindowFreezeWithOptions(border->cid, border->wid, NULL);
    SLSSetWindowShape(border->cid, border->wid, border->origin.x, border->origin.y, frame_region);
    CFRelease(frame_region);

    border->needs_redraw = true;
    border->frame = frame;
  }

  if (border->needs_redraw) border_draw(border, frame, settings);

  CFTypeRef transaction = SLSTransactionCreate(cid);
  if(!transaction) return;
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

  uint64_t set_tags = (1ULL << 1) | (1ULL << 9);
  uint64_t clear_tags = 0;

  if (border->sticky) {
    set_tags |= WINDOW_TAG_STICKY;
    clear_tags |= (1ULL << 45);
  }

  SLSSetWindowTags(cid, border->wid, &set_tags, 0x40);
  SLSClearWindowTags(cid, border->wid, &clear_tags, 0x40);

  if (disabled_update) SLSReenableUpdate(cid);
}

static void* border_update_async_proc(void* context) {
  struct {
    struct border* border;
    struct settings settings;
  }* payload = context;

  pthread_mutex_lock(&payload->border->mutex);
  border_update_internal(payload->border, &payload->settings);
  pthread_mutex_unlock(&payload->border->mutex);
  free(payload);
  return NULL;
}

void border_init(struct border* border, int cid) {
  memset(border, 0, sizeof(struct border));
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&border->mutex, &mattr);
  animation_init(&border->animation);
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
  return;

  if (!border->wid || !try_async) {
    border_update_internal(border, settings);
    pthread_mutex_unlock(&border->mutex);
    return;
  }

  struct payload {
    struct border* border;
    struct settings settings;
  }* payload = malloc(sizeof(struct payload));

  payload->border = border;
  payload->settings = *settings;

  pthread_t thread;
  pthread_create(&thread, NULL, border_update_async_proc, payload);
  pthread_detach(thread);
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

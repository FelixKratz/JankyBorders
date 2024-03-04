#include "border.h"
#include "hashtable.h"
#include "windows.h"
#include <pthread.h>

extern struct settings g_settings;

void border_init(struct border* border) {
  memset(border, 0, sizeof(struct border));
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&border->mutex, &mattr);
}

static void border_destroy_window(struct border* border, int cid) {
  if (border->wid) SLSReleaseWindow(cid, border->wid);
  if (border->context) CGContextRelease(border->context);
  border->wid = 0;
  border->context = NULL;
}

static bool border_check_too_small(struct border* border, CGRect window_frame) {
  CGRect smallest_rect = CGRectInset(window_frame, 1.0, 1.0);
  if (smallest_rect.size.width < 2.f * BORDER_INNER_RADIUS
      || smallest_rect.size.height < 2.f * BORDER_INNER_RADIUS) {
    return true;
  }
  return false;
}

static bool border_calculate_bounds(struct border* border, int cid, CGRect* frame) {
  CGRect window_frame;
  if (border->is_proxy) window_frame = border->target_bounds;
  else SLSGetWindowBounds(cid, border->target_wid, &window_frame);
  border->target_bounds = window_frame;

  border->too_small = border_check_too_small(border, window_frame);
  if (border->too_small) {
    border_hide(border, cid);
    return false;
  }

  float border_offset = - g_settings.border_width - BORDER_PADDING;
  *frame = CGRectInset(window_frame, border_offset, border_offset);

  border->origin = frame->origin;
  frame->origin = CGPointZero;
  window_frame.origin = (CGPoint){ -border_offset, -border_offset };
  border->drawing_bounds = window_frame;
  return true;
}

static void border_draw(struct border* border, CGRect frame) {
  CGContextSaveGState(border->context);
  border->needs_redraw = false;
  struct color_style color_style = border->focused
                                   ? g_settings.active_window
                                   : g_settings.inactive_window;

  CGGradientRef gradient = NULL;
  CGPoint gradient_direction[2];
  if (color_style.stype == COLOR_STYLE_SOLID
     || color_style.stype == COLOR_STYLE_GLOW) {
    float a,r,g,b;
    colors_from_hex(color_style.color, &a, &r, &g, &b);
    CGContextSetRGBFillColor(border->context, r, g, b, a);
    CGContextSetRGBStrokeColor(border->context, r, g, b, a);

    if (color_style.stype == COLOR_STYLE_GLOW) {
      CGColorRef color_ref = CGColorCreateGenericRGB(r, g, b, 1.0);
      CGContextSetShadowWithColor(border->context, CGSizeZero, 10.0, color_ref);
      CGColorRelease(color_ref);
    }
  } else if (color_style.stype == COLOR_STYLE_GRADIENT) {
    float a1, a2, r1, r2, g1, g2, b1, b2;
    colors_from_hex(color_style.gradient.color1, &a1, &r1, &g1, &b1);
    colors_from_hex(color_style.gradient.color2, &a2, &r2, &g2, &b2);
    CGColorRef c[] = { CGColorCreateSRGB(r1, g1, b1, a1),
                       CGColorCreateSRGB(r2, g2, b2, a2) };
    CFArrayRef cfc = CFArrayCreate(NULL,
                                   (const void **)c,
                                   2,
                                   &kCFTypeArrayCallBacks);
    gradient = CGGradientCreateWithColors(NULL, cfc, NULL);
    CFRelease(cfc);
    CGColorRelease(c[0]);
    CGColorRelease(c[1]);
    if (color_style.gradient.direction == TR_TO_BL) {
      gradient_direction[0] = CGPointMake(frame.size.width,
                                          frame.size.height);
      gradient_direction[1] = CGPointZero;
    } else if (color_style.gradient.direction == TL_TO_BR) {
      gradient_direction[0] = CGPointMake(0, frame.size.height);
      gradient_direction[1] = CGPointMake(frame.size.width, 0);
    }
  }

  CGContextSetLineWidth(border->context, g_settings.border_width);
  CGContextClearRect(border->context, frame);

  CGRect path_rect = border->drawing_bounds;
  CGMutablePathRef clip_path = CGPathCreateMutable();
  CGMutablePathRef inner_clip_path = CGPathCreateMutable();
  CGPathAddRect(clip_path, NULL, frame);

  if (g_settings.border_style == BORDER_STYLE_SQUARE
      && g_settings.border_order == BORDER_ORDER_ABOVE
      && g_settings.border_width >= BORDER_TSMW) {
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
                         BORDER_INNER_RADIUS,
                         BORDER_INNER_RADIUS              );
  }

  CGPathAddPath(clip_path, NULL, inner_clip_path);
  CGContextAddPath(border->context, clip_path);
  CGContextEOClip(border->context);

  if (g_settings.border_style == BORDER_STYLE_SQUARE) {
    CGRect square_rect = CGRectInset(path_rect,
                                     -g_settings.border_width / 2.f,
                                     -g_settings.border_width / 2.f );

    CGPathRef square_path = CGPathCreateWithRect(square_rect, NULL);
    CGContextAddPath(border->context, square_path);

    if (color_style.stype == COLOR_STYLE_SOLID
       || color_style.stype == COLOR_STYLE_GLOW) {
      CGContextFillPath(border->context);
    }
    else if (color_style.stype == COLOR_STYLE_GRADIENT) {
      CGContextClip(border->context);
      CGContextDrawLinearGradient(border->context,
                                  gradient,
                                  gradient_direction[0],
                                  gradient_direction[1],
                                  0                     );
    }
    CFRelease(square_path);
  } else {
    CGPathRef stroke_path = CGPathCreateWithRoundedRect(path_rect,
                                                        BORDER_RADIUS,
                                                        BORDER_RADIUS,
                                                        NULL          );

    CGContextAddPath(border->context, stroke_path);

    if (color_style.stype == COLOR_STYLE_SOLID
       || color_style.stype == COLOR_STYLE_GLOW) {
      CGContextStrokePath(border->context);
    } else if (color_style.stype == COLOR_STYLE_GRADIENT) {
      CGContextReplacePathWithStrokedPath(border->context);
      CGContextClip(border->context);
      CGContextDrawLinearGradient(border->context,
                                  gradient,
                                  gradient_direction[0],
                                  gradient_direction[1],
                                  0                     );
    }

    CFRelease(stroke_path);
  }
  CGGradientRelease(gradient);

  if (g_settings.show_background && g_settings.border_order != 1) {
    CGContextRestoreGState(border->context);
    CGContextSaveGState(border->context);
    color_style = g_settings.background;
    if (color_style.stype == COLOR_STYLE_SOLID
       || color_style.stype == COLOR_STYLE_GLOW) {
      float a,r,g,b;
      colors_from_hex(color_style.color, &a, &r, &g, &b);
      CGContextSetRGBFillColor(border->context, r, g, b, a);
      CGContextSetRGBStrokeColor(border->context, 0, 0, 0, 0);

      CGContextAddPath(border->context, inner_clip_path);
      CGContextFillPath(border->context);
    }
  }
  CFRelease(clip_path);
  CFRelease(inner_clip_path);
  CGContextFlush(border->context);
  CGContextRestoreGState(border->context);
}

static void border_update_internal(struct border* border, int cid) {
  pthread_mutex_lock(&border->mutex);
  if (border->proxy_wid) {
    pthread_mutex_unlock(&border->mutex);
    return;
  } 

  uint64_t tags = window_tags(cid, border->target_wid);
  border->sticky = tags & WINDOW_TAG_STICKY;
  if (!border->sticky && !is_space_visible(cid, border->sid)) {
    pthread_mutex_unlock(&border->mutex);
    return;
  }


  bool shown = false;
  SLSWindowIsOrderedIn(cid, border->target_wid, &shown);
  if (!shown && !border->is_proxy) {
    border_hide(border, cid);
    pthread_mutex_unlock(&border->mutex);
    return;
  } 

  CGRect frame;
  if (!border_calculate_bounds(border, cid, &frame)) {
    pthread_mutex_unlock(&border->mutex);
    return;
  }
  int level = window_level(cid, border->target_wid);
  int sub_level = window_sub_level(border->target_wid);

  SLSDisableUpdate(cid);
  if (!border->wid) border_create_window(border, cid, frame, border->is_proxy);

  if (!CGRectEqualToRect(frame, border->frame)) {
    CFTypeRef frame_region;
    CGSNewRegionWithRect(&frame, &frame_region);
    SLSSetWindowShape(cid, border->wid, -9999, -9999, frame_region);
    CFRelease(frame_region);

    border->needs_redraw = true;
    border->frame = frame;
  }

  if (border->needs_redraw) border_draw(border, frame);

  SLSMoveWindow(cid, border->wid, &border->origin);
  CGAffineTransform transform = CGAffineTransformIdentity;
  transform.tx = -border->origin.x;
  transform.ty = -border->origin.y;
  SLSSetWindowTransform(cid, border->wid, transform);
  SLSSetWindowLevel(cid, border->wid, level);
  SLSSetWindowSubLevel(cid, border->wid, sub_level);
  SLSOrderWindow(cid,
                 border->wid,
                 g_settings.border_order,
                 border->target_wid      );
  SLSSetWindowBackgroundBlurRadius(cid, border->wid, g_settings.blur_radius);

  uint64_t set_tags = (1ULL << 1) | (1ULL << 9);
  uint64_t clear_tags = 0;

  if (border->sticky) {
    set_tags |= WINDOW_TAG_STICKY;
    clear_tags |= (1ULL << 45);
  }

  SLSSetWindowTags(cid, border->wid, &set_tags, 0x40);
  SLSClearWindowTags(cid, border->wid, &clear_tags, 0x40);

  SLSReenableUpdate(cid);
  pthread_mutex_unlock(&border->mutex);
}

static void* border_update_async_proc(void* context) {
  struct { struct border* border; int cid; }* payload = context;
  border_update_internal(payload->border, payload->cid);
  free(payload);
  return NULL;
}

void border_create_window(struct border* border, int cid, CGRect frame, bool unmanaged) {
  pthread_mutex_lock(&border->mutex);
  border->wid = window_create(cid,
                              frame,
                              g_settings.hidpi,
                              unmanaged        );

  border->frame = frame;
  border->needs_redraw = true;
  border->context = SLWindowContextCreate(cid, border->wid, NULL);
  CGContextSetInterpolationQuality(border->context, kCGInterpolationNone);

  if (!border->sid) border->sid = window_space_id(cid, border->target_wid);
  window_send_to_space(cid, border->wid, border->sid);
  pthread_mutex_unlock(&border->mutex);
}

void border_destroy(struct border* border, int cid) {
  pthread_mutex_lock(&border->mutex);
  border_destroy_window(border, cid);
  if (border->proxy) border_destroy(border->proxy, cid);
  pthread_mutex_unlock(&border->mutex);
  free(border);
}

void border_move(struct border* border, int cid) {
  pthread_mutex_lock(&border->mutex);
  if (border->proxy_wid) {
    pthread_mutex_unlock(&border->mutex);
    return;
  }
  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);

  CGPoint origin = { .x = window_frame.origin.x
                          - g_settings.border_width
                          - BORDER_PADDING,
                     .y = window_frame.origin.y
                          - g_settings.border_width
                          - BORDER_PADDING          };

  SLSMoveWindow(cid, border->wid, &origin);
  border->target_bounds = window_frame;
  border->origin = origin;
  pthread_mutex_unlock(&border->mutex);
}

void border_update(struct border* border, int cid, bool try_async) {
  pthread_mutex_lock(&border->mutex);
  if (!border->wid || !try_async) {
    border_update_internal(border, cid);
    pthread_mutex_unlock(&border->mutex);
    return;
  }

  struct payload { struct border* border; int cid; }* payload
                                            = malloc(sizeof(struct payload));
  payload->border = border;
  payload->cid = cid;
  pthread_t thread;
  pthread_create(&thread, NULL, border_update_async_proc, payload);
  pthread_detach(thread);
  pthread_mutex_unlock(&border->mutex);
}

void border_hide(struct border* border, int cid) {
  pthread_mutex_lock(&border->mutex);
  if (border->wid) {
    SLSOrderWindow(cid, border->wid, 0, border->target_wid);
  }
  pthread_mutex_unlock(&border->mutex);
}

void border_unhide(struct border* border, int cid) {
  pthread_mutex_lock(&border->mutex);
  if (border->too_small
      || border->proxy_wid
      || (!border->sticky && !is_space_visible(cid, border->sid))) {
    pthread_mutex_unlock(&border->mutex);
    return;
  }

  if (border->wid) {
    SLSOrderWindow(cid,
                   border->wid,
                   g_settings.border_order,
                   border->target_wid      );

  } else border_update(border, cid, false);
  pthread_mutex_unlock(&border->mutex);
}

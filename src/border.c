#include "border.h"
#include "hashtable.h"
#include "windows.h"

extern struct settings g_settings;
void border_init(struct border* border) {
  memset(border, 0, sizeof(struct border));
}

void border_destroy(struct border* border) {
  SLSReleaseWindow(SLSMainConnectionID(), border->wid);
  if (border->context) CGContextRelease(border->context);
  free(border);
}

void border_move(struct border* border) {
  int cid = SLSMainConnectionID();

  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);
  CGRect frame = CGRectInset(window_frame,
                             -g_settings.border_width,
                             -g_settings.border_width );

  SLSMoveWindow(cid, border->wid, &frame.origin);
}

void border_draw(struct border* border) {
  static const float border_radius = 9.f;
  static const float inner_border_radius = 12.f;

  int cid = SLSMainConnectionID();
  if (!is_space_visible(cid, border->sid)) return;

  bool shown = false;
  SLSWindowIsOrderedIn(cid, border->target_wid, &shown);
  if (!shown) {
    border_hide(border);
    return;
  } 

  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);
  CGRect smallest_rect = CGRectInset(window_frame, 1.0, 1.0);
  if (smallest_rect.size.width < 2.f * inner_border_radius
      || smallest_rect.size.height < 2.f * inner_border_radius) {
    border->disable = true;
    border_hide(border);
    return;
  }
  border->disable = false;

  CGRect frame = CGRectInset(window_frame,
                             -g_settings.border_width,
                             -g_settings.border_width );
  CGPoint origin = frame.origin;
  frame.origin = CGPointZero;

  SLSDisableUpdate(cid);
  if (!border->wid) {
    border->wid = window_create(cid, frame);
    border->bounds = frame;
    border->needs_redraw = true;
    border->context = SLWindowContextCreate(cid, border->wid, NULL);
    CGContextSetInterpolationQuality(border->context, kCGInterpolationNone);

    if (!border->sid) border->sid = window_space_id(cid, border->target_wid);
    window_send_to_space(cid, border->wid, border->sid);
  }

  if (!CGRectEqualToRect(frame, border->bounds)) {
    CFTypeRef frame_region;
    CGSNewRegionWithRect(&frame, &frame_region);
    SLSSetWindowShape(cid, border->wid, -9999, -9999, frame_region);
    CFRelease(frame_region);

    border->needs_redraw = true;
    border->bounds = frame;
  }

  int level = window_level(cid, border->target_wid);
  int sub_level = 0;
  SLSGetWindowSubLevel(cid, border->target_wid, &sub_level);

  if (border->needs_redraw) {
    border->needs_redraw = false;
    uint32_t color = border->focused
                     ? g_settings.active_window_color
                     : g_settings.inactive_window_color;

    float r = ((color >> 16) & 0xff) / 255.f;
    float g = ((color >> 8) & 0xff) / 255.f;
    float b = ((color >> 0) & 0xff) / 255.f;
    float a = ((color >> 24) & 0xff) / 255.f;

    CGContextSetRGBStrokeColor(border->context, r, g, b, a);
    CGContextSetRGBFillColor(border->context, r, g, b, a);
    CGContextSetLineWidth(border->context, g_settings.border_width);
    CGContextClearRect(border->context, frame);
    CGContextSaveGState(border->context);

    CGRect path_rect = CGRectInset(frame,
                                   g_settings.border_width,
                                   g_settings.border_width );

    CGMutablePathRef clip_path = CGPathCreateMutable();
    CGPathAddRect(clip_path, NULL, frame);
    CGPathAddRoundedRect(clip_path,
                         NULL,
                         CGRectInset(path_rect, 1.0, 1.0),
                         inner_border_radius,
                         inner_border_radius              );

    CGContextAddPath(border->context, clip_path);
    CGContextEOClip(border->context);

    if (g_settings.border_style == BORDER_STYLE_SQUARE) {
      CGRect square_rect = CGRectInset(path_rect,
                                       -g_settings.border_width / 2.f,
                                       -g_settings.border_width / 2.f );

      CGPathRef square_path = CGPathCreateWithRect(square_rect, NULL);
      CGContextAddPath(border->context, square_path);
      CGContextFillPath(border->context);
      CFRelease(square_path);
    } else {
      CGPathRef stroke_path = CGPathCreateWithRoundedRect(path_rect,
                                                          border_radius,
                                                          border_radius,
                                                          NULL          );

      CGContextAddPath(border->context, stroke_path);
      CGContextStrokePath(border->context);
      CFRelease(stroke_path);
    }
    CFRelease(clip_path);
  
    CGContextFlush(border->context);
    CGContextRestoreGState(border->context);
  }

  SLSMoveWindow(cid, border->wid, &origin);
  SLSSetWindowLevel(cid, border->wid, level);
  SLSSetWindowSubLevel(cid, border->wid, sub_level);
  SLSOrderWindow(cid, border->wid, -1, border->target_wid);
  SLSReenableUpdate(cid);
}

void border_hide(struct border* border) {
  if (border->wid) {
    SLSOrderWindow(SLSMainConnectionID(), border->wid, 0, border->target_wid);
  }
}

void border_unhide(struct border* border) {
  if (border->disable) return;
  if (border->wid) {
    SLSOrderWindow(SLSMainConnectionID(), border->wid, -1, border->target_wid);
  }
  else border_draw(border);
}

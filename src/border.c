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
  CFTypeRef transaction = SLSTransactionCreate(cid);

  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);
  CGRect frame = CGRectInset(window_frame,
                             -g_settings.border_width,
                             -g_settings.border_width );
  SLSTransactionMoveWindowWithGroup(transaction, border->wid, frame.origin);

  SLSTransactionCommit(transaction, true);
  border->origin = window_frame.origin;

  CFRelease(transaction);
}

void border_draw(struct border* border) {
  float border_radius = 9.f;

  int cid = SLSMainConnectionID();
  if (!is_space_visible(cid, border->sid)) return;

  bool shown = false;
  SLSWindowIsOrderedIn(cid, border->target_wid, &shown);
  if (!shown && border->wid) {
    SLSReleaseWindow(cid, border->wid);
    border->wid = 0;
    CGContextRelease(border->context);
    border->context = NULL;
  }
  if (!shown) return;

  int level = window_level(cid, border->target_wid);
  int sub_level = 0;
  SLSGetWindowSubLevel(cid, border->target_wid, &sub_level);

  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);
  CGRect frame = CGRectInset(window_frame,
                             -g_settings.border_width,
                             -g_settings.border_width );

  CGPoint origin = frame.origin;
  frame.origin = CGPointZero;

  SLSDisableUpdate(cid);
  if (!border->wid) {
    CFTypeRef frame_region;
    CGSNewRegionWithRect(&frame, &frame_region);

    uint64_t set_tags = 1ULL <<  1;
    uint64_t clear_tags = 0;

    uint64_t id;
    SLSNewWindow(cid,
                 kCGBackingStoreBuffered,
                 -9999,
                 -9999,
                 frame_region,
                 &id                     );

    border->wid = (uint32_t)id;

    SLSSetWindowResolution(cid, border->wid, 1.0f);
    SLSSetWindowTags(cid, border->wid, &set_tags, 64);
    SLSClearWindowTags(cid, border->wid, &clear_tags, 64);
    SLSSetWindowOpacity(cid, border->wid, 0);

    border->context = SLWindowContextCreate(cid, border->wid, NULL);
    CGContextSetInterpolationQuality(border->context, kCGInterpolationNone);
    border->bounds = frame;
    border->origin = origin;
    border->needs_redraw = true;
    CFRelease(frame_region);

    if (!border->sid) border->sid = window_space_id(cid, border->target_wid);
    window_send_to_space(cid, border->wid, border->sid);
  }

  if (!CGRectEqualToRect(frame, border->bounds)) {
    CFTypeRef frame_region;
    CGSNewRegionWithRect(&frame, &frame_region);

    border->bounds = frame;
    SLSSetWindowShape(cid, border->wid, -9999, -9999, frame_region);
    border->needs_redraw = true;
    CFRelease(frame_region);
  }

  SLSSetWindowLevel(cid, border->wid, level);
  SLSSetWindowSubLevel(cid, border->wid, sub_level);
  SLSOrderWindow(cid, border->wid, -1, border->target_wid);

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

    if (g_settings.border_style == BORDER_STYLE_SQUARE) {
      CGRect square_rect = CGRectInset(frame,
                                       g_settings.border_width / 2.f,
                                       g_settings.border_width / 2.f );

      CGPathRef square = CGPathCreateWithRect(square_rect, NULL);
      CGContextAddPath(border->context, square);
      CGContextFillPath(border->context);

      CGRect clip_rect = CGRectInset(frame,
                                     2.f*g_settings.border_width,
                                     2.f*g_settings.border_width );

      CGPathRef clip = CGPathCreateWithRoundedRect(clip_rect,
                                                   border_radius,
                                                   border_radius,
                                                   NULL          );

      CGContextSetBlendMode(border->context, kCGBlendModeDestinationOut);
      CGContextSetRGBFillColor(border->context, 0.f, 0.f, 0.f, 1.f);
      CGContextAddPath(border->context, clip);
      CGContextDrawPath(border->context, kCGPathFillStroke);
      CGContextSetBlendMode(border->context, kCGBlendModeNormal);
      CFRelease(clip);
    } else {
      CGRect path_rect = CGRectInset(frame, g_settings.border_width, g_settings.border_width);
      CGPathRef path = CGPathCreateWithRoundedRect(path_rect,
                                                   border_radius,
                                                   border_radius,
                                                   NULL          );

      CGContextAddPath(border->context, path);
      CGContextStrokePath(border->context);
      CFRelease(path);
    }
  
    CGContextFlush(border->context);
  }

  CFTypeRef transaction = SLSTransactionCreate(cid);
  SLSTransactionMoveWindowWithGroup(transaction, border->wid, origin);
  SLSTransactionCommit(transaction, true);
  CFRelease(transaction);
  SLSReenableUpdate(cid);
}

void border_hide(struct border* border) {
  if (border->wid) {
    int cid = SLSMainConnectionID();
    SLSOrderWindow(cid, border->wid, 0, border->target_wid);
  }
}

void border_unhide(struct border* border) {
  border_draw(border);
}

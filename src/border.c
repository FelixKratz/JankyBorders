#include "border.h"
#include "hashtable.h"
#include "windows.h"

extern struct settings g_settings;
void border_init(struct border* border) {
  memset(border, 0, sizeof(struct border));
}

void border_destroy_window(struct border* border) {
  if (border->wid) SLSReleaseWindow(SLSMainConnectionID(), border->wid);
  if (border->context) CGContextRelease(border->context);
  border->wid = 0;
  border->context = NULL;
}

void border_destroy(struct border* border) {
  border_destroy_window(border);
  free(border);
}

void border_move(struct border* border) {
  int cid = SLSMainConnectionID();

  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);
  CGPoint origin = { .x = window_frame.origin.x
                          - g_settings.border_width
                          - BORDER_PADDING,
                     .y = window_frame.origin.y
                          - g_settings.border_width
                          - BORDER_PADDING          };

  SLSMoveWindow(cid, border->wid, &origin);
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

  float border_offset = - g_settings.border_width - BORDER_PADDING;
  CGRect frame = CGRectInset(window_frame, border_offset, border_offset);

  CGPoint origin = frame.origin;
  frame.origin = CGPointZero;
  window_frame.origin = (CGPoint){ -border_offset, -border_offset };

  SLSDisableUpdate(cid);
  if (!border->wid) {
    border->wid = window_create(cid, frame, g_settings.hidpi);
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
    CGContextSaveGState(border->context);
    border->needs_redraw = false;
    struct color_style color_style = border->focused
                                     ? g_settings.active_window
                                     : g_settings.inactive_window;

    CGGradientRef gradient = NULL;
    CGPoint gradient_direction[2];
    if (color_style.stype == COLOR_STYLE_SOLID
       || color_style.stype == COLOR_STYLE_GLOW) {
      uint32_t color = color_style.color;
      float a = ((color_style.color >> 24) & 0xff) / 255.f;
      float r = ((color_style.color >> 16) & 0xff) / 255.f;
      float g = ((color_style.color >> 8) & 0xff) / 255.f;
      float b = ((color_style.color >> 0) & 0xff) / 255.f;
      CGContextSetRGBFillColor(border->context, r, g, b, a);
      CGContextSetRGBStrokeColor(border->context, r, g, b, a);

      if (color_style.stype == COLOR_STYLE_GLOW) {
        CGColorRef color_ref = CGColorCreateGenericRGB(r, g, b, 1.0);
        CGContextSetShadowWithColor(border->context, CGSizeZero, 10.0, color_ref);
        CGColorRelease(color_ref);
      }
    } else if (color_style.stype == COLOR_STYLE_GRADIENT) {
      float a1 = ((color_style.gradient.color1 >> 24) & 0xff) / 255.f;
      float a2 = ((color_style.gradient.color2 >> 24) & 0xff) / 255.f;
      float r1 = ((color_style.gradient.color1 >> 16) & 0xff) / 255.f;
      float r2 = ((color_style.gradient.color2 >> 16) & 0xff) / 255.f;
      float g1 = ((color_style.gradient.color1 >> 8) & 0xff) / 255.f;
      float g2 = ((color_style.gradient.color2 >> 8) & 0xff) / 255.f;
      float b1 = ((color_style.gradient.color1 >> 0) & 0xff) / 255.f;
      float b2 = ((color_style.gradient.color2 >> 0) & 0xff) / 255.f;

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
      CGPoint point1;
      CGPoint point2;
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

    CGRect path_rect = window_frame;

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
                                                          border_radius,
                                                          border_radius,
                                                          NULL          );

      CGContextAddPath(border->context, stroke_path);

      if (color_style.stype == COLOR_STYLE_SOLID
         || color_style.stype == COLOR_STYLE_GLOW) {
        CGContextStrokePath(border->context);
      }
      else if (color_style.stype == COLOR_STYLE_GRADIENT) {
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
    CFRelease(clip_path);
    CGGradientRelease(gradient);
  
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

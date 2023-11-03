#include "border.h"
#include "hashtable.h"

extern uint32_t g_active_window_color;
extern uint32_t g_inactive_window_color;
extern float g_border_width;
extern struct table g_windows;

static void border_init(struct border* border) {
  memset(border, 0, sizeof(struct border));
}

void border_destroy(struct border* border) {
  SLSReleaseWindow(SLSMainConnectionID(), border->wid);
  if (border->context) CGContextRelease(border->context);
  free(border);
}

static void border_move(struct border* border) {
  int cid = SLSMainConnectionID();
  CFTypeRef transaction = SLSTransactionCreate(cid);

  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);
  CGRect frame = CGRectInset(window_frame, -g_border_width, -g_border_width);
  SLSTransactionMoveWindowWithGroup(transaction, border->wid, frame.origin);

  SLSTransactionCommit(transaction, true);
  border->origin = window_frame.origin;

  CFRelease(transaction);
}

static void border_draw(struct border* border) {
  int cid = SLSMainConnectionID();

  bool shown = false;
  SLSWindowIsOrderedIn(cid, border->target_wid, &shown);
  if (!shown && border->wid) {
    SLSReleaseWindow(cid, border->wid);
    border->wid = 0;
    CGContextRelease(border->context);
    border->context = NULL;
  }
  if (!shown) return;

  CGRect window_frame;
  SLSGetWindowBounds(cid, border->target_wid, &window_frame);

  float border_width = g_border_width;
  float border_radius = 9.f;
  CGRect frame = CGRectInset(window_frame, -border_width, -border_width);

  CFArrayRef target_ref = cfarray_of_cfnumbers(&border->target_wid,
                                               sizeof(uint32_t),
                                               1,
                                               kCFNumberSInt32Type );

  CFTypeRef query = SLSWindowQueryWindows(cid, target_ref, 1);
  CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
  int level = SLSWindowIteratorGetLevel(iterator, 0);
  CFRelease(iterator);
  CFRelease(query);

  int sub_level = 0;
  SLSGetWindowSubLevel(cid, border->target_wid, &sub_level);

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

    if (!border->sid) {
      CFArrayRef spaces = SLSCopySpacesForWindows(cid, 0x2, target_ref);
      if (CFArrayGetCount(spaces) > 0) {
        CFNumberRef number = CFArrayGetValueAtIndex(spaces, 0);
        uint64_t sid;
        CFNumberGetValue(number, CFNumberGetType(number), &sid);
        border->sid = sid;
        CFRelease(number);
      }
      CFRelease(spaces);
    }
    CFRelease(target_ref);

    CFArrayRef window_list = cfarray_of_cfnumbers(&border->wid,
                                                  sizeof(uint32_t),
                                                  1,
                                                  kCFNumberSInt32Type);

    SLSMoveWindowsToManagedSpace(cid, window_list, border->sid);
    CFRelease(window_list);
  }

  CFTypeRef transaction = SLSTransactionCreate(cid);
  if (!CGRectEqualToRect(frame, border->bounds)) {
    CFTypeRef frame_region;
    CGSNewRegionWithRect(&frame, &frame_region);

    border->bounds = frame;
    SLSSetWindowShape(cid, border->wid, -9999, -9999, frame_region);
    border->needs_redraw = true;
    CFRelease(frame_region);
  }

  SLSTransactionMoveWindowWithGroup(transaction, border->wid, origin);

  SLSSetWindowLevel(cid, border->wid, level);
  SLSSetWindowSubLevel(cid, border->wid, sub_level);

  SLSOrderWindow(cid, border->wid, -1, border->target_wid);

  if (border->needs_redraw) {
    border->needs_redraw = false;
    CGRect path_rect = (CGRect) {{ border_width, border_width },
                                 { frame.size.width - 2.f*border_width,
                                   frame.size.height - 2.f*border_width }};

    CGPathRef path = CGPathCreateWithRoundedRect(path_rect,
                                                 border_radius,
                                                 border_radius,
                                                 NULL          );

    if (border->focused) {
      CGContextSetRGBStrokeColor(border->context,
                                 ((g_active_window_color >> 16) & 0xff) / 255.f,
                                 ((g_active_window_color >> 8) & 0xff) / 255.f,
                                 ((g_active_window_color >> 0) & 0xff) / 255.f,
                                 ((g_active_window_color >> 24) & 0xff) / 255.f );
    } else {
      CGContextSetRGBStrokeColor(border->context,
                                 ((g_inactive_window_color >> 16) & 0xff) / 255.f,
                                 ((g_inactive_window_color >> 8) & 0xff) / 255.f,
                                 ((g_inactive_window_color >> 0) & 0xff) / 255.f,
                                 ((g_inactive_window_color >> 24) & 0xff) / 255.f );
    }
    CGContextSetLineWidth(border->context, border_width);

    CGContextClearRect(border->context, frame);
    CGContextAddPath(border->context, path);
    CGContextStrokePath(border->context);
    CGContextFlush(border->context);
    CFRelease(path);
  }

  SLSTransactionCommit(transaction, true);
  CFRelease(transaction);
  SLSReenableUpdate(cid);
}

struct border* border_create(uint32_t wid, uint64_t sid) {
  struct border* border = table_find(&g_windows, &wid);
  if (!border) {
    border = malloc(sizeof(struct border));
    border_init(border);
    table_add(&g_windows, &wid, border);
  }

  border->target_wid = wid;
  border->sid = sid;
  border->needs_redraw = true;
  border_draw(border);
  return border;
}

void borders_remove_border(uint32_t wid, uint64_t sid) {
  struct border* border = table_find(&g_windows, &wid);
  if (border) {
    table_remove(&g_windows, &wid);
    border_destroy(border);
  }
}

void borders_update_border(uint32_t wid) {
  struct border* border = table_find(&g_windows, &wid);
  if (border) {
    border_draw(border);
  }
}

void borders_window_focus(uint32_t wid) {
  for (int window_index = 0; window_index < g_windows.capacity; ++window_index) {
    struct bucket *bucket = g_windows.buckets[window_index];
    while (bucket) {
      if (bucket->value) {
          struct border* border = bucket->value;
          if (border->focused && border->target_wid != wid) {
            border->focused = false;
            border->needs_redraw = true;
            border_draw(border);
          }

          if (border->target_wid == wid) {
            if (!border->focused) {
              border->focused = true;
              border->needs_redraw = true;
              border_draw(border);
            }
          }
      }

      bucket = bucket->next;
    }
  }
}

void borders_move_border(uint32_t wid) {
  struct border* border = table_find(&g_windows, &wid);
  if (border) {
    border_move(border);
  }
}

uint32_t get_front_window() {
  int cid = SLSMainConnectionID();
  ProcessSerialNumber psn;
  _SLPSGetFrontProcess(&psn);
  int target_cid;
  SLSGetConnectionIDForPSN(cid, &psn, &target_cid);

  CFArrayRef displays = SLSCopyManagedDisplays(cid);
  uint32_t space_count = CFArrayGetCount(displays);
  uint64_t space_list[space_count];

  for (int i = 0; i < space_count; i++) {
    space_list[i] = SLSManagedDisplayGetCurrentSpace(cid,
                                          CFArrayGetValueAtIndex(displays, i));
  }

  CFRelease(displays);

  CFArrayRef space_list_ref = cfarray_of_cfnumbers(space_list,
                                                   sizeof(uint64_t),
                                                   space_count,
                                                   kCFNumberSInt64Type);

  uint64_t set_tags = 1;
  uint64_t clear_tags = 0;
  CFArrayRef window_list = SLSCopyWindowsWithOptionsAndTags(cid,
                                                            target_cid,
                                                            space_list_ref,
                                                            0x2,
                                                            &set_tags,
                                                            &clear_tags    );

  uint32_t wid = 0;
  if (window_list) {
    uint32_t window_count = CFArrayGetCount(window_list);
    CFTypeRef query = SLSWindowQueryWindows(cid, window_list, window_count);
    if (query) {
      CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
      if (iterator) {
        wid = SLSWindowIteratorGetWindowID(iterator);
        CFRelease(iterator);
      }
      CFRelease(query);
    }
    CFRelease(window_list);
  }

  CFRelease(space_list_ref);
  return wid;
}

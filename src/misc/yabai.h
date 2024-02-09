#pragma once
#include "extern.h"
#include "../windows.h"
#include <pthread.h>
#include <CoreVideo/CoreVideo.h>

#define _YABAI_INTEGRATION

struct track_transform_payload {
  volatile bool stop;

  int cid;
  uint32_t border_wid;
  uint32_t target_wid;
  CGRect border_frame;
  CGRect target_frame;
};

static CVReturn frame_callback(CVDisplayLinkRef display_link, const CVTimeStamp* now, const CVTimeStamp* output_time, CVOptionFlags flags, CVOptionFlags* flags_out, void* context) {
  struct track_transform_payload* payload = context;
  CGAffineTransform target_transform = CGAffineTransformIdentity;
  CGAffineTransform border_transform = CGAffineTransformIdentity;
  CGError error = SLSGetWindowTransform(payload->cid,
                                        payload->target_wid,
                                        &target_transform   );

  if (error != 0) {
    CVDisplayLinkStop(display_link);
    CVDisplayLinkRelease(display_link);
    free(payload);
    return kCVReturnSuccess;
  }

  border_transform = target_transform;
  float delta_x = payload->border_frame.size.width
                - payload->target_frame.size.width;
  float delta_y = payload->border_frame.size.height
                - payload->target_frame.size.height;

  border_transform.tx += 0.5*delta_x;
  border_transform.ty += 0.5*delta_y;
  SLSSetWindowTransform(payload->cid, payload->border_wid, border_transform);

  return kCVReturnSuccess;
}

static void border_track_transform(struct track_transform_payload* payload) {
  CVDisplayLinkRef link;
  CVDisplayLinkCreateWithActiveCGDisplays(&link);
  CVDisplayLinkSetOutputCallback(link,
                                 frame_callback,
                                 payload        );

  CVDisplayLinkStart(link);
}


static inline uint32_t actual_wid_from_yabai_proxy(int cid, int yabai_cid, uint32_t wid) {
  uint32_t result = 0;
  static char num_str[255] = { 0 };
  snprintf(num_str, sizeof(num_str), "%d", wid);

  CFStringRef key_ref = CFStringCreateWithCString(NULL,
                                                  num_str,
                                                  kCFStringEncodingMacRoman);
  CFTypeRef value_ref = NULL;
  SLSCopyConnectionProperty(cid, yabai_cid, key_ref, &value_ref);
  CFRelease(key_ref);
  if (value_ref) {
    CFNumberGetValue(value_ref, kCFNumberSInt32Type, &result);
    CFRelease(value_ref);
  }
  return result;
}

static inline bool yabai_proxy_exists(struct table* proxies, uint32_t wid) {
  return table_find(proxies, &wid);
}

static inline void check_yabai_proxy_begin(struct table* windows, struct table* proxies, int cid, int yabai_cid, uint32_t wid) {
  uint32_t real_wid = actual_wid_from_yabai_proxy(cid, yabai_cid, wid);
  if (!real_wid) return;

  table_remove(proxies, &real_wid);
  table_add(proxies, &real_wid, (void*)(intptr_t)wid);
  table_add(proxies, &wid, (void*)(intptr_t)real_wid);
  struct border* border = table_find(windows, &real_wid);
  if (border) {
    struct track_transform_payload* payload
                              = malloc(sizeof(struct track_transform_payload));

    payload->border_wid = border->wid;
    payload->target_wid = wid;
    payload->cid = cid;
    payload->border_frame = border->bounds;
    payload->target_frame = border->target_bounds;
    border->disable = true;
    border_track_transform(payload);
  }
}

static inline void check_yabai_proxy_end(struct table* windows, struct table* proxies, uint32_t wid) {
  uint32_t real_wid = (intptr_t)table_find(proxies, &wid);
  if (real_wid) {
    table_remove(proxies, &wid);
    uint32_t current_proxy_wid = (intptr_t)table_find(proxies, &real_wid);
    if (wid == current_proxy_wid) {
      table_remove(proxies, &real_wid);
      struct border* border = table_find(windows, &real_wid);
      if (border) {
        border->disable = false;
        border_draw(border);
      }
    }
  }
}

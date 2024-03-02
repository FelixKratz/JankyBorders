#pragma once
#include "extern.h"
#include "../windows.h"
#include <CoreVideo/CoreVideo.h>

#define _YABAI_INTEGRATION

struct track_transform_payload {
  int cid;
  uint32_t border_wid;
  uint32_t target_wid;
  uint64_t frame_delay;
  CGAffineTransform initial_transform;
};

static CVReturn frame_callback(CVDisplayLinkRef display_link, const CVTimeStamp* now, const CVTimeStamp* output_time, CVOptionFlags flags, CVOptionFlags* flags_out, void* context) {
  struct track_transform_payload* payload = context;
  CGAffineTransform target_transform, border_transform;

  usleep(payload->frame_delay);
  CGError error = SLSGetWindowTransform(payload->cid,
                                        payload->target_wid,
                                        &target_transform   );

  if (error != kCGErrorSuccess) {
    CVDisplayLinkStop(display_link);
    CVDisplayLinkRelease(display_link);
    free(payload);
    return kCVReturnSuccess;
  }

  border_transform = CGAffineTransformConcat(target_transform,
                                             payload->initial_transform);

  SLSSetWindowTransform(payload->cid, payload->border_wid, border_transform);
  return kCVReturnSuccess;
}

static inline void border_track_transform(struct track_transform_payload* payload) {
  CVDisplayLinkRef link;
  CVDisplayLinkCreateWithActiveCGDisplays(&link);
  CVTime refresh_period = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
  payload->frame_delay = 0.25e6 * (double)refresh_period.timeValue
                        / (double)refresh_period.timeScale;

  CVDisplayLinkSetOutputCallback(link, frame_callback, payload);
  CVDisplayLinkStart(link);
}


static inline bool yabai_proxy_exists(struct table* proxies, uint32_t wid) {
  return table_find(proxies, &wid);
}

static inline void yabai_proxy_begin(struct table* windows, struct table* proxies, int cid, uint32_t wid, uint32_t real_wid) {
  if (!real_wid) return;

  table_remove(proxies, &real_wid);
  table_add(proxies, &real_wid, (void*)(intptr_t)wid);
  table_add(proxies, &wid, (void*)(intptr_t)real_wid);
  struct border* border = table_find(windows, &real_wid);
  if (border) {
    struct track_transform_payload* payload
                              = malloc(sizeof(struct track_transform_payload));

    CGRect proxy_frame;
    SLSGetWindowBounds(cid, wid, &proxy_frame);
    border->unmanaged = true;
    border->recreate_window = true;
    border_draw(border);

    payload->border_wid = border->wid;
    payload->target_wid = wid;
    payload->cid = cid;

    payload->initial_transform = CGAffineTransformIdentity;
    payload->initial_transform.a = border->target_bounds.size.width
                                   / proxy_frame.size.width;
    payload->initial_transform.d = border->target_bounds.size.height
                                   / proxy_frame.size.height;
    payload->initial_transform.tx = 0.5*(border->bounds.size.width
                                    - border->target_bounds.size.width);
    payload->initial_transform.ty = 0.5*(border->bounds.size.height
                                    - border->target_bounds.size.height);

    border->disable_update = true;
    border_track_transform(payload);
  }
}

static inline void yabai_proxy_end(struct table* windows, struct table* proxies, uint32_t wid) {
  uint32_t real_wid = (intptr_t)table_find(proxies, &wid);
  if (real_wid) {
    table_remove(proxies, &wid);
    uint32_t current_proxy_wid = (intptr_t)table_find(proxies, &real_wid);
    if (wid == current_proxy_wid) {
      table_remove(proxies, &real_wid);
      struct border* border = table_find(windows, &real_wid);
      if (border) {
        border->disable_update = false;
        border->unmanaged = false;
        border->recreate_window = true;
        border_draw(border);
      }
    }
  }
}

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

  uint32_t current_proxy_wid = (intptr_t)table_find(proxies, &real_wid);
  table_remove(proxies, &real_wid);
  table_add(proxies, &real_wid, (void*)(intptr_t)wid);

  struct border* border = table_find(windows, &real_wid);
  struct border* proxy = table_find(proxies, &current_proxy_wid);
  table_remove(proxies, &current_proxy_wid);

  if (border) {
    border->disable_update = true;
    if (!proxy) {
      proxy = malloc(sizeof(struct border));
      border_init(proxy);
      proxy->target_bounds = border->target_bounds;
      proxy->focused = border->focused;
      proxy->target_wid = real_wid;
      proxy->sid = border->sid;
      proxy->unmanaged = true;
      border_update(proxy);
      proxy->disable_update = true;
    }
    table_add(proxies, &wid, (void*)proxy);

    CFTypeRef transaction = SLSTransactionCreate(cid);
    SLSTransactionSetWindowAlpha(transaction, border->wid, 0.f);
    SLSTransactionSetWindowAlpha(transaction, proxy->wid, 1.f);
    SLSTransactionCommit(transaction, 1);
    CFRelease(transaction);

    struct track_transform_payload* payload
                              = malloc(sizeof(struct track_transform_payload));

    CGRect proxy_frame;
    SLSGetWindowBounds(cid, wid, &proxy_frame);

    payload->border_wid = proxy->wid;
    payload->target_wid = wid;
    payload->cid = cid;

    payload->initial_transform = CGAffineTransformIdentity;
    payload->initial_transform.a = border->target_bounds.size.width
                                   / proxy_frame.size.width;
    payload->initial_transform.d = border->target_bounds.size.height
                                   / proxy_frame.size.height;
    payload->initial_transform.tx = 0.5*(border->frame.size.width
                                    - border->target_bounds.size.width);
    payload->initial_transform.ty = 0.5*(border->frame.size.height
                                    - border->target_bounds.size.height);

    border_track_transform(payload);
  } else if (proxy) border_destroy(proxy);
}

static inline void yabai_proxy_end(struct table* windows, struct table* proxies, uint32_t wid) {
  struct border* proxy = (struct border*)table_find(proxies, &wid);
  if (proxy) {
    uint32_t real_wid = proxy->target_wid;
    table_remove(proxies, &wid);

    uint32_t current_proxy_wid = (intptr_t)table_find(proxies, &real_wid);
    if (wid == current_proxy_wid) {
      table_remove(proxies, &real_wid);
      struct border* border = table_find(windows, &real_wid);
      if (border) {
        border->disable_update = false;
        border_update(border);
        int cid = SLSMainConnectionID();
        CFTypeRef transaction = SLSTransactionCreate(cid);
        SLSTransactionSetWindowAlpha(transaction, proxy->wid, 0.f);
        SLSTransactionSetWindowAlpha(transaction, border->wid, 1.f);
        SLSTransactionCommit(transaction, 1);
        CFRelease(transaction);
      }
    }
    border_destroy(proxy);
  }
}

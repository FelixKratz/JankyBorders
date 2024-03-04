#pragma once
#include "extern.h"
#include "../windows.h"
#include <CoreVideo/CoreVideo.h>
#include <pthread.h>

#define _YABAI_INTEGRATION

struct track_transform_payload {
  int cid;
  uint32_t border_wid;
  uint32_t target_wid;
  uint64_t frame_delay;
  CGAffineTransform initial_transform;
};

struct yabai_proxy_payload {
  struct border* proxy;
  uint32_t border_wid;
  uint32_t real_wid;
  uint32_t proxy_wid;
  int cid;
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
    SLSReleaseConnection(payload->cid);
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

static void* yabai_proxy_begin_proc(void* context) {
  struct yabai_proxy_payload* info = context;
  struct border* proxy = info->proxy;
  int cid = info->cid;
  pthread_mutex_lock(&proxy->mutex);

  if (!proxy->is_proxy) {
    proxy->is_proxy = true;
    border_update(proxy, cid, false);

    CFTypeRef transaction = SLSTransactionCreate(cid);
    SLSTransactionSetWindowAlpha(transaction, info->border_wid, 0.f);
    SLSTransactionSetWindowAlpha(transaction, proxy->wid, 1.f);
    SLSTransactionCommit(transaction, 1);
    CFRelease(transaction);
  }
  struct track_transform_payload* payload
                            = malloc(sizeof(struct track_transform_payload));

  CGRect proxy_frame;
  SLSGetWindowBounds(cid, info->proxy_wid, &proxy_frame);

  payload->border_wid = proxy->wid;
  payload->target_wid = info->proxy_wid;
  payload->cid = cid;

  payload->initial_transform = CGAffineTransformIdentity;
  payload->initial_transform.a = proxy->target_bounds.size.width
                                / proxy_frame.size.width;
  payload->initial_transform.d = proxy->target_bounds.size.height
                                / proxy_frame.size.height;
  payload->initial_transform.tx = 0.5*(proxy->frame.size.width
                                 - proxy->target_bounds.size.width);
  payload->initial_transform.ty = 0.5*(proxy->frame.size.height
                                 - proxy->target_bounds.size.height);

  pthread_mutex_unlock(&proxy->mutex);
  border_track_transform(payload);
  free(context);
  return NULL;
}

static inline void yabai_proxy_begin(struct table* windows, int cid, uint32_t wid, uint32_t real_wid) {
  if (!real_wid) return;
  struct border* border = table_find(windows, &real_wid);

  if (border) {
    border->proxy_wid = wid;
    if (!border->proxy) {
      border->proxy = malloc(sizeof(struct border));
      border_init(border->proxy);
      border_create_window(border->proxy, cid, CGRectNull, true);
      border->proxy->target_bounds = border->target_bounds;
      border->proxy->focused = border->focused;
      border->proxy->target_wid = border->target_wid;
      border->proxy->sid = border->sid;
    }

    struct yabai_proxy_payload* payload
                            = malloc(sizeof(struct yabai_proxy_payload));
    payload->proxy = border->proxy;
    payload->border_wid = border->wid;
    payload->proxy_wid = wid;
    payload->real_wid = real_wid;
    payload->cid = cid;

    pthread_t thread;
    pthread_create(&thread, NULL, yabai_proxy_begin_proc, payload);
    pthread_detach(thread);
  }
}

static inline void yabai_proxy_end(struct table* windows, int cid, uint32_t wid, uint32_t real_wid) {
  struct border* border = (struct border*)table_find(windows, &real_wid);
  if (border && border->proxy && border->proxy_wid == wid) {
    struct border* proxy = border->proxy;
    border->proxy_wid = 0;
    border->proxy = NULL;

    CGAffineTransform transform;
    SLSGetWindowTransform(cid, proxy->wid, &transform);
    CFTypeRef transaction = SLSTransactionCreate(cid);
    SLSTransactionSetWindowTransform(transaction,
                                     border->wid,
                                     0,
                                     0,
                                     transform   );

    SLSTransactionSetWindowAlpha(transaction, proxy->wid, 0.f);
    SLSTransactionSetWindowAlpha(transaction, border->wid, 1.f);
    SLSTransactionCommit(transaction, 1);
    CFRelease(transaction);

    border_destroy(proxy, cid);
    border_update(border, cid, true);
  }
}

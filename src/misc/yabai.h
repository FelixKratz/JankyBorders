#pragma once
#define _YABAI_INTEGRATION

#ifdef _YABAI_INTEGRATION
#include "extern.h"
#include "../windows.h"
#include "../mach.h"
#include <CoreVideo/CoreVideo.h>
#include <pthread.h>

// Additional border interfaces needed for the yabai integration
void border_init(struct border* border, int cid);
void border_create_window(struct border* border, CGRect frame, bool unmanaged, bool hidpi);
void border_update_internal(struct border* border, struct settings* settings);

struct track_transform_payload {
  int cid;
  uint32_t border_wid;
  uint32_t proxy_wid;
  uint32_t target_wid;
  CGAffineTransform initial_transform;
};

struct yabai_proxy_payload {
  union { struct border* proxy; struct border* border; };
  struct settings settings;
  uint32_t border_wid;
  uint32_t real_wid;
  uint32_t external_proxy_wid;
};

static CVReturn track_transform(CVDisplayLinkRef display_link, const CVTimeStamp* now, const CVTimeStamp* output_time, CVOptionFlags flags, CVOptionFlags* flags_out, void* context) {
  struct animation* animation = context;
  usleep(0.25*animation->frame_time);

  struct track_transform_payload* payload = animation->context;
  CGAffineTransform target_transform, border_transform;
  CGError error = SLSGetWindowTransform(payload->cid,
                                        payload->target_wid,
                                        &target_transform   );

  if (error != kCGErrorSuccess) return kCVReturnSuccess;

  border_transform = CGAffineTransformConcat(target_transform,
                                             payload->initial_transform);

  CFTypeRef transaction = SLSTransactionCreate(payload->cid);
  if (transaction) {
    SLSTransactionSetWindowTransform(transaction, payload->proxy_wid, 0, 0, border_transform);
    SLSTransactionSetWindowTransform(transaction, payload->border_wid, 0, 0, border_transform);
    SLSTransactionCommit(transaction, 1);
    CFRelease(transaction);
  }
  return kCVReturnSuccess;
}

static void* yabai_proxy_begin_proc(void* context) {
  struct yabai_proxy_payload* info = context;
  struct border* proxy = info->proxy;
  pthread_mutex_lock(&proxy->mutex);

  struct track_transform_payload* payload
                            = malloc(sizeof(struct track_transform_payload));

  CGRect proxy_frame;
  SLSGetWindowBounds(proxy->cid, info->external_proxy_wid, &proxy_frame);

  payload->proxy_wid = proxy->wid;
  payload->border_wid = info->border_wid;
  payload->target_wid = info->external_proxy_wid;
  payload->cid = proxy->cid;

  payload->initial_transform = CGAffineTransformIdentity;
  payload->initial_transform.a = proxy->target_bounds.size.width
                                / proxy_frame.size.width;
  payload->initial_transform.d = proxy->target_bounds.size.height
                                / proxy_frame.size.height;
  payload->initial_transform.tx = 0.5*(proxy->frame.size.width
                                 - proxy->target_bounds.size.width);
  payload->initial_transform.ty = 0.5*(proxy->frame.size.height
                                 - proxy->target_bounds.size.height);

  animation_stop(&proxy->animation);
  animation_start(&proxy->animation, track_transform, payload);

  if (!proxy->is_proxy) {
    proxy->is_proxy = true;
    proxy->frame = CGRectNull;
    border_update_internal(proxy, &info->settings);

    CFTypeRef transaction = SLSTransactionCreate(proxy->cid);
    if (transaction) {
      SLSTransactionSetWindowAlpha(transaction, info->border_wid, 0.f);
      SLSTransactionSetWindowAlpha(transaction, proxy->wid, 1.f);
      SLSTransactionCommit(transaction, 1);
      CFRelease(transaction);
    }
  }
  pthread_mutex_unlock(&proxy->mutex);
  free(context);
  return NULL;
}

static void* yabai_proxy_end_proc(void* context) {
  struct yabai_proxy_payload* info = context;
  struct border* border = info->border;
  pthread_mutex_lock(&border->mutex);
  border->event_buffer.disable_coalescing = true;
  border->external_proxy_wid = 0;
  border_update_internal(border, &info->settings);
  border->event_buffer.disable_coalescing = false;
  pthread_mutex_unlock(&border->mutex);
  free(context);
  return NULL;
}

static inline void yabai_proxy_begin(struct table* windows, uint32_t wid, uint32_t real_wid) {
  if (!real_wid || !wid) return;
  struct border* border = table_find(windows, &real_wid);

  if (border) {
    pthread_mutex_lock(&border->mutex);
    border->external_proxy_wid = wid;
    if (!border->proxy) {
      border->proxy = malloc(sizeof(struct border));
      border_init(border->proxy, border->cid);
      border_create_window(border->proxy, CGRectNull, true, false);
      border->proxy->target_bounds = border->target_bounds;
      border->proxy->frame = border->frame;
      border->proxy->focused = border->focused;
      border->proxy->target_wid = border->target_wid;
      border->proxy->sid = border->sid;
    }

    struct yabai_proxy_payload* payload
                            = malloc(sizeof(struct yabai_proxy_payload));
    payload->proxy = border->proxy;
    payload->border_wid = border->wid;
    payload->external_proxy_wid = border->external_proxy_wid;
    payload->real_wid = real_wid;
    payload->settings = *border_get_settings(border);

    pthread_t thread;
    pthread_create(&thread, NULL, yabai_proxy_begin_proc, payload);
    pthread_detach(thread);
    pthread_mutex_unlock(&border->mutex);
  }
}

static inline void yabai_proxy_end(struct table* windows, uint32_t wid, uint32_t real_wid) {
  if (!real_wid || !wid) return;
  struct border* border = (struct border*)table_find(windows, &real_wid);
  if (border) pthread_mutex_lock(&border->mutex);
  if (border && border->proxy && border->external_proxy_wid == wid) {
    struct border* proxy = border->proxy;
    border->proxy = NULL;

    CFTypeRef transaction = SLSTransactionCreate(border->cid);
    if (transaction) {
      SLSTransactionSetWindowAlpha(transaction, proxy->wid, 0.f);
      SLSTransactionSetWindowAlpha(transaction, border->wid, 1.f);
      SLSTransactionCommit(transaction, 1);
      CFRelease(transaction);
    }

    border_destroy(proxy);

    struct yabai_proxy_payload* payload
                                  = malloc(sizeof(struct yabai_proxy_payload));

    payload->border = border;
    payload->border_wid = border->wid;
    payload->settings = *border_get_settings(border);

    pthread_t thread;
    pthread_create(&thread, NULL, yabai_proxy_end_proc, payload);
    pthread_detach(thread);
  }
  if (border) pthread_mutex_unlock(&border->mutex);
}

static void yabai_message(CFMachPortRef port, void* data, CFIndex size, void* context) {
  if (size == sizeof(struct mach_message)) {
    struct mach_message* message = data;

    struct payload {
      uint32_t event;
      uint32_t count;
      uint32_t proxy_wid[512];
      uint32_t real_wid[512];
    };

    if (message->descriptor.size == sizeof(struct payload)) {
      struct payload* payload = message->descriptor.address;
      if (payload->event == 1325) {
        for (int i = 0; i < payload->count; i++) {
          yabai_proxy_begin(context,
                            payload->proxy_wid[i],
                            payload->real_wid[i]  );
        }
      } else if (payload->event == 1326) {
        for (int i = 0; i < payload->count; i++) {
          yabai_proxy_end(context,
                          payload->proxy_wid[i],
                          payload->real_wid[i]  );
        }
      }
    }
    mach_msg_destroy(&message->header);
  }
}

static inline void yabai_register_mach_port(struct table* windows) {
  ipc_space_t task = mach_task_self();
  mach_port_t port;
  if (mach_port_allocate(task,
                         MACH_PORT_RIGHT_RECEIVE,
                         &port                   ) != KERN_SUCCESS) {
    return;
  }

  struct mach_port_limits limits = { 1 };
  if (mach_port_set_attributes(task,
                               port,
                               MACH_PORT_LIMITS_INFO,
                               (mach_port_info_t)&limits,
                               MACH_PORT_LIMITS_INFO_COUNT) != KERN_SUCCESS) {
    return;
  }

  if (!mach_register_port(port, "git.felix.jbevent")) return;

  CFMachPortContext context = {0, (void*)windows};

  CFMachPortRef cf_mach_port = CFMachPortCreateWithPort(NULL,
                                                        port,
                                                        yabai_message,
                                                        &context,
                                                        false         );

  CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL,
                                                            cf_mach_port,
                                                            0            );

  CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
  CFRelease(source);
  CFRelease(cf_mach_port);
}
#endif

#pragma once
#include "extern.h"
#include "../windows.h"

#define _YABAI_INTEGRATION

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
    border->disable = true;
    border_hide(border);
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

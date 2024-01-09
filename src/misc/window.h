#pragma once
#include "helpers.h"
#include "space.h"

static inline bool window_suitable(CFTypeRef iterator) {
  uint64_t tags = SLSWindowIteratorGetTags(iterator);
  uint64_t attributes = SLSWindowIteratorGetAttributes(iterator);
  uint32_t parent_wid = SLSWindowIteratorGetParentID(iterator);
  if (((parent_wid == 0)
        && ((attributes & 0x2)
          || (tags & 0x400000000000000))
        && (((tags & 0x1))
          || ((tags & 0x2)
            && (tags & 0x80000000))))) {
    return true;
  }
  return false;
}

static inline uint32_t get_front_window(int cid) {
  uint64_t active_sid = get_active_space_id(cid);
  debug("Active space id: %d\n", active_sid);
  CFArrayRef space_list_ref = cfarray_of_cfnumbers(&active_sid,
                                                   sizeof(uint64_t),
                                                   1,
                                                   kCFNumberSInt64Type);

  ProcessSerialNumber psn;
  _SLPSGetFrontProcess(&psn);
  int target_cid;
  SLSGetConnectionIDForPSN(cid, &psn, &target_cid);

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
    if (window_count > 0) {
      CFTypeRef query = SLSWindowQueryWindows(cid, window_list, 0x0);
      if (query) {
        CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
        if (iterator && SLSWindowIteratorGetCount(iterator) > 0) {
          while (SLSWindowIteratorAdvance(iterator)) {
            if (window_suitable(iterator)) {
              wid = SLSWindowIteratorGetWindowID(iterator);
              break;
            }
          }
        }
        if (iterator) CFRelease(iterator);
        CFRelease(query);
      }
    } else {
      debug("Empty window list\n");
    }
    CFRelease(window_list);
  }
  CFRelease(space_list_ref);
  return wid;
}

static inline uint64_t window_space_id(int cid, uint32_t wid) {
  uint64_t sid = 0;

  CFArrayRef window_list_ref = cfarray_of_cfnumbers(&wid,
                                                    sizeof(uint32_t),
                                                    1,
                                                    kCFNumberSInt32Type);

  CFArrayRef space_list_ref = SLSCopySpacesForWindows(cid,
                                                      0x7,
                                                      window_list_ref);


  if (space_list_ref) {
    int count = CFArrayGetCount(space_list_ref);
    if (count > 0) {
      CFNumberRef id_ref = (CFNumberRef)CFArrayGetValueAtIndex(space_list_ref,
                                                               0             );
      CFNumberGetValue(id_ref, CFNumberGetType(id_ref), &sid);
    }
    CFRelease(space_list_ref);
  }
  CFRelease(window_list_ref);

  if (sid) return sid;

  CFStringRef uuid = SLSCopyManagedDisplayForWindow(cid, wid);
  if (uuid) {
    uint64_t sid = SLSManagedDisplayGetCurrentSpace(cid, uuid);
    CFRelease(uuid);
    return sid;
  }

  return 0;
}

static inline int window_level(int cid, uint32_t wid) {
  CFArrayRef target_ref = cfarray_of_cfnumbers(&wid,
                                               sizeof(uint32_t),
                                               1,
                                               kCFNumberSInt32Type );

  CFTypeRef query = SLSWindowQueryWindows(cid, target_ref, 0x0);
  CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
  int level = 0;
  if (iterator && SLSWindowIteratorAdvance(iterator)) {
    level = SLSWindowIteratorGetLevel(iterator);
  }
  if (iterator) CFRelease(iterator);

  CFRelease(query);
  CFRelease(target_ref);

  return level;
}

static inline void window_send_to_space(int cid, uint32_t wid, uint32_t sid) {
  CFArrayRef window_list = cfarray_of_cfnumbers(&wid,
                                                sizeof(uint32_t),
                                                1,
                                                kCFNumberSInt32Type);

  SLSMoveWindowsToManagedSpace(cid, window_list, sid);
  CFRelease(window_list);
}

static inline uint32_t window_create(int cid, CGRect frame, bool hidpi) {
  uint64_t id;
  CFTypeRef frame_region;

  CGSNewRegionWithRect(&frame, &frame_region);
  SLSNewWindow(cid,
               kCGBackingStoreBuffered,
               -9999,
               -9999,
               frame_region,
               &id                     );
  CFRelease(frame_region);

  uint32_t wid = id;
  uint64_t set_tags = (1ULL << 1) | (1ULL << 9);
  uint64_t clear_tags = 0;

  SLSSetWindowResolution(cid, wid, hidpi ? 2.0f : 1.0f);
  SLSSetWindowTags(cid, wid, &set_tags, 64);
  SLSClearWindowTags(cid, wid, &clear_tags, 64);
  SLSSetWindowOpacity(cid, wid, 0);

  CFIndex shadow_density = 0;
  CFNumberRef shadow_density_cf = CFNumberCreate(kCFAllocatorDefault,
                                                 kCFNumberCFIndexType,
                                                 &shadow_density      );

  const void *keys[1] = { CFSTR("com.apple.WindowShadowDensity") };
  const void *values[1] = { shadow_density_cf };
  CFDictionaryRef shadow_props_cf = CFDictionaryCreate(NULL,
                                             keys,
                                             values,
                                             1,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);

  SLSWindowSetShadowProperties(wid, shadow_props_cf);
  CFRelease(shadow_density_cf);
  CFRelease(shadow_props_cf);

  return wid;
}

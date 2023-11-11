#include "extern.h"
#include "ApplicationServices/ApplicationServices.h"

#define ITERATOR_WINDOW_SUITABLE(iterator, code) { \
  uint64_t tags = SLSWindowIteratorGetTags(iterator); \
  uint64_t attributes = SLSWindowIteratorGetAttributes(iterator); \
  uint32_t parent_wid = SLSWindowIteratorGetParentID(iterator); \
  if (((parent_wid == 0) \
        && ((attributes & 0x2) \
          || (tags & 0x400000000000000)) \
        && (((tags & 0x1)) \
          || ((tags & 0x2) \
            && (tags & 0x80000000))))) { \
    code \
  } \
}

static inline void debug(const char* message, ...) {
#ifdef DEBUG
  va_list va;
  va_start(va, message);
  vprintf(message, va);
  va_end(va);
#endif
}

static inline void error(const char* message, ...) {
  va_list va;
  va_start(va, message);
  vfprintf(stderr, message, va);
  va_end(va);
  exit(EXIT_FAILURE);
}

static inline  CFArrayRef cfarray_of_cfnumbers(void* values, size_t size, int count, CFNumberType type) {
  CFNumberRef temp[count];

  for (int i = 0; i < count; ++i) {
    temp[i] = CFNumberCreate(NULL, type, ((char *)values) + (size * i));
  }

  CFArrayRef result = CFArrayCreate(NULL,
                                    (const void **)temp,
                                    count,
                                    &kCFTypeArrayCallBacks);

  for (int i = 0; i < count; ++i) CFRelease(temp[i]);

  return result;
}

static inline uint64_t get_active_space_id(int cid) {
  uint32_t count;
  CGGetActiveDisplayList(0, NULL, &count);

  CFStringRef uuid_ref;
  if (count == 1) {
    uint32_t did = 0;
    uint32_t count = 0;
    CGGetActiveDisplayList(1, &did, &count);
    if (count == 1) {
      CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(did);
      uuid_ref = CFUUIDCreateString(NULL, uuid);
      CFRelease(uuid);
    }
    else {
      printf("[!] ERROR (id): No active display detected!\n");
      return 0;
    }
  } else {
    uuid_ref = SLSCopyActiveMenuBarDisplayIdentifier(cid);
  }

  uint64_t sid = SLSManagedDisplayGetCurrentSpace(cid, uuid_ref);
  CFRelease(uuid_ref);
  return sid;
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
      CFTypeRef query = SLSWindowQueryWindows(cid, window_list, window_count);
      if (query) {
        CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
        if (iterator && SLSWindowIteratorGetCount(iterator) > 0) {
          while (SLSWindowIteratorAdvance(iterator)) {
            ITERATOR_WINDOW_SUITABLE(iterator, {
                wid = SLSWindowIteratorGetWindowID(iterator);
                break;
            });
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

static inline bool is_space_visible(int cid, uint64_t sid) {
  CFArrayRef displays = SLSCopyManagedDisplays(cid);
  uint32_t space_count = CFArrayGetCount(displays);

  for (int i = 0; i < space_count; i++) {
    if (sid == SLSManagedDisplayGetCurrentSpace(cid,
                           (CFStringRef)CFArrayGetValueAtIndex(displays, i))) {
      CFRelease(displays);
      return true;
    }
  }

  CFRelease(displays);
  return false;
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

  CFTypeRef query = SLSWindowQueryWindows(cid, target_ref, 1);
  CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
  int level = SLSWindowIteratorGetLevel(iterator, 0);
  CFRelease(iterator);
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

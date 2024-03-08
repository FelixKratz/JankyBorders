#pragma once
#include "helpers.h"

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

  if (!uuid_ref) return 0;
  uint64_t sid = SLSManagedDisplayGetCurrentSpace(cid, uuid_ref);
  CFRelease(uuid_ref);
  return sid;
}

static inline bool is_space_visible(int cid, uint64_t sid) {
  CFArrayRef displays = SLSCopyManagedDisplays(cid);
  if (!displays) return false;
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

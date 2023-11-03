#include "windows.h"
#include <string.h>

void windows_init(struct windows* windows) {
  memset(windows, 0, sizeof(struct windows));
}

static void windows_add_border_to_current_space(struct windows* windows, struct borders* borders) {
  int cid = SLSMainConnectionID();
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
                                                            0,
                                                            space_list_ref,
                                                            0x2,
                                                            &set_tags,
                                                            &clear_tags    );

  if (window_list) {
    uint32_t window_count = CFArrayGetCount(window_list);
    CFTypeRef query = SLSWindowQueryWindows(cid, window_list, window_count);
    if (query) {
      CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
      if (iterator) {
        while(SLSWindowIteratorAdvance(iterator)) {
          ITERATOR_WINDOW_SUITABLE(iterator, {
            uint32_t wid = SLSWindowIteratorGetWindowID(iterator);
            borders_add_border(borders, wid, 0);
          });
        }
      }
      CFRelease(query);
    }
    CFRelease(window_list);
  }

  CFRelease(space_list_ref);
}

void windows_add_existing_windows(int cid, struct windows* windows, struct borders* borders) {
  uint64_t *space_list = NULL;
  int space_count = 0;

  CFArrayRef display_spaces_ref = SLSCopyManagedDisplaySpaces(cid);
  if (display_spaces_ref) {
    int display_spaces_count = CFArrayGetCount(display_spaces_ref);
    for (int i = 0; i < display_spaces_count; ++i) {
      CFDictionaryRef display_ref
                               = CFArrayGetValueAtIndex(display_spaces_ref, i);
      CFArrayRef spaces_ref = CFDictionaryGetValue(display_ref,
                                                   CFSTR("Spaces"));
      int spaces_count = CFArrayGetCount(spaces_ref);

      space_list = (uint64_t*)realloc(space_list,
                                      sizeof(uint64_t)*(space_count
                                                        + spaces_count));

      for (int j = 0; j < spaces_count; ++j) {
        CFDictionaryRef space_ref = CFArrayGetValueAtIndex(spaces_ref, j);
        CFNumberRef sid_ref = CFDictionaryGetValue(space_ref, CFSTR("id64"));
        CFNumberGetValue(sid_ref,
                         CFNumberGetType(sid_ref),
                         space_list + space_count + j);
      }
      space_count += spaces_count;
    }
    CFRelease(display_spaces_ref);
  }

  uint64_t set_tags = 1;
  uint64_t clear_tags = 0;

  CFArrayRef space_list_ref = cfarray_of_cfnumbers(space_list,
                                                   sizeof(uint64_t),
                                                   space_count,
                                                   kCFNumberSInt64Type);

  CFArrayRef window_list_ref = SLSCopyWindowsWithOptionsAndTags(cid,
                                                                0,
                                                                space_list_ref,
                                                                0x7,
                                                                &set_tags,
                                                                &clear_tags  );
  if (window_list_ref) {
    int count = CFArrayGetCount(window_list_ref);
    if (count > 0) {
      CFTypeRef query = SLSWindowQueryWindows(cid, window_list_ref, count);
      CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);

      while (SLSWindowIteratorAdvance(iterator)) {
        ITERATOR_WINDOW_SUITABLE(iterator, {
          uint32_t wid = SLSWindowIteratorGetWindowID(iterator);
          windows_add_window(windows, wid);
        });
      }

      SLSRequestNotificationsForWindows(cid,
                                        windows->wids,
                                        windows->num_windows);

      CFRelease(query);
      CFRelease(iterator);
    }
    CFRelease(window_list_ref);
  }

  CFRelease(space_list_ref);
  free(space_list);
  windows_add_border_to_current_space(windows, borders);
}

void windows_add_window(struct windows* windows, uint32_t wid) {
  for (int i = 0; i < windows->num_windows; i++) {
    if (!windows->wids[i]) {
      windows->wids[i] = wid;
      return;
    };
  }

  windows->wids = (uint32_t*)realloc(windows->wids,
                                     sizeof(uint32_t)*++windows->num_windows);
  windows->wids[windows->num_windows - 1] = wid;
}

bool windows_remove_window(struct windows* windows, uint32_t wid) {
  for (int i = 0; i < windows->num_windows; i++) {
    if (windows->wids[i] == wid) {
      windows->wids[i] = 0;
      return true;
    }
  }
  return false;
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

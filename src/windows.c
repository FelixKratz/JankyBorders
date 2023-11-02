#include "windows.h"
#include <string.h>

extern bool g_first_time;

void windows_init(struct windows* windows) {
  memset(windows, 0, sizeof(struct windows));
}

static uint64_t window_space_id(int cid, uint32_t wid)
{
    uint64_t sid = 0;

    CFArrayRef window_list_ref = cfarray_of_cfnumbers(&wid, sizeof(uint32_t), 1, kCFNumberSInt32Type);
    CFArrayRef space_list_ref = SLSCopySpacesForWindows(cid, 0x7, window_list_ref);
    if (!space_list_ref) goto err;

    int count = CFArrayGetCount(space_list_ref);
    if (!count) goto free;

    CFNumberRef id_ref = CFArrayGetValueAtIndex(space_list_ref, 0);
    CFNumberGetValue(id_ref, CFNumberGetType(id_ref), &sid);

free:
    CFRelease(space_list_ref);
err:
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

void windows_add_existing_windows(int cid, struct windows* windows, struct borders* borders) {
    uint64_t *space_list = NULL;
    int space_count = 0;

    CFArrayRef display_spaces_ref = SLSCopyManagedDisplaySpaces(cid);
    if (display_spaces_ref) {
        int display_spaces_count = CFArrayGetCount(display_spaces_ref);
        for (int i = 0; i < display_spaces_count; ++i) {
            CFDictionaryRef display_ref = CFArrayGetValueAtIndex(display_spaces_ref, i);
            CFArrayRef spaces_ref = CFDictionaryGetValue(display_ref, CFSTR("Spaces"));
            int spaces_count = CFArrayGetCount(spaces_ref);

            space_list = (uint64_t*)realloc(space_list, sizeof(uint64_t)*(space_count + spaces_count));

            for (int j = 0; j < spaces_count; ++j) {
                CFDictionaryRef space_ref = CFArrayGetValueAtIndex(spaces_ref, j);
                CFNumberRef sid_ref = CFDictionaryGetValue(space_ref, CFSTR("id64"));
                CFNumberGetValue(sid_ref, CFNumberGetType(sid_ref), space_list + space_count + j);
            }
            space_count += spaces_count;
        }
        CFRelease(display_spaces_ref);
    }

    uint64_t set_tags = 1;
    uint64_t clear_tags = 0;
    uint32_t options = 0x2;

    CFArrayRef space_list_ref = cfarray_of_cfnumbers(space_list, sizeof(uint64_t), space_count, kCFNumberSInt64Type);
    CFArrayRef window_list_ref = SLSCopyWindowsWithOptionsAndTags(cid, 0, space_list_ref, options, &set_tags, &clear_tags);
    if (!window_list_ref) goto err;

    int count = CFArrayGetCount(window_list_ref);
    if (!count) goto out;

    CFTypeRef query = SLSWindowQueryWindows(cid, window_list_ref, count);
    CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);

    while (SLSWindowIteratorAdvance(iterator)) {
        uint64_t tags = SLSWindowIteratorGetTags(iterator);
        uint64_t attributes = SLSWindowIteratorGetAttributes(iterator);
        uint32_t parent_wid = SLSWindowIteratorGetParentID(iterator);
        uint32_t wid = SLSWindowIteratorGetWindowID(iterator);
        uint64_t sid = window_space_id(cid, wid);

          if (((parent_wid == 0)
               && ((attributes & 0x2)
                   || (tags & 0x400000000000000))
               && (((tags & 0x1))
                    || ((tags & 0x2)
                        && (tags & 0x80000000))))) {
            windows_add_window(windows, wid);
            SLSRequestNotificationsForWindows(cid,
                                              windows->wids,
                                              windows->num_windows);
            struct border* border = borders_add_border(borders, wid, sid);
            if (g_first_time) {
              g_first_time = false;
              borders_remove_border(borders, wid, sid);
              border = borders_add_border(borders, wid, sid);
            }
            CFArrayRef border_ref = cfarray_of_cfnumbers(&border->wid,
                                                         sizeof(uint32_t),
                                                         1,
                                                         kCFNumberSInt32Type);
            border->sid = sid;
            SLSMoveWindowsToManagedSpace(cid, border_ref, sid);
            CFRelease(border_ref);
        }
    }

    CFRelease(query);
    CFRelease(iterator);
out:
    CFRelease(window_list_ref);
err:
    CFRelease(space_list_ref);
    free(space_list);
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

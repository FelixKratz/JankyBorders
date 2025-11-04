#include "windows.h"
#include "hashtable.h"
#include "border.h"
#include "misc/app_rules.h"
#include "misc/ax.h"
#include <string.h>
#include <libproc.h>
#include <stdlib.h>

extern pid_t g_pid;
extern struct settings g_settings;

// Loaded via dlsym in main.c
extern CFArrayRef (*JBSLSWindowIteratorGetCornerRadii)(CFTypeRef);

static char* copy_string_from_cfstring(CFStringRef value) {
  if (!value || CFGetTypeID(value) != CFStringGetTypeID()) return NULL;
  CFIndex length = CFStringGetLength(value);
  CFIndex max_size = CFStringGetMaximumSizeForEncoding(length,
                                                       kCFStringEncodingUTF8);
  char* buffer = malloc((max_size + 1) * sizeof(char));
  if (!buffer) return NULL;
  if (!CFStringGetCString(value, buffer, max_size + 1, kCFStringEncodingUTF8)) {
    free(buffer);
    return NULL;
  }
  return buffer;
}

static char* window_title_from_ax(uint32_t wid, pid_t pid) {
  if (!ax_check_trust(true)) return NULL;
  AXUIElementRef app = AXUIElementCreateApplication(pid);
  if (!app) return NULL;

  CFArrayRef window_list = NULL;
  AXError error = AXUIElementCopyAttributeValue(app,
                                                kAXWindowsAttribute,
                                                (CFTypeRef*)&window_list);
  if (error != kAXErrorSuccess || !window_list) {
    if (app) CFRelease(app);
    return NULL;
  }

  char* title = NULL;
  CFIndex count = CFArrayGetCount(window_list);
  for (CFIndex i = 0; i < count; ++i) {
    AXUIElementRef window = CFArrayGetValueAtIndex(window_list, i);
    if (!window) continue;

    uint32_t window_id = 0;
    _AXUIElementGetWindow(window, &window_id);
    if (window_id != wid) continue;

    CFStringRef title_ref = NULL;
    AXError title_error = AXUIElementCopyAttributeValue(window,
                                                        kAXTitleAttribute,
                                                        (CFTypeRef*)&title_ref);
    if (title_error == kAXErrorSuccess && title_ref) {
      title = copy_string_from_cfstring(title_ref);
      CFRelease(title_ref);
      break;
    }
    if (title_ref) CFRelease(title_ref);
  }

  if (window_list) CFRelease(window_list);
  CFRelease(app);

  return title;
}

static char* window_copy_title(uint32_t wid, pid_t pid) {
  return window_title_from_ax(wid, pid);
}

static bool app_allowed(struct settings* settings,
                        char* app_name,
                        uint32_t wid,
                        pid_t pid) {
  char* title = NULL;

  if (settings->whitelist_enabled) {
    struct app_rule* whitelist_rule = table_find(&settings->whitelist,
                                                 app_name);
    if (!whitelist_rule) return false;
    if (!whitelist_rule->match_all_windows) {
      title = window_copy_title(wid, pid);
      if (!app_rule_matches(whitelist_rule, title ? title : "")) {
        if (title) free(title);
        return false;
      }
    }
  }

  if (settings->blacklist_enabled) {
    struct app_rule* blacklist_rule = table_find(&settings->blacklist,
                                                 app_name);
    if (blacklist_rule) {
      const char* window_title = NULL;
      if (!blacklist_rule->match_all_windows) {
        if (!title) title = window_copy_title(wid, pid);
        window_title = title ? title : "";
      }
      if (app_rule_matches(blacklist_rule, window_title)) {
        if (title) free(title);
        return false;
      }
    }
  }

  if (title) free(title);
  return true;
}

bool windows_window_create(struct table* windows, uint32_t wid, uint64_t sid) {
  bool window_created = false;
  int cid = SLSMainConnectionID();
  int wid_cid = 0;
  SLSGetWindowOwner(cid, wid, &wid_cid);

  pid_t pid = 0;
  SLSConnectionGetPID(wid_cid, &pid);
  static char pid_name_buffer[PROC_PIDPATHINFO_MAXSIZE];
  proc_name(pid, pid_name_buffer, sizeof(pid_name_buffer));

  if (pid == g_pid
      || !app_allowed(&g_settings, pid_name_buffer, wid, pid)) return false;

  CFArrayRef target_ref = cfarray_of_cfnumbers(&wid,
                                               sizeof(uint32_t),
                                               1,
                                               kCFNumberSInt32Type);

  if (!target_ref) return false;

  CFTypeRef query = SLSWindowQueryWindows(cid, target_ref, 0x0);
  if (query) {
    CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
    if (iterator && SLSWindowIteratorGetCount(iterator) > 0) {
      if (SLSWindowIteratorAdvance(iterator)) {
        if (window_suitable(iterator)) {
          struct border* border = table_find(windows, &wid);
          if (!border) {
            border = border_create();
            table_add(windows, &wid, border);
            window_created = true;
          }

          int32_t radius = 0;

          // Determine window corner radius
          if (JBSLSWindowIteratorGetCornerRadii) {
            CFArrayRef radii_ref = JBSLSWindowIteratorGetCornerRadii(iterator);
            if (radii_ref && CFArrayGetCount(radii_ref) > 0) {
              CFNumberRef value = CFArrayGetValueAtIndex(radii_ref, 0);
              CFNumberGetValue(value, kCFNumberSInt32Type, &radius);
            }
            if (radii_ref) CFRelease(radii_ref);
          }
          radius = radius > 0 ? radius : 9;

          border->radius = radius;
          border->inner_radius = radius + 1;
          border->target_wid = wid;
          border->sid = sid;
          border_update(border, false);
          windows_update_notifications(windows);
        }
      }
    }
    if (iterator) CFRelease(iterator);
    CFRelease(query);
  }
  CFRelease(target_ref);

  return window_created;
}

static void windows_remove_all(struct table* windows) {
  for (int i = 0; i < windows->capacity; ++i) {
    struct bucket* bucket = windows->buckets[i];
    while (bucket) {
      if (bucket->value) {
        struct border* border = bucket->value;
        border_destroy(border);
      }
      bucket = bucket->next;
    }
  }
  table_clear(windows);
  windows_update_notifications(windows);
}

void windows_recreate_all_borders(struct table* windows) {
  windows_remove_all(windows);
  windows_add_existing_windows(windows);
}

void windows_update_all(struct table* windows) {
  for (int i = 0; i < windows->capacity; ++i) {
    struct bucket* bucket = windows->buckets[i];
    while (bucket) {
      if (bucket->value) {
        struct border* border = bucket->value;
        if (border) {
          border->needs_redraw = true;
          border_update(border, true);
        }
      }
      bucket = bucket->next;
    }
  }
}

void windows_update_active(struct table* windows) {
  for (int i = 0; i < windows->capacity; ++i) {
    struct bucket* bucket = windows->buckets[i];
    while (bucket) {
      if (bucket->value) {
        struct border* border = bucket->value;
        if (border && border->focused) {
          border->needs_redraw = true;
          border_update(border, true);
        }
      }
      bucket = bucket->next;
    }
  }
}

void windows_update_inactive(struct table* windows) {
  for (int i = 0; i < windows->capacity; ++i) {
    struct bucket* bucket = windows->buckets[i];
    while (bucket) {
      if (bucket->value) {
        struct border* border = bucket->value;
        if (border && !border->focused) {
          border->needs_redraw = true;
          border_update(border, true);
        }
      }
      bucket = bucket->next;
    }
  }
}

void windows_window_update(struct table* windows, uint32_t wid) {
  struct border* border = table_find(windows, &wid);
  if (border) border_update(border, true);
}

static bool windows_window_focus(struct table* windows, uint32_t wid) {
  bool found_window = false;
  for (int i = 0; i < windows->capacity; ++i) {
    struct bucket* bucket = windows->buckets[i];
    while (bucket) {
      if (bucket->value) {
        struct border* border = bucket->value;
        if (border->focused && border->target_wid != wid) {
          border->focused = false;
          border->needs_redraw = true;
          border_update(border, true);
        }

        if (!border->focused && border->target_wid == wid) {
          border->focused = true;
          border->needs_redraw = true;
          border_update(border, true);
        }

        if (border->target_wid == wid) found_window = true;
      }
      bucket = bucket->next;
    }
  }

  return found_window;
}

void windows_window_move(struct table* windows, uint32_t wid) {
  struct border* border = table_find(windows, &wid);
  if (border) border_move(border);
}

void windows_window_hide(struct table* windows, uint32_t wid) {
  struct border* border = table_find(windows, &wid);
  if (border) border_hide(border);
}

void windows_window_unhide(struct table* windows, uint32_t wid) {
  struct border* border = table_find(windows, &wid);
  if (border) border_unhide(border);
}

bool windows_window_destroy(struct table* windows, uint32_t wid, uint32_t sid) {
  struct border* border = table_find(windows, &wid);
  if (border && (border->sid == sid || border->sticky || sid == 0)) {
    table_remove(windows, &wid);
    border_destroy(border);
    windows_update_notifications(windows);
    return true;
  }
  return false;
}

void windows_update_notifications(struct table* windows) {
  int window_count = 0;
  uint32_t window_list[1024] = {};

  for (int i = 0; i < windows->capacity; ++i) {
    struct bucket *bucket = windows->buckets[i];
    while (bucket) {
      if (bucket->value) {
        uint32_t wid = *(uint32_t *) bucket->key;
        window_list[window_count++] = wid;
      }
      bucket = bucket->next;
    }
  }

  int cid = SLSMainConnectionID();
  SLSRequestNotificationsForWindows(cid, window_list, window_count);
}

void windows_determine_and_focus_active_window(struct table* windows) {
  int cid = SLSMainConnectionID();
  uint32_t front_wid = g_settings.ax_focus
                       ? ax_get_front_window(cid)
                       : get_front_window(cid);

  debug("Front window: %d\n", front_wid);
  if (!windows_window_focus(windows, front_wid)) {
    debug("Taking slow window focus path: %d\n", front_wid);
    if (front_wid && windows_window_create(windows,
                                           front_wid,
                                           window_space_id(cid, front_wid))) {
      windows_window_focus(windows, front_wid);
    }
  }
}

void windows_draw_borders_on_current_spaces(struct table* windows) {
  debug("Space Change: Consistency check\n");
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
    CFTypeRef query = SLSWindowQueryWindows(cid, window_list, 0x0);
    if (query) {
      CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
      if (iterator) {
        while(SLSWindowIteratorAdvance(iterator)) {
          if (window_suitable(iterator)) {
            uint32_t wid = SLSWindowIteratorGetWindowID(iterator);
            struct border* border = table_find(windows, &wid);
            if (border) border_update(border, true);
            else {
              debug("Creating Missing Window: %d\n", wid);
              windows_window_create(windows, wid, window_space_id(cid, wid));
            }
          }
        }
        CFRelease(iterator);
      }
      CFRelease(query);
    }
    CFRelease(window_list);
  }
  CFRelease(space_list_ref);
}

void windows_add_existing_windows(struct table* windows) {
  int cid = SLSMainConnectionID();
  uint64_t* space_list = NULL;
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
                                                                0x2,
                                                                &set_tags,
                                                                &clear_tags  );
  if (window_list_ref) {
    int count = CFArrayGetCount(window_list_ref);
    if (count > 0) {
      CFTypeRef query = SLSWindowQueryWindows(cid, window_list_ref, 0x0);
      CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);

      while (SLSWindowIteratorAdvance(iterator)) {
        if (window_suitable(iterator)) {
          uint32_t wid = SLSWindowIteratorGetWindowID(iterator);
          windows_window_create(windows, wid, window_space_id(cid, wid));
        }
      }

      windows_update_notifications(windows);
      CFRelease(query);
      CFRelease(iterator);
    }
    CFRelease(window_list_ref);
  }
  CFRelease(space_list_ref);
  free(space_list);
}

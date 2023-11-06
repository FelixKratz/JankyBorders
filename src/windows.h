#pragma once
#include <stdlib.h>
#include "border.h"
#include "hashtable.h"

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

void windows_update_inactive(struct table* windows);
void windows_update_active(struct table* windows);
void windows_update_all(struct table* windows);
void windows_update_notifications(struct table* windows);

void windows_window_update(struct table* windows, uint32_t wid);
bool windows_window_focus(struct table* windows, uint32_t wid);
void windows_window_hide(struct table* windows, uint32_t wid);
void windows_window_unhide(struct table* windows, uint32_t wid);
void windows_window_move(struct table* windows, uint32_t wid);
bool windows_window_create(struct table* windows, uint32_t wid, uint64_t sid);
bool windows_window_destroy(struct table* windows, uint32_t wid, uint32_t sid);

void windows_add_existing_windows(struct table* windows);
void windows_draw_borders_on_current_spaces(struct table* windows);

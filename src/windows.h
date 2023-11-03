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

void windows_add_existing_windows(int cid, struct table* windows);
uint64_t window_space_id(int cid, uint32_t wid);
struct border* windows_add_window(struct table* windows, uint32_t wid, uint64_t sid);
bool windows_remove_window(struct table* windows, uint32_t wid, uint64_t sid);
void update_window_notifications(void);

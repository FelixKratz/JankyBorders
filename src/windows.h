#pragma once
#include <stdlib.h>
#include "border.h"

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

struct windows {
  uint32_t num_windows;
  uint32_t* wids;
};

void windows_init(struct windows* windows);
void windows_add_existing_windows(int cid, struct windows* windows, struct borders* borders);
void windows_add_window(struct windows* windows, uint32_t wid);
bool windows_remove_window(struct windows* windows, uint32_t wid);

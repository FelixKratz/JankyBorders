#pragma once
#include "extern.h"
#include "hashtable.h"

struct border {
  bool focused;
  bool needs_redraw;

  CGRect bounds;
  CGPoint origin;

  uint32_t wid;
  uint64_t sid;
  uint32_t target_wid;

  CGContextRef context;
};

void borders_remove_border(uint32_t wid);
void borders_update_border(uint32_t wid);
void borders_window_focus(uint32_t wid);
void borders_move_border(uint32_t wid);
struct border* border_create(uint32_t wid, uint64_t sid);
void border_destroy(struct border* border);

uint32_t get_front_window();

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


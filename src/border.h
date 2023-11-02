#pragma once
#include "extern.h"

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

struct borders {
  uint32_t num_borders;
  struct border* borders;
};

void borders_init(struct borders* borders);
struct border* borders_add_border(struct borders* borders, uint32_t wid, uint64_t sid);
void borders_remove_border(struct borders* borders, uint32_t wid, uint64_t sid);
void borders_update_border(struct borders* borders, uint32_t wid);
void borders_window_focus(struct borders* borders, uint32_t wid);
void borders_move_border(struct borders* borders, uint32_t wid);

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


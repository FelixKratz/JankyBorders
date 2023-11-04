#pragma once
#include "helpers.h"
#include "hashtable.h"

#define BORDER_STYLE_ROUND  'r'
#define BORDER_STYLE_SQUARE 's'

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

void border_init(struct border* border);
void border_unhide(struct border* border);
void border_hide(struct border* border);
void border_move(struct border* border);
void border_draw(struct border* border);

void border_destroy(struct border* border);

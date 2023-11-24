#pragma once
#include "helpers.h"
#include "hashtable.h"

#define BORDER_STYLE_ROUND  'r'
#define BORDER_STYLE_SQUARE 's'

#define BORDER_UPDATE_MASK_NONE      0
#define BORDER_UPDATE_MASK_ACTIVE   (1 << 0)
#define BORDER_UPDATE_MASK_INACTIVE (1 << 1)
#define BORDER_UPDATE_MASK_ALL      (BORDER_UPDATE_MASK_ACTIVE \
                                     | BORDER_UPDATE_MASK_INACTIVE)

typedef struct window_style {
  enum { GRADIENT, SOLID } stype;
  union style {
    struct gradient {
      uint32_t color1;
      uint32_t color2;
      enum { TOPLEFT_BOTTOMRIGHT, TOPRIGHT_BOTTOMLEFT } direction;
    } gradient;
    uint32_t color;
  } style;
} window_style;

struct settings {
  window_style active_window;
  window_style inactive_window;
  float border_width;
  char border_style;
};

struct border {
  bool focused;
  bool needs_redraw;
  bool disable;

  uint64_t sid;
  uint32_t wid;
  uint32_t target_wid;

  CGRect bounds;
  CGContextRef context;
};

void border_init(struct border* border);
void border_destroy(struct border* border);

void border_move(struct border* border);
void border_draw(struct border* border);
void border_hide(struct border* border);
void border_unhide(struct border* border);

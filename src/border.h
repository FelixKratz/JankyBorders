#pragma once
#include "helpers.h"
#include "hashtable.h"

#define BORDER_STYLE_ROUND  'r'
#define BORDER_STYLE_SQUARE 's'
#define BORDER_PADDING 8.0

struct gradient {
  enum { TL_TO_BR, TR_TO_BL } direction;
  uint32_t color1;
  uint32_t color2;
};

struct color_style {
  enum { COLOR_STYLE_GRADIENT, COLOR_STYLE_SOLID, COLOR_STYLE_GLOW } stype;
  union {
    uint32_t color;
    struct gradient gradient;
  };
};

struct settings {
  struct color_style active_window;
  struct color_style inactive_window;

  float border_width;
  char border_style;
  bool hidpi;

  struct table blacklist;
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
void border_destroy_window(struct border* border);
void border_destroy(struct border* border);

void border_move(struct border* border);
void border_draw(struct border* border);
void border_hide(struct border* border);
void border_unhide(struct border* border);

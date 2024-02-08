#pragma once
#include "misc/helpers.h"
#include "misc/window.h"
#include "hashtable.h"

#define BORDER_ORDER_ABOVE 1
#define BORDER_ORDER_BELOW -1
#define BORDER_STYLE_ROUND  'r'
#define BORDER_STYLE_SQUARE 's'
#define BORDER_PADDING 8.0
#define BORDER_TSMN 3.27f
#define BORDER_TSMW 8.f

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
  struct color_style background;

  float border_width;
  float blur_radius;
  char border_style;
  bool hidpi;
  bool show_background;
  int border_order;

  bool blacklist_enabled;
  struct table blacklist;

  bool whitelist_enabled;
  struct table whitelist;
};

struct border {
  bool focused;
  bool needs_redraw;
  bool too_small;
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

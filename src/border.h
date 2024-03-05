#pragma once
#include <pthread.h>
#include "misc/helpers.h"
#include "misc/window.h"
#include "misc/drawing.h"
#include "hashtable.h"

#define BORDER_ORDER_ABOVE 1
#define BORDER_ORDER_BELOW -1
#define BORDER_STYLE_ROUND  'r'
#define BORDER_STYLE_SQUARE 's'
#define BORDER_PADDING 8.0
#define BORDER_TSMN 3.27f
#define BORDER_TSMW 8.f
#define BORDER_RADIUS 9.f
#define BORDER_INNER_RADIUS 10.f

struct color_style {
  enum { COLOR_STYLE_GRADIENT, COLOR_STYLE_SOLID, COLOR_STYLE_GLOW } stype;
  union {
    uint32_t color;
    struct gradient gradient;
  };
};

struct settings {
  bool enabled;

  struct color_style active_window;
  struct color_style inactive_window;
  struct color_style background;

  float border_width;
  float blur_radius;
  char border_style;
  bool hidpi;
  bool show_background;
  int border_order;
  bool ax_focus;

  bool blacklist_enabled;
  struct table blacklist;

  bool whitelist_enabled;
  struct table whitelist;
};

struct border {
  pthread_mutex_t mutex;

  bool focused;
  bool needs_redraw;
  bool too_small;
  bool sticky;

  uint64_t sid;
  uint32_t wid;
  uint32_t target_wid;

  CGPoint origin;
  CGRect frame;
  CGRect target_bounds;
  CGRect drawing_bounds;
  CGContextRef context;

  bool disable_coalescing;
  int64_t last_coalesce_attempt;

  bool is_proxy;
  struct border* proxy;
  uint32_t external_proxy_wid;

  struct settings setting_override;
};

void border_init(struct border* border);
void border_destroy(struct border* border, int cid);
void border_create_window(struct border* border, int cid, CGRect frame, bool unmanaged, bool hidpi);

void border_move(struct border* border, int cid);
void border_update(struct border* border, int cid, bool try_async);
void border_hide(struct border* border, int cid);
void border_unhide(struct border* border, int cid);

struct settings* border_get_settings(struct border* border);

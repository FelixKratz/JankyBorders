#pragma once
#include "border.h"

// A layer backed border is a type 5 SkyLight window without a backing store,
// composited by the render server from a CALayer tree. The artwork is a small
// nine slice template stretched through CALayer.contentsCenter, so a resize
// changes a layer frame and rasterises nothing.

struct layer_border;

struct layer_style {
  struct color_style color;
  struct color_style background;
  bool show_background;
  float border_width;
  float corner_radius;
  float inner_radius;
  float outer_offset;
  char border_style;
  bool truly_square;
};

bool layer_supported(void);

// Returns NULL on failure, the caller then falls back to window_create
struct layer_border* layer_border_create(int cid, CGRect frame, bool unmanaged, bool hidpi, uint32_t* wid);

// Has to run before the window itself is released
void layer_border_destroy(struct layer_border* layer);

// Returns false if the artwork could not be rasterised
bool layer_border_update(struct layer_border* layer, const struct layer_style* style, CGSize size);

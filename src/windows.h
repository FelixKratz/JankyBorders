#pragma once
#include <stdlib.h>
#include "border.h"

struct windows {
  uint32_t num_windows;
  uint32_t* wids;
};

void windows_init(struct windows* windows);
void windows_add_existing_windows(int cid, struct windows* windows, struct borders* borders);
void windows_add_window(struct windows* windows, uint32_t wid);
bool windows_remove_window(struct windows* windows, uint32_t wid);

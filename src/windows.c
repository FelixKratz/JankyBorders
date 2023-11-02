#include "windows.h"
#include <string.h>

void windows_init(struct windows* windows) {
  memset(windows, 0, sizeof(struct windows));
}

void windows_add_window(struct windows* windows, uint32_t wid) {
  for (int i = 0; i < windows->num_windows; i++) {
    if (!windows->wids[i]) {
      windows->wids[i] = wid;
      return;
    };
  }

  windows->wids = (uint32_t*)realloc(windows->wids,
                                     sizeof(uint32_t)*++windows->num_windows);
  windows->wids[windows->num_windows - 1] = wid;
}

bool windows_remove_window(struct windows* windows, uint32_t wid) {
  for (int i = 0; i < windows->num_windows; i++) {
    if (windows->wids[i] == wid) {
      windows->wids[i] = 0;
      return true;
    }
  }
  return false;
}

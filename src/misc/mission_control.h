#pragma once
#include "helpers.h"
#include "window.h"
#include "../windows.h"
#include <libproc.h>

// Since macOS 27 Mission Control is drawn by the WindowManager process and the
// WindowServer no longer hides our border windows while it is shown. While
// Mission Control is open, WindowManager places a backdrop window at this
// level covering each display, so we hide all borders for as long as any of
// those backdrops exist.
#define MISSION_CONTROL_OWNER          "WindowManager"
#define MISSION_CONTROL_BACKDROP_LEVEL 19
#define MISSION_CONTROL_MAX_BACKDROPS  16

extern struct table g_windows;
extern bool g_mission_control_active;

static uint32_t g_mission_control_backdrops[MISSION_CONTROL_MAX_BACKDROPS];
static int g_mission_control_backdrop_count = 0;

static inline bool mission_control_window_exists(int cid, uint32_t wid) {
  int wid_cid = 0;
  return SLSGetWindowOwner(cid, wid, &wid_cid) == kCGErrorSuccess && wid_cid;
}

static inline bool mission_control_covers_display(CGRect bounds) {
  uint32_t count = 0;
  CGGetActiveDisplayList(0, NULL, &count);
  if (!count) return false;

  CGDirectDisplayID displays[count];
  CGGetActiveDisplayList(count, displays, &count);
  for (int i = 0; i < count; i++) {
    if (CGRectEqualToRect(bounds, CGDisplayBounds(displays[i]))) return true;
  }
  return false;
}

static inline bool mission_control_is_backdrop(int cid, uint32_t wid) {
  if (window_level(cid, wid) != MISSION_CONTROL_BACKDROP_LEVEL) return false;

  int wid_cid = 0;
  pid_t pid = 0;
  char name[PROC_PIDPATHINFO_MAXSIZE];
  SLSGetWindowOwner(cid, wid, &wid_cid);
  SLSConnectionGetPID(wid_cid, &pid);
  if (proc_name(pid, name, sizeof(name)) <= 0
      || strcmp(name, MISSION_CONTROL_OWNER) != 0) {
    return false;
  }

  CGRect bounds;
  SLSGetWindowBounds(cid, wid, &bounds);
  return mission_control_covers_display(bounds);
}

static inline int mission_control_find_backdrop(uint32_t wid) {
  for (int i = 0; i < g_mission_control_backdrop_count; i++) {
    if (g_mission_control_backdrops[i] == wid) return i;
  }
  return -1;
}

static inline void mission_control_exit() {
  debug("Mission Control Exit\n");
  g_mission_control_active = false;
  DELAY_ASYNC_EXEC_ON_MAIN_THREAD(50000, {
    windows_draw_borders_on_current_spaces(&g_windows);
    windows_determine_and_focus_active_window(&g_windows);
  });
}

static inline void mission_control_window_created(int cid, uint32_t wid) {
  if (mission_control_find_backdrop(wid) >= 0
      || g_mission_control_backdrop_count >= MISSION_CONTROL_MAX_BACKDROPS
      || !mission_control_is_backdrop(cid, wid)) {
    return;
  }

  g_mission_control_backdrops[g_mission_control_backdrop_count++] = wid;
  if (!g_mission_control_active) {
    debug("Mission Control Enter (backdrop %d)\n", wid);
    g_mission_control_active = true;
    windows_hide_all(&g_windows);
  }
}

// Drops backdrops that no longer exist. A destroy event is also sent when a
// backdrop merely moves between spaces, so we check the window itself.
static inline void mission_control_prune(int cid) {
  if (!g_mission_control_backdrop_count) return;

  int count = 0;
  for (int i = 0; i < g_mission_control_backdrop_count; i++) {
    uint32_t wid = g_mission_control_backdrops[i];
    if (mission_control_window_exists(cid, wid)) {
      g_mission_control_backdrops[count++] = wid;
    }
  }
  g_mission_control_backdrop_count = count;

  if (!count && g_mission_control_active) mission_control_exit();
}

static inline void mission_control_window_destroyed(int cid, uint32_t wid) {
  if (mission_control_find_backdrop(wid) < 0) return;
  mission_control_prune(cid);
}

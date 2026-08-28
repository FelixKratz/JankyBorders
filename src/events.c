#include "events.h"
#include "misc/extern.h"
#include "windows.h"
#include "border.h"
#include "misc/window.h"

#include <pthread.h>
#include <time.h>

extern struct table g_windows;
extern pid_t g_pid;

// Requests are coalesced into one pending run at the earliest deadline asked
// for, plus a trailing run at the latest one, so that a storm of events can
// neither push the run out nor drop the longer delay it asked for.
struct coalesced_job {
  bool pending;
  uint32_t generation;
  uint64_t deadline;
  uint64_t latest;
};

#define COALESCE_SLACK_NS (1 * NSEC_PER_MSEC)

static void schedule_coalesced(struct coalesced_job* job, uint64_t delay_us, dispatch_block_t action) {
  if (!pthread_main_np()) {
    dispatch_async(dispatch_get_main_queue(), ^{
      schedule_coalesced(job, delay_us, action);
    });
    return;
  }

  uint64_t delay_ns = delay_us * NSEC_PER_USEC;
  uint64_t deadline = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + delay_ns;

  if (job->latest < deadline) job->latest = deadline;

  if (job->pending && job->deadline <= deadline) return;

  job->pending = true;
  job->deadline = deadline;
  uint32_t generation = ++job->generation;

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)delay_ns),
                 dispatch_get_main_queue(), ^{
    if (job->generation != generation) return;
    job->pending = false;
    action();

    uint64_t now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    uint64_t latest = job->latest;
    job->latest = 0;
    if (latest > now + COALESCE_SLACK_NS) {
      schedule_coalesced(job, (latest - now) / NSEC_PER_USEC, action);
    }
  });
}

static struct coalesced_job g_focus_job;
static struct coalesced_job g_space_job;

static inline void schedule_focus_update(uint64_t delay_us) {
  schedule_coalesced(&g_focus_job, delay_us, ^{
    windows_determine_and_focus_active_window(&g_windows);
  });
}

static inline void schedule_space_update(uint64_t delay_us) {
  schedule_coalesced(&g_space_job, delay_us, ^{
    windows_draw_borders_on_current_spaces(&g_windows);
  });
}

#ifdef DEBUG
static void dump_event(void* data, size_t data_length) {
  for (int i = 0; i < data_length; i++) {
    printf("%02x ", *((unsigned char*)data + i));
  }
  printf("\n");
}

static void event_watcher(uint32_t event, void* data, size_t data_length, void* context) {
  static int count = 0;
  printf("(%d) Event: %d; Payload:\n", ++count, event);
  dump_event(data, data_length);
}
#endif

struct window_spawn_data {
  uint64_t sid;
  uint32_t wid;
};

static bool is_own_window(int cid, uint32_t wid) {
  int wid_cid = 0;
  SLSGetWindowOwner(cid, wid, &wid_cid);
  pid_t pid = 0;
  SLSConnectionGetPID(wid_cid, &pid);
  return pid == g_pid;
}

static void window_spawn_handler(uint32_t event, struct window_spawn_data* data, size_t _, int cid) {
  struct table* windows = &g_windows;
  uint32_t wid = data->wid;
  uint64_t sid = data->sid;

  if (!wid || !sid || is_own_window(cid, wid)) return;

  if (event == EVENT_WINDOW_CREATE) {
    if (windows_window_create(windows, wid, sid)) {
      debug("Window Created: %d %d\n", wid, sid);
      windows_determine_and_focus_active_window(windows);
    }
  } else if (event == EVENT_WINDOW_DESTROY) {
    if (windows_window_destroy(windows, wid, sid)) {
      debug("Window Destroyed: %d %d\n", wid, sid);
    }
    windows_determine_and_focus_active_window(windows);
  }
}

static void window_modify_handler(uint32_t event, uint32_t* window_id, size_t _, int cid) {
  uint32_t wid = *window_id;
  struct table* windows = &g_windows;

  if (is_own_window(cid, wid)) return;

  if (event == EVENT_WINDOW_MOVE) {
    debug("Window Move: %d\n", wid);
    windows_window_move(windows, wid);
  } else if (event == EVENT_WINDOW_RESIZE) {
    debug("Window Resize: %d\n", wid);
    windows_window_update(windows, wid);
  } else if (event == EVENT_WINDOW_REORDER) {
    debug("Window Reorder (and focus): %d\n", wid);
    windows_window_refresh(windows, wid);
    schedule_focus_update(10000);
  } else if (event == EVENT_WINDOW_LEVEL) {
    debug("Window Level: %d\n", wid);
    windows_window_refresh(windows, wid);
  } else if (event == EVENT_WINDOW_TITLE || event == EVENT_WINDOW_UPDATE) {
    debug("Window Focus\n");
    schedule_focus_update(50000);
  } else if (event == EVENT_WINDOW_UNHIDE) {
    debug("Window Unhide: %d\n", wid);
    windows_window_unhide(windows, wid);
  } else if (event == EVENT_WINDOW_HIDE) {
    debug("Window Hide: %d\n", wid);
    windows_window_hide(windows, wid);
  } else if (event == EVENT_WINDOW_CLOSE) {
    debug("Window Close: %d\n", wid);
    windows_window_destroy(windows, wid, 0);
  }
}

static void front_app_handler() {
  debug("Window Focus\n");
  schedule_focus_update(50000);
}

static void space_handler() {
  // Not all native-fullscreen windows have yet updated their space id...
  schedule_space_update(20000);
}

void events_register(int cid) {
  void* cid_ctx = (void*)(intptr_t)cid;

  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_CLOSE, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_MOVE, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_RESIZE, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_LEVEL, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_UNHIDE, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_HIDE, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_TITLE, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_REORDER, cid_ctx);
  SLSRegisterNotifyProc(window_modify_handler, EVENT_WINDOW_UPDATE, cid_ctx);
  SLSRegisterNotifyProc(window_spawn_handler, EVENT_WINDOW_CREATE, cid_ctx);
  SLSRegisterNotifyProc(window_spawn_handler, EVENT_WINDOW_DESTROY, cid_ctx);

  SLSRegisterNotifyProc(space_handler, EVENT_SPACE_CHANGE, cid_ctx);

  SLSRegisterNotifyProc(front_app_handler, EVENT_FRONT_CHANGE, cid_ctx);

#ifdef DEBUG
  for (int i = 0; i < 2000; i++) {
    SLSRegisterNotifyProc(event_watcher, i, NULL);
  }
#endif
}

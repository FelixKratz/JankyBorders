#include "events.h"
#include "windows.h"
#include "border.h"

extern struct table g_windows;
extern pid_t g_pid;

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

static void window_spawn_handler(uint32_t event, void* data, size_t data_length, void* context) {
  struct table* windows = &g_windows;
  int cid = (intptr_t)context;
  uint32_t wid = *(uint32_t *)(data + 0x8);
  uint64_t sid = *(uint64_t *)data;

  if (!wid || !sid) return;

  int wid_cid = 0;
  SLSGetWindowOwner(cid, wid, &wid_cid);

  if (event == EVENT_WINDOW_CREATE) {
    pid_t pid = 0;
    SLSConnectionGetPID(wid_cid, &pid);
    if (pid == g_pid) return;

    CFArrayRef target_ref = cfarray_of_cfnumbers(&wid,
                                                 sizeof(uint32_t),
                                                 1,
                                                 kCFNumberSInt32Type);

    if (!target_ref) return;

    CFTypeRef query = SLSWindowQueryWindows(cid, target_ref, 1);
    if (query) {
      CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);
      if (iterator && SLSWindowIteratorGetCount(iterator) > 0) {
        if (SLSWindowIteratorAdvance(iterator)) {
          ITERATOR_WINDOW_SUITABLE(iterator, {
            windows_window_create(windows, wid, sid);
            windows_update_notifications(windows);
          });
        }
      }
      if (iterator) CFRelease(iterator);
      CFRelease(query);
    }
    CFRelease(target_ref);
  } else if (event == EVENT_WINDOW_DESTROY) {
    printf("Window Destroyed: %d\n", wid);
    if (windows_window_destroy(windows, wid, sid)) {
      windows_update_notifications(windows);
    }

    windows_window_focus(windows, get_front_window());
  }
}

static void window_modify_handler(uint32_t event, void* data, size_t data_length, void* context) {
  int cid = (intptr_t)context;
  uint32_t wid = *(uint32_t *)(data);
  struct table* windows = &g_windows;

  if (event == EVENT_WINDOW_MOVE) {
    printf("Window Move: %d\n", wid);
    windows_window_move(windows, wid);
  } else if (event == EVENT_WINDOW_RESIZE) {
    windows_window_update(windows, wid);
  } else if (event == EVENT_WINDOW_REORDER) {
    printf("Window Reorder: %d\n", wid);

    // The update of the front window might not have taken place yet...
    usleep(10000);

    windows_window_focus(windows, get_front_window());
    windows_window_update(windows, wid);
  } else if (event == EVENT_WINDOW_LEVEL) {
    printf("Window Level: %d\n", wid);
    windows_window_update(windows, wid);
  } else if (event == EVENT_WINDOW_TITLE || event == EVENT_WINDOW_UPDATE) {
    wid = get_front_window();
    printf("Window Focus: %d\n", wid);
    windows_window_focus(windows, wid);
  } else if (event == EVENT_WINDOW_UNHIDE) {
    printf("Window Unhide: %d\n", wid);
    windows_window_unhide(windows, wid);
  } else if (event == EVENT_WINDOW_HIDE) {
    printf("Window Hide: %d\n", wid);
    windows_window_hide(windows, wid);
  }
}

static void space_handler(uint32_t event, void* data, size_t data_length, void* context) {
  windows_draw_borders_on_current_spaces(&g_windows);
}

void events_register() {
  int cid = SLSMainConnectionID();
  void* cid_ctx = (void*)(intptr_t)cid;

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

  // for (int i = 0; i < 2000; i++) {
  //   SLSRegisterNotifyProc(event_watcher, i, NULL);
  // }
}

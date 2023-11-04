#include "hashtable.h"
#include "events.h"
#include "windows.h"
#include <stdio.h>

#define BORDER_UPDATE_MASK_NONE     0
#define BORDER_UPDATE_MASK_ACTIVE   (1 << 0)
#define BORDER_UPDATE_MASK_INACTIVE (1 << 1)
#define BORDER_UPDATE_MASK_ALL      (BORDER_UPDATE_MASK_INACTIVE \
                                     | BORDER_UPDATE_MASK_ACTIVE)

pid_t g_pid;
struct table g_windows;
struct settings g_settings;

static TABLE_HASH_FUNC(hash_windows)
{
  return *(uint32_t *) key;
}

static TABLE_COMPARE_FUNC(cmp_windows)
{
  return *(uint32_t *) key_a == *(uint32_t *) key_b;
}

static void acquire_lockfile(void) {
  char *user = getenv("USER");
  if (!user) {
    error("JankyBorders: 'env USER' not set! abort..\n");
  }

  char lock_file[4096] = {};
  snprintf(lock_file, sizeof(lock_file), "/tmp/JankyBorders_%s.lock", user);

  int handle = open(lock_file, O_CREAT | O_WRONLY, 0600);
  if (handle == -1) {
    error("JankyBorders: could not create lock-file! abort..\n");
  }

  struct flock lockfd = {
    .l_start  = 0,
    .l_len    = 0,
    .l_pid    = g_pid,
    .l_type   = F_WRLCK,
    .l_whence = SEEK_SET
  };

  if (fcntl(handle, F_SETLK, &lockfd) == -1) {
    error("JankyBorders: could not acquire lock-file! already running?\n");
  }
}

void callback(CFMachPortRef port, void* message, CFIndex size, void* context) {
  int cid = SLSMainConnectionID();
  CGEventRef event = SLEventCreateNextEvent(cid);
  if (!event) return;
  do {
    CGEventType type = CGEventGetType(event);
    CFRelease(event);
    event = SLEventCreateNextEvent(cid);
  } while (event != NULL);
}

static int parse_settings(struct settings* settings, int count, char** arguments) {
  int update_mask = 0;
  for (int i = 0; i < count; i++) {
    if (sscanf(arguments[i],
               "active_color=0x%x",
               &settings->active_window_color) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ACTIVE;
      continue;
    }
    else if (sscanf(arguments[i],
             "inactive_color=0x%x",
             &settings->inactive_window_color) == 1) {
      update_mask |= BORDER_UPDATE_MASK_INACTIVE;
      continue;
    }
    else if (sscanf(arguments[i], "width=%f", &settings->border_width) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
      continue;
    }
    else if (sscanf(arguments[i], "style=%c", &settings->border_style) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
      continue;
    } else {
      printf("[?] Borders: Invalid argument '%s'\n", arguments[i]);
    }
  }
  return update_mask;
}

int main(int argc, char** argv) {
  if (argc > 1) {
    parse_settings(&g_settings, argc - 1, argv + 1);
  }

  pid_for_task(mach_task_self(), &g_pid);
  acquire_lockfile();
  table_init(&g_windows, 1024, hash_windows, cmp_windows);
  events_register();

  SLSWindowManagementBridgeSetDelegate(NULL);
  mach_port_t port;
  CGError error = SLSGetEventPort(SLSMainConnectionID(), &port);
  CFMachPortRef cf_mach_port = NULL;
  CFRunLoopSourceRef source = NULL;
  if (error == kCGErrorSuccess) {
    CFMachPortRef cf_mach_port = CFMachPortCreateWithPort(NULL,
                                                          port,
                                                          callback,
                                                          NULL,
                                                          false    );

    _CFMachPortSetOptions(cf_mach_port, 0x40);
    CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL,
                                                              cf_mach_port,
                                                              0            );

    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
    CFRelease(cf_mach_port);
    CFRelease(source);
  }

  windows_add_existing_windows(&g_windows);
  CFRunLoopRun();

  return 0;
}


#include "border.h"
#include "hashtable.h"
#include "events.h"
#include "misc/extern.h"
#include "windows.h"
#include "mach.h"
#include "parse.h"
#include "misc/connection.h"
#include "misc/ax.h"
#include "misc/yabai.h"
#include <stdio.h>

#define VERSION_OPT_LONG "--version"
#define VERSION_OPT_SHRT "-v"

#define HELP_OPT_LONG "--help"
#define HELP_OPT_SHRT "-h"

#define MAJOR 1
#define MINOR 8
#define PATCH 2

pid_t g_pid;
mach_port_t g_server_port;
struct table g_windows;
struct mach_server g_mach_server;
struct settings g_settings = { .enabled = true,
                               .active_window = { .stype = COLOR_STYLE_SOLID,
                                                  .color = 0xffe1e3e4 },
                               .inactive_window = { .stype = COLOR_STYLE_SOLID,
                                                    .color =  0x00000000 },
                               .background = { .stype = COLOR_STYLE_SOLID,
                                               .color = 0x00000000         },
                               .border_width = 4.f,
                               .blur_radius = 0,
                               .border_style = BORDER_STYLE_ROUND,
                               .hidpi = false,
                               .show_background = false,
                               .border_order = BORDER_ORDER_BELOW,
                               .ax_focus = false,
                               .blacklist_enabled = false,
                               .whitelist_enabled = false                    };

static TABLE_HASH_FUNC(hash_windows) {
  return *(uint32_t *) key;
}

static TABLE_COMPARE_FUNC(cmp_windows) {
  return *(uint32_t *) key_a == *(uint32_t *) key_b;
}

static TABLE_HASH_FUNC(hash_blacklist) {
  // djb2 by Dan Bernstein
  unsigned long hash = 5381;
  char c;
  while((c = *((char*)key++))) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

static TABLE_COMPARE_FUNC(cmp_blacklist) {
  return strcmp((char*)key_a, (char*)key_b) == 0;
}

static void message_handler(void* data, uint32_t len) {
  char* message = data;
  uint32_t update_mask = 0;
  struct settings settings = g_settings;

  while(message && *message) {
    update_mask |= parse_settings(&settings, 1, &message);
    message += strlen(message) + 1;
  }

  if (settings.apply_to > 0) {
    struct border* border = table_find(&g_windows, &settings.apply_to);
    if (border) {
      border->setting_override = settings;
      border->setting_override.enabled = true;
      border->needs_redraw = true;
      border_update(border, true);
    }
    return;
  } else {
    g_settings = settings;
    for (int i = 0; i < g_windows.capacity; ++i) {
      struct bucket* bucket = g_windows.buckets[i];
      while (bucket) {
        if (bucket->value) {
          struct border* border = bucket->value;
          if (border->setting_override.enabled) {
            char* message = data;
            uint32_t window_update_mask = 0;
            while(message && *message) {
              window_update_mask |= parse_settings(&border->setting_override,
                                                   1,
                                                   &message                  );
              message += strlen(message) + 1;
            }

            if (window_update_mask
                && !((update_mask & BORDER_UPDATE_MASK_ALL)
                     || (update_mask & BORDER_UPDATE_MASK_RECREATE_ALL))) {
              border->needs_redraw = true;
              border_update(border, true);
            }
          }
        }
        bucket = bucket->next;
      }
    }
  }

  if (update_mask & BORDER_UPDATE_MASK_RECREATE_ALL) {
    windows_recreate_all_borders(&g_windows);
  } else if (update_mask & BORDER_UPDATE_MASK_ALL) {
    windows_update_all(&g_windows);
  } else if (update_mask & BORDER_UPDATE_MASK_ACTIVE) {
    windows_update_active(&g_windows);
  } else if (update_mask & BORDER_UPDATE_MASK_INACTIVE) {
    windows_update_inactive(&g_windows);
  }
}

static void send_args_to_server(mach_port_t port, int argc, char** argv) {
  int message_length = argc;
  int argl[argc];

  for (int i = 1; i < argc; i++) {
    argl[i] = strlen(argv[i]);
    message_length += argl[i] + 1;
  }

  char message[(sizeof(char) * message_length)];
  char* temp = message;

  for (int i = 1; i < argc; i++) {
    memcpy(temp, argv[i], argl[i]);
    temp += argl[i];
    *temp++ = '\0';
  }
  *temp++ = '\0';

  mach_send_message(port, message, message_length);
}

static void event_callback(CFMachPortRef port, void* message, CFIndex size, void* context) {
  int cid = SLSMainConnectionID();
  CGEventRef event = SLEventCreateNextEvent(cid);
  if (!event) return;
  do {
    CFRelease(event);
    event = SLEventCreateNextEvent(cid);
  } while (event);
}

int main(int argc, char** argv) {
  if (argc > 1 && ((strcmp(argv[1], VERSION_OPT_LONG) == 0)
                   || (strcmp(argv[1], VERSION_OPT_SHRT) == 0))) {
    fprintf(stdout, "borders-v%d.%d.%d\n", MAJOR, MINOR, PATCH);
    exit(EXIT_SUCCESS);
  }

  if (argc > 1 && ((strcmp(argv[1], HELP_OPT_LONG) == 0)
                   || (strcmp(argv[1], HELP_OPT_SHRT) == 0))) {
    fprintf(stdout, "Refer to the man page for help: man borders\n");
    exit(EXIT_SUCCESS);
  }

  table_init(&g_settings.blacklist, 64, hash_blacklist, cmp_blacklist);
  table_init(&g_settings.whitelist, 64, hash_blacklist, cmp_blacklist);
  g_settings.ax_focus = ax_check_trust(true);

  uint32_t update_mask = parse_settings(&g_settings, argc - 1, argv + 1);
  mach_port_t server_port = mach_get_bs_port(BS_NAME);
  if (server_port && update_mask) {
    send_args_to_server(server_port, argc, argv);
    return 0;
  } else if (server_port) {
    error("A borders instance is already running and no valid arguments"
          " where provided. To modify properties of the running instance"
          " provide them as arguments.\n");
  }

  pid_for_task(mach_task_self(), &g_pid);
  table_init(&g_windows, 1024, hash_windows, cmp_windows);

  g_server_port = create_connection_server_port();

  int cid = SLSMainConnectionID();
  events_register(cid);

  mach_port_t port;
  CGError err = SLSGetEventPort(cid, &port);
  if (err == kCGErrorSuccess) {
    CFMachPortRef cf_mach_port = CFMachPortCreateWithPort(NULL,
                                                          port,
                                                          event_callback,
                                                          NULL,
                                                          false          );

    _CFMachPortSetOptions(cf_mach_port, 0x40);
    CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL,
                                                              cf_mach_port,
                                                              0            );

    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
    CFRelease(cf_mach_port);
    CFRelease(source);
  }

  windows_add_existing_windows(&g_windows);

  mach_server_begin(&g_mach_server, message_handler);
  if (!update_mask) execute_config_file("borders", "bordersrc");

  #ifdef _YABAI_INTEGRATION
  yabai_register_mach_port(&g_windows);
  #endif
  CFRunLoopRun();
  return 0;
}

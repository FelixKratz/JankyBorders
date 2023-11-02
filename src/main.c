#include "events.h"
#include "windows.h"
#include <stdio.h>

struct windows g_windows;
struct borders g_borders;
pid_t g_pid;
uint32_t g_active_window_color = 0xffe1e3e4;
uint32_t g_inactive_window_color = 0xff494d64;
float g_border_width = 4.f;

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

extern CGError SLSWindowManagementBridgeSetDelegate(void* delegate);
int main(int argc, char** argv) {
  if (argc == 4) {
    if (sscanf(argv[1], "active_color=0x%x", &g_active_window_color) != 1) {
      printf("Invalid first argument\n");
      return 1;
    }

    if (sscanf(argv[2], "inactive_color=0x%x", &g_inactive_window_color) != 1) {
      printf("Invalid second argument\n");
      return 1;
    }

    if (sscanf(argv[3], "width=%f", &g_border_width) != 1) {
      printf("Invalid third argument\n");
      return 1;
    }
  } else if (argc > 1) {
    printf("Usage: <binary> <active_color_hex>"
           " <inactive_color_hex> <border_width_float>\n");
    return 1;
  }

  pid_for_task(mach_task_self(), &g_pid);
  borders_init(&g_borders);
  windows_init(&g_windows);
  events_register();
  windows_add_existing_windows(SLSMainConnectionID(), &g_windows, &g_borders);

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

  CFRunLoopRun();

  return 0;
}


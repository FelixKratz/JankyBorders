#pragma once
#include <mach/mach.h>

#define BS_NAME "git.felix.borders"

struct mach_message {
  mach_msg_header_t header;
  mach_msg_size_t msgh_descriptor_count;
  mach_msg_ool_descriptor_t descriptor;
};

#define MACH_HANDLER(name) void name(void* message, uint32_t len)
typedef MACH_HANDLER(mach_handler);

struct mach_server {
  bool is_running;
  mach_port_name_t task;
  mach_port_t port;
  mach_port_t bs_port;

  mach_handler* handler;
};

mach_port_t mach_get_bs_port(char* bs_name);
bool mach_server_begin(struct mach_server* mach_server, mach_handler handler);
void mach_send_message(mach_port_t port, void* message, uint32_t len);

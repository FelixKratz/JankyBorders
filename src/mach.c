#include <bootstrap.h>
#include <CoreFoundation/CoreFoundation.h>
#include "mach.h"

mach_port_t mach_get_bs_port(char* bs_name) {
  mach_port_name_t task = mach_task_self();

  mach_port_t bs_port = 0;
  if (task_get_special_port(task,
                            TASK_BOOTSTRAP_PORT,
                            &bs_port            ) != KERN_SUCCESS) {
    return 0;
  }

  mach_port_t port = 0;
  if (bootstrap_look_up(bs_port,
                        bs_name,
                        &port   ) != KERN_SUCCESS) {
    return 0;
  }

  return port;
}

void mach_send_message(mach_port_t port, void* message, uint32_t len) {
  if (!message || !port) return;

  struct mach_message msg = { 0 };
  msg.header.msgh_remote_port = port;
  msg.header.msgh_bits = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_COPY_SEND
                                            & MACH_MSGH_BITS_REMOTE_MASK,
                                            0,
                                            0,
                                            MACH_MSGH_BITS_COMPLEX       );

  msg.header.msgh_size = sizeof(struct mach_message);

  msg.msgh_descriptor_count = 1;
  msg.descriptor.address = message;
  msg.descriptor.size = len;
  msg.descriptor.copy = MACH_MSG_VIRTUAL_COPY;
  msg.descriptor.deallocate = false;
  msg.descriptor.type = MACH_MSG_OOL_DESCRIPTOR;

  mach_msg(&msg.header,
           MACH_SEND_MSG,
           sizeof(struct mach_message),
           0,
           MACH_PORT_NULL,
           MACH_MSG_TIMEOUT_NONE,
           MACH_PORT_NULL             );
}

void mach_message_callback(CFMachPortRef port, void* data, CFIndex size, void* context) {
  struct mach_server* mach_server = context;
  struct mach_message* message = data;
  mach_server->handler(message->descriptor.address, message->descriptor.size);
  mach_msg_destroy(&message->header);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool mach_register_port(mach_port_t port, char* name) {
  mach_port_name_t task = mach_task_self();
  mach_port_t bs_port = 0;

  if (task_get_special_port(task,
                            TASK_BOOTSTRAP_PORT,
                            &bs_port) != KERN_SUCCESS) {
    return false;
  }

  if (bootstrap_register(bs_port, name, port) != KERN_SUCCESS) {
    return false;
  }

  return true;
}


bool mach_server_begin(struct mach_server* mach_server, mach_handler handler) {
  mach_server->task = mach_task_self();

  if (mach_port_allocate(mach_server->task,
                         MACH_PORT_RIGHT_RECEIVE,
                         &mach_server->port      ) != KERN_SUCCESS) {
    return false;
  }

  struct mach_port_limits limits = {};
  limits.mpl_qlimit = MACH_PORT_QLIMIT_LARGE;

  if (mach_port_set_attributes(mach_server->task,
                               mach_server->port,
                               MACH_PORT_LIMITS_INFO,
                               (mach_port_info_t)&limits,
                               MACH_PORT_LIMITS_INFO_COUNT) != KERN_SUCCESS) {
    return false;
  }

  if (mach_port_insert_right(mach_server->task,
                             mach_server->port,
                             mach_server->port,
                             MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
    return false;
  }

  if (!mach_register_port(mach_server->port, BS_NAME)) return false;

  mach_server->handler = handler;
  mach_server->is_running = true;

  CFMachPortContext context = {0, (void*)mach_server};

  CFMachPortRef cf_mach_port = CFMachPortCreateWithPort(NULL,
                                                        mach_server->port,
                                                        mach_message_callback,
                                                        &context,
                                                        false                );

  CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL,
                                                            cf_mach_port,
                                                            0            );

  CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
  CFRelease(source);
  CFRelease(cf_mach_port);
  return true;
}
#pragma clang diagnostic pop

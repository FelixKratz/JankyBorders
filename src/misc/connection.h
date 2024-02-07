#include "extern.h"
#include <mach/mach.h>

mach_port_t create_connection_server_port() {
  #pragma pack(push,2)
  struct {
    mach_msg_header_t header;

    int32_t magic1;
    mach_port_t server_port;
    int32_t padding;

    int32_t magic2;
    int64_t padding2;

    int64_t magic3;
    int32_t padding3;
    int32_t cid;

    int32_t bundle_size;
    char bundle_name[0x8];
    int32_t connection_count;
    int32_t launched_before_login;
    int32_t is_ui_process;
    int32_t context_id;
  } msg = { 0 };
  #pragma pack(pop)

  msg.header.msgh_bits = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_COPY_SEND,
                                            MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                            0,
                                            MACH_MSGH_BITS_REMOTE_MASK
                                            | MACH_MSGH_BITS_COMPLEX     );

  msg.header.msgh_local_port = mig_get_special_reply_port(); 
  msg.header.msgh_remote_port = SLSServerPort(0);
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_id = 0x7468;

  msg.magic1 = 2;
  msg.magic2 = 0x110000;
  msg.magic3 = 0x110000;

  snprintf(msg.bundle_name, sizeof(msg.bundle_name), "borders");
  msg.bundle_size = sizeof(msg.bundle_name);

  kern_return_t error = mach_msg(&msg.header,
                                 MACH_SEND_MSG
                                 | MACH_SEND_SYNC_OVERRIDE
                                 | MACH_SEND_PROPAGATE_QOS
                                 | MACH_RCV_MSG
                                 | MACH_RCV_SYNC_WAIT,
                                 sizeof(msg),
                                 0x48,
                                 msg.header.msgh_local_port,
                                 0,
                                 0                          );

  if (error != KERN_SUCCESS) {
    printf("Connection: Error receiving message\n");
    mig_dealloc_special_reply_port(msg.header.msgh_local_port);
    return 0;
  }

  if (msg.header.msgh_id != 0x74cc
      || msg.header.msgh_size != 0x40
      || msg.magic1 != 2
      || !(msg.magic2 & 0x110000)
      || !(msg.magic3 & 0x110000)) {
    printf("Connection: Wrong message received.\n");
    mach_msg_destroy(&msg.header);
    return 0;
  }

  return msg.server_port;
}

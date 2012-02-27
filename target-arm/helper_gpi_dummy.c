#include "debug_ch.h"
MULTI_DEBUG_CHANNEL(qemu, arm_dummy);

int call_gpi(int pid, int call_num, char *in_args, int args_len, char *r_buffer, int r_length);

int call_gpi(int pid, int call_num, char *in_args, int args_len, char *r_buffer, int r_length){
    ERR("virtio -> call_gpi(arm_dummy) called!!!\n");
    return 0;
}

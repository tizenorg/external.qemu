#include "qemu-common.h"
#include "opengl_func.h"

extern int decode_call(CPUState *env, int func_number, int pid, target_ulong target_ret_string, target_ulong in_args, target_ulong in_args_size);
extern void helper_opengl(CPUState *env);

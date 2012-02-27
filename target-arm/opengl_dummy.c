#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "debug_ch.h"

MULTI_DEBUG_CHANNEL(qemu, arm_dummy);

void *init_opengl_server(void *arg);

void *init_opengl_server(void *arg){
    ERR("init_open_gl_server(arm_dummy) called!!!\n");
    return 0;
}

#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
void opengl_exec_set_parent_window(Display* _dpy, Window _parent_window);
void opengl_exec_set_parent_window(Display* _dpy, Window _parent_window)
{
    ERR("opengl_exec_set_parent_window(arm_dummy) called!!!\n");
}
#endif

/*
 * Emulator
 *
 * Copyright (C) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * DoHyung Hong <don.hong@samsung.com>
 * SeokYeon Hwang <syeon.hwang@samsung.com>
 * Hyunjun Son <hj79.son@samsung.com>
 * SangJin Kim <sangjin3.kim@samsung.com>
 * MunKyu Im <munkyu.im@samsung.com>
 * KiTae Kim <kt920.kim@samsung.com>
 * JinHyung Jo <jinhyung.jo@samsung.com>
 * SungMin Ha <sungmin82.ha@samsung.com>
 * JiHye Kim <jihye1128.kim@samsung.com>
 * GiWoong Kim <giwoong.kim@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 * DongKyun Yun <dk77.yun@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

/**
 * @file emulator.h
 * @brief - header of file these are config struecture and defines in emulator
 */

#ifndef __EMULATOR_H__
#define __EMULATOR_H__

#define ISE_TOOLKIT_NUM             5

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>

#include <stdio.h>
#include <locale.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <glib.h>
#include <glib/gstdio.h>
#ifndef _WIN32
#include <pthread.h>
#else
#include <windows.h>
#include <glib/gthread.h>
#include <process.h>
#endif

typedef struct _arglist arglist;

#include "utils.h"
#include "configuration.h"
#include "emulsignal.h"
#include "tools.h"
#include "fileio.h"
#include "extern.h"
#include "menu.h"
#include "vinit_process.h"
#include "qemu_gtk_widget.h"
#include "event_handler.h"

void append_argvlist(arglist* al, const char *fmt, ...) __attribute((__format__(__printf__,2,3)));

enum {
    EMUL_BOOTING = 0, //not used yet
    EMUL_NORMAL, //not used yet
    EMUL_SHUTTING_DOWN,
};

extern void save_emulator_state(void);
extern void exit_emulator(void);
extern gboolean  update_progress_bar(GIOChannel *, GIOCondition , gpointer);

extern CONFIGURATION configuration;     /**<    configuration structure which hold system-wide information during running   */
extern SYSINFO SYSTEMINFO;
extern STARTUP_OPTION startup_option;   /**<    command line option structure which hold some command line option information   */
extern PHONEMODELINFO *phone_info;

extern GtkWidget *EventItem1;
extern GtkWidget *EventItem2;

int get_emulator_condition(void);
void set_emulator_condition(int state);
int emul_create_process(const gchar cmd[]);
void emul_kill_all_process(void);
extern int qemu_arch_is_arm(void); /* hack */
int device_set_rotation(int rotation);
int socket_init(void);
void exit_emulator_post_process( void );
int make_shdmem(void);
#endif

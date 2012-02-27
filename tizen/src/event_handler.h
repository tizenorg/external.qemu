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

#ifndef _EVENT_HANDLER__H_
#define _EVENT_HANDLER_H_

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "qemu_gtk_widget.h"
#include "ui_imageid.h"
#include "defines.h"
#include "emulator.h"

/* generic keyboard conversion */
#include "../../ui/sdl_keysym.h"
#include "../../ui/keymaps.h"

#ifndef _WIN32
#include <signal.h>
#endif

extern GtkWidget *xo_popup_menu;

#define X_POSITION  PHONE.mode[0].key_map_list[i].key_map_region.x
#define Y_POSITION  PHONE.mode[0].key_map_list[i].key_map_region.y
#define W_POSITION  PHONE.mode[0].key_map_list[i].key_map_region.w
#define H_POSITION  PHONE.mode[0].key_map_list[i].key_map_region.h

#define KEYCODE     PHONE.mode[0].key_map_list[i].event_info[j].event_value[j].key_code

//#define skin_scale    PHONE.mode[0].lcd_list[0].lcd_region.s

gboolean key_event_handler(GtkWidget *wid, GdkEventKey *event);
gint motion_notify_event_handler(GtkWidget *widget, GdkEventButton *event, gpointer data);
gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data);
gboolean query_tooltip_event(GtkWidget *widget, gint x, gint y, gboolean keyboard_tip, GtkTooltip *tooltip, gpointer data);

/* vl.c */
void kbd_put_keycode(int keycode);
void ps2kbd_put_keycode(int keycode);

#endif

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
 * @file menu_callback.h
 * @brief - header file for event part of emulator
 */

#ifndef MENU_CALLBACK_H
#define MENU_CALLBACK_H
#ifdef  __cplusplus
extern "C" {
#endif

#include <string.h>

#include "option.h"
#include "extern.h"
#include "vinit_process.h"


/* callback from popup menu */
void menu_create_eiwidget_callback(GtkWidget *widget, GtkWidget *menu);

void menu_open_app_callback(GtkWidget *widget, gpointer data);
void menu_open_recent_app_callback(GtkWidget *widget, gpointer data);
int menu_option_callback(GtkWidget *widget, gpointer data);
int menu_frame_rate_callback(void);
int mask_main_lcd(GtkWidget *widget, PHONEMODELINFO *pDev, CONFIGURATION *pconfiguration, int nMode);
void rotate_event_callback(PHONEMODELINFO *device, int nMode);
void scale_event_callback(PHONEMODELINFO *device, int nMode);
void menu_rotate_callback(PHONEMODELINFO *device, int nMode);
void menu_keyboard_callback(GtkWidget *widget, gpointer data);
void menu_event_callback(GtkWidget *widget, gpointer data);
void menu_device_info_callback(GtkWidget *widget, gpointer data);
void show_about_window(GtkWidget *parent);
void menu_about_callback(GtkWidget *widget, gpointer data);

int sort_recent_app_list(CONFIGURATION *pconfiguration);
void do_shutdown(void);
void show_info_window(GtkWidget *widget, gpointer data);

extern int keyboard_state;
#ifdef __cplusplus
}
#endif
#endif /* ifndef MENU_CALLBACK_H */
/**
 * vim:set tabstop=4 shiftwidth=4 foldmethod=marker wrap:
 *
 */


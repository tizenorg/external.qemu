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


#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <string.h>
#include <glib-object.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <libgen.h>

#ifndef _WIN32
#include <error.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
//#include <unistd.h>

#include "defines.h"
#include "extern.h"
#include "ui_imageid.h"
#include "utils.h"
#include "fileio.h"

typedef struct _Target_list
{
    gchar target[128];  
    struct _Target_list *next;
} Target_list;
extern Target_list target_list;

extern GtkWidget *entry_user_cmd;
extern GtkWidget *frame_buffer_entry;
extern CONFIGURATION preference_entrys;
  
void kernel_select_cb (GtkButton *file_chooser_button, gpointer data);
void sdcard_select_cb (GtkButton *file_chooser_button, gpointer data);
gchar* get_png_from_dbi(gchar *filename);
void show_preview_image_cb (GtkFileChooser *file_chooser_button, gpointer entry);
void skin_select_cb (GtkButton *file_chooser_button, gpointer entry);
void virtual_target_select_cb(GtkComboBox *virtual_target_combobox, gpointer data);
void scale_select_cb (GtkWidget *widget, gpointer data);

void set_telnet_status_active_cb(void);
void set_sdcard_status_active_cb(void);
#ifndef ENABLE_GENERIC
void no_wait_connection_cb(void);
#endif
void serial_console_command_cb(void);
void use_host_proxy_cb(void);
void use_host_DNS_cb(void);
void always_on_top_cb(GtkWidget *widget, gpointer data);
void snapshot_boot_cb(void);
void default_clicked_cb(GtkWidget *widget, gpointer entry);

int ok_clicked_cb(GtkWidget *widget, gpointer entry);
gboolean emulator_deleted_callback (void);


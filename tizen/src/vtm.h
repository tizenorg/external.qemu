/*
 * Emulator Manager
 *
 * Copyright (C) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: 
 * MunKyu Im <munkyu.im@samsung.com>
 * DoHyung Hong <don.hong@samsung.com>
 * SeokYeon Hwang <syeon.hwang@samsung.com>
 * Hyunjun Son <hj79.son@samsung.com>
 * SangJin Kim <sangjin3.kim@samsung.com>
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


#ifndef __VTM_H__
#define __VTM_H__

#include <gtk/gtk.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "defines.h"
#include "ui_imageid.h"
#include "fileio.h"
#include "vt_utils.h"
#include "utils.h"
#include "dialog.h"
#define MAX_LEN 1024

gchar *remove_chars(const gchar *str);
gboolean run_cmd(char *cmd);
void fill_virtual_target_info(void);
int create_config_file(gchar *filepath);
int write_config_file(gchar *filepath);
int name_collision_check(void);

void exit_vtm(void);
void create_window_deleted_cb(void);

void resolution_select_cb(void);
void buttontype_select_cb(void);
void sdcard_size_select_cb(void);
void set_sdcard_create_active_cb(void);
void set_sdcard_select_active_cb(void);
void set_disk_select_active_cb(void);
void set_sdcard_none_active_cb(void);
void set_default_image(char *target_name);
void set_disk_default_active_cb(void);
void sdcard_file_select_cb(void);
void disk_file_select_cb(void);
void ram_select_cb(void);
void ok_clicked_cb(void);

void setup_create_frame(void);
void setup_create_button(void);
void setup_resolution_frame(void);
void setup_buttontype_frame(void);
void setup_sdcard_frame(void);
void setup_disk_frame(void);
void setup_ram_frame(void);

void setup_modify_frame(char *target_name);
void setup_modify_button(char *target_name);
void setup_modify_resolution_frame(char *target_name);
void setup_modify_sdcard_frame(char *target_name);
void setup_modify_disk_frame(char *target_name);
void setup_modify_ram_frame(char *target_name);
void setup_modify_buttontype_frame(char *target_name);
void modify_ok_clicked_cb(GtkWidget *widget, gpointer selection);

void show_create_window(void);
void show_modify_window(char* target_name);
void construct_main_window(void);
GtkWidget *setup_tree_view(void);
void delete_clicked_cb(GtkWidget *widget, gpointer selection);
void reset_clicked_cb(GtkWidget *widget, gpointer selection);
void cursor_changed_cb(GtkWidget *widget, gpointer selection);
void details_clicked_cb(GtkWidget *widget, gpointer selection);
void modify_clicked_cb(GtkWidget *widget, gpointer selection);
void activate_clicked_cb(GtkWidget *widget, gpointer selection);
void refresh_clicked_cb(void);
void activate_target(char *target_name);
void arch_select_cb(GtkWidget *widget, gpointer data);
void env_init(void);
void entry_changed(GtkEditable *entry, gpointer data);
void make_default_image(char *default_targetname);
int check_shdmem(char *target_name, int type);
int socket_init(void);
char *check_kvm(char *info_file, int *status);
void version_init(char *default_targetname, char* target_list_filepath);
int delete_group(char* target_list_filepath, char* target_name, int type);
void make_tizen_vms(void);

int remove_dir(char *path);
void lock_file(char *path);
int create_diskimg(char *arch, char *dest_path);
int create_sdcard(char *dest_path);
int modify_sdcard(char *arch, char *dest_path);
int change_modify_target_name(char *arch, char *dest_path, char *name, char* target_name);
int check_modify_target_name(char *name);
int set_modify_variable(char *target_name);
#ifdef __linux__
void set_mesa_lib(void);
#endif
#ifdef _WIN32
void socket_cleanup(void);
#endif
#endif 

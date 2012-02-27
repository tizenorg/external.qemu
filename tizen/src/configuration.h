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
 * @file configuration.h
 * @brief - header file
 */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>

#ifndef _WIN32
#include <linux/ioctl.h>
#endif

//#include <libxml/xmlmemory.h>
//#include <libxml/parser.h>

#include "option.h"
#include "fileio.h"
#include "vt_utils.h"
#include "emulator.h"

extern int startup;
extern int startup_option_config_done;

int fill_qemu_configuration(void);
int fill_configuration(int status);

int set_config_value_list(gchar *filepath, const gchar *group, const gchar *field, PROGRAMNAME *runapp_info, gsize num);
PROGRAMNAME *get_config_value_list(gchar *filepath, const gchar *group, const gchar *field);
gchar *get_skindbi_path(gchar *target);

int create_config_file(gchar *conf_filepath);
int write_config_file(gchar *filepath, CONFIGURATION *pconfiguration);
int read_config_file(gchar *filepath, CONFIGURATION *pconfiguration);
int read_virtual_target_info_file(gchar *virtual_target_name, VIRTUALTARGETINFO *pvirtual_target_info);

int is_valid_target(const gchar *path);
int is_valid_skin(gchar *file);
int is_valid_targetlist_file(void);

int load_targetlistig_file(SYSINFO *pSYSTEMINFO);
int load_config_file(SYSINFO *pSYSTEMINFO);
int determine_skin(VIRTUALTARGETINFO *pvirtual_target_info, CONFIGURATION *pconfiguration);

void qemu_option_set_to_config(arglist* al);
const char *get_target_path(void);

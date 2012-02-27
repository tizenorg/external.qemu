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
 * @file dbi_parser.h
 * @brief - header file for DBI parser
 */

#ifndef DBI_PARSER_H
#define DBI_PARSER_H
#ifdef  __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <gtk/gtk.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <stdlib.h>

#include "defines.h"
#include "extern.h"

#define KEY_PRESS       "VKS_KEY_PRESSED"   // key press   key code
#define KEY_RELEASE     "VKS_KEY_RELEASED"  // key release key code

int parse_dbi_file(const gchar * filename, PHONEMODELINFO * pDeviceData);
int free_dbi_file(PHONEMODELINFO *pDeviceData);

#ifdef __cplusplus
}
#endif
#endif /* ifndef DBI_PARSER_H */
/**
 * vim:set tabstop=4 shiftwidth=4 foldmethod=marker wrap:
 *
 */


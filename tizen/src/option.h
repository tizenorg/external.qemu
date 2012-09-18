/* 
 * Emulator
 *
 * Copyright (C) 2011, 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: 
 * SeokYeon Hwang <syeon.hwang@samsung.com>
 * HyunJun Son <hj79.son@samsung.com>
 * MunKyu Im <munkyu.im@samsung.com>
 * GiWoong Kim <giwoong.kim@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
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

#ifndef __OPTION_H__
#define __OPTION_H__

#include <glib.h>
#ifndef _WIN32
#include <error.h>
#endif
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gethostDNS(char *dns1, char *dns2);
int gethostproxy(char *http_proxy, char *https_proxy, char *ftp_proxy, char *socks_proxy);
void getlinuxproxy(char *http_proxy, char *https_proxy, char *ftp_proxy, char *socks_proxy);
void remove_string(char *src, char *dst, const char *toremove);
#endif


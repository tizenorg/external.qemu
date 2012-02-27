/*
 * Virtio general purpose interface
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact:
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

#ifndef GPI_H
#define GPI_H

#define CAll_HANDLE_MAX 128
#define FTN_SYNC_SDB_PORT 1

/* operations valid on all objects */
typedef struct object_ops object_ops_t;
struct object_ops {
	int (*ftn)(char *args_in, int args_len, char *r_buffer, int r_len);
};

int call_gpi(int pid, int ftn_num, char *in_args, int args_len, char *r_buffer, int r_len);

#endif 

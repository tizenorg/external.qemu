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

#define _XOPEN_SOURCE 600

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "qemu-common.h"
#include "qemu-log.h"
#include "helper_gpi.h"

#include "sdb.h"
#include "tizen/src/debug_ch.h"
MULTI_DEBUG_CHANNEL(qemu, gpi);

/**
 *  Global variables
 *
 */
static struct object_ops *call_ops = NULL;

/* do_call_gpi()
 *
 */
static int sync_sdb_port(char *args_in, int args_len, char *r_buffer, int r_len)
{
	char tmp[32] = {0};

	snprintf(tmp, sizeof(tmp)-1, "%d", get_sdb_base_port());

	if( r_len < strlen(tmp) ){
		ERR("short return buffer length [%d < %d(%s)] \n", r_len, strlen(tmp), tmp);
		return 0;
	}

	memcpy(r_buffer, tmp, strlen(tmp));
	TRACE("sdb-port: => return [%s]\n", r_buffer);

	return 1;
}

static int dummy_function(char *args_in, int args_len, char *r_buffer, int r_len)
{
	ERR("Need the specific operation \n");

	return 1;
}

static void do_call_init(void)
{
	int i;

	call_ops = qemu_mallocz(CAll_HANDLE_MAX*sizeof(object_ops_t) );

	/* set dummy function */
	for( i = 0; i < CAll_HANDLE_MAX ; i++ ) {
		call_ops[i].ftn = &dummy_function;
	}

	/* init function */
	call_ops[FTN_SYNC_SDB_PORT].ftn = &sync_sdb_port;

	return;
}


int call_gpi(int pid, int ftn_num, char *in_args, int args_len, char *r_buffer, int r_len)
{
	static int init = 0;
	int ret;

	TRACE("ftn_num(%d) args_len(%d) r_len(%d) \n", ftn_num, args_len, r_len);

	if( init == 0 ){
		do_call_init();
		init = 1;
	}

	ret = call_ops[ftn_num].ftn(in_args, args_len, r_buffer, r_len);
	TRACE("return [%s]\n", r_buffer);

	if(!ret){
		/* error */
	}

	return ret;
}

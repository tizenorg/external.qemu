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
    @file     command.c
    @brief    functions to manage terminal window
*/

#include "command.h"
#include "dialog.h"
#include "configuration.h"
#include "fileio.h"
#include "sdb.h"

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, command);

#if 0
void* system_telnet(void)
{
    gchar cmd[256] = "";
    //sprintf (cmd, "start cmd /C telnet %s",configuration.qemu_configuration.root_file_system_ip); 
    //sprintf (cmd, "start cmd /C ssh root@%s",configuration.qemu_configuration.root_file_system_ip); 
    system(cmd);
}
#endif

/* execute a terminal connected to the target */
void create_cmdwindow(void)
{
    gchar cmd[256];

    gchar* sdb_path = get_sdb_path();
    if (access(sdb_path, 0) != 0) {
        show_message("Sdb file is not exist.", sdb_path);
        g_free(sdb_path);
        return;
    }

    const char *terminal = getenv("EMULATOR_TERMINAL");
    int sdb_port = get_sdb_base_port();

#ifdef _WIN32
    sprintf (cmd, "start \"emulator-%d\" cmd /C %s -s emulator-%d shell", sdb_port, sdb_path, sdb_port);
    system(cmd);
    fflush(stdout);
#elif __linux__
    /* gnome-terminal */
    if (!terminal) {
        terminal = "/usr/bin/gnome-terminal --disable-factory";
        //terminal = "/usr/bin/xterm -l -e";
    }
    sprintf(cmd, "%s --title=emulator-%d -x %s -s emulator-%d shell", terminal, sdb_port, sdb_path, sdb_port);

    if (emul_create_process(cmd) == TRUE) {
        INFO( "start command window\n");
    } else {
        ERR( "falied to start command window\n");
    }
#endif
}


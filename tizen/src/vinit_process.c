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
 * @file vinit_process.c
 * @brief - implementation file
 */
#ifndef _WIN32
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#else
#include <windows.h>
#include <winsock2.h>
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

#include "vinit_process.h"
#include "ui_imageid.h"
#include "extern.h"
#ifndef _WIN32
#include "defines.h"
#endif

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, vinit_process);

#define MAX_COMMAND 256


/**
 * @brief    load ssh process
 *
 * @return   success  0,  fail   1
 * @date     June 29. 2009
 * */

int load_ssh(char *command, char *args)
{
#ifndef _WIN32
    gchar run_command[MAXBUF] = { 0, };
    pid_t pid;      

    if (args)
        snprintf(run_command, sizeof(run_command), "`source /root/.profile; %s %s&`", command, args);   
    else
        snprintf(run_command, sizeof(run_command), "`source /root/.profile; %s&`", command);    
    

    if ((pid = vfork()) < 0) {
        WARN( "fork error\n");
        return -1;
    }

    if (pid == 0) { /* child */

        execlp("ssh", "ssh", "-o", "StrictHostKeyChecking=no", "-p", "1202", "root@localhost", run_command, (char *)0);
        exit(0);
    } 
    
    else if (pid > 0) { /* parent */
//      int status = 0;
        TRACE( "ssh pid = %d\n", pid);
//      sleep(4);
//      kill(pid, SIGTERM);
//      wait4(pid, &status, 0, NULL);
    }
#else
    gchar connect_ssh[64] = { 0, };
    gchar run_command[512] = { 0, };
    pid_t pid;      

    //snprintf(connect_ssh, sizeof(connect_ssh), "root@%s", configuration.qemu_configuration.root_file_system_ip);

    if (args)
        sprintf(run_command, "ssh -o StrictHostKeyChecking=no root@192.168.128.3 \"`source /root/.profile;%s %s $`", command, args);
    else
        snprintf(run_command, sizeof(run_command), "\"%s &\"", command);    
    /*
    sprintf(run_command, "ssh -o StrictHostKeyChecking=no root@192.168.128.3 \"`source /root/.profile;%s $`\"", command );
    snprintf(run_command, sizeof(run_command), "`source /root/.profile; %s&`", command);    
    sprintf(cmdbuf,"D:/cygwin/home/okdear/ssh/bin/ssh.exe -o StrictHostKeyChecking=no %s %s", connect_ssh, run_command);
    execlp("ssh", "ssh", "-o StrictHostKeyChecking=no", connect_ssh, run_command, (char *)0);
    */
    
    char cmdbuf[512] = "";
    sprintf(cmdbuf,"ssh.exe -o StrictHostKeyChecking=no %s %s", connect_ssh, run_command);

    system(cmdbuf);

#endif
    return pid;
}   


/**
 * @brief    use scp for file transfer process
 *
 * @return   success  0,  fail   1
 * @date     June 29. 2009
 * */
int file_transfer_scp( char* src, char* dest )
{
#ifndef _WIN32
    pid_t pid;

    if( (pid = vfork()) < 0 ) {
        WARN( "fork error\n");
        return -1;
    }

    if (pid == 0) { /* child */
        execlp("scp", "scp", "-o StrictHostKeyChecking=no", src, dest, (char *)0);
        exit(0);
    }else if (pid > 0) { /* parent */
        TRACE( "ssh pid = %d\n", pid);
        sleep(1);
        kill(pid, SIGTERM);
    }
#endif
    return 0;
}


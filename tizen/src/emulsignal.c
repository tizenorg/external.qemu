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

#include "emulsignal.h"
#include "debug_ch.h"
#include "sdb.h"
#ifndef _WIN32
#include <sys/ipc.h>  
#include <sys/shm.h> 
#endif
//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, emulsignal);

static sigset_t cur_sigset, old_sigset;
extern void emul_kill_all_process(void);

/**
 * @brief    destroy window
 * @param    widget
 * @param    gpointer
 * @date     Nov 20. 2008   
 */
static int destroy_window(void)
{
    /* 1. check event injector */
    GtkWidget *win = NULL;
    
    if (UISTATE.is_ei_run == TRUE) {        
        win = get_window(EVENT_INJECTOR_ID);
        gtk_widget_destroy(win);
        TRACE( "event injector destroy\n");
    }

    /* 2. check event manager */
    
    if (UISTATE.is_em_run == TRUE) {
        win = get_window(EVENT_MANAGER_ID);
        gtk_widget_destroy(win);
        TRACE( "event manager destroy\n");  
    }

    /* 4. check screen shot */
    
    if(UISTATE.is_screenshot_run == TRUE) {
        win = get_window(SCREEN_SHOT_ID);
        gtk_widget_destroy(win);
        TRACE( "screen shot destroy\n");
    }

    return 0;
}


/**
 * @brief    destroy emulator
 * @date     Nov 20. 2008   
 */
void destroy_emulator(void)
{
    /* 1. terminal closed */

    emul_kill_all_process();

    /* 2. window destroy */

    destroy_window();
    
    /* 3. save configuration conf, main_x, main_y */

    set_config_type(SYSTEMINFO.virtual_target_info_file, EMULATOR_GROUP, MAIN_X_KEY, configuration.main_x);
    set_config_type(SYSTEMINFO.virtual_target_info_file, EMULATOR_GROUP, MAIN_Y_KEY, configuration.main_y);

    INFO( "Emulator Stop: set config type\n");
    
    /* 4. remove shared memory */

#ifndef _WIN32
    int shmid = shmget((key_t)tizen_base_port, 64, 0); 
    if(shmctl(shmid, IPC_RMID, 0) == -1)
        ERR( "fail to remove shared memory\n");
    else
        INFO( "succedd to remove shared memory\n");
#else
    char *port_in_use;
    port_in_use = g_strdup_printf("%d", tizen_base_port);
    HANDLE hMapFile = OpenFileMapping(FILE_MAP_READ, TRUE, port_in_use);
    if(hMapFile)
        CloseHandle(hMapFile);
#endif

    return;
}


/**
 * @brief    handle signal
 *
 * @param    signo: singnal number
 * @return   None
 * @owner    okdear.park
 */
void sig_handler (int signo)
{
#ifndef _WIN32
    sigset_t sigset, oldset;
    //int status;
    //pid_t pid;
    
    TRACE("signo %d happens\n", signo);
    switch (signo) {        
    case SIGCHLD:
#if 0               

        if ((pid = waitpid(-1, &status, 0)) > 0) { 
            INFO( "child %d stop with signal %d\n", pid, signo);
        } 

        else if ((pid == -1) && (errno == EINTR)) {
            INFO( "pid == -1 or errno EINTR \n");
            //continue;
        } else {
            INFO( "pid = %d errono = %d\n",pid, errno );
            break;
        }

#endif
#if 0
    pid_t pid;
    int status;

    sigfillset (&sigset);
    if (sigprocmask (SIG_BLOCK, &sigset, &oldset) < 0) 
    
        TRACE("sigprocmask %d error \n", signo);

        /* wait for any child process to change state */
        
        while (1) {

            if ((pid = waitpid(-1, &status, WNOHANG)) > 0) { 
                INFO( "child %d stop with signal %d\n", pid, signo);
            } else if ((pid == -1) && (errno == EINTR)) {
                INFO( "pid == -1 or errno EINTR \n");
                continue;
            } else {
                INFO( "pid = %d errono = %d\n",pid, errno );
                break;
            }
        }

        if (sigprocmask (SIG_SETMASK, &oldset, NULL) < 0) 
            TRACE( "sigprocmask error \n");

        INFO( "child(%d) die %d\n", pid, signo);

        return;
#endif
        break;
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
    sigfillset (&sigset);

    if (sigprocmask (SIG_BLOCK, &sigset, &oldset) < 0) {
        ERR( "sigprocmask %d error \n", signo);
        exit_emulator();
    }

    if (sigprocmask (SIG_SETMASK, &oldset, NULL) < 0) {
        ERR( "sigprocmask error \n");
    }

    exit_emulator();
    exit(0);
    break;

    default:
        break;
    }
#endif
}

/**
 * @brief    block signal 
 *
 * @return   success  0,  fail  -1
 * @date     Oct 18. 2008
 * */

int sig_block() 
{
#ifndef _WIN32
    sigfillset (&cur_sigset);

    if (sigprocmask (SIG_BLOCK, &cur_sigset, &old_sigset) < 0) {
        ERR( "sigprocmask error \n");
    }
#endif
    return 0;

}


/**
 * @brief    unblock signal 
 *
 * @return   success  0,  fail  -1
 * @date     Oct 18. 2008
 * */
int sig_unblock() 
{
#ifndef _WIN32
    sigfillset (&cur_sigset);

    if (sigprocmask (SIG_SETMASK, &old_sigset, NULL) < 0) {
        ERR( "sigprocmask error \n");
    }
#endif
    return 0;
}

/**
 * @brief    regist signal 
 *
 * @return   success  0,  fail  -1
 * @date     Oct 18. 2008
 * */

int register_sig_handler()
{
#ifndef _WIN32
    signal (SIGTERM, sig_handler);
    signal (SIGHUP, sig_handler);
    signal (SIGQUIT, sig_handler);
    signal (SIGINT, sig_handler);
    signal (SIGCHLD, sig_handler);
#endif
    TRACE( "resist sig handler\n");

    return 0;
}


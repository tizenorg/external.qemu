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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */


#include "maru_common.h"
#include <stdlib.h>
#ifdef CONFIG_SDL
#include <SDL.h>
#endif
#include "emulator.h"
#include "guest_debug.h"
#include "sdb.h"
#include "string.h"
#include "skin/maruskin_server.h"
#include "skin/maruskin_client.h"
#include "guest_server.h"
#include "debug_ch.h"
#include "option.h"
#include "emul_state.h"
#include "qemu_socket.h"
#include "build_info.h"
#include "maru_err_table.h"
#include <glib.h>
#include <glib/gstdio.h>

#if defined(CONFIG_WIN32)
#include <windows.h>
#elif defined(CONFIG_LINUX)
#include <linux/version.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include "mloop_event.h"

MULTI_DEBUG_CHANNEL(qemu, main);

#define QEMU_ARGS_PREFIX "--qemu-args"
#define SKIN_ARGS_PREFIX "--skin-args"
#define IMAGE_PATH_PREFIX   "file="
#define IMAGE_PATH_SUFFIX   ",if=virtio"
#define SDB_PORT_PREFIX     "sdb_port="
#define LOGS_SUFFIX         "/logs/"
#define LOGFILE             "emulator.log"
#define LCD_WIDTH_PREFIX "width="
#define LCD_HEIGHT_PREFIX "height="

#define MIDBUF  128

int tizen_base_port;
char tizen_target_path[MAXLEN];
char logpath[MAXLEN];

static int skin_argc;
static char **skin_argv;
static int qemu_argc;
static char **qemu_argv;

void maru_display_fini(void);

char *get_logpath(void)
{
    return logpath;
}

void exit_emulator(void)
{
    mloop_ev_stop();
    shutdown_skin_server();
    shutdown_guest_server();

    maru_display_fini();
}

void check_shdmem(void)
{
#if defined(CONFIG_LINUX)
    int shm_id;
    void *shm_addr;
    u_int port;
    int val;
    struct shmid_ds shm_info;

    for (port = 26100; port < 26200; port += 10) {
        shm_id = shmget((key_t)port, 0, 0);
        if (shm_id != -1) {
            shm_addr = shmat(shm_id, (void *)0, 0);
            if ((void *)-1 == shm_addr) {
                ERR("error occured at shmat()\n");
                break;
            }

            val = shmctl(shm_id, IPC_STAT, &shm_info);
            if (val != -1) {
                INFO("count of process that use shared memory : %d\n",
                    shm_info.shm_nattch);
                if ((shm_info.shm_nattch > 0) &&
                    strcmp(tizen_target_path, (char *)shm_addr) == 0) {
                    if (check_port_bind_listen(port + 1) > 0) {
                        shmdt(shm_addr);
                        continue;
                    }
                    shmdt(shm_addr);
                    maru_register_exit_msg(MARU_EXIT_UNKNOWN,
                                        (char *)"Can not execute this VM.\n \
                                        The same name is running now.");
                    exit(0);
                } else {
                    shmdt(shm_addr);
                }
            }
        }
    }

#elif defined(CONFIG_WIN32)
    u_int port;
    char *base_port = NULL;
    char *pBuf;
    HANDLE hMapFile;
    for (port = 26100; port < 26200; port += 10) {
        base_port = g_strdup_printf("%d", port);
        hMapFile = OpenFileMapping(FILE_MAP_READ, TRUE, base_port);
        if (hMapFile == NULL) {
            INFO("port %s is not used.\n", base_port);
            continue;
        } else {
             pBuf = (char *)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 50);
            if (pBuf == NULL) {
                ERR("Could not map view of file (%d).\n", GetLastError());
                CloseHandle(hMapFile);
            }

            if (strcmp(pBuf, tizen_target_path) == 0) {
                maru_register_exit_msg(MARU_EXIT_UNKNOWN,
                    "Can not execute this VM.\nThe same name is running now.");
                UnmapViewOfFile(pBuf);
                CloseHandle(hMapFile);
                free(base_port);
                exit(0);
            } else {
                UnmapViewOfFile(pBuf);
            }
        }

        CloseHandle(hMapFile);
        free(base_port);
    }
#elif defined(CONFIG_DARWIN)
    /* TODO: */
#endif
}

void make_shdmem(void)
{
#if defined(CONFIG_LINUX)
    int shmid;
    char *shared_memory;

    shmid = shmget((key_t)tizen_base_port, MAXLEN, 0666|IPC_CREAT);
    if (shmid == -1) {
        ERR("shmget failed\n");
        return;
    }

    shared_memory = shmat(shmid, (char *)0x00, 0);
    if (shared_memory == (void *)-1) {
        ERR("shmat failed\n");
        return;
    }
    sprintf(shared_memory, "%s", tizen_target_path);
    INFO("shared memory key: %d value: %s\n",
        tizen_base_port, (char *)shared_memory);
#elif defined(CONFIG_WIN32)
    HANDLE hMapFile;
    char *pBuf;
    char *port_in_use;
    char *shared_memory;

    shared_memory = g_strdup_printf("%s", tizen_target_path);
    port_in_use =  g_strdup_printf("%d", tizen_base_port);
    hMapFile = CreateFileMapping(
                 INVALID_HANDLE_VALUE, /* use paging file */
                 NULL,                 /* default security */
                 PAGE_READWRITE,       /* read/write access */
                 0,                /* maximum object size (high-order DWORD) */
                 50,               /* maximum object size (low-order DWORD) */
                 port_in_use);         /* name of mapping object */
    if (hMapFile == NULL) {
        ERR("Could not create file mapping object (%d).\n", GetLastError());
        return;
    }
    pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 50);

    if (pBuf == NULL) {
        ERR("Could not map view of file (%d).\n", GetLastError());
        CloseHandle(hMapFile);
        return;
    }

    CopyMemory((PVOID)pBuf, shared_memory, strlen(shared_memory));
    free(port_in_use);
    free(shared_memory);
#elif defined(CONFIG_DARWIN)
    /* TODO: */
#endif
    return;
}

static void construct_main_window(int skin_argc, char *skin_argv[],
                                int qemu_argc, char *qemu_argv[])
{
    INFO("construct main window\n");

    start_skin_server(skin_argc, skin_argv, qemu_argc, qemu_argv);

    /* the next line checks for debugging and etc.. */
    if (get_emul_skin_enable() == 1) {
        if (0 > start_skin_client(skin_argc, skin_argv)) {
            maru_register_exit_msg(MARU_EXIT_SKIN_SERVER_FAILED, NULL);
            exit(-1);
        }
    }

    set_emul_caps_lock_state(0);
    set_emul_num_lock_state(0);
}

static void parse_options(int argc, char *argv[], int *skin_argc,
                        char ***skin_argv, int *qemu_argc, char ***qemu_argv)
{
    int i = 0;
    int j = 0; //skin args index

    /* classification */
    for (i = 1; i < argc; ++i) {
        if (strstr(argv[i], SKIN_ARGS_PREFIX)) {
            *skin_argv = &(argv[i + 1]);
            break;
        }
    }

    for (j = i; j < argc; ++j) {
        if (strstr(argv[j], QEMU_ARGS_PREFIX)) {
            *skin_argc = j - i - 1;

            *qemu_argc = argc - j - i + 1;
            *qemu_argv = &(argv[j]);

            argv[j] = argv[0];
        }
    }
}

static void get_bin_dir(char *exec_argv)
{

    if (!exec_argv) {
        return;
    }

    char *data = strdup(exec_argv);
    if (!data) {
        fprintf(stderr, "Fail to strdup for paring a binary directory.\n");
        return;
    }

    char *p = NULL;
#ifdef _WIN32
    p = strrchr(data, '\\');
    if (!p) {
        p = strrchr(data, '/');
    }
#else
    p = strrchr(data, '/');
#endif
    if (!p) {
        free(data);
        return;
    }

    strncpy(bin_dir, data, strlen(data) - strlen(p));

    free(data);

}

void set_image_and_log_path(char *qemu_argv)
{
    int i, j = 0;
    int name_len = 0;
    int prefix_len = 0;
    int suffix_len = 0;
    int max = 0;
    char *path = malloc(MAXLEN);
    name_len = strlen(qemu_argv);
    prefix_len = strlen(IMAGE_PATH_PREFIX);
    suffix_len = strlen(IMAGE_PATH_SUFFIX);
    max = name_len - suffix_len;
    for (i = prefix_len , j = 0; i < max; i++) {
        path[j++] = qemu_argv[i];
    }
    path[j] = '\0';
    if (!g_path_is_absolute(path)) {
        strcpy(tizen_target_path, g_get_current_dir());
    } else {
        strcpy(tizen_target_path, g_path_get_dirname(path));
    }

    strcpy(logpath, tizen_target_path);
    strcat(logpath, LOGS_SUFFIX);
#ifdef CONFIG_WIN32
    if (access(g_win32_locale_filename_from_utf8(logpath), R_OK) != 0) {
        g_mkdir(g_win32_locale_filename_from_utf8(logpath), 0755);
    }
#else
    if (access(logpath, R_OK) != 0) {
        g_mkdir(logpath, 0755);
    }
#endif
    strcat(logpath, LOGFILE);
    set_log_path(logpath);
}

void redir_output(void)
{
    FILE *fp;

    fp = freopen(logpath, "a+", stdout);
    if (fp == NULL) {
        fprintf(stderr, "log file open error\n");
    }

    fp = freopen(logpath, "a+", stderr);
    if (fp == NULL) {
        fprintf(stderr, "log file open error\n");
    }
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
}

void extract_qemu_info(int qemu_argc, char **qemu_argv)
{
    int i = 0;

    for (i = 0; i < qemu_argc; ++i) {
        if (strstr(qemu_argv[i], IMAGE_PATH_PREFIX) != NULL) {
            set_image_and_log_path(qemu_argv[i]);
            break;
        }
    }

    tizen_base_port = get_sdb_base_port();
}

void extract_skin_info(int skin_argc, char **skin_argv)
{
    int i = 0;
    int w = 0, h = 0;

    for (i = 0; i < skin_argc; ++i) {
        if (strstr(skin_argv[i], LCD_WIDTH_PREFIX) != NULL) {
            char *width_arg = skin_argv[i] + strlen(LCD_WIDTH_PREFIX);
            w = atoi(width_arg);

            INFO("lcd width option = %d\n", w);
        } else if (strstr(skin_argv[i], LCD_HEIGHT_PREFIX) != NULL) {
            char *height_arg = skin_argv[i] + strlen(LCD_HEIGHT_PREFIX);
            h = atoi(height_arg);

            INFO("lcd height option = %d\n", h);
        }

        if (w != 0 && h != 0) {
            set_emul_lcd_size(w, h);
            break;
        }
    }
}


static void system_info(void)
{
#define DIV 1024

    char timeinfo[64] = {0, };
    struct tm *tm_time;
    struct timeval tval;

    INFO("* SDK Version : %s\n", build_version);
    INFO("* Package %s\n", pkginfo_version);
    INFO("* Package %s\n", pkginfo_maintainer);
    INFO("* Git Head : %s\n", pkginfo_githead);
    INFO("* User name : %s\n", g_get_real_name());
    INFO("* Host name : %s\n", g_get_host_name());

    /* timestamp */
    INFO("* Build date : %s\n", build_date);
    gettimeofday(&tval, NULL);
    tm_time = localtime(&(tval.tv_sec));
    strftime(timeinfo, sizeof(timeinfo), "%Y/%m/%d %H:%M:%S", tm_time);
    INFO("* Current time : %s\n", timeinfo);

#ifdef CONFIG_SDL
    /* Gets the version of the dynamically linked SDL library */
    INFO("* Host sdl version : (%d, %d, %d)\n",
        SDL_Linked_Version()->major,
        SDL_Linked_Version()->minor,
        SDL_Linked_Version()->patch);
#endif

#if defined(CONFIG_WIN32)
    /* Retrieves information about the current os */
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (GetVersionEx(&osvi)) {
        INFO("* MajorVersion : %d, MinorVersion : %d, BuildNumber : %d, \
            PlatformId : %d, CSDVersion : %s\n", osvi.dwMajorVersion,
            osvi.dwMinorVersion, osvi.dwBuildNumber,
            osvi.dwPlatformId, osvi.szCSDVersion);
    }

    /* Retrieves information about the current system */
    SYSTEM_INFO sysi;
    ZeroMemory(&sysi, sizeof(SYSTEM_INFO));

    GetSystemInfo(&sysi);
    INFO("* Processor type : %d, Number of processors : %d\n",
            sysi.dwProcessorType, sysi.dwNumberOfProcessors);

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    INFO("* Total Ram : %llu kB, Free: %lld kB\n",
            memInfo.ullTotalPhys / DIV, memInfo.ullAvailPhys / DIV);

#elif defined(CONFIG_LINUX)
    /* depends on building */
    INFO("* Qemu build machine linux kernel version : (%d, %d, %d)\n",
            LINUX_VERSION_CODE >> 16,
            (LINUX_VERSION_CODE >> 8) & 0xff,
            LINUX_VERSION_CODE & 0xff);

     /* depends on launching */
    struct utsname host_uname_buf;
    if (uname(&host_uname_buf) == 0) {
        INFO("* Host machine uname : %s %s %s %s %s\n",
            host_uname_buf.sysname, host_uname_buf.nodename,
            host_uname_buf.release, host_uname_buf.version,
            host_uname_buf.machine);
    }

    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        INFO("* Total Ram : %llu kB, Free: %llu kB\n",
            sys_info.totalram * (unsigned long long)sys_info.mem_unit / DIV,
            sys_info.freeram * (unsigned long long)sys_info.mem_unit / DIV);
    }

    /* pci device description */
    INFO("* Pci devices :\n");
    char lscmd[MAXLEN] = "lspci >> ";
    strcat(lscmd, logpath);
    int i = system(lscmd);
    INFO("system function command : %s, \
        system function returned value : %d\n", lscmd, i);

#elif defined(CONFIG_DARWIN)
    /* TODO: */
#endif

    INFO("\n");
}

void prepare_maru(void)
{
    INFO("Prepare maru specified feature\n");

    INFO("call construct_main_window\n");

    construct_main_window(skin_argc, skin_argv, qemu_argc, qemu_argv);

    int guest_server_port = tizen_base_port + SDB_UDP_SENSOR_INDEX;
    start_guest_server(guest_server_port);

    mloop_ev_init();
}

int qemu_main(int argc, char **argv, char **envp);

int main(int argc, char *argv[])
{
    parse_options(argc, argv, &skin_argc, &skin_argv, &qemu_argc, &qemu_argv);
    get_bin_dir(qemu_argv[0]);
    socket_init();
    extract_qemu_info(qemu_argc, qemu_argv);

    INFO("Emulator start !!!\n");
    atexit(maru_atexit);

    extract_skin_info(skin_argc, skin_argv);

    check_shdmem();
    make_shdmem();
    sdb_setup();

    system_info();

    INFO("Prepare running...\n");
    /* Redirect stdout and stderr after debug_ch is initialized. */
    redir_output();

    int i;

    fprintf(stdout, "qemu args : =========================================\n");
    for (i = 0; i < qemu_argc; ++i) {
        fprintf(stdout, "%s ", qemu_argv[i]);
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "=====================================================\n");

    fprintf(stdout, "skin args : =========================================\n");
    for (i = 0; i < skin_argc; ++i) {
        fprintf(stdout, "%s ", skin_argv[i]);
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "=====================================================\n");

    INFO("qemu main start!\n");
    qemu_main(qemu_argc, qemu_argv, NULL);

    exit_emulator();

    return 0;
}


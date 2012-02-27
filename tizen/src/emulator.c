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
 * @file     emulator.c
 * @brief    main implementation file of emulator for controling player screen, initialization function, etc.
 * @mainpage emulator for ISE
 * @section  INTRO
 *   program module name: ISE emulator
 *   emulator program can run both standalone and with ISE
 */

#include "emulator.h"
#include "about_version.h"
#include "vl.h"
#include "sensor_server.h"
#include <assert.h>

/* changes for saving emulator state */
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#ifdef __linux__
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/utsname.h>
#include <linux/version.h>
#elif _WIN32
#include <windows.h>
#endif

#include "opengl_server.h"
#include "sdb.h"
#include "nbd.h"
#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, main);

#define RCVBUFSIZE 40
#define MAX_COMMANDS 5
#define MAX_LENGTH 24
#define MAX_TIME_STR 100

/* enable opengl_server thread */
#define ENABLE_OPENGL_SERVER

/* configuration : global variable for saving config file
 * sysinfo : global variable for using in this program
 * startup_option : global variable for loading emulator option
 * */

CONFIGURATION configuration;
SYSINFO SYSTEMINFO;
STARTUP_OPTION startup_option;
PHONEMODELINFO *phone_info;
VIRTUALTARGETINFO virtual_target_info;
FILE *g_out_fp;
FILE *g_err_fp;

UIFLAG UISTATE = {
    .last_index = -1,
    .button_press_flag = -1,
    .key_button_press_flag = 0,
    .frame_buffer_ctrl = 0,
    .scale = 1.0,
    .current_mode = 0,
    .config_flag = 0,
    .PID_flag = 0,
    .is_ei_run = FALSE,
    .is_em_run = FALSE,
    .is_gps_run = FALSE,
    .is_compass_run = FALSE,
    .is_screenshot_run = FALSE,
    .sub_window_flag = FALSE,
    .network_read_flag = FALSE,
};

int _emulator_condition = 0;
PHONEMODELINFO PHONE;
GtkWidget *g_main_window;

GtkWidget *pixmap_widget;
GtkWidget *fixed;
#ifdef __linux__
struct utsname host_uname_buf;
#endif

/* Widgets for savevm */
GtkWidget *savevm_window;
GtkProgressBar *savevm_progress;
GtkWidget *savevm_label;
int        vmstate=0;
int        vmsock=-1;
int        device_count = 0;
GIOChannel *channel=NULL;

int get_emulator_condition(void)
{
    return _emulator_condition;
}

void set_emulator_condition(int state)
{
    _emulator_condition = state;
}

struct _arglist {
    char *argv[QEMUARGC];
    int argc;
};

static arglist g_qemu_arglist = {{0,}, 0};
int tizen_base_port = 0;
void append_argvlist(arglist* al, const char *fmt, ...)
{
    char buf[MAXBUF];
    va_list va;

    va_start(va, fmt);
    vsnprintf(buf, sizeof buf, fmt, va);
    va_end(va);
    al->argv[al->argc++] = strdup(buf);
    assert(al->argc < QEMUARGC);
}

#ifndef _WIN32
static GSList* emul_process_list = NULL; /**<linked list of running terminal*/
pthread_t unfsd_thread;
#else
DWORD unfsd_thread;
#endif

#ifdef ENABLE_OPENGL_SERVER
pthread_t thread_opengl_id;
#endif  /* ENABLE_OPENGL_SERVER */

#ifndef _WIN32

static pthread_mutex_t mutex_emul = PTHREAD_MUTEX_INITIALIZER;

void emulator_mutex_lock(void)
{
    pthread_mutex_lock(&mutex_emul);
}

void emulator_mutex_unlock(void)
{
    pthread_mutex_unlock(&mutex_emul);
}

static void emulator_mutex_init(void)
{
}

#else

static HANDLE mutex_emul;

void emulator_mutex_lock(void)
{
    WaitForSingleObject(mutex_emul, INFINITE);
}

void emulator_mutex_unlock(void)
{
    ReleaseMutex(mutex_emul);
}

static void emulator_mutex_init(void)
{
    mutex_emul = CreateMutex(NULL, 0, NULL);
}

#endif

#ifndef _WIN32
/**
 *     @brief  called when command window closed.
 *     it registered with g_child_watch_add function when creating each process
 *     @param  pid: pid of each process been created by emulator
 *     @param  status: dummy
 *     @param  data: dummy
 *     @see    create_cmdwindow
 */
static void emul_process_close_handle (GPid pid, gint status, gpointer data)
{
    TRACE( "remove pid=%d\n", pid);
    g_spawn_close_pid (pid);
    emul_process_list = g_slist_remove(emul_process_list, (gpointer) pid);
    TRACE( "remove complete pid=%d\n", pid);
}

/**
  @brief  send SIGTERM to process
  @param    data: pid of terminal
  @user_data: dummy
 */
static void emul_kill_process(gpointer data, gpointer user_data)
{
    WARN( "kill terminal pid=%d\n", (int)data);
    kill( (pid_t)(gpointer)data, SIGTERM);
}

/**
  @brief    call emul_kill_process for all the node in emul_process_list
 */
void emul_kill_all_process(void)
{
    g_slist_foreach(emul_process_list, emul_kill_process, NULL);
}

/**
  @brief  create a process
  @param    data: command of starting process
  @return success: TRUE
 */
int emul_create_process(const gchar cmd[])
{
    GPid pid = 0;
    GError* error = NULL;
    gchar **argv_fork;
    gint argc_fork;
    int ret = TRUE;


    emulator_mutex_lock();

    g_shell_parse_argv (cmd, &argc_fork, &argv_fork, NULL);

    if (g_spawn_async_with_pipes ("./", argv_fork, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL, NULL, NULL, &error) == TRUE) {
        emul_process_list = g_slist_append(emul_process_list, (gpointer)pid);
        g_child_watch_add(pid, emul_process_close_handle, NULL);
    }

    else {
        //  ERR( "Error in g_spawn_async\n");
        ret = FALSE;
    }

    g_strfreev (argv_fork);

    if (error) {
        //g_error(error->message);
        ret = FALSE;
    }

    TRACE("create PID = %d\n", pid);

    emulator_mutex_unlock();

    return ret;

}
#else
void emul_kill_all_process(void)
{

}
int emul_create_process(const gchar cmd[])
{
    return TRUE;
}
#endif

#ifdef _WIN32
void socket_cleanup(void)
{
    WSACleanup();
}
#endif

int socket_init(void)
{
#ifdef _WIN32
    WSADATA Data;
    int ret, err;

    ret = WSAStartup(MAKEWORD(2,0), &Data);
    if (ret != 0) {
        err = WSAGetLastError();
        fprintf(stderr, "WSAStartup: %d\n", err);
        return -1;
    }
    atexit(socket_cleanup);
#endif
    return 0;
}

void exit_emulator_post_process( void ) {

    set_emulator_condition(EMUL_SHUTTING_DOWN);

    free_dbi_file(&PHONE);

    /* 1. emulator and driver destroy */
    destroy_emulator();
    INFO( "Emulator Stop: destroy emulator \n");

    /* 2. destroy hash */
    window_hash_destroy();

    /* 3. quit SDL */
    //  SDL_Quit();

    /* 4. quit main */
    gtk_main_quit();
    INFO( "Emulator Stop: shutdown qemu system, gtk_main quit complete \n");

    /* 5. Flush output */
    fclose(stdout);
    fclose(stderr);

    /* 6. close fp which is opened when redirect_log() */

    fclose(g_out_fp);
    fclose(g_err_fp);
    INFO( "Close fp which is opened when redirect_log()\n");

#ifdef ENABLE_OPENGL_SERVER
    pthread_cancel(thread_opengl_id);
    INFO( "opengl_server thread is quited.\n");
#endif  /* ENABLE_OPENGL_SERVER */

}

#if 0
static int send_info_to_emuld(char *send_buf, int buf_size)
{
    int   s;

    s = tcp_socket_outgoing("127.0.0.1", (uint16_t)(get_sdb_base_port() + SDB_TCP_EMULD_INDEX));
    if (s < 0) {
        TRACE( "can't create socket to talk to the sdb forwarding session \n");
        TRACE( "[127.0.0.1:%d/tcp] connect fail (%d:%s)\n"
                , get_sdb_base_port() + SDB_TCP_EMULD_INDEX
                , errno, strerror(errno)
             );
        return -1;
    }

    socket_send(s, "system\n\n\n\n", 10);
    socket_send(s, &buf_size, 4);
    socket_send(s, send_buf, buf_size);

    INFO( "send(size: %d) te 127.0.0.1:%d/tcp \n"
            , buf_size, get_sdb_base_port() + SDB_TCP_EMULD_INDEX);

#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif

    return 1;
}
#endif

#if 0
static void *graceful_shutdown_ftn(void* arg)
{
    int i;

    INFO("send command shutdown to emuld \n");
    send_info_to_emuld("shutdown", 8);

    /* wait 7 seconds */
    INFO("wait 7 seconds \n");
    for(i=0; i<7; i++){
#ifdef _WIN32
        Sleep(1000);
#else
        usleep(1000000);
#endif
    }

    INFO("qemu_system_shutdown_request call \n");
    qemu_system_shutdown_request();

    return 0;
}
#endif // #if 0

/**
 * @brief    destroy emulator
 * @param    widget
 * @param    gpointer
 * @date     Nov 20. 2008
 */
void exit_emulator(void)
{

#if 1 /* graceful shutdown */

    /* 1st way : long press => power key */
    ps2kbd_put_keycode( 103 & 0x7f );
#ifdef _WIN32
    Sleep( 1.6 * 1000 ); // 1.6 seconds
#else
    usleep( 1.6 * 1000 * 1000 ); // 1.6 seconds
#endif
    ps2kbd_put_keycode( 103 | 0x80 );
    // If user selects 'Yes' in Power off poup, 'qemu_system_shutdown_request' in vl.c is supposed to be called.

#if 0
    /* 2nd way : send command shutdown to emuld in guest image */
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, graceful_shutdown_ftn, NULL) != 0) {
        ERR("pthread_create fail \n");
        qemu_system_shutdown_request();
    }
#endif

#else

    /* 1. emulator and driver destroy */

    destroy_emulator();

    INFO( "Emulator Stop: destroy emulator \n");

    /* 2. destroy hash */

    window_hash_destroy();

    /* 3. quit SDL */

    //  SDL_Quit();

    /* 4. shutdown qemu system */

    qemu_system_shutdown_request();

    /* 5. quit main */

    gtk_main_quit();
    INFO( "Emulator Stop: shutdown qemu system, gtk_main quit complete \n");

#ifdef ENABLE_OPENGL_SERVER
    pthread_cancel(thread_opengl_id);
    INFO( "opengl_server thread is quited.\n");
#endif  /* ENABLE_OPENGL_SERVER */

    exit(0);

#endif /* graceful shutdown */

}

#ifdef _WIN32
/**
  @brief  enumerates display monitors
  @param dwData: host screen resolution
  @return success: TRUE
 */
static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    RECT *pHostScreen;
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &mi);

    pHostScreen = (RECT *)dwData;
    pHostScreen->left = MIN(lprcMonitor->left, pHostScreen->left);
    pHostScreen->top = MIN(lprcMonitor->top, pHostScreen->top);
    pHostScreen->right = MAX(lprcMonitor->right, pHostScreen->right);
    pHostScreen->bottom = MAX(lprcMonitor->bottom,pHostScreen->bottom);

    return TRUE;
}
#endif

static void construct_main_window(void)
{

    gchar emul_img_dir[512] = {0,};
    gchar *name;
    const gchar *skin;
    GdkBitmap *SkinMask = NULL;
    GtkWidget *popup_menu = NULL;
    GtkWidget *sdl_widget = NULL;

    UISTATE.current_mode = 0;

    /* 1. create main_window without border */

    g_main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    add_window (g_main_window, EMULATOR_ID);
#if GTK_CHECK_VERSION(2,20,0)
    gtk_widget_set_can_focus(g_main_window,TRUE);
#else
    GTK_WIDGET_SET_FLAGS(g_main_window, GTK_CAN_FOCUS);
#endif
    GdkColor color;
    color.red = color.green = color.blue = 0x8888;
    gtk_widget_modify_bg(g_main_window, GTK_STATE_NORMAL, &color);
    INFO("sets the emulator window background color (%x, %x, %x)\n", color.red, color.green, color.blue);

    gtk_window_set_decorated (GTK_WINDOW (g_main_window), FALSE);

    /* 2.1 emulator taskbar icon image */

    skin = get_skin_path();
    if (skin == NULL) {
        ERR( "getting skin path is failed!!\n");
        exit (1);
    }
#ifdef _WIN32
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    if ((osvi.dwMajorVersion >= 6) && (osvi.dwMinorVersion >= 1))
        sprintf(emul_img_dir, "%s/icons/Emulator.ico", skin);
    else
        sprintf(emul_img_dir, "%s/icons/Emulator_20x20.png", skin);

#else /* _WIN32 */
    sprintf(emul_img_dir, "%s/icons/Emulator.ico", skin);
#endif/* _WIN32 */

    if (g_file_test(emul_img_dir, G_FILE_TEST_EXISTS) == FALSE) {
        ERR( "emulator icon directory %s doesn't exist!!\n", emul_img_dir);
    }
    gtk_window_set_icon_from_file(GTK_WINDOW(g_main_window), emul_img_dir, NULL);


    /* 2.2 emulator taskbar name */

    name = g_strdup_printf("emulator-%d", get_sdb_base_port());
    gtk_window_set_title (GTK_WINDOW (g_main_window), name);
    g_free(name);

    /* 3. skin load */

    if (load_skin_image(&PHONE) < 0) {
        ERR( "emulator skin image is not loaded.\n");
        exit(1);
    }

    /* 4. skin mask process */

    pixmap_widget = gtk_image_new_from_pixbuf (PHONE.mode_SkinImg[UISTATE.current_mode].pPixImg);
    gdk_pixbuf_render_pixmap_and_mask (PHONE.mode_SkinImg[UISTATE.current_mode].pPixImg, NULL, &SkinMask, 1);
    gtk_widget_shape_combine_mask (g_main_window, SkinMask, 0, 0);
    INFO("sets a shape for emulator window\n");

    if (SkinMask != NULL) {
        g_object_unref (SkinMask);
    }

    /* 5. emulator container */

    fixed = gtk_fixed_new ();
    if (!qemu_widget_new(&sdl_widget)) {
        ERR( "sdl_widget is failed!!\n");
        exit(1);
    }

    gtk_fixed_put (GTK_FIXED (fixed), pixmap_widget, 0, 0);
    gtk_fixed_put (GTK_FIXED (fixed), sdl_widget, PHONE.mode[UISTATE.current_mode].lcd_list[0].lcd_region.x,
            PHONE.mode[UISTATE.current_mode].lcd_list[0].lcd_region.y);
    gtk_container_add (GTK_CONTAINER (g_main_window), fixed);

#ifdef __linux__
    if (strcmp(host_uname_buf.release, "2.6.35-22-generic") == 0) { // for ubuntu 10.10 resize window bug
        gtk_window_resize (GTK_WINDOW(g_main_window),
            PHONE.mode_SkinImg[0].nImgHeight, PHONE.mode_SkinImg[0].nImgHeight);
    }
#endif

    /* 6. emulator start position */
    UISTATE.scale = ((float)get_config_type(SYSTEMINFO.virtual_target_info_file, EMULATOR_GROUP, SCALE_KEY)) / 100;
    if (UISTATE.scale <= 0 || UISTATE.scale > 1.0) {
        UISTATE.scale = 1.0;
    }
    TRACE("scale = %f\n", UISTATE.scale);

#ifdef _WIN32
    RECT host_screen;
    int emulator_w, emulator_h;
    int range;

    emulator_w = emulator_h = 0;
    ZeroMemory(&host_screen, sizeof(RECT));
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&host_screen);

    if (host_screen.left < 0) {
        int shift = host_screen.left * -1;
        host_screen.left += shift;
        host_screen.right += shift;
    }
    if (host_screen.top < 0) {
        int shift = host_screen.top * -1;
        host_screen.top += shift;
        host_screen.bottom += shift;
    }

    emulator_w = PHONE.mode_SkinImg[UISTATE.current_mode].nImgWidth * UISTATE.scale;
    emulator_h = PHONE.mode_SkinImg[UISTATE.current_mode].nImgHeight * UISTATE.scale;

    // position correction of x
    range = host_screen.left - emulator_w;
    if (configuration.main_x <= range)
    {
        INFO("configuration.main_x=%d is out of monitor range. (%d ~ %d)\n",
                configuration.main_x, range, host_screen.right);
        configuration.main_x = host_screen.left;
    }
    else if (configuration.main_x >= host_screen.right)
    {
        INFO("configuration.main_x=%d is out of monitor range. (%d ~ %d)\n",
                configuration.main_x, range, host_screen.right);
        configuration.main_x = host_screen.right - emulator_w;
    }

    // position correction of y
    range = host_screen.top - emulator_h;
    if (configuration.main_y <= range)
    {
        INFO("configuration.main_y=%d is out of monitor range. (%d ~ %d)\n",
                configuration.main_x, range, host_screen.bottom);
        configuration.main_y = host_screen.top;
    }
    else if (configuration.main_y >= host_screen.bottom)
    {
        INFO("configuration.main_y=%d is out of monitor range. (%d ~ %d)\n",
                configuration.main_x, range, host_screen.bottom);
        configuration.main_y = host_screen.bottom - emulator_h;
    }
#endif

    gtk_window_move (GTK_WINDOW (g_main_window), configuration.main_x, configuration.main_y);
    INFO("emulator window is moved (%d, %d)\n", configuration.main_x, configuration.main_y);

    /* 7. create popup menu */
    create_popup_menu (&popup_menu, &PHONE, &configuration);
    INFO("popup menu is created\n");

    /* 8. Signal connect */

    g_signal_connect (G_OBJECT(g_main_window), "motion_notify_event", G_CALLBACK(motion_notify_event_handler), NULL);
    g_signal_connect (G_OBJECT(g_main_window), "button_press_event", G_CALLBACK(motion_notify_event_handler), NULL);
    g_signal_connect (G_OBJECT(g_main_window), "button_release_event", G_CALLBACK(motion_notify_event_handler), NULL);
    g_signal_connect (G_OBJECT(g_main_window), "key_press_event", G_CALLBACK(key_event_handler), NULL);
    g_signal_connect (G_OBJECT(g_main_window), "key_release_event", G_CALLBACK(key_event_handler), NULL);
    g_signal_connect (G_OBJECT(g_main_window), "delete-event", G_CALLBACK(exit_emulator), NULL);
    g_signal_connect (G_OBJECT(g_main_window), "configure_event", G_CALLBACK(configure_event), NULL);

    gtk_widget_set_has_tooltip(g_main_window, TRUE);
    g_signal_connect (G_OBJECT(g_main_window), "query-tooltip", G_CALLBACK(query_tooltip_event), NULL);

    gtk_widget_set_events (g_main_window, GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);// | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    /* 9. widget show all */

    gtk_window_set_keep_above(GTK_WINDOW (g_main_window), configuration.always_on_top);
    gtk_widget_show_all (g_main_window);
    gtk_widget_queue_resize (g_main_window);

    if (UISTATE.scale != 1.0) {
        scale_event_callback(&PHONE, UISTATE.current_mode);
    }
}

static void* run_gtk_main(void* arg)
{
    /* 10. gtk main start */
    init_sensor_server();
    gtk_main();

    return NULL;
}

#ifdef _WIN32
static void* construct_main_window_and_run_gtk_main(void* arg)
{
    construct_main_window();
    init_sensor_server();
    gtk_main();
    return NULL;
}
#endif



/**
 * @brief    init startup structure
 * @return   success  0,  fail    -1
 * @date     Nov 3. 2008
 * */

static void init_startup_option(void)
{
    memset(&(startup_option), 0x00, sizeof(startup_option));
    startup_option.run_level = 5;
    startup_option.mountPort = 1301;
    startup_option.telnet_port = 1201;
    startup_option.ssh_port = 1202;
    if(ENABLE_MULTI)
    {
        while(check_port(LOCALHOST, startup_option.mountPort) == 0)
            startup_option.mountPort++;
    }

    startup_option.no_dump = FALSE;
    startup_option.vtm = "default";
}


/**
 * @brief    startup option
 *           kill application and load kernel driver
 *
 * @return   success  0,  fail    -1
 * @date     May 18. 2009
 * */

static int startup_option_parser(int *argc, char ***argv)
{
    /* 1. Goption handling */

    gboolean version = FALSE;
    GOptionContext *context = NULL;
    GError *error = NULL;
    char timeinfo[256] = { 0, };
    struct tm *tm_time;
    struct timeval tval;
    char string[MAXBUF];
    FILE *fp = NULL;
    char *info_file;
    char *arch = getenv(EMULATOR_ARCH);
    const gchar *exec_path = get_exec_path();
    if(!arch) /* for stand alone */
    {
        char *binary = g_path_get_basename(exec_path);
        if(strstr(binary, EMULATOR_X86))
            arch = g_strdup_printf(X86);
        else if(strstr(binary, EMULATOR_ARM))
            arch = g_strdup_printf(ARM);
        else
        {
            ERR( "binary setting failed\n");
            exit(1);
        }
        free(binary);
    }

    GOptionEntry options[] = {
        {"disk", 0, 0, G_OPTION_ARG_STRING, &startup_option.disk, "Disk image path", "\"disk path\""},
        {"vtm", 0, 0, G_OPTION_ARG_STRING, &startup_option.vtm, "Virtual target image file", "\"*.x86 or *.arm\""},
        {"run-level", 0, 0, G_OPTION_ARG_INT, &startup_option.run_level, "Run level", "5"},
        {"version", 0, 0, G_OPTION_ARG_NONE, &version, "Version info", NULL},
        {"Port", 0, 0, G_OPTION_ARG_INT, &startup_option.mountPort, "Port for NFS mounting", "\"default is 1301\""},
        {"ssh-port", 0, 0, G_OPTION_ARG_INT, &startup_option.ssh_port, "Port for ssh to guest", NULL},
        {"telnet-port", 0, 0, G_OPTION_ARG_INT, &startup_option.telnet_port, "Port for telnet to guest", NULL},
        {"no-dump", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &startup_option.no_dump, "Disable dump feature", NULL},
        {NULL}
    };

    /* 2. Goption parsing */

    context = g_option_context_new ("- Tizen SDK Emulator");
    g_option_context_set_help_enabled(context, FALSE);
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));

    if (!g_option_context_parse (context, argc, argv, &error)) {
        fprintf(stderr, "%s: option parsing failed\n", (*argv)[0]);
        exit (1);
    }

    /* 3. starting info */
    gettimeofday(&tval, NULL);
    tm_time = localtime(&(tval.tv_sec));
    strftime(timeinfo, sizeof(timeinfo), "%Y/%m/%d %H:%M:%S", tm_time);
    INFO("=========INFO START========\n");
    INFO("Current time : %s\n", timeinfo);
    INFO("SDK version : %s(%s), Build date : %s\n", build_version, build_git, build_date);
#ifdef __linux__
    INFO("Qemu build machine linux kernel version : (%d, %d, %d)\n",
        LINUX_VERSION_CODE >> 16, (LINUX_VERSION_CODE >> 8) & 0xff , LINUX_VERSION_CODE & 0xff);

        if (uname(&host_uname_buf) == 0) {
        INFO("Host uname : %s %s %s %s %s\n", host_uname_buf.sysname, host_uname_buf.nodename,
            host_uname_buf.release, host_uname_buf.version, host_uname_buf.machine);
        }
#endif

    INFO("Host sdl version : (%d, %d, %d)\n", SDL_Linked_Version()->major, SDL_Linked_Version()->minor, SDL_Linked_Version()->patch);

    char *virtual_target_path = get_virtual_target_path(startup_option.vtm);
    info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
    if( (fp = fopen(info_file, "r")) == NULL )
    {
        ERR("can't open %s", info_file);
        exit(1);
    }

    if (!startup_option.vtm)
        startup_option.vtm = g_strdup_printf("default");
    startup_option.disk = g_strdup_printf("%semulimg-%s.%s",virtual_target_path, startup_option.vtm, arch);
    INFO("target name : %s, disk path : %s\n", startup_option.vtm, startup_option.disk);

    INFO("\n");
    while(fgets(string, MAXBUF, fp)!=NULL)
        INFO("%s", string);
    dbg_printf("\n");
    INFO("=========INFO END========\n");

    fclose(fp);
    free(virtual_target_path);
    free(info_file);

    return 0;
}


/**
 * @brief    init structure
 * @return   success  0,  fail    -1
 * @date     Nov 3. 2008
 * */

static int init_structure(void)
{

    /* 3. make startup option structure */

    init_startup_option();

    return 0;
}


/**
 * @brief   init emulator
 * @param argc  number of argument
 * @param argv  argument vector
 * @return  void
 * @date    4. 14. 2008
 * */

static void init_emulator(int *argc, char ***argv)
{
    /* 1. thread_init */

    emulator_mutex_init();
#ifndef _WIN32
    g_thread_init(NULL);
    XInitThreads();
#endif

    /* 3. gtk_init */

    gtk_init (argc, argv);

    /* 4. structure init */

    init_structure();

    /* 5. make hash init */

    window_hash_init ();
}


/**
 * @brief    function to load config parsed to qemu option
 * @return   success  0,  fail    -1
 * @date     Apr 22. 2009
 * */

static int load_config_passed_to_qemu (arglist* al, int argc, char **argv)
{
    int i;

    /* 1. load configuration and show option window */

    if (load_config_file(&SYSTEMINFO) < 0) {
        ERR( "load configuration file error!!\n");
        return -1;
    }

    TRACE( "load config file complete\n");



    if (determine_skin(&virtual_target_info, &configuration) < 0) {
        ERR( "invalid skin file\n");
        return -1;
    }

    /* 2. skin parse dbi file and fill the structure */

    if (skin_parser(configuration.skin_path, &PHONE) < 0) {
        ERR( "skin parse error\n");
        return -1;
    }

    TRACE( "skin parse complete\n");

    /* 3. parsed to qemu startup option when ok clicked */

    qemu_option_set_to_config(al);

    /*
     * note: g_option_context_parse modifies argc and argv
     * Append args after -- to QEMU command line
     */
    if (argc > 2 && !strcmp(argv[1], "--")) {
        for (i=2; i<argc; i++) {
            /* if snapshot boot set then skip -loadvm snapshot option */
            if(configuration.qemu_configuration.save_emulator_state == 0 && strcmp(argv[i],"-loadvm") == 0){
                i++;    // skip snapshot id
            }
            else append_argvlist(al, "%s", argv[i]);
        }
    }

    return 0;
}

#ifndef _WIN32
static void emul_prepare_process(void)
{
    gchar cmd[256] = "";

    /* start the vmodem*/
    if(qemu_arch_is_arm()) {
        const char* target_path = get_target_path();

        if(configuration.qemu_configuration.diskimg_type)
            sprintf (cmd, "/opt/tizen_sdk/simulator/vmodem_arm");
        else
            sprintf (cmd, "%s/usr/bin/vmodem_arm", target_path);

        if(emul_create_process(cmd) == FALSE)
            fprintf(stderr, "create vmodem failed\n");
    }

    /* start serial console */
    if(configuration.qemu_configuration.serial_console_command_type == 1 &&
            configuration.qemu_configuration.telnet_type == 1) {
        sprintf(cmd, "%s", configuration.qemu_configuration.serial_console_command);

        if(emul_create_process(cmd) == FALSE)
            fprintf(stderr, "create serial console failed\n");
    }
}
#endif

int make_shdmem()
{
    char *virtual_target_path = NULL;
    virtual_target_path = get_virtual_target_path(startup_option.vtm);
    tizen_base_port = get_sdb_base_port();
#ifndef _WIN32
    int shmid;
    char *shared_memory;
    shmid = shmget((key_t)tizen_base_port, MAXPATH, 0666|IPC_CREAT);
    if (shmid == -1)
    {
        ERR("shmget failed");
        return -1;
    }
    shared_memory = shmat(shmid, (char*)0x00, 0);
    if (shared_memory == (void *)-1)
    {
        ERR("shmat failed");
        return -1;
    }
    sprintf(shared_memory, "%s", virtual_target_path);
    INFO( "shared memory key: %d value: %s\n", tizen_base_port, (char*)shared_memory);
#else
    HANDLE hMapFile;
    char* pBuf;
    char* port_in_use;
    char *shared_memory;
    shared_memory = g_strdup_printf("%s", virtual_target_path);
    port_in_use =  g_strdup_printf("%d", tizen_base_port);
    hMapFile = CreateFileMapping(
                 INVALID_HANDLE_VALUE,    // use paging file
                 NULL,                    // default security
                 PAGE_READWRITE,          // read/write access
                 0,                       // maximum object size (high-order DWORD)
                 50,                // maximum object size (low-order DWORD)
                 port_in_use);                 // name of mapping object
    if (hMapFile == NULL)
    {
        ERR("Could not create file mapping object (%d).\n", GetLastError());
        return -1;
    }
    pBuf = MapViewOfFile(hMapFile,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        50);

    if (pBuf == NULL)
    {
        ERR("Could not map view of file (%d).\n", GetLastError());
        CloseHandle(hMapFile);
        return -1;
    }

    CopyMemory((PVOID)pBuf, shared_memory, strlen(shared_memory));
    free(port_in_use);
    free(shared_memory);
#endif
    free(virtual_target_path);
    return 0;
}


static void redirect_log(void)
{
    static char logfile[256] = { 0, };

    strcpy(logfile, get_virtual_target_log_path(startup_option.vtm));
    strcat(logfile, EMUL_LOGFILE);

    g_out_fp = freopen(logfile, "a+", stdout);
    if (g_out_fp ==NULL) {
        fprintf(stderr, "log file open error\n");
    }

    g_err_fp = freopen(logfile, "a+", stderr);
    if (g_err_fp ==NULL) {
        fprintf(stderr, "log file open error\n");
    }

    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

}

/**
 * @brief    function to create emulator
 * @param argc        number of argument
 * @param argv        argument vector
 *
 * @return   success  0,  fail    -1
 * @date     Apr 22. 2009
 * */

int main(int argc, char** argv)
{
    //int sensor_port = SENSOR_PORT;
    int i, r;
    pthread_t thread_gtk_id;

    init_emulator(&argc, &argv);

    startup_option_parser(&argc, &argv);

    /* redirect stderr, stdout to log file */

    redirect_log();

    /* initailize socket for windows */

    socket_init();

    /* make shared memory not for launching multiple instance of one target */
    make_shdmem();
    INFO("created a shared memory\n");

    /* option parsed and pass to qemu option */

    r = load_config_passed_to_qemu(&g_qemu_arglist, argc, argv);
    if (r < 0) {
        ERR( "option parsed and pass to qemu option error!!\n");
        return -1;
    }
    INFO("Arguments : ");
    for(i=0; i<g_qemu_arglist.argc; i++){
        dbg_printf_nonewline("%s ", g_qemu_arglist.argv[i]);
    }
    dbg_printf("\n");

    /* signal handler */

    register_sig_handler();

#ifndef _WIN32
    construct_main_window();

    /* create gtk thread  */
    if (pthread_create(&thread_gtk_id, NULL, run_gtk_main, NULL) != 0) {
        ERR( "error creating gtk_id thread!!\n");
        return -1;
    }
#else /* _WIN32 */
    /* if _WIN32, window creation and gtk main must be run in a thread */
    if (pthread_create(&thread_gtk_id, NULL, construct_main_window_and_run_gtk_main, NULL) != 0) {
        ERR( "error creating gtk_id thread!!\n");
        return -1;
    }
#endif
    INFO("run_gtk_main\n");

#ifdef ENABLE_OPENGL_SERVER
    /* create OPENGL server thread */
    if (pthread_create(&thread_opengl_id, NULL, init_opengl_server, NULL) != 0) {
        ERR( "error creating opengl_id thread!!");
        return -1;
    }
#endif  /* ENABLE_OPENGL_SERVER */

    /* create serial console and vmodem, and other processes */
#ifndef _WIN32
    emul_prepare_process();
#endif

    qemu_main(g_qemu_arglist.argc, g_qemu_arglist.argv, NULL);

    return 0;
}

gboolean  update_progress_bar(GIOChannel *channel, GIOCondition condition, gpointer data)
{
    unsigned int len = 0;
    GIOError error;
    GIOStatus status;
    gchar *recvbuffer;
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[MAX_TIME_STR];
    char telnet_commands[MAX_COMMANDS][MAX_LENGTH] = {
        "read", //dummy command for local use
        "delvm snapshot\r",
        "read", //dummy command for local use
        "savevm snapshot\r",
        "read", //dummy command for local use
    };

    if((condition==G_IO_IN) && !(vmstate%2))
    {
        status = g_io_channel_read_line(channel, &recvbuffer, NULL, NULL, NULL);
        if(status!=G_IO_STATUS_NORMAL)
        {
            printf("recv() failed or connection closed prematurely %d\n", status);
            vmstate = -1;
            g_io_channel_unref (channel);
            g_io_channel_shutdown(channel, TRUE, NULL);
            return FALSE;
        }
        else
        {
            char *ptr = NULL;
            if((ptr = strstr(recvbuffer, "Completed="))!=NULL)
            {
                ptr[18]='\0';
                gdouble fraction = atof(ptr+10);
                gtk_progress_bar_set_fraction(savevm_progress,fraction);
            }
            else if (strstr(recvbuffer, "SaveVM Complete"))
            {
                gtk_progress_bar_set_fraction(savevm_progress,1);
                gtk_progress_bar_set_text(savevm_progress,"Save State Successful");
                gtk_label_set_text(GTK_LABEL(savevm_label), "Please close this dialog");
                g_free(recvbuffer);
                g_io_channel_shutdown(channel, TRUE, NULL);
                g_io_channel_unref (channel);
                vmstate = 0;
                rawtime = time(NULL);
                timeinfo = localtime(&rawtime);
                strftime(time_str, MAX_TIME_STR, "%Y-%m-%d %H:%M:%S", timeinfo);
                printf("%s\n", time_str);

                virtual_target_info.snapshot_saved = 1;
                snprintf(virtual_target_info.snapshot_saved_date, MAXBUF, "%s", time_str);
                set_config_type(SYSTEMINFO.virtual_target_info_file, ETC_GROUP, SNAPSHOT_SAVED_KEY, virtual_target_info.snapshot_saved);
                set_config_value(SYSTEMINFO.virtual_target_info_file, ETC_GROUP, SNAPSHOT_SAVED_DATE_KEY, virtual_target_info.snapshot_saved_date);

                return FALSE;
            }

            g_free(recvbuffer);

            if(vmstate<(MAX_COMMANDS-1))
                vmstate++;

            return TRUE;
        }
    }

    if(vmstate%2)
    {
        error = g_io_channel_write(channel,telnet_commands[vmstate],\
                strlen(telnet_commands[vmstate]), \
                &len);
        if(error!=G_IO_ERROR_NONE)
        {
            g_io_channel_unref (channel);
            g_io_channel_shutdown(channel, TRUE, NULL);
            vmstate = -1;
            printf("send() failed or connection closed prematurely %d\n", G_IO_ERROR_UNKNOWN);
            return FALSE;
        }
        else
        {
            if(vmstate<MAX_COMMANDS)
                vmstate++;
            return TRUE;
        }
    }

    if(((condition==G_IO_ERR) || (condition==G_IO_HUP))&&(vmstate<=(MAX_COMMANDS-1)))
    {
        g_io_channel_unref (channel);
        g_io_channel_shutdown(channel, TRUE, NULL);
        close(vmsock);
        vmstate = 0;
        return FALSE;
    }
    return TRUE;
}

void save_emulator_state(void)
{
    /* Connect to the monitor console */
    struct sockaddr_in server;
    unsigned short server_port = 9000;
    char *server_ip = "127.0.0.1";


    /* build the UI */
    GtkBuilder *builder = gtk_builder_new();
    char full_glade_path[256];
    sprintf(full_glade_path, "%s/savevm.glade", get_root_path());
    gtk_builder_add_from_file(builder, full_glade_path, NULL);

    //get objects from the UI
    savevm_window = (GtkWidget *)gtk_builder_get_object(builder, "savevm_window");
    savevm_progress = (GtkProgressBar *)gtk_builder_get_object(builder, "savevm_progress");
    savevm_label = (GtkWidget *)gtk_builder_get_object(builder, "savevm_label");
    gtk_progress_bar_set_text(savevm_progress,"Saving State in progress...");
    //gtk_builder_connect_signals(builder, NULL);
    gtk_widget_show(savevm_window);

    if ((vmsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        printf("socket() failed\n");
        return ;
    }
    memset(&server, 0, sizeof(server));
    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = inet_addr(server_ip);
    server.sin_port        = htons(server_port);
    if (connect(vmsock, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
        printf("connect() failed\n");
        close(vmsock);
        return ;
    }

    channel=g_io_channel_unix_new(vmsock);
    if(channel==NULL)
    {
        printf("gnet_tcp_socket_get_io_channel() failed\n");
        close(vmsock);
        return ;
    }
    //g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, NULL);

    guint sourceid = g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP, \
            update_progress_bar, NULL);

    if(sourceid<=0)
    {
        printf("g_io_add_watch() failed\n");
        g_io_channel_unref(channel);
        close(vmsock);
        return ;
    }
    printf("Added to gmain loop = %d\n", sourceid);
}

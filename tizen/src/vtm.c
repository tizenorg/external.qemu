/*
 * Emulator Manager
 *
 * Copyright (C) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: 
 * MunKyu Im <munkyu.im@samsung.com>
 * DoHyung Hong <don.hong@samsung.com>
 * SeokYeon Hwang <syeon.hwang@samsung.com>
 * Hyunjun Son <hj79.son@samsung.com>
 * SangJin Kim <sangjin3.kim@samsung.com>
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
#include "vtm.h"
#include "debug_ch.h"
#ifndef _WIN32
#include <sys/ipc.h>  
#include <sys/shm.h> 
#endif
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else /* !_WIN32 */
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif /* !_WIN32 */

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, emulmgr);

GtkBuilder *g_builder;
GtkBuilder *g_create_builder;
enum {
    TARGET_NAME,
    RAM_SIZE,
    RESOLUTION,
    MINOR,
    N_COL
};
GtkWidget *g_main_window;
VIRTUALTARGETINFO virtual_target_info;
gchar *g_target_list_filepath;
gchar *g_info_file;
GtkWidget *treeview;
int sdcard_create_size;
GtkWidget *f_entry;
gchar g_icon_image[MAXPATH] = {0, };
const char *g_major_version;
int g_minor_version;
gchar *g_userfile;
#ifdef _WIN32
HANDLE g_hFile;
void socket_cleanup(void)
{
    WSACleanup();
}
#else
int g_fd;
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

static int check_port_bind_listen(u_int port)
{
    struct sockaddr_in addr;
    int s, opt = 1;
    int ret = -1;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, addrlen);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (((s = socket(AF_INET,SOCK_STREAM,0)) < 0) ||
            (setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(int)) < 0) ||
            (bind(s,(struct sockaddr *)&addr, sizeof(addr)) < 0) ||
            (listen(s,1) < 0)) {

        /* fail */
        ret = -1;
        ERR( "port(%d) listen  fail \n", port);
    }else{
        /*fsucess*/
        ret = 1;
        INFO( "port(%d) listen  ok \n", port);
    }

#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif

    return ret;
}

void activate_target(char *target_name)
{
    char *cmd = NULL;
    char *enable_kvm = NULL;
    gchar *path;
    char *info_file;
    char *virtual_target_path;
    char *binary = NULL;
    char *emul_add_opt = NULL;
    char *qemu_add_opt = NULL;
    char *disk_path = NULL;
    char *basedisk_path = NULL;
    char *error_log = NULL;
    char *arch= NULL;
    int info_file_status;   
    int status;

    if(check_shdmem(target_name, CREATE_MODE) == -1)
        return ;

    arch = getenv("EMULATOR_ARCH");
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return ;
    }

    path = (char*)get_arch_path();
    virtual_target_path = get_virtual_target_path(target_name);
    info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
    info_file_status = is_exist_file(info_file);
    //if targetlist exist but config file not exists
    if(info_file_status == -1 || info_file_status == FILE_NOT_EXISTS)
    {
        ERR( "target info file does not exist : %s\n", target_name);
        char *message = g_strdup_printf("config.ini file does not exist\n\n"
                                "   - [%s]", info_file);
        show_message("Error", message);
        free(message);
        return ;
    }

    disk_path = get_config_value(info_file, HARDWARE_GROUP, DISK_PATH_KEY);
    basedisk_path = get_config_value(info_file, HARDWARE_GROUP, BASEDISK_PATH_KEY);
    /* check image & base image */
    if(access(disk_path, R_OK) != 0){
        error_log = g_strdup_printf("The image does not exist. \n\n"
                "    - [%s]", disk_path);
        show_message("Error", error_log);
        g_free(error_log);
        return;
    }    
    if(access(basedisk_path, R_OK) != 0){
        error_log = g_strdup_printf("The base image does not exist. \n\n"
                "    - [%s]", basedisk_path);
        show_message("Error", error_log);
        g_free(error_log);
        return;
    } 

    enable_kvm = check_kvm(info_file, &status);
    if(status < 0)
    {
        show_message("Error", "Fail to set kvm.");
        return;
    }

    binary = get_config_value(info_file, QEMU_GROUP, BINARY_KEY);
    emul_add_opt = get_config_value(info_file, ADDITIONAL_OPTION_GROUP, EMULATOR_OPTION_KEY);
    qemu_add_opt = get_config_value(info_file, ADDITIONAL_OPTION_GROUP, QEMU_OPTION_KEY);

    if(emul_add_opt == 0)
        emul_add_opt = g_strdup_printf(" ");
    if(qemu_add_opt == 0)
        qemu_add_opt = g_strdup_printf(" ");
#ifndef _WIN32
    if(strcmp(arch, X86) == 0)
    {
        cmd = g_strdup_printf("./%s --vtm %s %s"
                "-- -vga tizen -bios bios.bin -L %s/data/pc-bios -kernel %s/data/kernel-img/bzImage %s %s",
                binary, target_name, emul_add_opt, path, path, enable_kvm, qemu_add_opt);
    }
    else if(strcmp(arch, ARM)== 0)
    {
        cmd = g_strdup_printf("./%s --vtm %s %s"
            "--  -kernel %s/data/kernel-img/zImage %s",
            binary, target_name, emul_add_opt, path, qemu_add_opt);
    }
    else
    {
        show_message("Error", "Architecture setting failed.");
        return ;
    }
#else /*_WIN32 */
    if(strcmp(arch, X86) == 0)
    {
        cmd = g_strdup_printf("\"%s\" --vtm %s %s"
                "-- -vga tizen -bios bios.bin -L \"%s/data/pc-bios\" -kernel \"%s/data/kernel-img/bzImage\" %s %s",
                binary, target_name, emul_add_opt, path, path, enable_kvm, qemu_add_opt );
    }else if(strcmp(arch, ARM) == 0)
    {
            cmd = g_strdup_printf("\"%s\" --vtm %s %s"
            "--  -kernel \"%s/data/kernel-img/zImage\" %s",
            binary, target_name, emul_add_opt, path, qemu_add_opt);
    }
    else
    {
        show_message("Error", "Architecture setting failed.");
        return ;
    }
#endif

#ifdef _WIN32
    if(WinExec(cmd, SW_SHOW) < 31)
    {
        show_message("Error", "Fail to start Emulator!");
        g_free(cmd);
        return;
    }
#else
    GError *error = NULL;
    if(!g_spawn_command_line_async(cmd, &error))
    {
        TRACE( "Failed to invoke command: %s\n", error->message);
        show_message("Failed to invoke command", error->message);
        g_error_free(error);
        g_free(cmd);
        return ;
    }
#endif
    g_free(cmd);

    return;
}

char *check_kvm(char *info_file, int *status)
{
    char *enable_kvm = NULL;
#ifndef _WIN32
    char *kvm = NULL;
    int fd;
    kvm = get_config_value(info_file, QEMU_GROUP, KVM_KEY);
    if(g_file_test("/dev/kvm", G_FILE_TEST_EXISTS) && strcmp(kvm,"1") == 0)
    {
        fd = open("/dev/kvm", O_RDWR);
        if (fd == -1)
        {
            ERR( "Could not access KVM kernel module: %m\n");
            show_message("Error", "Could not access KVM kernel module: Permission denied\n"
                "You can add the login user to the KVM group by following command \n"
                 " - $sudo addgroup `whoami` kvm");
            *status = fd;
        }
        else
            enable_kvm = g_strdup_printf("-enable-kvm");
         
        close(fd);
    }
    else
        enable_kvm = g_strdup_printf(" ");
#else /* _WIN32 */
    enable_kvm = g_strdup_printf(" ");
#endif
    *status = 1;
    return enable_kvm;
}

int check_shdmem(char *target_name, int type)
{
    char *virtual_target_path = NULL;
    virtual_target_path = get_virtual_target_path(target_name);
#ifndef _WIN32
    int shm_id;
    void *shm_addr;
    u_int port;
    int val;
    struct shmid_ds shm_info;

    for(port=26100;port < 26200; port += 10)
    {
        if ( -1 != ( shm_id = shmget( (key_t)port, 0, 0)))
        {
            if((void *)-1 == (shm_addr = shmat(shm_id, (void *)0, 0)))
            {
                ERR( "error occured at shmat()");
                break;
            }

            val = shmctl(shm_id, IPC_STAT, &shm_info);
            if(val != -1)
            {
                INFO( "count of process that use shared memory : %d\n", shm_info.shm_nattch);
                if(shm_info.shm_nattch > 0 && strcmp(virtual_target_path, (char*)shm_addr) == 0)
                {
                    if(check_port_bind_listen(port+1) > 0){
                        shmdt(shm_addr);
                        continue;
                    }
                    switch(type)
                    {
                    case CREATE_MODE:
                        show_message("Warning", "Can not activate this target.\n"
                                "Virtual target with the same name is running now!");
                        break;
                    case DELETE_MODE:
                        show_message("Warning", "Can not delete this target.\n"
                                "Virtual target with the same name is running now!");
                        break;
                    case MODIFY_MODE:
                        show_message("Warning", "Can not modify this target.\n"
                                "Virtual target with the same name is running now!");
                        break;
                    case RESET_MODE:
                        show_message("Warning", "Can not reset this target.\n"
                                "Virtual target with the same name is running now!");
                        break;
                    case DELETE_GROUP_MODE:
                        show_message("Warning", "Can not delete this group.\n"
                                "Some virtual target in this group is running now!");
                        break;

                    default:
                        ERR("wrong type passed\n");
                    }

                    shmdt(shm_addr);
                    return -1;
                }
                else{
                    shmdt(shm_addr);
                }
            }
        }
    }

#else /* _WIN32*/
    u_int port;
    char* base_port = NULL;
    char* pBuf;
    HANDLE hMapFile;
    for(port=26100;port < 26200; port += 10)
    {
        base_port = g_strdup_printf("%d", port);
        hMapFile = OpenFileMapping(
                 FILE_MAP_READ,
                 TRUE,
                 base_port);          
        if(hMapFile == NULL)
        {
            INFO("port %s is not used.\n", base_port);
            continue;
        }
        else
        {
             pBuf = (char*)MapViewOfFile(hMapFile,
                        FILE_MAP_READ,
                        0,
                        0,
                        50);
            if (pBuf == NULL)
            {
                ERR("Could not map view of file (%d).\n", GetLastError());
                CloseHandle(hMapFile);
                return -1;
            }

            if(strcmp(pBuf, virtual_target_path) == 0)
            {
                if(check_port_bind_listen(port+1) > 0)
                {
                    UnmapViewOfFile(pBuf);
                    CloseHandle(hMapFile);
                    continue;
                }
                
                switch(type)
                {
                case CREATE_MODE:
                    show_message("Warning", "Can not activate this target.\n"
                            "Virtual target with the same name is running now!");
                    break;
                case DELETE_MODE:
                    show_message("Warning", "Can not delete this target.\n"
                            "Virtual target with the same name is running now!");
                    break;
                case MODIFY_MODE:
                    show_message("Warning", "Can not modify this target.\n"
                            "Virtual target with the same name is running now!");
                    break;
                case RESET_MODE:
                    show_message("Warning", "Can not reset this target.\n"
                            "Virtual target with the same name is running now!");
                    break;
                default:
                    ERR("wrong type passed\n");
                }
                
                UnmapViewOfFile(pBuf);
                CloseHandle(hMapFile);
                free(base_port);
                return -1;
            }
            else
                UnmapViewOfFile(pBuf);
        }

        CloseHandle(hMapFile);
        free(base_port);
    }
#endif
    free(virtual_target_path);
    return 0;

}

void entry_changed(GtkEditable *entry, gpointer data)
{
    const gchar *name = gtk_entry_get_text (GTK_ENTRY (entry));
    char* target_name = (char*)data;
    GtkWidget *label4 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "label4");
    gtk_label_set_text(GTK_LABEL(label4),"Input name of the virtual target.");

    GtkWidget *ok_button = (GtkWidget *)gtk_builder_get_object(g_create_builder, "button7");
    char* dst =  malloc(VT_NAME_MAXBUF);

    if(strlen(name) == VT_NAME_MAXBUF)
    {
        WARN( "The length of name can not be greater than 20 letters.\n");
        gtk_label_set_text(GTK_LABEL(label4),"The length of name can not be greater\nthan 20 letters.");
        gtk_widget_set_sensitive(ok_button, FALSE);
        return ;
    }
    escapeStr(name, dst);
    if(strcmp(name, dst) != 0)
    {
        WARN( "Virtual target name is invalid! Valid characters are 0-9 a-z A-Z -_\n");
        gtk_label_set_text(GTK_LABEL(label4),"Name is invalid!\nValid characters are 0-9 a-z A-Z -_");
        gtk_widget_set_sensitive(ok_button, FALSE);
        free(dst);
        return;
    }

    if(strcmp(name, "") == 0)
    {
        WARN( "Input name of the virtual target.\n");
        gtk_label_set_text(GTK_LABEL(label4),"Input name of the virtual target.");
        gtk_widget_set_sensitive(ok_button, FALSE);
        return;
    }
    else
        snprintf(virtual_target_info.virtual_target_name, MAXBUF, "%s", name);

    if(!target_name) // means create mode
    {
        if(name_collision_check() == 1)
        {
            WARN( "Virtual target with the same name exists! \n Choose another name.\n");
            gtk_label_set_text(GTK_LABEL(label4),"Virtual target with the same name exists!\nChoose another name.");
            gtk_widget_set_sensitive(ok_button, FALSE);
            return;
        }
    }
    else
    {
        if(strcmp(name, target_name)== 0)
        {
            gtk_widget_set_sensitive(ok_button, TRUE);
            return;
        }
        else
        {
            if(name_collision_check() == 1)
            {
                WARN( "Virtual target with the same name exists! \nChoose another name.\n");
                gtk_label_set_text(GTK_LABEL(label4),"Virtual target with the same name exists!\nChoose another name.");
                gtk_widget_set_sensitive(ok_button, FALSE);
                return;
            }
            else
                snprintf(virtual_target_info.virtual_target_name, MAXBUF, "%s", name);
        }
    }

    gtk_widget_set_sensitive(ok_button, TRUE);
}

void show_modify_window(char *target_name)
{
    GtkWidget *sub_window;
    char *virtual_target_path;
    int info_file_status;
    char full_glade_path[MAX_LEN];
    char *arch = (char*)g_getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return ;
    }

    g_create_builder = gtk_builder_new();
    sprintf(full_glade_path, "%s/etc/vtm.glade", get_root_path());

    gtk_builder_add_from_file(g_create_builder, full_glade_path, NULL);

    sub_window = (GtkWidget *)gtk_builder_get_object(g_create_builder, "window2");

    add_window(sub_window, VTM_CREATE_ID);

    if(strcmp(arch, X86) == 0)
        gtk_window_set_title(GTK_WINDOW(sub_window), "Modify existing Virtual Target(x86)");
    else if(strcmp(arch, ARM) == 0)
        gtk_window_set_title(GTK_WINDOW(sub_window), "Modify existing Virtual Target(arm)");    

    virtual_target_path = get_virtual_target_path(target_name);
    g_info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
    info_file_status = is_exist_file(g_info_file);
    //if targetlist exist but config file not exists
    if(info_file_status == -1 || info_file_status == FILE_NOT_EXISTS)
    {
        ERR( "target info file does not exist : %s\n", target_name);
        return;
    }

    //  fill_modify_target_info();
    /* setup and fill name */
    GtkWidget *name_entry = (GtkWidget *)gtk_builder_get_object(g_create_builder, "entry1");
    gtk_entry_set_text(GTK_ENTRY(name_entry), target_name);
    gtk_entry_set_max_length(GTK_ENTRY(name_entry), VT_NAME_MAXBUF); 

    GtkWidget *label4 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "label4");
    gtk_label_set_text(GTK_LABEL(label4),"Input name of the virtual target.");
    g_signal_connect(G_OBJECT (name_entry), "changed",  G_CALLBACK (entry_changed), (gpointer*)target_name);

    setup_modify_frame(target_name);
    setup_modify_button(target_name);

    gtk_window_set_icon_from_file(GTK_WINDOW(sub_window), g_icon_image, NULL);

    g_signal_connect(GTK_OBJECT(sub_window), "delete_event", G_CALLBACK(create_window_deleted_cb), NULL);

    gtk_widget_show_all(sub_window);

    gtk_main();
}

void env_init(void)
{
    char* arch;
    char version_path[MAX_LEN];
    //architecture setting
    if(!g_getenv("EMULATOR_ARCH"))
        arch = g_strdup_printf(X86);
    else
        return;

    g_setenv(EMULATOR_ARCH, arch, 1);
    INFO( "architecture : %s\n", arch);

    //latest version setting
    sprintf(version_path, "%s/version.ini",get_etc_path());
    g_major_version = get_config_value(version_path, VERSION_GROUP, MAJOR_VERSION_KEY);
    g_minor_version = get_config_type(version_path, VERSION_GROUP, MINOR_VERSION_KEY);
    
    //make user directories or targetlist.ini if they don't exist.
    make_tizen_vms();

    //make default target of the latest version
    make_default_image(DEFAULT_TARGET);

    refresh_clicked_cb();

}

void arch_select_cb(GtkWidget *widget, gpointer data)
{
    gchar *arch;

    GtkToggleButton *toggled_button = GTK_TOGGLE_BUTTON(data);

    if(gtk_toggle_button_get_active(toggled_button) == TRUE)
    {
        arch = (char*)gtk_button_get_label(GTK_BUTTON(toggled_button));
        g_unsetenv(EMULATOR_ARCH);
        g_setenv(EMULATOR_ARCH, arch, 1);
        INFO( "architecture : %s\n", arch);
        g_target_list_filepath = get_targetlist_filepath();
        refresh_clicked_cb();
        //      g_free(arch);
    }

}

void modify_clicked_cb(GtkWidget *widget, gpointer selection)
{
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkTreeIter  iter;
    char *target_name;
    char *virtual_target_path;

    store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(treeview));

    if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) 
        return;

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
                &model, &iter)) {
        //get target name
        gtk_tree_model_get(model, &iter, TARGET_NAME, &target_name, -1);

        if(check_shdmem(target_name, MODIFY_MODE)== -1)
            return;

        virtual_target_path = get_virtual_target_path(target_name);
        show_modify_window(target_name);    
        g_free(virtual_target_path);
    }
    else
    {
        show_message("Warning", "Target is not selected. Firstly select a target and modify.");
        return;
    }
}

void activate_clicked_cb(GtkWidget *widget, gpointer selection)
{
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkTreeIter  iter;
    char *target_name;
    char *virtual_target_path;

    store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(treeview));

    if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) 
        return;

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
                &model, &iter)) {
        //get target name
        gtk_tree_model_get(model, &iter, TARGET_NAME, &target_name, -1);
        virtual_target_path = get_virtual_target_path(target_name);
        activate_target(target_name);
        //change name if it's running
//      static const char suffix[] = "(running)";
//      char *new_name = malloc(strlen(target_name) + sizeof suffix);
//      strcpy(new_name, target_name);
//      strcat(new_name, suffix);
//      gtk_list_store_set(store, &iter, TARGET_NAME, new_name, -1);
//      free(new_name);
        g_free(virtual_target_path);
        g_free(target_name);
    }
    else
    {
        show_message("Warning", "Target is not selected. Firstly select a target and activate.");
        return;
    }
}

void cursor_changed_cb(GtkWidget *widget, gpointer selection)
{
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkTreeIter  iter;
    char *target_name;
    GtkWidget *modify_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button3");
    GtkWidget *reset_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button9");
    GtkWidget *start_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button4");
    GtkWidget *details_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button5");
    GtkWidget *delete_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button2");

    store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(treeview));

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
                &model, &iter)) {
        
        gtk_tree_model_get(model, &iter, TARGET_NAME, &target_name, -1);

        if(is_group(target_name) == TRUE){
            if(strcmp(target_name, g_major_version) == 0 || 
                    strcmp(target_name, CUSTOM_GROUP) == 0)
                gtk_widget_set_sensitive(delete_button, FALSE);
            else
                gtk_widget_set_sensitive(delete_button, TRUE);
            
            gtk_widget_set_sensitive(details_button, FALSE);
            gtk_widget_set_sensitive(modify_button, FALSE);
            gtk_widget_set_sensitive(reset_button, FALSE);
            gtk_widget_set_sensitive(start_button, FALSE);
        }
        else
        {
            if(strcmp(target_name, DEFAULT_TARGET) == 0)
            {
                gtk_widget_set_sensitive(delete_button, FALSE);
                gtk_widget_set_sensitive(modify_button, FALSE);
                gtk_widget_set_sensitive(reset_button, TRUE);
                gtk_widget_set_sensitive(start_button, TRUE);
                gtk_widget_set_sensitive(details_button, TRUE);
            }
            else
            {
                gtk_widget_set_sensitive(delete_button, TRUE);
                gtk_widget_set_sensitive(modify_button, TRUE);
                gtk_widget_set_sensitive(reset_button, TRUE);
                gtk_widget_set_sensitive(start_button, TRUE);
                gtk_widget_set_sensitive(details_button, TRUE);

            }
        }
    }
}

void reset_clicked_cb(GtkWidget *widget, gpointer selection)
{
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkTreeIter  iter;
    char *target_name;
    char *cmd = NULL;
    char *virtual_target_path;
    char *info_file;
    char *disk_path;
    int file_status;
    char* basedisk_path = NULL;
    store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(treeview));

    if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) 
        return;

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
                &model, &iter)) {
        //get target name
        gtk_tree_model_get(model, &iter, TARGET_NAME, &target_name, -1);

        if(check_shdmem(target_name, RESET_MODE)== -1)
            return;

        gboolean bResult = show_ok_cancel_message("Warning", "This target will be reset, Are you sure to continue?");
        if(bResult == FALSE)
            return;
        virtual_target_path = get_virtual_target_path(target_name);
        info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
        file_status = is_exist_file(info_file);
        if(file_status == -1 || file_status == FILE_NOT_EXISTS)
        {
            ERR( "target info file not exists : %s\n", target_name);
            return;
        }

        basedisk_path = get_config_value(info_file, HARDWARE_GROUP, BASEDISK_PATH_KEY);
        file_status = is_exist_file(basedisk_path);
        if(file_status == -1 || file_status == FILE_NOT_EXISTS)
        {
            ERR( "Base image file not exists : %s\n", basedisk_path);
            return;
        }
        disk_path = get_config_value(info_file, HARDWARE_GROUP, DISK_PATH_KEY);

        // reset emulator image
#ifdef _WIN32
        cmd = g_strdup_printf("\"%s/bin/qemu-img.exe\" create -b \"%s\" -f qcow2 \"%s\"", 
                get_root_path(), basedisk_path, disk_path);
        if(WinExec(cmd, SW_HIDE) < 31)
#else
        cmd = g_strdup_printf("./qemu-img create -b %s -f qcow2 %s", 
                basedisk_path, disk_path);
        if(!run_cmd(cmd))
#endif
        {
            g_free(cmd);
            free(basedisk_path);
            free(disk_path);
            show_message("Error", "emulator image reset failed!");
            return;
        }
    
        g_free(cmd);
        g_free(target_name);
        free(basedisk_path);
        free(disk_path);
        show_message("INFO","Success reset virtual target!");
        return;
    }

    show_message("Warning", "Target is not selected. Firstly select a target and reset.");
}

void details_clicked_cb(GtkWidget *widget, gpointer selection)
{
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkTreeIter  iter;
    char *target_name;
    char *virtual_target_path;
    char *info_file;
    int info_file_status;
    char *resolution = NULL;
    char *sdcard_type = NULL;
    char *sdcard_path = NULL;
    char *ram_size = NULL;
    char *dpi = NULL;
    char *disk_path = NULL;
    char *basedisk_path = NULL;
    char *arch = NULL;
    char *sdcard_detail = NULL;
    char *ram_size_detail = NULL;
    char *sdcard_path_detail = NULL;
    char *details = NULL;

    arch = getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return ;
    }

    store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

    if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) 
        return;

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
                &model, &iter)) {
        //get target name
        gtk_tree_model_get(model, &iter, TARGET_NAME, &target_name, -1);

        virtual_target_path = get_virtual_target_path(target_name);
        info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
        info_file_status = is_exist_file(info_file);

        //if targetlist exist but config file not exists
        if(info_file_status == -1 || info_file_status == FILE_NOT_EXISTS)
        {
            ERR( "target info file not exists : %s\n", target_name);
            return;
        }
        //get info from config.ini
        resolution= get_config_value(info_file, HARDWARE_GROUP, RESOLUTION_KEY);
        sdcard_type= get_config_value(info_file, HARDWARE_GROUP, SDCARD_TYPE_KEY);
        sdcard_path= get_config_value(info_file, HARDWARE_GROUP, SDCARD_PATH_KEY);
        ram_size = get_config_value(info_file, HARDWARE_GROUP, RAM_SIZE_KEY);
        dpi = get_config_value(info_file, HARDWARE_GROUP, DPI_KEY);
        disk_path = get_config_value(info_file, HARDWARE_GROUP, DISK_PATH_KEY);
        basedisk_path = get_config_value(info_file, HARDWARE_GROUP, BASEDISK_PATH_KEY);

        if(strcmp(sdcard_type, "0") == 0)
        {
            sdcard_detail = g_strdup_printf("Not supported");
            sdcard_path_detail = g_strdup_printf("None");
        }
        else
        {
            sdcard_detail = g_strdup_printf("Supported");
            sdcard_path_detail = g_strdup_printf("%s", sdcard_path); 
        }

        ram_size_detail = g_strdup_printf("%sMB", ram_size); 

        if(access(disk_path, R_OK) != 0){
            details = g_strdup_printf("The image does not exist. \n\n"
                    "    - [%s]", disk_path);
            show_message("Error", details);
            g_free(details);
        }
        if(access(basedisk_path, R_OK) != 0){
            details = g_strdup_printf("The base image does not exist. \n\n"
                    "    - [%s]", basedisk_path);
            show_message("Error", details);
            g_free(details);
        }

#ifndef _WIN32      
        /* check image & base image */

        details = g_strdup_printf(""
                " - Name: %s\n"
                " - CPU: %s\n"
                " - Resolution: %s\n"
                " - RAM Size: %s\n"
                " - SD Card: %s\n"
                " - SD Path: %s\n"
                " - Image Path: %s\n"
                " - Base Image Path: %s \n"
                , target_name, arch, resolution, ram_size_detail
                , sdcard_detail, sdcard_path_detail, disk_path, basedisk_path);

        show_sized_message("Virtual Target Details", details, DIALOG_MAX_WIDTH);

#else /* _WIN32 */
        /* todo: check image & base image */
        gchar *details_win = NULL;

        details = g_strdup_printf(""
                " - Name: %s\n"
                " - CPU: %s\n"
                " - Resolution: %s\n"
                " - RAM Size: %s\n"
                " - SD Card: %s\n"
                " - SD Path: %s\n"
                " - Image Path: %s\n"
                " - Base Image Path: %s \n"
                , target_name, arch, resolution, ram_size_detail
                , sdcard_detail, sdcard_path_detail, disk_path, basedisk_path);

        details_win = change_path_from_slash(details);

        show_sized_message("Virtual Target Details", details_win, DIALOG_MAX_WIDTH);

        free(details_win);
#endif
        g_free(resolution);
        g_free(sdcard_type);
        g_free(sdcard_path);
        g_free(ram_size);
        g_free(dpi);
        g_free(disk_path);
        g_free(sdcard_detail);
        g_free(ram_size_detail);
        g_free(sdcard_path_detail);
        g_free(details);
        return;
    }

    show_message("Warning", "Target is not selected. Firstly select a target and press the button.");
}

int delete_group(char* target_list_filepath, char* target_name, int type)
{
    GKeyFile *keyfile;
    GError *error = NULL;
    gchar **target_list = NULL;
    int list_num;
    int i;
    char *virtual_target_path = NULL;
    char *group_baseimage_path = NULL;
    char *arch = getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return -1;
    }

    keyfile = g_key_file_new();
    if (!g_key_file_load_from_file(keyfile, target_list_filepath, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        ERR( "loading key file form %s is failed.\n", target_list_filepath);
        return -1;
    }

    if(g_key_file_has_group(keyfile, target_name) == FALSE)
        return 1;
    else // target_name is a group name
    {
        target_list = get_virtual_target_list(target_list_filepath, target_name, &list_num);
    
        gboolean bResult = show_ok_cancel_message("Warning", "All targets under this group will be removed, Are you sure to continue?");
        if(bResult == FALSE)
            return -1;
        
        if(!target_list)
            goto DEL_GROUP;

        for(i = 0; i < list_num; i++)
        {
            if(check_shdmem(target_list[i], DELETE_MODE) == -1)
                return -1;
        }

        for(i = 0; i < list_num; i++)
        {
            virtual_target_path = get_virtual_target_path(target_list[i]);
            if((strcmp(target_name, g_major_version) != 0) && 
                    strcmp(target_list[i], DEFAULT) != 0)
            {
                if(remove_dir(virtual_target_path) == -1)
                {
                    char *message = g_strdup_printf("Failed to delete target name: %s", target_list[i]); 
                    show_message("Error", message);
                    free(message);
                    return -1;
                }
                
                del_config_key(target_list_filepath, target_name, target_list[i]);
            }
            else
                show_message("INFO", "Can not delete the latest default target of the latest group.\n"
                        "The others are deleting");

            free(virtual_target_path);
            g_strfreev(target_list);
        }

DEL_GROUP:  
        //do not delete base image of g_major_version
        if(strcmp(target_name, g_major_version) != 0)
        {
            INFO( "delete group name : %s\n", target_name);
            del_config_group(target_list_filepath, target_name);
        
            INFO( "delete group base image : %s\n", target_name);
            group_baseimage_path = g_strdup_printf("%s/emulimg-%s.%s", get_arch_path(), target_name, arch);
            
            if(g_remove(group_baseimage_path) == -1)
                INFO( "fail deleting %s\n", group_baseimage_path);
            else
                INFO( "success deleting %s\n", group_baseimage_path);
        }
        else
        {
            show_message("INFO", "Can not delete the latest version.");
            refresh_clicked_cb();
            free(group_baseimage_path);
            g_key_file_free(keyfile);
            return 0;
        }
        
        refresh_clicked_cb();
        free(group_baseimage_path);
        g_key_file_free(keyfile);
    }

    show_message("INFO","Success deleting group!");
    return 0;
}

void delete_clicked_cb(GtkWidget *widget, gpointer selection)
{
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkTreeIter  iter;
    char *target_name;
    char *group_name;
    char *cmd = NULL;
    char *virtual_target_path;
    int target_list_status;

    g_target_list_filepath = get_targetlist_filepath();
    target_list_status = is_exist_file(g_target_list_filepath);
    if(target_list_status == -1 || target_list_status == FILE_NOT_EXISTS)
    {
        INFO( "target info file not exists : %s\n", g_target_list_filepath);
        return;
    }
    
    store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

    if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) 
        return;

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
                &model, &iter)) {
        //get target name
        gtk_tree_model_get(model, &iter, TARGET_NAME, &target_name, -1);

        //check if selection is group name or target name   
        if(delete_group(g_target_list_filepath, target_name, DELETE_GROUP_MODE) <= 0)
            return;

        if(check_shdmem(target_name, DELETE_MODE) == -1)
            return;

        gboolean bResult = show_ok_cancel_message("Warning", "This target will be removed, Are you sure to continue?");
        if(bResult == FALSE)
            return;

        virtual_target_path = get_virtual_target_path(target_name);

#ifdef _WIN32
        char *virtual_target_win_path = change_path_from_slash(virtual_target_path);
        cmd = g_strdup_printf("rmdir /Q /S \"%s\"", virtual_target_win_path);
        if(system(cmd) == -1)
#else
        cmd = g_strdup_printf("rm -rf %s", virtual_target_path);
#endif
        if(!run_cmd(cmd))
        {
            g_free(cmd);
            g_free(virtual_target_path);
            TRACE( "Failed to delete target name: %s", target_name);
            show_message("Failed to delete target name: %s", target_name);
            return;
        }
        //find group of target_name and delete the target_name
        group_name = get_group_name(g_target_list_filepath, target_name);
        if(!group_name)
        {
            ERR( "%s is not under any group in targetlist.ini\n", target_name);
            return;
        }
        
        del_config_key(g_target_list_filepath, group_name, target_name);

        g_free(group_name);
        g_free(cmd);
        g_free(virtual_target_path);
#ifdef _WIN32
        g_free(virtual_target_win_path);
#endif
        gtk_tree_store_remove(store, &iter);
        g_free(target_name);
        show_message("INFO","Success deleting virtual target!");
        return;
    }
    show_message("Warning", "Target is not selected. Firstly select a target and delete.");
}

void refresh_clicked_cb(void)
{
    GtkTreeStore *store;
    GtkTreeIter iter, child;
    int i;
    int list_num = 0;
    int group_num = 0;
    gchar **target_groups = NULL;
    gchar **target_list = NULL;
    char *virtual_target_path;
    char *info_file;
    char *resolution = NULL;
    int minor_version = 0;
    char *buf;
    char *vms_path = NULL;
    gchar *ram_size = NULL;
    int info_file_status;
    gchar *local_target_list_filepath;
    GtkTreePath *first_col_path = NULL;

    store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    local_target_list_filepath = get_targetlist_filepath();

    //check tizen_vms path
    vms_path = (char*)get_tizen_vms_arch_path();
    if (g_file_test(vms_path, G_FILE_TEST_EXISTS) == FALSE) {
        char *message = g_strdup_printf("tizen_vms directory does not exist."
                " Check if EMULATOR_IMAGE installed.\n\n"
                "   - [%s]", vms_path);
        show_message("Error", message);
        free(message);
        free(vms_path);
        exit(0);
    }
    else
        free(vms_path);
    
    target_groups = get_virtual_target_groups(local_target_list_filepath, &group_num);
    gtk_tree_store_clear(store);
    group_num -= 1;
    //search group name order in reverse.
    for( ; group_num >= 0; group_num--)
    {
        target_list = get_virtual_target_list(local_target_list_filepath, target_groups[group_num], &list_num);
        if(!target_list)
        {
            if(strcmp(target_groups[group_num], CUSTOM_GROUP) != 0)
            {
                INFO( "delete group name : %s\n", target_groups[group_num]);
                del_config_group(local_target_list_filepath, target_groups[group_num]); 
            }
            continue ; 
        }
        gtk_tree_store_append(store, &iter, NULL);
        gtk_tree_store_set(store, &iter, 
                TARGET_NAME, target_groups[group_num], RESOLUTION, "", -1);

        for(i = 0; i < list_num; i++)
        {
            gtk_tree_store_append(store, &child, &iter);
        
            virtual_target_path = get_virtual_target_path(target_list[i]);
            info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
            info_file_status = is_exist_file(info_file);
            //if targetlist exist but config file not exists
            if(info_file_status == -1 || info_file_status == FILE_NOT_EXISTS)
            {
                INFO( "target info file not exists : %s\n", target_list[i]);
                del_config_key(local_target_list_filepath, target_groups[group_num], target_list[i]);
                target_list = get_virtual_target_list(local_target_list_filepath, target_groups[group_num], &list_num);
                gtk_tree_store_remove(store, &child);
                i -= 1;
                continue;
            }

            buf = get_config_value(info_file, HARDWARE_GROUP, RAM_SIZE_KEY);
            ram_size = g_strdup_printf("%sMB", buf); 
            resolution = get_config_value(info_file, HARDWARE_GROUP, RESOLUTION_KEY);
            minor_version = get_config_type(info_file, COMMON_GROUP, MINOR_VERSION_KEY);
            gtk_tree_store_set(store, &child, TARGET_NAME, target_list[i], RESOLUTION, resolution, RAM_SIZE, ram_size, -1);

            g_free(buf);

            g_free(ram_size);
            g_free(resolution);
            g_free(virtual_target_path);
            g_free(info_file);
        }
    }
    first_col_path = gtk_tree_path_new_from_indices(0, -1);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), first_col_path, NULL, 0);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));

    g_free(local_target_list_filepath);
    g_strfreev(target_list);
    g_strfreev(target_groups);

}

int remove_dir(char *path)
{   
    char *cmd = NULL;
#ifdef _WIN32
    char *win_path = change_path_from_slash(path);
    cmd = g_strdup_printf("rmdir /Q /S \"%s\"", win_path);  
    if(system(cmd) == -1)
#else
    cmd = g_strdup_printf("rm -rf %s", path);
    if(!run_cmd(cmd))
#endif
    {
        free(cmd);
        TRACE( "Failed to delete directory: %s", path);
        return -1;
    }
    return 0;
}

void make_tizen_vms(void)
{
    FILE *fp;
    int file_status;
    char *target_list_filepath;
    char *tizen_vms_path = (char*)get_tizen_vms_arch_path();
    char *screenshots_path = (char*)get_screenshots_path();
    char *log_path = (char*)get_virtual_target_log_path(DEFAULT);

    if(access(tizen_vms_path, R_OK) != 0){
        INFO("tizen_vms dir not exist. is making now : %s\n", tizen_vms_path);
        g_mkdir_with_parents(tizen_vms_path, 0755);
    }
    
    if(access(screenshots_path, R_OK) != 0){
        INFO("screenshots dir not exist. is making now : %s\n", screenshots_path);
        g_mkdir(screenshots_path, 0755);
    }

    if(access(log_path, R_OK) != 0){
        INFO("screenshots dir not exist. is making now : %s\n", log_path);
        g_mkdir(log_path, 0755);
    }

    target_list_filepath = get_targetlist_filepath();
    file_status = is_exist_file(target_list_filepath);
    if(file_status == -1 || file_status == FILE_NOT_EXISTS)
    {
        INFO("target info file not exists. is makeing now : %s\n", target_list_filepath);
        fp = g_fopen(target_list_filepath, "w+");
        if(fp != NULL) {
            g_fprintf(fp, "[%s]", CUSTOM_GROUP);
            fclose(fp); 
        }   
    }

    free(tizen_vms_path);
    free(screenshots_path);
    free(log_path);
    free(target_list_filepath);
}

void make_default_image(char *default_targetname)
{
    char *cmd = NULL;
    int file_status;
    char *default_img;
    char *arch;
    char *default_dir;
    char *default_path; 
    char *log_dir;
    char *conf_path;
    char *targetlist;
    char *base_img_path;
    char *target_list_filepath;
    char *info_file;
    char *virtual_target_path;
    GError *err;
    GFile *file_conf_path;
    GFile *file_default_path;
    int i,j;

#ifdef __arm__
    j = 2;
#else
    j = 1;
#endif

    for(i=0; i < j; i++)
    {
        if(i == 0)
            g_setenv(EMULATOR_ARCH, X86, 1);
        else
            g_setenv(EMULATOR_ARCH, ARM, 1);
    
        virtual_target_path = get_virtual_target_path(default_targetname);
        info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
        file_status = is_exist_file(info_file);
        if(file_status == FILE_EXISTS)
        {
            if(get_config_type(info_file, COMMON_GROUP, MINOR_VERSION_KEY) < g_minor_version)
            {
                if(remove_dir(virtual_target_path) == -1)
                {
                    show_message("Error", "Failed to delete default target!");
                    free(virtual_target_path);
                    free(info_file);
                    break ;
                }
            }
                
        }
        
        arch = (char*)g_getenv(EMULATOR_ARCH);
        target_list_filepath = get_targetlist_filepath();
        file_status = is_exist_file(target_list_filepath);
        if(file_status == -1 || file_status == FILE_NOT_EXISTS)
        {
            ERR( "load target list file error\n");
            show_message("Error", "load target list file error!");
            free(virtual_target_path);
            free(info_file);
            break;
        }
        
        default_img = g_strdup_printf("%semulimg-default.%s", virtual_target_path, arch);
        default_dir = g_strdup_printf("%s/%s", get_tizen_vms_arch_path(), default_targetname);
        default_path = g_strdup_printf("%s/config.ini", default_dir);
        log_dir = get_virtual_target_log_path(default_targetname);
        conf_path = g_strdup_printf("%s/config.ini", get_conf_path());
        file_status = is_exist_file(conf_path);
        if(file_status == -1 || file_status == FILE_NOT_EXISTS)
        {
            char *message = g_strdup_printf("File does not exist.\n\n"
                    "   -[%s]", conf_path);
            show_message("Error", message);
            free(message);
            break;
        }
        targetlist = get_targetlist_filepath();
        file_conf_path = g_file_new_for_path(conf_path);
        file_default_path = g_file_new_for_path(default_path);
        //make default image if it does not exist.
        file_status = is_exist_file(default_img);
        if(file_status == -1 || file_status == FILE_NOT_EXISTS)
        {
            file_status = 0;
            INFO( "%s default image does not exists. is making now.\n", arch);
            if(access(default_dir, R_OK) != 0)
                g_mkdir(default_dir, 0755);
            if(access(log_dir, R_OK) != 0)
                g_mkdir(log_dir, 0755);
            //copy config.ini
            if(!g_file_copy(file_conf_path, file_default_path, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err)){
                ERR("fail to copy config.ini: %s\n", err->message);
                g_error_free(err);
                g_free(conf_path);
                break;
            }
            //find base image   
            base_img_path = g_strdup_printf("%s/emulimg-%s.%s", get_arch_path(), g_major_version, arch);
            file_status = is_exist_file(base_img_path);
            if(file_status == -1 || file_status == FILE_NOT_EXISTS)
            {
                ERR( "file not exist: %s", base_img_path);
                char *message = g_strdup_printf("File does not exist.\n\n"
                        "   -[%s]", base_img_path);
                show_message("Error", message);
                free(message);
                free(base_img_path);
                break;
            }
        // create emulator image
#ifdef _WIN32
            cmd = g_strdup_printf("\"%s/qemu-img.exe\" create -b \"%s\" -f qcow2 \"%s\"",
                    get_bin_path(), base_img_path, default_img);
            if(WinExec(cmd, SW_HIDE) < 31)
#else
            cmd = g_strdup_printf("./qemu-img create -b %s -f qcow2 %s",
                    base_img_path, default_img);
            if(!run_cmd(cmd))
#endif
            {
                free(cmd);
                ERR("default %s image creation failed!\n", arch);
                show_message("Error", "default image creation failed!");
                free(default_dir);
                free(default_path);
                free(log_dir);
                free(conf_path);
                free(targetlist);
                free(base_img_path);
                break;
            }

            INFO( "%s default image creation succeeded!\n", arch);
            // check main(latest) version
            version_init(default_targetname, target_list_filepath);
    
            free(cmd);
            free(default_dir);
            free(default_path);
            free(log_dir);
            free(conf_path);
            free(targetlist);
            free(base_img_path);
    
            set_config_value(info_file, HARDWARE_GROUP, BASEDISK_PATH_KEY, get_baseimg_path());
            set_config_value(info_file, HARDWARE_GROUP, DISK_PATH_KEY, default_img);
            set_config_value(info_file, COMMON_GROUP, MAJOR_VERSION_KEY, g_major_version);
            set_config_type(info_file, COMMON_GROUP, MINOR_VERSION_KEY, g_minor_version);
            
            free(virtual_target_path);
            free(info_file);
        }
        free(default_img);
    }

    g_setenv(EMULATOR_ARCH, X86, 1);
}

gboolean run_cmd(char *cmd)
{
    char *s_out = NULL;
    char *s_err = NULL;
    int exit_status;
    GError *err = NULL;

    g_return_val_if_fail(cmd != NULL, FALSE);
    INFO( "Command: %s\n", cmd);
    if (!g_spawn_command_line_sync(cmd, &s_out, &s_err, &exit_status, &err)) {
        ERR( "Failed to invoke command: %s\n", err->message);
        show_message("Failed to invoke command", err->message);
        g_error_free(err);
        g_free(s_out);
        g_free(s_err);
        return FALSE;
    }
    if (exit_status != 0) {
        ERR( "Command returns error: %s\n", s_out);
        //      show_message("Command returns error", s_out);
        g_free(s_out);
        g_free(s_err);
        return FALSE;
    }

    INFO( "Command success: %s\n", cmd);
    //  show_message("Command success!", s_out);
    g_free(s_out);
    g_free(s_err);
    return TRUE;
}

void fill_virtual_target_info(void)
{
    snprintf(virtual_target_info.resolution, MAXBUF, "480x800");
    virtual_target_info.sdcard_type = 0;
    sdcard_create_size = 512;
    virtual_target_info.ram_size = 512;
    snprintf(virtual_target_info.dpi, MAXBUF, "2070");
}

int create_config_file(gchar* filepath)
{
    char *arch = getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return -1;
    }
    FILE *fp = g_fopen(filepath, "w+");

    if (fp != NULL) {
        gint x, y;
        GtkWidget *win = get_window(VTM_CREATE_ID);
        gtk_window_get_position(GTK_WINDOW(win), &x, &y);

        g_fprintf (fp, "[%s]\n", COMMON_GROUP);
        g_fprintf (fp, "%s=0\n", ALWAYS_ON_TOP_KEY);
        g_fprintf (fp, "%s=\n", MAJOR_VERSION_KEY);
        g_fprintf (fp, "%s=\n", MINOR_VERSION_KEY);
        
        g_fprintf (fp, "\n[%s]\n", EMULATOR_GROUP);
        g_fprintf (fp, "%s=%d\n", MAIN_X_KEY, x);
        g_fprintf (fp, "%s=%d\n", MAIN_Y_KEY, y);
        g_fprintf (fp, "%s=50\n", SCALE_KEY);
        g_fprintf (fp, "%s=0\n", ENABLE_SHELL_KEY);

        g_fprintf (fp, "\n[%s]\n", QEMU_GROUP);
        g_fprintf (fp, "%s=\n", BINARY_KEY);
        g_fprintf (fp, "%s=1\n", HTTP_PROXY_KEY);
        g_fprintf (fp, "%s=1\n", DNS_SERVER_KEY);
        g_fprintf (fp, "%s=1200\n", TELNET_PORT_KEY);
        //      g_fprintf (fp, "%s=\n", SNAPSHOT_SAVED_KEY);
        //      g_fprintf (fp, "%s=\n", SNAPSHOT_SAVED_DATE_KEY);
        g_fprintf (fp, "%s=1\n", KVM_KEY);

        g_fprintf (fp, "\n[%s]\n", ADDITIONAL_OPTION_GROUP);
        g_fprintf (fp, "%s=\n", EMULATOR_OPTION_KEY);
        if(strcmp(arch, X86) == 0)
            g_fprintf (fp, "%s=%s\n", QEMU_OPTION_KEY,"-M tizen-x86-machine -usb -usbdevice maru-touchscreen -usbdevice keyboard -net user -net nic,model=virtio -rtc base=utc");
        else if(strcmp(arch, ARM) == 0)
            g_fprintf (fp, "%s=%s\n", QEMU_OPTION_KEY," -M s5pc110 -net user -net nic,model=s5pc1xx-usb-otg -usbdevice keyboard -rtc base=utc -redir tcp:1202:10.0.2.16:22");
        g_fprintf (fp, "[%s]\n", HARDWARE_GROUP);
        g_fprintf (fp, "%s=\n", RESOLUTION_KEY);
        g_fprintf (fp, "%s=1\n", BUTTON_TYPE_KEY);
        g_fprintf (fp, "%s=\n", SDCARD_TYPE_KEY);
        g_fprintf (fp, "%s=\n", SDCARD_PATH_KEY);
        g_fprintf (fp, "%s=\n", RAM_SIZE_KEY);
        g_fprintf (fp, "%s=\n", DPI_KEY);
        g_fprintf (fp, "%s=0\n", DISK_TYPE_KEY);
        g_fprintf (fp, "%s=\n", BASEDISK_PATH_KEY);
        g_fprintf (fp, "%s=\n", DISK_PATH_KEY);

        fclose(fp);
    }
    else {
        ERR( "Can't open file path. (%s)\n", filepath);
        return -1;
    }

    g_chmod(filepath, 0666);

    return 0;
}

int write_config_file(gchar *filepath)
{
    /*  QEMU option (09.05.26)*/

    char *arch = (char*)g_getenv(EMULATOR_ARCH);
    if(strcmp(arch, X86) == 0)
        set_config_value(filepath, QEMU_GROUP, BINARY_KEY, EMULATOR_X86);
    else if(strcmp(arch, ARM) == 0)
        set_config_value(filepath, QEMU_GROUP, BINARY_KEY, EMULATOR_ARM);
    else
    {
        show_message("Error", "Architecture setting failed.\n");
        ERR( "architecture setting error");
        return -1;
    }
    //  set_config_type(filepath, QEMU_GROUP, TELNET_CONSOLE_COMMAND_TYPE_KEY, 0);
    //  set_config_value(filepath, QEMU_GROUP, TELNET_CONSOLE_COMMAND_KEY, "/usr/bin/putty -telnet -P 1200 localhost");
    //  set_config_type(filepath, QEMU_GROUP, KVM_KEY, 1);
    //  set_config_type(filepath, QEMU_GROUP, SDCARD_TYPE_KEY, pconfiguration->qemu_configuration.sdcard_type);
    //  set_config_value(filepath, QEMU_GROUP, SDCARD_PATH_KEY, pconfiguration->qemu_configuration.sdcard_path);
    //  set_config_type(filepath, QEMU_GROUP, SAVEVM_KEY, pconfiguration->qemu_configuration.save_emulator_state);
    //  set_config_type(filepath, QEMU_GROUP, SNAPSHOT_SAVED_KEY, pconfiguration->qemu_configuration.snapshot_saved);
    //  set_config_value(filepath, QEMU_GROUP, SNAPSHOT_SAVED_DATE_KEY, pconfiguration->qemu_configuration.snapshot_saved_date);


    set_config_value(filepath, COMMON_GROUP, MAJOR_VERSION_KEY, virtual_target_info.major_version);
    set_config_type(filepath, COMMON_GROUP, MINOR_VERSION_KEY, virtual_target_info.minor_version);
    set_config_value(filepath, HARDWARE_GROUP, RESOLUTION_KEY, virtual_target_info.resolution);
    set_config_type(filepath, HARDWARE_GROUP, SDCARD_TYPE_KEY, virtual_target_info.sdcard_type);
    set_config_value(filepath, HARDWARE_GROUP, SDCARD_PATH_KEY, virtual_target_info.sdcard_path);
    set_config_type(filepath, HARDWARE_GROUP, RAM_SIZE_KEY, virtual_target_info.ram_size);
    set_config_type(filepath, HARDWARE_GROUP, DISK_TYPE_KEY, virtual_target_info.disk_type);
    set_config_value(filepath, HARDWARE_GROUP, DPI_KEY, virtual_target_info.dpi);
    //  set_config_type(filepath, HARDWARE_GROUP, BUTTON_TYPE_KEY, virtual_target_info.button_type);
    set_config_value(filepath, HARDWARE_GROUP, DISK_PATH_KEY, virtual_target_info.diskimg_path);
    set_config_value(filepath, HARDWARE_GROUP, BASEDISK_PATH_KEY, virtual_target_info.basedisk_path);

    return 0;
}

int name_collision_check(void)
{
    int i,j;
    int list_num = 0;
    int group_num = 0;
    gchar **target_list = NULL;
    gchar **target_groups = NULL;
    g_target_list_filepath = get_targetlist_filepath();
    
    target_groups = get_virtual_target_groups(g_target_list_filepath, &group_num);
    for(i = 0; i < group_num; i++)
    {
        target_list = get_virtual_target_list(g_target_list_filepath, target_groups[i], &list_num);
        for(j = 0; j < list_num; j++)
        {
            if(strcmp(target_list[j], virtual_target_info.virtual_target_name) == 0)
            return 1;
        }
    }
    g_strfreev(target_list);    
    g_strfreev(target_groups);  
    return 0;
}

void exit_vtm(void)
{
    INFO( "virtual target manager exit \n");
    window_hash_destroy();
    g_object_unref(G_OBJECT(g_builder));
#ifndef _WIN32
    flock(g_fd, LOCK_UN);
    close(g_fd);
#else
    CloseHandle(g_hFile);
#endif
    g_remove(g_userfile);
    gtk_main_quit();

}

GtkWidget *setup_tree_view(void)
{
    GtkWidget *sc_win;
    GtkTreeStore *store;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    sc_win = gtk_scrolled_window_new(NULL, NULL);
    store = gtk_tree_store_new(N_COL, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();

    //set text alignment 
    column = gtk_tree_view_column_new_with_attributes("Target Name", cell, "text", TARGET_NAME, NULL);
    gtk_tree_view_column_set_alignment(column,0.0);
    gtk_tree_view_column_set_min_width(column,130);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    column = gtk_tree_view_column_new_with_attributes("RAM Size", cell, "text", RAM_SIZE, NULL);
    gtk_tree_view_column_set_alignment(column,0.0);
    gtk_tree_view_column_set_min_width(column,100);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    column = gtk_tree_view_column_new_with_attributes("Resolution", cell, "text", RESOLUTION, NULL);
    gtk_tree_view_column_set_alignment(column,0.0);
    gtk_tree_view_column_set_max_width(column,60);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    //annotate for future use   
//  column = gtk_tree_view_column_new_with_attributes("Minor Version", cell, "text", MINOR, NULL);
//  gtk_tree_view_column_set_alignment(column,0.0);
//  gtk_tree_view_column_set_max_width(column,60);
//  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sc_win), treeview);
    g_object_unref( G_OBJECT(store));

    return sc_win;
}

void create_window_deleted_cb(void)
{
    GtkWidget *win = NULL;
    INFO( "create window exit \n");

    win = get_window(VTM_CREATE_ID);

    gtk_widget_destroy(win);

    gtk_main_quit();
}

void resolution_select_cb(void)
{
    char *resolution;
    
    GtkComboBox *resolution_combobox = 
        (GtkComboBox *)get_widget(VTM_CREATE_ID, VTM_CREATE_RESOLUTION_COMBOBOX);

    resolution = escape_resolution_str(gtk_combo_box_get_active_text(resolution_combobox));
    snprintf(virtual_target_info.resolution, MAXBUF, "%s", resolution);
    INFO( "resolution size : %s\n", resolution);
    g_free(resolution);

}

void buttontype_select_cb(void)
{
    gboolean active = FALSE;

    GtkWidget *create_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton10");
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(create_radiobutton));
    if(active == TRUE)
        virtual_target_info.button_type = 1;
    else
        virtual_target_info.button_type = 3;

    INFO( "button_type : %d\n", virtual_target_info.button_type);
}

void sdcard_size_select_cb(void)
{
    char *size;

    GtkComboBox *sdcard_combo_box = (GtkComboBox *)get_widget(VTM_CREATE_ID, VTM_CREATE_SDCARD_COMBOBOX);   

    size = gtk_combo_box_get_active_text(sdcard_combo_box);
    sdcard_create_size = atoi(size);
    INFO( "sdcard create size : %d\n", atoi(size));

    g_free(size);
}

void set_sdcard_create_active_cb(void)
{
    gboolean active = FALSE;

    GtkWidget *create_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton4");
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(create_radiobutton));

    if(active == TRUE)
        virtual_target_info.sdcard_type = 1;

    GtkWidget *sdcard_combo_box = (GtkWidget *)get_widget(VTM_CREATE_ID, VTM_CREATE_SDCARD_COMBOBOX);

    gtk_widget_set_sensitive(sdcard_combo_box, active);
}

void set_disk_select_active_cb(void)
{
    gboolean active = FALSE;

    GtkWidget *select_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton13");
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(select_radiobutton));

    if(active == TRUE)
        virtual_target_info.disk_type = 1;

    GtkWidget *sdcard_filechooser2 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton2");

    gtk_widget_set_sensitive(sdcard_filechooser2, active);

}

void set_sdcard_select_active_cb(void)
{
    gboolean active = FALSE;

    GtkWidget *select_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton5");
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(select_radiobutton));

    if(active == TRUE)
        virtual_target_info.sdcard_type = 2;

    GtkWidget *sdcard_filechooser = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton1");

    gtk_widget_set_sensitive(sdcard_filechooser, active);
}

void set_disk_default_active_cb(void)
{
    gboolean active = FALSE;

    GtkWidget *default_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton12");
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(default_radiobutton));

    if(active == TRUE)
    {
        virtual_target_info.disk_type = 0;
        snprintf(virtual_target_info.major_version, MAXBUF, "%s", g_major_version);
        virtual_target_info.minor_version = g_minor_version;
        snprintf(virtual_target_info.basedisk_path, MAXBUF, "%s", get_baseimg_path());
        INFO( "default disk path : %s\n", virtual_target_info.basedisk_path);
    }
    else
        virtual_target_info.disk_type = 1;
}

void set_default_image(char *target_name)
{
    gboolean active = FALSE;
    char *virtual_target_path;
    char *conf_path;

    GtkWidget *default_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton12");
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(default_radiobutton));

    if(active == TRUE)
    {
        virtual_target_info.disk_type = 0;
        virtual_target_path = get_virtual_target_path(target_name);
        conf_path = g_strdup_printf("%sconfig.ini", virtual_target_path);
        snprintf(virtual_target_info.major_version, MAXBUF, "%s", get_config_value(conf_path, COMMON_GROUP, MAJOR_VERSION_KEY));
        virtual_target_info.minor_version = get_config_type(conf_path, COMMON_GROUP, MINOR_VERSION_KEY);
        snprintf(virtual_target_info.basedisk_path, MAXBUF, "%s", get_baseimg_path());
        INFO( "default disk path : %s\n", virtual_target_info.basedisk_path);
        INFO( "major version : %s\n", virtual_target_info.major_version);
        INFO( "minor version : %d\n", virtual_target_info.minor_version);

        free(virtual_target_path);
        free(conf_path);
    }
    else
        virtual_target_info.disk_type = 1;
}

void set_sdcard_none_active_cb(void)
{
    gboolean active = FALSE;

    GtkWidget *none_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton6");
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(none_radiobutton));

    if(active == TRUE)
        virtual_target_info.sdcard_type = 0;
}

void disk_file_select_cb(void)
{
    gchar *path = NULL;

    GtkWidget *sdcard_filechooser2 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton2");

    path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(sdcard_filechooser2));
    
    snprintf(virtual_target_info.major_version, MAXBUF, CUSTOM_GROUP);
    virtual_target_info.minor_version = 0;
    INFO( "major version : %s, minor version: %d\n", virtual_target_info.major_version, virtual_target_info.minor_version);
#ifdef _WIN32
    snprintf(virtual_target_info.basedisk_path, MAXBUF, change_path_to_slash(path));
#else
    snprintf(virtual_target_info.basedisk_path, MAXBUF, "%s", path);
#endif
    INFO( "disk path : %s\n", path);

    g_free(path);

}

void sdcard_file_select_cb(void)
{
    gchar *path = NULL;

    GtkWidget *sdcard_filechooser = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton1");

    path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(sdcard_filechooser));
    snprintf(virtual_target_info.sdcard_path, MAXBUF, "%s", path);
    INFO( "sdcard path : %s\n", path);

    g_free(path);
}

void ram_select_cb(void)
{
    char *size;

    GtkComboBox *ram_combobox = (GtkComboBox *)get_widget(VTM_CREATE_ID, VTM_CREATE_RAM_COMBOBOX);

    size = gtk_combo_box_get_active_text(ram_combobox);
    virtual_target_info.ram_size = atoi(size);
    INFO( "ram size : %d\n", atoi(size));

    g_free(size);
}

int set_modify_variable(char *target_name)
{
    ram_select_cb();
    resolution_select_cb();
    buttontype_select_cb();

    if(virtual_target_info.disk_type == 0)
        set_default_image(target_name);
    else if(virtual_target_info.disk_type == 1)
        disk_file_select_cb();
    else
    {
        WARN( "disk type is wrong");
        show_message("Warning", "Disk type is wrong.");
        return -1;
    }
    return 0;
}

int check_modify_target_name(char *name)
{
    char *dst;
        
    //  name character validation check
    dst =  malloc(VT_NAME_MAXBUF);
    escapeStr(name, dst);
    if(strcmp(name, dst) != 0)
    {
        WARN( "virtual target name only allowed numbers, a-z, A-Z, -_");
        show_message("Warning", "Virtual target name is not correct! \n (only allowed numbers, a-z, A-Z, -_)");
        free(dst);
        return -1;
    }
    free(dst);

    // no name validation check
    if(strcmp(name, "") == 0)
    {
        WARN( "Specify name of the virtual target!");
        show_message("Warning", "Specify name of the virtual target!");
        return -1;
    }
    else
    {
        snprintf(virtual_target_info.virtual_target_name, MAXBUF, "%s", name);
    }
    return 0;

}

int change_modify_target_name(char *arch, char *dest_path, char *name, char* target_name)
{
    char *vms_path = NULL;

    // if try to change the target name
    if(strcmp(name, target_name) != 0)
    {
        vms_path = (char*)get_tizen_vms_arch_path();
        if(name_collision_check() == 1)
        {
            WARN( "Virtual target with the same name exists! Choose another name.");
            show_message("Warning", "Virtual target with the same name exists! Choose another name.");
            return -1;
        }
        //start name changing procesure
#ifndef _WIN32
        char *cmd = NULL;
        char *cmd2 = NULL;
        cmd = g_strdup_printf("mv %s/%s %s/%s", 
                vms_path, target_name, vms_path, name);
        cmd2 = g_strdup_printf("mv %s/%s/emulimg-%s.%s %s/%s/emulimg-%s.%s", 
                vms_path, name, target_name, arch, vms_path, name, name, arch);

        if(!run_cmd(cmd))
        {
            ERR( "Fail to run %s\n", cmd);
            g_free(cmd);
            g_free(dest_path);
            show_message("Error", "Fail to change the target name!");
            return -1;
        }
        g_free(cmd);

        if(!run_cmd(cmd2))
        {
            ERR( "Fail to run %s\n", cmd2);
            g_free(cmd);
            g_free(cmd2);
            g_free(dest_path);
            show_message("Error", "Fail to change the target name!");
            return -1;
        }
        g_free(cmd2);

#else /* WIN32 */
        char *src_path = g_strdup_printf("%s/%s", vms_path, target_name);
        char *dst_path = g_strdup_printf("%s/%s", vms_path, name);
        char *src_img_path = g_strdup_printf("%s/%s/emulimg-%s.%s", vms_path, name, target_name, arch);
        char *dst_img_path = g_strdup_printf("%s/%s/emulimg-%s.%s", vms_path, name, name, arch);

        gchar *src_path_for_win = change_path_from_slash(src_path);
        gchar *dst_path_for_win = change_path_from_slash(dst_path);
        gchar *src_img_path_for_win = change_path_from_slash(src_img_path);
        gchar *dst_img_path_for_win = change_path_from_slash(dst_img_path);

        g_free(src_path);
        g_free(dst_path);
        g_free(src_img_path);
        g_free(dst_img_path);

        if (g_rename(src_path_for_win, dst_path_for_win) != 0)
        {
            g_free(src_path_for_win);
            g_free(dst_path_for_win);
            show_message("Error", "Fail to change the target name!");
            return -1;
        }
        g_free(src_path_for_win);
        g_free(dst_path_for_win);

        if (g_rename(src_img_path_for_win, dst_img_path_for_win) != 0)
        {
            g_free(src_img_path_for_win);
            g_free(dst_img_path_for_win);
            show_message("Error", "Fail to change the target name!");
            return -1;
        }
        g_free(src_img_path_for_win);
        g_free(dst_img_path_for_win);
#endif
        g_free(vms_path);
    } // end chage name precedure
    
    return 0;
}

int modify_sdcard(char *arch, char *dest_path)
{
    char *sdcard_name = NULL;
    int file_status;
    // 0 : None
    if(virtual_target_info.sdcard_type == 0)
    {
        memset(virtual_target_info.sdcard_path, 0x00, MAXBUF);
        INFO( "[sdcard_type:0]virtual_target_info.sdcard_path: %s\n", virtual_target_info.sdcard_path);
    }
    // 1 : Create New Image
    else if(virtual_target_info.sdcard_type == 1)
    {
        // sdcard create
        sdcard_size_select_cb();
#ifndef _WIN32
        char *cmd = NULL;
        cmd = g_strdup_printf("cp %s/sdcard_%d.img %s", get_data_path(), sdcard_create_size, dest_path);

        if(!run_cmd(cmd))
        {
            g_free(cmd);
            g_free(dest_path);
            show_message("Error", "SD Card img create failed!");
            return -1;
        }
        g_free(cmd);
#else
        char *src_sdcard_path = g_strdup_printf("%s/sdcard_%d.img", get_data_path(), sdcard_create_size);
        char *dst_sdcard_path = g_strdup_printf("%s/sdcard_%d.img", dest_path, sdcard_create_size);

        gchar *src_dos_path = change_path_from_slash(src_sdcard_path);
        gchar *dst_dos_path = change_path_from_slash(dst_sdcard_path);

        g_free(src_sdcard_path);
        g_free(dst_sdcard_path);

        if(!CopyFileA(src_dos_path, dst_dos_path, FALSE))
        {
            g_free(dest_path);
            g_free(src_dos_path);
            g_free(dst_dos_path);
            show_message("Error", "SD Card img create failed!");
            return -1;
        }
        g_free(src_dos_path);
        g_free(dst_dos_path);

#endif
        snprintf(virtual_target_info.sdcard_path, MAXBUF, "%ssdcard_%d.img", dest_path, sdcard_create_size);
        INFO( "[sdcard_type:1]virtual_target_info.sdcard_path: %s\n", virtual_target_info.sdcard_path);
    }
    // 2 : Select From Existing Image
    else if(virtual_target_info.sdcard_type == 2){
        GtkWidget *sdcard_filechooser = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton1");
        char *sdcard_uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(sdcard_filechooser));
        if(sdcard_uri == NULL || strcmp(virtual_target_info.sdcard_path, "") == 0){
            show_message("Warning", "You didn't select an existing sdcard image!");
            return -1;
        }
        sdcard_file_select_cb();
        file_status = is_exist_file(virtual_target_info.sdcard_path);
        if(file_status == -1 || file_status == FILE_NOT_EXISTS)
        {
            // apply sdcard path that is changed by modifying target name
            sdcard_name = g_path_get_basename(virtual_target_info.sdcard_path);
            memset(virtual_target_info.sdcard_path, 0x00, MAXBUF);
            snprintf(virtual_target_info.sdcard_path, MAXBUF, "%s%s", dest_path, sdcard_name);
            INFO( "[sdcard_type:2]virtual_target_info.sdcard_path: %s\n", virtual_target_info.sdcard_path);
            free(sdcard_name);
        }
    }
    else
    {
        INFO( "virtual_target_info.sdcard_type: %d\n", virtual_target_info.sdcard_type);
        show_message("Warning", "SD card type is wrong!");
        return -1;
    }

    return 0;
}

void modify_ok_clicked_cb(GtkWidget *widget, gpointer data)
{
    GtkWidget *win = get_window(VTM_CREATE_ID);
    GtkWidget *name_entry;
    char *target_name = (char*)data;
    char *dest_path = NULL;
    char *conf_file = NULL;
    char *name = NULL;
    //find arch name
    char *arch = (char*)g_getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "architecture setting failed.");
        return;
    }
    
    if(set_modify_variable(target_name) == -1)
        return;
    
    name_entry = (GtkWidget *)gtk_builder_get_object(g_create_builder, "entry1");
    name = (char*)gtk_entry_get_text(GTK_ENTRY(name_entry));

    if(check_modify_target_name(name) == -1)
        return;
    
    dest_path = get_virtual_target_path(virtual_target_info.virtual_target_name);
    INFO( "virtual_target_path: %s\n", dest_path);

    //work when try to change target name
    if(change_modify_target_name(arch, dest_path, name, target_name) == -1)
        return;

    memset(virtual_target_info.diskimg_path, 0x00, MAXBUF);

    snprintf(virtual_target_info.diskimg_path, MAXBUF, 
            "%s/%s/emulimg-%s.%s", get_tizen_vms_arch_path(), name, name, arch);
    TRACE( "virtual_target_info.diskimg_path: %s\n",virtual_target_info.diskimg_path);
    
    if(modify_sdcard(arch, dest_path) == -1)
        return;
    
    //delete original target name
    g_target_list_filepath = get_targetlist_filepath();
    del_config_key(g_target_list_filepath, virtual_target_info.major_version, target_name);
    g_free(target_name);

    if(access(dest_path, R_OK) != 0)
        g_mkdir(dest_path, 0755);

    // add virtual target name to targetlist.ini
    set_config_value(g_target_list_filepath, virtual_target_info.major_version, 
            virtual_target_info.virtual_target_name, "");
    
    // write config.ini
    conf_file = g_strdup_printf("%sconfig.ini", dest_path);
    
    //  create_config_file(conf_file);
    snprintf(virtual_target_info.dpi, MAXBUF, "2070");
    if(write_config_file(conf_file) == -1)
    {
        show_message("Error", "Virtual target modification failed!");
        return;
    }

    show_message("INFO", "Success modifying virtual target!");

    gtk_widget_destroy(win);

    refresh_clicked_cb();

    g_object_unref(G_OBJECT(g_create_builder));

    gtk_main_quit();

    g_free(dest_path);
    g_free(conf_file);

    return;
}

int create_diskimg(char *arch, char *dest_path)
{
    int file_status;
    char *cmd = NULL;

    if(virtual_target_info.disk_type == 1){
        disk_file_select_cb();
        
        file_status = is_exist_file(virtual_target_info.basedisk_path);
        if(file_status == -1 || file_status == FILE_NOT_EXISTS)
        {
            ERR( "Base image does not exist : %s\n", virtual_target_info.basedisk_path);
            char *message = g_strdup_printf("Base image does not exist.\n\n"
                    "   -[%s]", virtual_target_info.basedisk_path);
            show_message("Error", message);
            free(message);
            return -1;
        }
#ifdef _WIN32
        cmd = g_strdup_printf("\"%s/bin/qemu-img.exe\" create -b \"%s\" -f qcow2 \"%semulimg-%s.%s\"", get_root_path(), virtual_target_info.basedisk_path,
                dest_path, virtual_target_info.virtual_target_name, arch);
#else
        cmd = g_strdup_printf("./qemu-img create -b %s -f qcow2 %semulimg-%s.%s", virtual_target_info.basedisk_path,
                dest_path, virtual_target_info.virtual_target_name, arch);
#endif
    }
    else if(virtual_target_info.disk_type == 0)
    {
        snprintf(virtual_target_info.basedisk_path, MAXBUF, "%s", get_baseimg_path());
#ifdef _WIN32
        cmd = g_strdup_printf("\"%s/bin/qemu-img.exe\" create -b \"%s\" -f qcow2 \"%semulimg-%s.%s\"", get_root_path(), virtual_target_info.basedisk_path,
                dest_path, virtual_target_info.virtual_target_name, arch);
#else
        cmd = g_strdup_printf("./qemu-img create -b %s -f qcow2 %semulimg-%s.%s", virtual_target_info.basedisk_path,
                dest_path, virtual_target_info.virtual_target_name, arch);
#endif
    }
    else
    {
        INFO("disk type : %d\n", virtual_target_info.disk_type);
        show_message("Error","disk type is wrong");
        return -1;
    }
#ifdef _WIN32
    if (WinExec(cmd, SW_HIDE) < 31)
#else
    if(!run_cmd(cmd))
#endif
    {
        g_free(cmd);
        g_free(dest_path);
        show_message("Error", "Emulator image create failed!");
        return -1;
    }
    g_free(cmd);

    // set diskimg_path
    snprintf(virtual_target_info.diskimg_path, MAXBUF, "%semulimg-%s.%s", dest_path, 
            virtual_target_info.virtual_target_name, arch);
    return 0;
}

int create_sdcard(char *dest_path)
{
    // sdcard
    if(virtual_target_info.sdcard_type == 0)
    {
        memset(virtual_target_info.sdcard_path, 0x00, MAXBUF);
    }
    else if(virtual_target_info.sdcard_type == 1)
    {
        // sdcard create
#ifndef _WIN32
        char *cmd = NULL;
        cmd = g_strdup_printf("cp %s/sdcard_%d.img %s", get_data_path(), sdcard_create_size, dest_path);

        if(!run_cmd(cmd))
        {
            g_free(cmd);
            g_free(dest_path);
            show_message("Error", "SD Card img create failed!");
            return -1;
        }
        g_free(cmd);
#else
        char *src_sdcard_path = g_strdup_printf("%s/sdcard_%d.img", get_data_path(), sdcard_create_size);
        char *dst_sdcard_path = g_strdup_printf("%s/sdcard_%d.img", dest_path, sdcard_create_size);

        gchar *src_dos_path = change_path_from_slash(src_sdcard_path);
        gchar *dst_dos_path = change_path_from_slash(dst_sdcard_path);

        g_free(src_sdcard_path);
        g_free(dst_sdcard_path);

        if(!CopyFileA(src_dos_path, dst_dos_path, FALSE))
        {
            g_free(dest_path);
            g_free(src_dos_path);
            g_free(dst_dos_path);
            show_message("Error", "SD Card img create failed!");
            return -1;
        }
        g_free(src_dos_path);
        g_free(dst_dos_path);

#endif
        snprintf(virtual_target_info.sdcard_path, MAXBUF, "%ssdcard_%d.img", dest_path, sdcard_create_size);
    }
    else if(virtual_target_info.sdcard_type == 2){
        if(strcmp(virtual_target_info.sdcard_path, "") == 0){
            show_message("Warning", "You didn't select an existing sdcard image!");
            return -1;
        }
    }
    return 0;
}   

void ok_clicked_cb(void)
{
    char *dest_path = NULL;
    char *log_path = NULL;
    char *conf_file = NULL;
    GtkWidget *win = get_window(VTM_CREATE_ID);
    char *arch = (char*)g_getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return ;
    }

    dest_path = get_virtual_target_path(virtual_target_info.virtual_target_name);
    if(access(dest_path, R_OK) != 0)
        g_mkdir(dest_path, 0755);
    
    log_path = get_virtual_target_log_path(virtual_target_info.virtual_target_name);
    if(access(log_path, R_OK) != 0)
        g_mkdir(log_path, 0755);
    
    if(create_sdcard(dest_path) == -1)
        return;

    if(create_diskimg(arch, dest_path) == -1)
        return;

    // add virtual target name to targetlist.ini
    set_config_value(g_target_list_filepath, virtual_target_info.major_version, virtual_target_info.virtual_target_name, "");
    // write config.ini
    conf_file = g_strdup_printf("%sconfig.ini", dest_path);
    create_config_file(conf_file);
    snprintf(virtual_target_info.dpi, MAXBUF, "2070");
    if(write_config_file(conf_file) == -1)
    {
        show_message("Error", "Virtual target creation failed!");
        return ;
    }
    show_message("INFO", "Success creating virtual target!");

    g_free(conf_file);
    g_free(dest_path);

    gtk_widget_destroy(win);
    refresh_clicked_cb();

    g_object_unref(G_OBJECT(g_create_builder));

    gtk_main_quit();
    return;
}

void setup_create_frame(void)
{
    setup_buttontype_frame();
    setup_resolution_frame();
    setup_sdcard_frame();
    setup_disk_frame();
    setup_ram_frame();
}

void setup_modify_frame(char *target_name)
{
    setup_modify_buttontype_frame(target_name);
    setup_modify_resolution_frame(target_name);
    setup_modify_sdcard_frame(target_name);
    setup_modify_disk_frame(target_name);
    setup_modify_ram_frame(target_name);
}

void setup_modify_resolution_frame(char *target_name)
{
    char *resolution;
    
    resolution = get_config_value(g_info_file, HARDWARE_GROUP, RESOLUTION_KEY);

    GtkWidget *hbox3 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "hbox3");
    GtkComboBox *resolution_combo_box = GTK_COMBO_BOX(gtk_combo_box_new_text());
    gtk_box_pack_start(GTK_BOX(hbox3), GTK_WIDGET(resolution_combo_box), FALSE, FALSE, 1);
    add_widget(VTM_CREATE_ID, VTM_CREATE_RESOLUTION_COMBOBOX, GTK_WIDGET(resolution_combo_box));

    gtk_combo_box_append_text(resolution_combo_box, HVGA); 
    gtk_combo_box_append_text(resolution_combo_box, WVGA); 
    gtk_combo_box_append_text(resolution_combo_box, WSVGA); 
    gtk_combo_box_append_text(resolution_combo_box, HD); 

    if(strcmp(resolution, HVGA_VALUE) == 0)
        gtk_combo_box_set_active(resolution_combo_box, RESOLUTION_HVGA);
    else if(strcmp(resolution, WVGA_VALUE) == 0)
        gtk_combo_box_set_active(resolution_combo_box, RESOLUTION_WVGA);
    else if(strcmp(resolution, WSVGA_VALUE) == 0)
        gtk_combo_box_set_active(resolution_combo_box, RESOLUTION_WSVGA);
    else
        gtk_combo_box_set_active(resolution_combo_box, RESOLUTION_HD);

    g_signal_connect(G_OBJECT(resolution_combo_box), "changed", G_CALLBACK(resolution_select_cb), NULL);

    INFO( "resolution : %s\n", resolution);
    g_free(resolution);
}

void setup_modify_disk_frame(char *target_name)
{
    char *disk_path;
    // radio button setup
    GtkWidget *default_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton12");
    GtkWidget *select_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton13");
    // file chooser setup
    GtkWidget *sdcard_filechooser2 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton2");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Disk Image Files");
    
    char *arch = (char*)g_getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return ;
    }

    char *filter_pattern = g_strdup_printf("*.%s",arch);
    gtk_file_filter_add_pattern(filter, filter_pattern);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(sdcard_filechooser2), filter);
    int disk_type= get_config_type(g_info_file, HARDWARE_GROUP, DISK_TYPE_KEY);
    if(disk_type == 1)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(select_radiobutton), TRUE);
        disk_path= get_config_value(g_info_file, HARDWARE_GROUP, BASEDISK_PATH_KEY);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(sdcard_filechooser2), disk_path);
    }
    else if(disk_type == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(default_radiobutton), TRUE);
    //can not modify baseimg. only can create.

    gtk_widget_set_sensitive(default_radiobutton, FALSE);
    gtk_widget_set_sensitive(select_radiobutton, FALSE);
    gtk_widget_set_sensitive(sdcard_filechooser2, FALSE);
}

void setup_modify_sdcard_frame(char *target_name)
{
    char *sdcard_type;
    char* sdcard_path;

    GtkWidget *hbox4 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "hbox4");

    GtkComboBox *sdcard_combo_box = GTK_COMBO_BOX(gtk_combo_box_new_text());
    gtk_box_pack_start(GTK_BOX(hbox4), GTK_WIDGET(sdcard_combo_box), FALSE, FALSE, 1);
    add_widget(VTM_CREATE_ID, VTM_CREATE_SDCARD_COMBOBOX, GTK_WIDGET(sdcard_combo_box));

    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_256); 
    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_512); 
    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_1024); 
    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_1536); 

    gtk_combo_box_set_active(sdcard_combo_box, SDCARD_DEFAULT_SIZE);

    // radio button setup
    GtkWidget *create_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton4");
    GtkWidget *select_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton5");
    GtkWidget *none_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton6");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(none_radiobutton), TRUE);

    // file chooser setup
    GtkWidget *sdcard_filechooser = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton1");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "SD Card Image Files(*.img)");
    gtk_file_filter_add_pattern(filter, "*.img");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(sdcard_filechooser), filter);

    sdcard_type= get_config_value(g_info_file, HARDWARE_GROUP, SDCARD_TYPE_KEY);
    if(strcmp(sdcard_type, "0") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(none_radiobutton), TRUE);
    else{
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(select_radiobutton), TRUE);
        gtk_widget_set_sensitive(sdcard_filechooser, TRUE);
        sdcard_path= get_config_value(g_info_file, HARDWARE_GROUP, SDCARD_PATH_KEY);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(sdcard_filechooser), sdcard_path);
    }

    set_sdcard_create_active_cb();  
    set_sdcard_select_active_cb();  

    g_signal_connect(G_OBJECT(sdcard_combo_box), "changed", G_CALLBACK(sdcard_size_select_cb), NULL);
    g_signal_connect(G_OBJECT(create_radiobutton), "toggled", G_CALLBACK(set_sdcard_create_active_cb), NULL);
    g_signal_connect(G_OBJECT(select_radiobutton), "toggled", G_CALLBACK(set_sdcard_select_active_cb), NULL);
    g_signal_connect(G_OBJECT(none_radiobutton), "toggled", G_CALLBACK(set_sdcard_none_active_cb), NULL);
    g_signal_connect(G_OBJECT(sdcard_filechooser), "selection-changed", G_CALLBACK(sdcard_file_select_cb), NULL);

}

void setup_modify_ram_frame(char *target_name)
{
    char *ram_size;

    ram_size = get_config_value(g_info_file, HARDWARE_GROUP, RAM_SIZE_KEY);

    GtkWidget *hbox6 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "hbox6");
    GtkComboBox *ram_combo_box = GTK_COMBO_BOX(gtk_combo_box_new_text());
    gtk_box_pack_start(GTK_BOX(hbox6), GTK_WIDGET(ram_combo_box), FALSE, FALSE, 1);
    add_widget(VTM_CREATE_ID, VTM_CREATE_RAM_COMBOBOX, GTK_WIDGET(ram_combo_box));

    gtk_combo_box_append_text(ram_combo_box, RAM_SIZE_512); 
    gtk_combo_box_append_text(ram_combo_box, RAM_SIZE_768); 
    gtk_combo_box_append_text(ram_combo_box, RAM_SIZE_1024); 

    if(strcmp(ram_size, RAM_SIZE_512) == 0)
        gtk_combo_box_set_active(ram_combo_box, RAM_DEFAULT_SIZE);
    else if(strcmp(ram_size, RAM_SIZE_768) == 0)
        gtk_combo_box_set_active(ram_combo_box, RAM_768_SIZE);
    else
        gtk_combo_box_set_active(ram_combo_box, RAM_1024_SIZE);

    g_signal_connect(G_OBJECT(ram_combo_box), "changed", G_CALLBACK(ram_select_cb), NULL);
}


void setup_create_button(void)
{
    GtkWidget *ok_button = (GtkWidget *)gtk_builder_get_object(g_create_builder, "button7");
    GtkWidget *cancel_button = (GtkWidget *)gtk_builder_get_object(g_create_builder, "button6");

    gtk_widget_set_sensitive(ok_button, FALSE);
    g_signal_connect(ok_button, "clicked", G_CALLBACK(ok_clicked_cb), NULL);
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(create_window_deleted_cb), NULL);
}

void setup_modify_button(char* target_name)
{
    GtkWidget *ok_button = (GtkWidget *)gtk_builder_get_object(g_create_builder, "button7");
    GtkWidget *cancel_button = (GtkWidget *)gtk_builder_get_object(g_create_builder, "button6");

    g_signal_connect(ok_button, "clicked", G_CALLBACK(modify_ok_clicked_cb), (gpointer*)target_name);
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(create_window_deleted_cb), NULL);
}

void setup_buttontype_frame(void)
{
    GtkWidget *radiobutton10 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton10");
    GtkWidget *radiobutton11 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton11");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton10), TRUE);

    g_signal_connect(GTK_RADIO_BUTTON(radiobutton10), "toggled", G_CALLBACK(buttontype_select_cb), NULL);
    g_signal_connect(GTK_RADIO_BUTTON(radiobutton11), "toggled", G_CALLBACK(buttontype_select_cb), NULL);
}

void setup_modify_buttontype_frame(char *target_name)
{
    int button_type;
    GtkWidget *radiobutton10 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton10");
    GtkWidget *radiobutton11 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton11");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton10), TRUE);

    g_signal_connect(GTK_RADIO_BUTTON(radiobutton10), "toggled", G_CALLBACK(buttontype_select_cb), NULL);
    g_signal_connect(GTK_RADIO_BUTTON(radiobutton11), "toggled", G_CALLBACK(buttontype_select_cb), NULL);

    button_type = get_config_type(g_info_file, HARDWARE_GROUP, BUTTON_TYPE_KEY);

    if(button_type == 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton10), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton11), TRUE);

    virtual_target_info.button_type = button_type;
    INFO( "button_type : %d\n", button_type);
}

void setup_resolution_frame(void)
{
    GtkWidget *hbox = (GtkWidget *)gtk_builder_get_object(g_create_builder, "hbox3");

    GtkComboBox *resolution_combo_box = GTK_COMBO_BOX(gtk_combo_box_new_text());
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(resolution_combo_box), FALSE, FALSE, 1);
    add_widget(VTM_CREATE_ID, VTM_CREATE_RESOLUTION_COMBOBOX, GTK_WIDGET(resolution_combo_box));

    gtk_combo_box_append_text(resolution_combo_box, HVGA); 
    gtk_combo_box_append_text(resolution_combo_box, WVGA); 
    gtk_combo_box_append_text(resolution_combo_box, WSVGA); 
    gtk_combo_box_append_text(resolution_combo_box, HD); 

    gtk_combo_box_set_active(resolution_combo_box, RESOLUTION_DEFAULT_SIZE);

    g_signal_connect(G_OBJECT(resolution_combo_box), "changed", G_CALLBACK(resolution_select_cb), NULL);
}

void setup_disk_frame(void)
{
    char *arch = (char*)g_getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return ;
    }
    // radio button setup
    GtkWidget *default_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton12");
    GtkWidget *select_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton13");
    // file chooser setup
    GtkWidget *disk_filechooser2 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton2");
    GtkFileFilter *filter = gtk_file_filter_new();
    if(strcmp(arch, X86) == 0)
    {
        gtk_file_chooser_button_set_title((GtkFileChooserButton *)disk_filechooser2,"Select existing Base Image(x86)");
        gtk_file_filter_set_name(filter, "Disk Image Files(*.x86)");
    }
    else if(strcmp(arch, ARM) == 0)
    {
        gtk_file_chooser_button_set_title((GtkFileChooserButton *)disk_filechooser2,"Select existing Base Image(arm)");
        gtk_file_filter_set_name(filter, "Disk Image Files(*.arm)");
    }
    
    char *filter_pattern = g_strdup_printf("*.%s",arch);
    gtk_file_filter_add_pattern(filter, filter_pattern);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(disk_filechooser2), filter);
    set_disk_default_active_cb();
    set_disk_select_active_cb();    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(default_radiobutton), TRUE);

    g_signal_connect(G_OBJECT(select_radiobutton), "toggled", G_CALLBACK(set_disk_select_active_cb), NULL);
    g_signal_connect(G_OBJECT(default_radiobutton), "toggled", G_CALLBACK(set_disk_default_active_cb), NULL);
    g_signal_connect(G_OBJECT(disk_filechooser2), "selection-changed", G_CALLBACK(disk_file_select_cb), NULL);

}

void setup_sdcard_frame(void)
{
    // sdcard size combo box setup
    GtkWidget *hbox = (GtkWidget *)gtk_builder_get_object(g_create_builder, "hbox4");

    GtkComboBox *sdcard_combo_box = GTK_COMBO_BOX(gtk_combo_box_new_text());
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(sdcard_combo_box), FALSE, FALSE, 1);
    add_widget(VTM_CREATE_ID, VTM_CREATE_SDCARD_COMBOBOX, GTK_WIDGET(sdcard_combo_box));

    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_256); 
    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_512); 
    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_1024); 
    gtk_combo_box_append_text(sdcard_combo_box, SDCARD_SIZE_1536); 

    gtk_combo_box_set_active(sdcard_combo_box, SDCARD_DEFAULT_SIZE);

    // radio button setup
    GtkWidget *create_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton4");
    GtkWidget *select_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton5");
    GtkWidget *none_radiobutton = (GtkWidget *)gtk_builder_get_object(g_create_builder, "radiobutton6");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(none_radiobutton), TRUE);

    // file chooser setup
    GtkWidget *sdcard_filechooser = (GtkWidget *)gtk_builder_get_object(g_create_builder, "filechooserbutton1");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "SD Card Image Files(*.img)");
    gtk_file_filter_add_pattern(filter, "*.img");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(sdcard_filechooser), filter);

    set_sdcard_create_active_cb();  
    set_sdcard_select_active_cb();

    g_signal_connect(G_OBJECT(sdcard_combo_box), "changed", G_CALLBACK(sdcard_size_select_cb), NULL);
    g_signal_connect(G_OBJECT(create_radiobutton), "toggled", G_CALLBACK(set_sdcard_create_active_cb), NULL);
    g_signal_connect(G_OBJECT(select_radiobutton), "toggled", G_CALLBACK(set_sdcard_select_active_cb), NULL);
    g_signal_connect(G_OBJECT(none_radiobutton), "toggled", G_CALLBACK(set_sdcard_none_active_cb), NULL);
    g_signal_connect(G_OBJECT(sdcard_filechooser), "selection-changed", G_CALLBACK(sdcard_file_select_cb), NULL);
}

void setup_ram_frame(void)
{
    GtkWidget *hbox = (GtkWidget *)gtk_builder_get_object(g_create_builder, "hbox6");

    GtkComboBox *ram_combo_box = GTK_COMBO_BOX(gtk_combo_box_new_text());
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ram_combo_box), FALSE, FALSE, 1);
    add_widget(VTM_CREATE_ID, VTM_CREATE_RAM_COMBOBOX, GTK_WIDGET(ram_combo_box));

    gtk_combo_box_append_text(ram_combo_box, RAM_SIZE_512); 
    gtk_combo_box_append_text(ram_combo_box, RAM_SIZE_768); 
    gtk_combo_box_append_text(ram_combo_box, RAM_SIZE_1024); 

    gtk_combo_box_set_active(ram_combo_box, RAM_DEFAULT_SIZE);

    g_signal_connect(G_OBJECT(ram_combo_box), "changed", G_CALLBACK(ram_select_cb), NULL);
}

void show_create_window(void)
{
    GtkWidget *sub_window;
    char *arch = (char*)g_getenv(EMULATOR_ARCH);
    if(arch == NULL)
    {
        ERR( "architecture setting failed\n");
        show_message("Error", "Architecture setting failed.");
        return ;
    }

    g_create_builder = gtk_builder_new();
    char full_glade_path[MAX_LEN];
    sprintf(full_glade_path, "%s/etc/vtm.glade", get_root_path());

    gtk_builder_add_from_file(g_create_builder, full_glade_path, NULL);

    sub_window = (GtkWidget *)gtk_builder_get_object(g_create_builder, "window2");

    add_window(sub_window, VTM_CREATE_ID);

    if(strcmp(arch, X86) == 0)
        gtk_window_set_title(GTK_WINDOW(sub_window), "Create new Virtual Target(x86)");
    else if(strcmp(arch, ARM) == 0)
        gtk_window_set_title(GTK_WINDOW(sub_window), "Create new Virtual Target(arm)"); 

    fill_virtual_target_info();

    GtkWidget *label4 = (GtkWidget *)gtk_builder_get_object(g_create_builder, "label4");
    gtk_label_set_text(GTK_LABEL(label4),"Input name of the virtual target.");
    GtkWidget *name_entry = (GtkWidget *)gtk_builder_get_object(g_create_builder, "entry1");
    gtk_entry_set_max_length(GTK_ENTRY(name_entry), VT_NAME_MAXBUF); 

    g_signal_connect(G_OBJECT (name_entry), "changed",  G_CALLBACK (entry_changed), NULL);

    setup_create_frame();
    setup_create_button();

    gtk_window_set_icon_from_file(GTK_WINDOW(sub_window), g_icon_image, NULL);

    g_signal_connect(GTK_OBJECT(sub_window), "delete_event", G_CALLBACK(create_window_deleted_cb), NULL);

    gtk_widget_show_all(sub_window);

    gtk_main();
}

void construct_main_window(void)
{
    GtkWidget *vbox;
    GtkWidget *tree_view;
    GtkTreeSelection *selection;
    GtkWidget *create_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button1");
    GtkWidget *delete_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button2");
    GtkWidget *modify_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button3");
    GtkWidget *start_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button4");
    GtkWidget *details_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button5");
    GtkWidget *refresh_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button8");
    GtkWidget *reset_button = (GtkWidget *)gtk_builder_get_object(g_builder, "button9");
    g_main_window = (GtkWidget *)gtk_builder_get_object(g_builder, "window1");
    gtk_window_set_icon_from_file(GTK_WINDOW(g_main_window), g_icon_image, NULL);
    GtkWidget *x86_radiobutton = (GtkWidget *)gtk_builder_get_object(g_builder, "radiobutton8");
    GtkWidget *arm_radiobutton = (GtkWidget *)gtk_builder_get_object(g_builder, "radiobutton9");

    vbox = GTK_WIDGET(gtk_builder_get_object(g_builder, "vbox3"));

    tree_view = setup_tree_view();
    gtk_widget_grab_focus(start_button);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(x86_radiobutton), TRUE);

    gtk_widget_set_sensitive(arm_radiobutton, FALSE);

    g_signal_connect(GTK_RADIO_BUTTON(x86_radiobutton), "toggled", G_CALLBACK(arch_select_cb), x86_radiobutton);
    g_signal_connect(GTK_RADIO_BUTTON(arm_radiobutton), "toggled", G_CALLBACK(arch_select_cb), arm_radiobutton);
    gtk_box_pack_start(GTK_BOX(vbox), tree_view, TRUE, TRUE, 0);
    selection  = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
//  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
    // add shortcut to buttons
    GtkAccelGroup *group;
    group = gtk_accel_group_new();
    gtk_window_add_accel_group (GTK_WINDOW (g_main_window), group);

    gtk_widget_add_accelerator (create_button, "clicked", group, GDK_Insert, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator (delete_button, "clicked", group, GDK_Delete, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator (modify_button, "clicked", group, GDK_M, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator (reset_button, "clicked", group, GDK_R, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator (refresh_button, "clicked", group, GDK_F5, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator (details_button, "clicked", group, GDK_D, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator (start_button, "clicked", group, GDK_S, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator (start_button, "clicked", group, GDK_Return, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    g_signal_connect(create_button, "clicked", G_CALLBACK(show_create_window), NULL); 
    g_signal_connect(delete_button, "clicked", G_CALLBACK(delete_clicked_cb), selection);
    g_signal_connect(details_button, "clicked", G_CALLBACK(details_clicked_cb), selection);
    g_signal_connect(modify_button, "clicked", G_CALLBACK(modify_clicked_cb), selection);
    g_signal_connect(start_button, "clicked", G_CALLBACK(activate_clicked_cb), selection);
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(refresh_clicked_cb), NULL);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(reset_clicked_cb), selection);
    g_signal_connect(G_OBJECT(g_main_window), "delete-event", G_CALLBACK(exit_vtm), NULL); 
    g_signal_connect(treeview, "cursor-changed", G_CALLBACK(cursor_changed_cb), selection);

    /* setup emulator architecture and path */
    gtk_widget_show_all(g_main_window);
}

#ifdef  __linux__
void set_mesa_lib(void)
{
    char *s_out = NULL;
    char *s_err = NULL;
    char *s_out2 = NULL;
    char *s_err2 = NULL;

    int exit_status;
    GError *err = NULL;

    if (!g_spawn_command_line_sync("cat /etc/issue", &s_out, &s_err, &exit_status, &err)) {
        TRACE( "Failed to invoke command: %s\n", err->message);
        show_message("Failed to invoke command", err->message);
        g_error_free(err);
        g_free(s_out);
        g_free(s_err);
        return ;
    }
    if (exit_status != 0) {
        TRACE( "Command returns error: %s\n", s_out);
        g_free(s_out);
        g_free(s_err);
        return;
    }
    
    if (!g_spawn_command_line_sync("lspci", &s_out2, &s_err2, &exit_status, &err)) {
        TRACE( "Failed to invoke command: %s\n", err->message);
        show_message("Failed to invoke command", err->message);
        g_error_free(err);
        g_free(s_out);
        g_free(s_err);
        g_free(s_out2);
        g_free(s_err2);
        return ;
    }
    if (exit_status != 0) {
        TRACE( "Command returns error: %s\n", s_out);
        g_free(s_out);
        g_free(s_err);
        g_free(s_out2);
        g_free(s_err2);
        return;
    }

    if(strstr(s_out, "10.10") && strstr(s_out2, "nVidia"))
    {
        INFO( "linux version :%s  Set to use mesa lib\n", s_out);
        g_setenv("LD_LIBRARY_PATH","/usr/lib/mesa:$LD_LIBRARY_PATH",1);
    }

    g_free(s_out);
    g_free(s_err);
    g_free(s_out2);
    g_free(s_err2);
    return ;

}
#endif

void version_init(char *default_targetname, char* target_list_filepath)
{
    GKeyFile *keyfile;
    int file_status;

    keyfile = g_key_file_new();

    file_status = is_exist_file(target_list_filepath);
    if(file_status == -1 || file_status == FILE_NOT_EXISTS)
    {
        show_message("File dose not exist", target_list_filepath);
        return;
    }

    if(g_key_file_has_group(keyfile, g_major_version) == FALSE)
        set_config_value(target_list_filepath, g_major_version, default_targetname, "");
    
    g_key_file_free(keyfile);
    
    return;
}

void lock_file(char *path)
{
    const gchar *username;
    int file_status;
    
    username = g_get_user_name();
    g_userfile = g_strdup_printf(".%s", username);

    file_status = is_exist_file(g_userfile);
    if(file_status == -1 || file_status == FILE_NOT_EXISTS)
    {
#ifdef _WIN32
    HANDLE hFile = NULL;
    hFile = CreateFile(change_path_from_slash(g_userfile),
                GENERIC_READ,
                0,
                NULL,
                CREATE_NEW,
                FILE_ATTRIBUTE_HIDDEN,
                NULL);

    CloseHandle(hFile);
#else
    FILE *fp = fopen(g_userfile, "w+");
    fclose(fp);
#endif
    }   
    
#ifdef _WIN32
    g_hFile = CreateFile(change_path_from_slash(g_userfile), // open path
                GENERIC_READ,             // open for reading
                0,                        // do not share
                NULL,                     // no security
                OPEN_EXISTING,            // existing file only
                FILE_ATTRIBUTE_NORMAL,    // normal file
                NULL);
    if(g_hFile == INVALID_HANDLE_VALUE)
    {
        show_message("Error", "Can not execute Emulator Manager!\n"
                "Another instance is already running.");
        free(g_userfile);
        exit(0);
    }

#else
    g_fd = open(g_userfile, O_RDWR);
    if(flock(g_fd, LOCK_EX|LOCK_NB) == -1)
    {
        show_message("Error", "Can not execute Emulator Manager!\n"
                "Another instance is already running.");
        close(g_fd);
        free(g_userfile);
        exit(0);
    }
#endif
}

int main(int argc, char** argv)
{
    char* working_dir;
    char *buf = argv[0];
    int status;
    char *skin = NULL;
    char full_glade_path[MAX_LEN];
    
    working_dir = g_path_get_dirname(buf);
    status = g_chdir(working_dir);
    if(status == -1)
    {
        ERR( "fail to change working directory\n");
        exit(1);
    }
    
    gtk_init(&argc, &argv);
    INFO( "virtual target manager start \n");

    socket_init();
    //if ubuntu 10.10 use mesa lib for opengl
#ifdef  __linux__
    set_mesa_lib();
#endif

    g_builder = gtk_builder_new();
    skin = (char*)get_skin_path();
    if(skin == NULL)
        WARN( "getting icon image path is failed!!\n");
    sprintf(g_icon_image, "%s/icons/vtm.ico", skin);

    sprintf(full_glade_path, "%s/etc/vtm.glade", get_root_path());

    gtk_builder_add_from_file(g_builder, full_glade_path, NULL);

    lock_file(full_glade_path);
    
    window_hash_init();

    construct_main_window();

    env_init();
    
    gtk_main();

    free(g_target_list_filepath);
    return 0;
}

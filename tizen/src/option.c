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
  @file option.c
  @brief    collection of dialog function
 */

#include "option.h"

#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <net/if.h>
#else
#ifdef WINVER < 0x0501
#undef WINVER
#define WINVER 0x0501
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <winreg.h>
#endif
#endif

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, option);

CONFIGURATION preference_entrys;
int startup_option_config_done = 0;

/**
  @brief    get host DNS server address
  @param    dns1: return value (first dns server address)
  @param    dns2: return value (second dns server address)
  @return always 0
 */
int gethostDNS(char *dns1, char *dns2)
{
#ifndef _WIN32
    FILE *resolv;
    char buf[255];
    memset(buf, 0, sizeof(char)*255);

    resolv = fopen("/etc/resolv.conf", "r");
    if (resolv <= 0) {
        ERR( "Cann't open \"/etc/resolv.conf.\"\n");
        return 1;
    }

    while(fscanf(resolv , "%s", buf) != EOF) {
        if(strcmp(buf, "nameserver") == 0)
        {
            fscanf(resolv , "%s", dns1);
            break;
        }
    }

    while(fscanf(resolv , "%s", buf) != EOF) {
        if(strcmp(buf, "nameserver") == 0)
        {
            fscanf(resolv , "%s", dns2);
            break;
        }
    }

    fclose(resolv);
#else
    PIP_ADAPTER_ADDRESSES pAdapterAddr;
    PIP_ADAPTER_ADDRESSES pAddr;
    PIP_ADAPTER_DNS_SERVER_ADDRESS pDnsAddr;
    unsigned long dwResult;
    unsigned long nBufferLength = sizeof(IP_ADAPTER_ADDRESSES);
    pAdapterAddr = (PIP_ADAPTER_ADDRESSES)malloc(nBufferLength);
    memset(pAdapterAddr, 0x00, nBufferLength);

    while ((dwResult = GetAdaptersAddresses(AF_INET, 0, NULL, pAdapterAddr, &nBufferLength))
            == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterAddr);
        pAdapterAddr = (PIP_ADAPTER_ADDRESSES)malloc(nBufferLength);
        memset(pAdapterAddr, 0x00, nBufferLength);
    }

    pAddr = pAdapterAddr;
    for (; pAddr != NULL; pAddr = pAddr->Next) {
        pDnsAddr = pAddr->FirstDnsServerAddress;
        for (; pDnsAddr != NULL; pDnsAddr = pDnsAddr->Next) {
            struct sockaddr_in *pSockAddr = (struct sockaddr_in*)pDnsAddr->Address.lpSockaddr;
            if(*dns1 == 0) {
                strcpy(dns1, inet_ntoa(pSockAddr->sin_addr));
                continue;
            }
            if(*dns2 == 0) {
                strcpy(dns2, inet_ntoa(pSockAddr->sin_addr));
                continue;
            }
        }
    }
    free(pAdapterAddr);
#endif
    return 0;
}

/**
  @brief    get host proxy server address
  @param    proxy: return value (proxy server address)
  @return always 0
 */
int gethostproxy(char *proxy)
{
#ifndef _WIN32
    char buf[255];
    FILE *output;

    emulator_mutex_lock();

    output = popen("gconftool-2 --get /system/proxy/mode", "r");
    fscanf(output, "%s", buf);
    pclose(output);

    if (strcmp(buf, "manual") == 0){
        output = popen("gconftool-2 --get /system/http_proxy/host", "r");
        fscanf(output , "%s", buf);
        sprintf(proxy, "%s", buf);
        pclose(output);

        output = popen("gconftool-2 --get /system/http_proxy/port", "r");
        fscanf(output , "%s", buf);
        sprintf(proxy, "%s:%s", proxy, buf);
        pclose(output);

    }else if (strcmp(buf, "auto") == 0){
        INFO( "Emulator can't support automatic proxy currently. starts up with normal proxy.\n");
        //can't support proxy auto setting
//      output = popen("gconftool-2 --get /system/proxy/autoconfig_url", "r");
//      fscanf(output , "%s", buf);
//      sprintf(proxy, "%s", buf);
//      pclose(output);
    }

    emulator_mutex_unlock();
#else
    HKEY hKey;
    int nRet;
    LONG lRet;
    BYTE *proxyenable, *proxyserver;
    DWORD dwLength = 0;
    nRet = RegOpenKeyEx(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
            0, KEY_QUERY_VALUE, &hKey);
    if (nRet != ERROR_SUCCESS) {
        fprintf(stderr, "Failed to open registry from %s\n",
                "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
        return 0;
    }
    lRet = RegQueryValueEx(hKey, "ProxyEnable", 0, NULL, NULL, &dwLength);
    if (lRet != ERROR_SUCCESS && dwLength == 0) {
        fprintf(stderr, "Failed to query value from from %s\n",
                "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
        RegCloseKey(hKey);
        return 0;
    }
    proxyenable = (BYTE*)malloc(dwLength);
    if (proxyenable == NULL) {
        fprintf(stderr, "Failed to allocate a buffer\n");
        RegCloseKey(hKey);
        return 0;
    }

    lRet = RegQueryValueEx(hKey, "ProxyEnable", 0, NULL, proxyenable, &dwLength);
    if (lRet != ERROR_SUCCESS) {
        free(proxyenable);
        fprintf(stderr, "Failed to query value from from %s\n",
                "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
        RegCloseKey(hKey);
        return 0;
    }
    if (*(char*)proxyenable == 0) {
        free(proxyenable);
        RegCloseKey(hKey);      
        return 0;
    }

    dwLength = 0;
    lRet = RegQueryValueEx(hKey, "ProxyServer", 0, NULL, NULL, &dwLength);
    if (lRet != ERROR_SUCCESS && dwLength == 0) {
        fprintf(stderr, "Failed to query value from from %s\n",
                "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
        RegCloseKey(hKey);      
        return 0;
    }

    proxyserver = (BYTE*)malloc(dwLength);
    if (proxyserver == NULL) {
        fprintf(stderr, "Failed to allocate a buffer\n");
        RegCloseKey(hKey);      
        return 0;
    }

    memset(proxyserver, 0x00, dwLength);
    lRet = RegQueryValueEx(hKey, "ProxyServer", 0, NULL, proxyserver, &dwLength);
    if (lRet != ERROR_SUCCESS) {
        free(proxyserver);
        fprintf(stderr, "Failed to query value from from %s\n",
                "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
        RegCloseKey(hKey);
        return 0;
    }
    if (proxyserver != NULL) strcpy(proxy, (char*)proxyserver);
    free(proxyserver);
    RegCloseKey(hKey);
#endif
    return 0;
}

/* This function get HOST IP address */
int gethostIP(char *host_ip)
{
#ifndef _WIN32

    int fd;
    struct if_nameindex *curif, *ifs;
    struct ifreq req;

    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if( fd != -1) 
    {
        ifs = if_nameindex();
        if(ifs) 
        {
            for(curif = ifs; curif && curif->if_name; curif++) 
            {
                strncpy(req.ifr_name, curif->if_name, IFNAMSIZ);
                req.ifr_name[IFNAMSIZ] = 0;

                if (ioctl(fd, SIOCGIFADDR, &req) < 0)
                {
                    ERR( "ioctl fail: %s \n", strerror(errno));
                }else{
                    TRACE( "%s: [%s]\n"
                            , curif->if_name
                            , inet_ntoa(((struct sockaddr_in*) &req.ifr_addr)->sin_addr));

                    if( strncmp(curif->if_name, "lo", 2) != 0){
                        sprintf(host_ip, "%s"
                                , inet_ntoa(((struct sockaddr_in*) &req.ifr_addr)->sin_addr));
                        break;
                    }
                }
            }

            if_freenameindex(ifs);
            close(fd);

        } else {
            ERR( "if_nameindex fail: %s \n", strerror(errno));
        }

    } else {
        ERR( "socket fail: %s \n", strerror(errno));
    }

#endif
    return 0;
}

static GtkWidget *make_virtual_target_frame(const gchar *frame_name)
{
    int i;
    gchar **target_list = NULL;
    int num = 0;

    GtkWidget *frame;
    frame = gtk_frame_new(frame_name);

    GtkWidget *vbox;
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    GtkWidget *hbox;
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new(_("Select virtual target"));
    gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 1);

    GtkWidget *virtual_target_combobox = (GtkWidget *)gtk_combo_box_new_text();
    add_widget(OPTION_ID, OPTION_VIRTUAL_TARGET_COMBOBOX, virtual_target_combobox);
    gtk_box_pack_start (GTK_BOX (hbox), virtual_target_combobox, TRUE, TRUE, 1);    

    target_list = get_virtual_target_list(SYSTEMINFO.target_list_file , TARGET_LIST_GROUP, &num);

    for(i = 0; i < num; i++)
    {
        gtk_combo_box_append_text(GTK_COMBO_BOX (virtual_target_combobox), target_list[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX (virtual_target_combobox), 0);

    if(startup_option_config_done == 1)
    {
        gtk_widget_set_sensitive(virtual_target_combobox, FALSE);
    }

    g_signal_connect(G_OBJECT(virtual_target_combobox), "changed", G_CALLBACK(virtual_target_select_cb), NULL);

    g_strfreev(target_list);
    return frame;
}

/*
   static GtkWidget *make_scale_frame(const gchar *frame_name)
   {
   GSList *group;

   GtkWidget *frame;
   frame = gtk_frame_new(frame_name);

   GtkWidget *vbox;
   vbox = gtk_vbox_new (FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), vbox);

   GtkWidget *hbox;
   hbox = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

   GtkWidget *scale_half_button = gtk_radio_button_new_with_label(NULL, "1/2x");
   add_widget(OPTION_ID, OPTION_SCALE_HALF_BUTTON, scale_half_button);
   group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(scale_half_button));
   gtk_box_pack_start (GTK_BOX (hbox), scale_half_button, TRUE, TRUE, 1);

   GtkWidget *scale_one_button = gtk_radio_button_new_with_label(group, "1x");
   add_widget(OPTION_ID, OPTION_SCALE_ONE_BUTTON, scale_one_button);
   gtk_box_pack_start (GTK_BOX (hbox), scale_one_button, TRUE, TRUE, 1);    

   if(preference_entrys.scale == 2)
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scale_half_button), TRUE);
   else
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scale_one_button), TRUE);

   if(startup_option_config_done == 1)
   {
   gtk_widget_set_sensitive(scale_half_button, FALSE);
   gtk_widget_set_sensitive(scale_one_button, FALSE);
   }

   g_signal_connect(G_OBJECT(scale_half_button), "toggled", G_CALLBACK(scale_select_cb), scale_half_button);
   g_signal_connect(G_OBJECT(scale_one_button), "toggled", G_CALLBACK(scale_select_cb), scale_one_button);

   return frame;
   }
 */

/**
  @brief    make a frame for always on frame buffer
  @param    frame_name: frame name
  @return newly created frame
 */
#if 0
static GtkWidget *make_serial_frame(const gchar *frame_name)
{
    /* 1. set box */

    GtkWidget *frame;
    frame = gtk_frame_new(frame_name);

    GtkWidget *vbox;
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* 2. Telnet Port check button and set inactive status by telnet type */
    /* 5. set inactive status by Telnet Type */
    GtkWidget *hbox;
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *telnet_button = gtk_check_button_new_with_label( _("Telnet Port"));
    add_widget(OPTION_ID, OPTION_TELNET_BUTTON, telnet_button);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(telnet_button), TRUE, TRUE, 1);
    gtk_toggle_button_set_active((GtkToggleButton *)telnet_button, preference_entrys.qemu_configuration.telnet_type);

    /* 3. Telnet Port Entry */

    GtkWidget *telnet_port_entry = gtk_entry_new();
    gtk_entry_set_max_length (GTK_ENTRY(telnet_port_entry), 4);
    gtk_widget_set_size_request(telnet_port_entry, 10, -1);
    add_widget(OPTION_ID, OPTION_TELNET_PORT_ENTRY, telnet_port_entry);
    gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET(telnet_port_entry), TRUE, TRUE, 1);
    gtk_entry_set_text(GTK_ENTRY(telnet_port_entry), preference_entrys.qemu_configuration.telnet_port);

    /* 6. set Serial console */

    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *serial_console_button = gtk_check_button_new_with_label( _("Serial Console"));
    add_widget(OPTION_ID, OPTION_SERIAL_CONSOLE_BUTTON, serial_console_button);
    gtk_box_pack_start (GTK_BOX (hbox), serial_console_button, TRUE, TRUE, 1);
    if(preference_entrys.qemu_configuration.serial_console_command_type== 1 &&  preference_entrys.qemu_configuration.telnet_type == 1)
        gtk_toggle_button_set_active((GtkToggleButton *)serial_console_button, TRUE);
    else
        gtk_toggle_button_set_active((GtkToggleButton *)serial_console_button, FALSE);

    /* 7. set Serial console entry */

    GtkWidget *serial_console_entry = gtk_entry_new();
    gtk_entry_set_max_length (GTK_ENTRY(serial_console_entry), MAXBUF);
    gtk_widget_set_size_request(serial_console_entry, 10, -1);
    add_widget(OPTION_ID, OPTION_SERIAL_CONSOLE_ENTRY, serial_console_entry);
    gtk_box_pack_start (GTK_BOX (hbox), serial_console_entry, TRUE, TRUE, 1);
    /* serial console command ==> start */
    preference_entrys.qemu_configuration.serial_console_command_type = configuration.qemu_configuration.serial_console_command_type;
    snprintf(preference_entrys.qemu_configuration.serial_console_command, MAXBUF, "%s", configuration.qemu_configuration.serial_console_command);
    gtk_entry_set_text(GTK_ENTRY(serial_console_entry), preference_entrys.qemu_configuration.serial_console_command);
    /* end */

    preference_entrys.qemu_configuration.sdcard_type = configuration.qemu_configuration.sdcard_type;
    snprintf(preference_entrys.qemu_configuration.sdcard_path, MAXBUF, "%s", configuration.qemu_configuration.sdcard_path);

    preference_entrys.qemu_configuration.diskimg_type = configuration.qemu_configuration.diskimg_type;
    snprintf(preference_entrys.qemu_configuration.diskimg_path, MAXBUF, "%s", configuration.qemu_configuration.diskimg_path);

    set_telnet_status_active_cb();

    g_signal_connect(G_OBJECT(telnet_button), "toggled", G_CALLBACK(set_telnet_status_active_cb), NULL);
    g_signal_connect(G_OBJECT(serial_console_button), "toggled", G_CALLBACK(serial_console_command_cb), NULL);
    return(frame);
}
#endif

/**
  @brief    make a frame for always on frame buffer
  @param    frame_name: frame name
  @return newly created frame
 */
static GtkWidget *make_internet_frame(const gchar *frame_name)
{
    /* 1. set box */

    GtkWidget *frame;
    frame = gtk_frame_new(frame_name);

    GtkWidget *vbox;
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* 2. http proxy entry */

    GtkWidget *hbox;
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *use_host_proxy = gtk_check_button_new_with_label( _("Use host proxy"));
    add_widget(OPTION_ID, OPTION_USE_HOST_PROXY, use_host_proxy);
    gtk_box_pack_start (GTK_BOX (hbox), use_host_proxy, TRUE, TRUE, 1);
    gtk_toggle_button_set_active((GtkToggleButton *)use_host_proxy, preference_entrys.qemu_configuration.use_host_http_proxy);

    GtkWidget *use_host_DNS = gtk_check_button_new_with_label( _("Use host DNS"));
    add_widget(OPTION_ID, OPTION_USE_HOST_DNS, use_host_DNS);
    gtk_box_pack_end (GTK_BOX (hbox), use_host_DNS, TRUE, TRUE, 1);
    gtk_toggle_button_set_active((GtkToggleButton *)use_host_DNS, preference_entrys.qemu_configuration.use_host_dns_server);

    if(startup_option_config_done == 1)
    {
        gtk_widget_set_sensitive(use_host_proxy, FALSE);
        gtk_widget_set_sensitive(use_host_DNS, FALSE);
    }

    g_signal_connect(G_OBJECT(use_host_proxy), "toggled", G_CALLBACK(use_host_proxy_cb), NULL);
    g_signal_connect(G_OBJECT(use_host_DNS), "toggled", G_CALLBACK(use_host_DNS_cb), NULL);

    return frame;
}

/**
 * @brief   make a frame for sdcard
 * @return  newly created frame
 */
#if 0
static GtkWidget *make_sdcard_frame(const gchar *frame_name)
{
    /* 1. set box */

    GtkWidget *frame;
    frame = gtk_frame_new(frame_name);

    GtkWidget *vbox;
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* 2. SD Card check button */

    GtkWidget *hbox;
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *sdcard_button = gtk_check_button_new_with_label( _("SD Card                      "));
    add_widget(OPTION_ID, OPTION_SDCARD_BUTTON, sdcard_button);
    gtk_box_pack_start (GTK_BOX (hbox), sdcard_button, FALSE, FALSE, 1);

    /* 3. SD Card button */

    gchar *sdcard_img_name = basename(preference_entrys.qemu_configuration.sdcard_path);
    GtkWidget *sdcard_img_button;
    if( !strcmp(".",sdcard_img_name) || sdcard_img_name==NULL )
        sdcard_img_button = gtk_button_new_with_label (_("Select SD Card Image"));
    else
        sdcard_img_button = gtk_button_new_with_label (sdcard_img_name);
    add_widget(OPTION_ID, OPTION_SDCARD_IMG_BUTTON, sdcard_img_button);
    gtk_box_pack_start (GTK_BOX (hbox), sdcard_img_button, TRUE, TRUE, 1);

    /* 4. set inactive status by SDcard Type */

    if(preference_entrys.qemu_configuration.sdcard_type == 1)
        gtk_toggle_button_set_active((GtkToggleButton *)sdcard_button, TRUE);
    else if(preference_entrys.qemu_configuration.sdcard_type == 0)
        gtk_toggle_button_set_active((GtkToggleButton *)sdcard_button, FALSE);

    set_sdcard_status_active_cb();

    if(startup_option_config_done == 1)
    {
        gtk_widget_set_sensitive(sdcard_button, FALSE);
        gtk_widget_set_sensitive(sdcard_img_button, FALSE);
    }

    g_signal_connect(G_OBJECT(sdcard_button), "toggled", G_CALLBACK(set_sdcard_status_active_cb), NULL);
    g_signal_connect(GTK_BUTTON(sdcard_img_button), "clicked", G_CALLBACK(sdcard_select_cb), NULL);

    return frame;
}
#endif


static GtkWidget *make_always_on_top_frame(const gchar *frame_name)
{
    GtkWidget *frame;
    frame = gtk_frame_new(frame_name);

    GtkWidget *vbox;
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    GtkWidget *hbox;
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *always_on_top = gtk_check_button_new_with_label( _("Emulator"));
    add_widget(OPTION_ID, OPTION_ALWAYS_ON_TOP_BUTTON, always_on_top);
    gtk_box_pack_start(GTK_BOX (hbox), always_on_top, TRUE, TRUE, 1);
    gtk_toggle_button_set_active((GtkToggleButton *)always_on_top, preference_entrys.always_on_top);

    g_signal_connect(G_OBJECT(always_on_top), "toggled", G_CALLBACK(always_on_top_cb), NULL);

    return frame;
}


/**
  @brief    make a frame for snapshot boot
  @param    frame_name: frame name
  @return newly created frame
 */
#if 0
static GtkWidget *make_boot_frame(const gchar *frame_name)
{
    char snapshot_date_str[MAXBUF];
    /* 1. set box */

    GtkWidget *frame;
    frame = gtk_frame_new(frame_name);

    GtkWidget *vbox;
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* 2. boot entry */

    GtkWidget *hbox;
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *snapshot_boot = gtk_check_button_new_with_label( _("Snapshot Boot                        "));
    add_widget(OPTION_ID, OPTION_SNAPSHOT_BOOT, snapshot_boot);
    gtk_box_pack_start (GTK_BOX (hbox), snapshot_boot, TRUE, TRUE, 1);
    gtk_toggle_button_set_active((GtkToggleButton *)snapshot_boot, preference_entrys.qemu_configuration.save_emulator_state);

    GtkWidget *snapshot_saved_date_entry = gtk_entry_new();
    gtk_widget_set_size_request(snapshot_saved_date_entry, -1, -1);
    add_widget(OPTION_ID, OPTION_SNAPSHOT_SAVED_DATE_ENTRY, snapshot_saved_date_entry);
    gtk_box_pack_start(GTK_BOX(hbox), snapshot_saved_date_entry, TRUE, TRUE, 1);

    if(preference_entrys.qemu_configuration.snapshot_saved == 1)
        snprintf(snapshot_date_str, MAXBUF, "saved at %s", preference_entrys.qemu_configuration.snapshot_saved_date);
    else
    {
        snprintf(snapshot_date_str, MAXBUF, "no snapshot saved");
        gtk_toggle_button_set_active((GtkToggleButton *)snapshot_boot, FALSE);
        gtk_widget_set_sensitive(snapshot_boot, FALSE);
        preference_entrys.qemu_configuration.save_emulator_state = 0;
    }

    gtk_entry_set_text(GTK_ENTRY(snapshot_saved_date_entry), snapshot_date_str);
    gtk_widget_set_sensitive(snapshot_saved_date_entry, FALSE);

    if(startup_option_config_done == 1)
    {
        gtk_widget_set_sensitive(snapshot_boot, FALSE);
    }

    g_signal_connect(G_OBJECT(snapshot_boot), "toggled", G_CALLBACK(snapshot_boot_cb), NULL);

    return frame;
}
#endif

/**
  @brief    set initial preference entry
  @return void
 */

static void set_initial_preference_entrys(void)
{
    preference_entrys.qemu_configuration.use_host_http_proxy = configuration.qemu_configuration.use_host_http_proxy;
    preference_entrys.qemu_configuration.use_host_dns_server = configuration.qemu_configuration.use_host_dns_server;

    preference_entrys.always_on_top = configuration.always_on_top;

    preference_entrys.qemu_configuration.save_emulator_state = configuration.qemu_configuration.save_emulator_state;
    preference_entrys.qemu_configuration.snapshot_saved = virtual_target_info.snapshot_saved;
    snprintf(preference_entrys.qemu_configuration.snapshot_saved_date, MAXBUF, "%s", virtual_target_info.snapshot_saved_date);

    preference_entrys.qemu_configuration.telnet_type = configuration.qemu_configuration.telnet_type;
    snprintf(preference_entrys.qemu_configuration.telnet_port, MAXBUF, "%s", configuration.qemu_configuration.telnet_port);
    preference_entrys.qemu_configuration.serial_console_command_type = configuration.qemu_configuration.serial_console_command_type;
    snprintf(preference_entrys.qemu_configuration.serial_console_command, MAXBUF, "%s", configuration.qemu_configuration.serial_console_command);

    preference_entrys.qemu_configuration.sdcard_type = virtual_target_info.sdcard_type;
    snprintf(preference_entrys.qemu_configuration.sdcard_path, MAXBUF, "%s", virtual_target_info.sdcard_path);

    preference_entrys.qemu_configuration.diskimg_type = configuration.qemu_configuration.diskimg_type;
    snprintf(preference_entrys.qemu_configuration.diskimg_path, MAXBUF, "%s", configuration.qemu_configuration.diskimg_path);

    snprintf(preference_entrys.target_path, MAXBUF, "%s", configuration.target_path);
}


/**
  @brief    create config frame
  @return newly created frame
 */

void create_config_frame(GtkWidget * vbox)
{
    GtkWidget *temp_vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), temp_vbox, TRUE, TRUE, 0);

#if 0 // serial setting will not be supported. below code will be removed later
    /* 2. serial frame create */

    GtkWidget *serial_frame = make_serial_frame(_("Serial Settings"));
    gtk_box_pack_start(GTK_BOX(temp_vbox), serial_frame, TRUE, TRUE, 0);
#endif

    GtkWidget *virtual_target_frame = make_virtual_target_frame(_("Virtual Target"));
    gtk_box_pack_start(GTK_BOX(temp_vbox), virtual_target_frame, TRUE, TRUE, 0);

    //  GtkWidget *scale_frame = make_scale_frame(_("Scale"));
    //  gtk_box_pack_start(GTK_BOX(temp_vbox), scale_frame, TRUE, TRUE, 0);


    /* 4. network internet setting frame create */

    GtkWidget *internet_frame = make_internet_frame(_("Internet Setting"));
    gtk_box_pack_start(GTK_BOX(temp_vbox), internet_frame, TRUE, TRUE, 0);

    /* 5. sdcard frame create */

    //  GtkWidget *sdcard_frame = make_sdcard_frame(_("SD Card"));
    //  gtk_box_pack_start(GTK_BOX(temp_vbox), sdcard_frame, TRUE, TRUE, 0);

    GtkWidget *always_on_top_frame = make_always_on_top_frame(_("Always On Top"));
    gtk_box_pack_start(GTK_BOX(temp_vbox), always_on_top_frame, TRUE, TRUE, 0);

    /* 6. boot frame create */

    //  GtkWidget *boot_frame = make_boot_frame(_("Boot"));
    //  gtk_box_pack_start(GTK_BOX(temp_vbox), boot_frame, TRUE, TRUE, 0);

}


/**
  @brief    create config button
  @param  GtkWidget : box pack
  @return newly created frame
 */
void create_config_button(GtkWidget *vbox)
{
    GtkWidget *button_box;
    button_box = gtk_hbutton_box_new();
    gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);

    /* 1. cancel button create */

    GtkWidget *cancel_btn = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_container_add (GTK_CONTAINER (button_box), cancel_btn);

    /* 2. ok button create */

    GtkWidget *ok_btn = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_container_add (GTK_CONTAINER (button_box), ok_btn);

    g_signal_connect_swapped ((gpointer) ok_btn, "clicked", G_CALLBACK (ok_clicked_cb), NULL);
    g_signal_connect_swapped ((gpointer) cancel_btn, "clicked", G_CALLBACK (emulator_deleted_callback), NULL);
}


/**
  @brief    create option window
  @param    parent: option window widget
 */
void create_config_page(GtkWidget *parent)
{
    /* 1. ontop, term, cmd, path frame ui */

    GtkWidget *vbox1;
    vbox1 = gtk_vbox_new (FALSE, 5);
    create_config_frame(vbox1);
    gtk_box_pack_start(GTK_BOX(parent), vbox1, TRUE, TRUE, 0);

    /* 2. save, cancel button */

    GtkWidget *vbox2;
    vbox2 = gtk_vbox_new (FALSE, 5);
    create_config_button(vbox2);
    gtk_box_pack_end(GTK_BOX(parent), vbox2, TRUE, TRUE, 1);

    /* 3. separator line */

    GtkWidget *separator;
    separator= gtk_hseparator_new();
    gtk_box_pack_end(GTK_BOX(parent), separator, FALSE, FALSE, 5);

    add_widget(OPTION_ID, OPTION_CONF_FRAME, vbox1);
    add_widget(OPTION_ID, OPTION_CONF_VBOX, parent);
}


/**
  @brief    show config window
  @param    parent: option window widget
 */
//int show_config_window ()
int show_config_window (GtkWidget *parent)
{
    gchar icon_image[MAXPATH] = {0, };
    const gchar *skin = NULL;

    /* 1. set initial preference entrys */

    set_initial_preference_entrys();

    /* 2. create option window */

    GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    add_window (win, OPTION_ID);

    gtk_window_set_title (GTK_WINDOW (win), _("Emulator Options"));

    skin = get_skin_path();
    if(skin == NULL)
        WARN( "getting icon image path is failed!!\n");
    sprintf(icon_image, "%s/icons/Emulator_20x20.png", skin);
    gtk_window_set_icon_from_file(GTK_WINDOW (win), icon_image, NULL);

    gtk_window_set_default_size(GTK_WINDOW (win), 360, -1);
    gtk_window_set_modal (GTK_WINDOW (win), TRUE);
    gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_CENTER);

    GtkWidget *vbox = vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    /* 2.1 create Preference Notebook */

    create_config_page (vbox);

    //#define _SENSOR_SOCKET
#ifdef _SENSOR_SOCKET
    /* 2.2 create Target Emulation Notebook */

    vbox = create_sub_page(notebook, _("Target Emulation"));
#endif

    if(parent != NULL)
        gtk_window_set_transient_for(GTK_WINDOW (win), GTK_WINDOW (parent));

    gtk_window_set_keep_above(GTK_WINDOW (win), TRUE);
    gtk_widget_show_all(win);

    g_signal_connect(GTK_OBJECT(win), "delete_event", G_CALLBACK(emulator_deleted_callback), NULL);

    /* 3. option window gtk main start */

    gtk_main();

    return 0;
}


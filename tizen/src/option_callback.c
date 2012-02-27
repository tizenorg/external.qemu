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


#include "option.h" //-> included #include "option_callback.h"

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, option_callback);


/**
    @brief  previewin option dialog
    @param  file_chooser_button: file chooser for selecting dbi file
    @param  entry
*/
void sdcard_select_cb (GtkButton *file_chooser_button, gpointer data)
{
    GtkWidget *file_chooser_dialog;
    file_chooser_dialog = gtk_file_chooser_dialog_new ("Select SD Card Image",
                            NULL,
                            GTK_FILE_CHOOSER_ACTION_OPEN,
                            GTK_STOCK_CANCEL,
                            GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OPEN,
                            GTK_RESPONSE_ACCEPT,
                            NULL);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (file_chooser_dialog), preference_entrys.qemu_configuration.sdcard_path);

    gchar *sdcard_img_name;
    if (gtk_dialog_run (GTK_DIALOG (file_chooser_dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(file_chooser_dialog));
        sdcard_img_name = basename(path);
        gtk_button_set_label(file_chooser_button, sdcard_img_name);
#ifndef _WIN32
        snprintf(preference_entrys.qemu_configuration.sdcard_path, MAXBUF, "%s", path);
#else
        gchar *changed_path = change_path_to_slash(path);
        snprintf(preference_entrys.qemu_configuration.sdcard_path, MAXBUF, "%s", changed_path);
        if (changed_path)
            g_free(changed_path);
#endif
    }

    gtk_widget_destroy (file_chooser_dialog);

}


/**
    @brief  get name of main image in dbi file
    @param  filename: dbi file for parsing
    @return filename of main image in dbi
*/
gchar* get_png_from_dbi(gchar* filename)
{
    //xmlChar *main_image;
    gchar* image_file = NULL;

    xmlDocPtr   doc;
    doc = xmlParseFile(filename);
    if (doc == NULL) {
        ERR( "Can't parse XML : %s \n", filename);
        return NULL;
    }

    xmlNode *root = NULL;
    root = xmlDocGetRootElement(doc);
    if(!root || !root->name || xmlStrcmp(root->name, (const xmlChar *)"device_info")) {
        xmlFreeDoc(doc);
        return NULL;
    }

    xmlNode *cur_node, *child_node, *region_node, *image_node;
    for(cur_node = root->children; cur_node != NULL; cur_node = cur_node->next)
        if(cur_node->type == XML_ELEMENT_NODE  && !xmlStrcmp(cur_node->name, (const xmlChar *) "mode_section")) {
            //mode_section
            for(child_node = cur_node->children; child_node != NULL; child_node = child_node->next)
                if(child_node->type == XML_ELEMENT_NODE  && !xmlStrcmp(child_node->name, (const xmlChar *) "mode" ))
                    for(region_node = child_node->children;  region_node != NULL; region_node = region_node->next)
                         if(region_node->type == XML_ELEMENT_NODE  && !xmlStrcmp(region_node->name, (const xmlChar *)"image_list"))
                            for(image_node = region_node->children; image_node != NULL; image_node = image_node->next)
                                if(image_node->type == XML_ELEMENT_NODE && !xmlStrcmp(image_node->name, (const xmlChar *)"main_image")) {
                                    image_file = (char *) xmlNodeGetContent(image_node);
                                    goto Found;
                                }
        }

    Found:
    xmlFreeDoc(doc);
    /* xmlCleanupParser not working normally on window os by cosmos in 20091112 */
#ifndef _WIN32
    xmlCleanupParser();
#endif

    return image_file;
}

/**
    @brief  default cb
    @return void
*/
void default_clicked_cb(GtkWidget *widget, gpointer entry)
{
    fill_configuration(CONF_DEFAULT_MODE);

    preference_entrys.cmd_type = configuration.cmd_type;

    GtkWidget *vbox= get_widget(OPTION_ID, OPTION_CONF_FRAME);
    GtkWidget *conf_vbox = get_widget(OPTION_ID, OPTION_CONF_VBOX);

    gtk_widget_destroy(vbox);
    create_config_frame(vbox);

    gtk_box_pack_start(GTK_BOX(conf_vbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show_all(conf_vbox);
}

void virtual_target_select_cb(GtkComboBox *virtual_target_combobox, gpointer data)
{
    int status = 0;
    char *name, *path;
    char *info_file;
    char snapshot_date_str[MAXBUF];

//  GtkWidget *virtual_target_combobox = get_widget(OPTION_ID, OPTION_VIRTUAL_TARGET_COMBOBOX);

    name = (char *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(virtual_target_combobox));

    status = read_virtual_target_info_file(name, &virtual_target_info);
    if(status == -1)
    {
        ERR( "load target info file error\n");
        show_message("Error", "Target info file is missing!\n");
        return;
    }
    else
    {
        snprintf(SYSTEMINFO.virtual_target_name, MAXBUF, "%s", name);
        path = get_virtual_target_path(name);
        info_file = g_strdup_printf("%sconfig.ini", path);
        snprintf(SYSTEMINFO.virtual_target_info_file, MAXPATH, "%s", info_file);
    }

    GtkWidget *snapshot_boot = get_widget(OPTION_ID, OPTION_SNAPSHOT_BOOT);
    GtkWidget *snapshot_saved_date_entry = get_widget(OPTION_ID, OPTION_SNAPSHOT_SAVED_DATE_ENTRY);
    if(virtual_target_info.snapshot_saved == 0)
    {
        snprintf(snapshot_date_str, MAXBUF, "no snapshot saved");
        gtk_toggle_button_set_active((GtkToggleButton *)snapshot_boot, FALSE);
        gtk_widget_set_sensitive(snapshot_boot, FALSE);
        preference_entrys.qemu_configuration.save_emulator_state = 0;
    }
    else
    {
        snprintf(snapshot_date_str, MAXBUF, "saved at %s", virtual_target_info.snapshot_saved_date);
        gtk_toggle_button_set_active((GtkToggleButton *)snapshot_boot, FALSE);
        gtk_widget_set_sensitive(snapshot_boot, TRUE);
    }
    gtk_entry_set_text(GTK_ENTRY(snapshot_saved_date_entry), snapshot_date_str);

    g_free(name);
    g_free(path);
    g_free(info_file);
}
/*
void scale_select_cb(GtkWidget *widget, gpointer data)
{
    GtkToggleButton *toggled_button = GTK_TOGGLE_BUTTON(data);

    if(gtk_toggle_button_get_active(toggled_button) == TRUE)
    {
        if(strcmp(gtk_button_get_label(GTK_BUTTON(toggled_button)), "1/2x") == 0)
            preference_entrys.scale = 2;
        else
            preference_entrys.scale = 1;
    }
}
*/

/**
    @brief  set telnet status active by telnet_port
    @param  void
    @return void
*/
void set_telnet_status_active_cb(void)
{
    gboolean active = 0;

    GtkWidget *telnet_button = get_widget(OPTION_ID, OPTION_TELNET_BUTTON);
    active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (telnet_button));

    preference_entrys.qemu_configuration.telnet_type = active;

    GtkWidget *telnet_port_entry = get_widget(OPTION_ID, OPTION_TELNET_PORT_ENTRY);
    GtkWidget *serial_console_button = get_widget(OPTION_ID, OPTION_SERIAL_CONSOLE_BUTTON);
    GtkWidget *serial_console_entry = get_widget(OPTION_ID, OPTION_SERIAL_CONSOLE_ENTRY);
    gtk_widget_set_sensitive (telnet_port_entry, active);
    gtk_widget_set_sensitive (serial_console_button, active);

    active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (serial_console_button));

    if (active)
        gtk_widget_set_sensitive (serial_console_entry, active);
    else
        gtk_widget_set_sensitive (serial_console_entry, FALSE);
}

/**
    @brief  set sdcard status active
    @param  void
    @return void
*/
void set_sdcard_status_active_cb(void)
{
    gboolean active = 0;

    GtkWidget *sdcard_button = get_widget(OPTION_ID, OPTION_SDCARD_BUTTON);
    active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sdcard_button));

    preference_entrys.qemu_configuration.sdcard_type = active;

    GtkWidget *sdcard_img_button = get_widget(OPTION_ID, OPTION_SDCARD_IMG_BUTTON);

    gtk_widget_set_sensitive (sdcard_img_button, active);
}


void serial_console_command_cb(void)
{
    gboolean active = 0;

    GtkWidget *serial_console_button = get_widget(OPTION_ID, OPTION_SERIAL_CONSOLE_BUTTON);
    active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (serial_console_button));


    preference_entrys.qemu_configuration.serial_console_command_type = active;

    GtkWidget *serial_console_entry = get_widget(OPTION_ID, OPTION_SERIAL_CONSOLE_ENTRY);

    gtk_widget_set_sensitive (serial_console_entry, active);
}


/**
    @brief  callback when use_host_proxy clicked in option window
    @param  void
    @return void
    @date   2009/11/17
*/
void use_host_proxy_cb(void)
{
    preference_entrys.qemu_configuration.use_host_http_proxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(get_widget(OPTION_ID, OPTION_USE_HOST_PROXY)));
}


/**
    @brief  callback when use_host_DNS clicked in option window
    @param  void
    @return void
    @date   2009/11/17
*/
void use_host_DNS_cb(void)
{
    preference_entrys.qemu_configuration.use_host_dns_server = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(get_widget(OPTION_ID, OPTION_USE_HOST_DNS)));
}


void always_on_top_cb(GtkWidget *widget, gpointer data)
{
    preference_entrys.always_on_top =
        gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(data));
    set_config_type(SYSTEMINFO.virtual_target_info_file, COMMON_GROUP, ALWAYS_ON_TOP_KEY, preference_entrys.always_on_top);

    configuration.always_on_top = preference_entrys.always_on_top;
    GtkWidget *win = get_window(EMULATOR_ID);
    if(win != NULL)
        gtk_window_set_keep_above(GTK_WINDOW (win), configuration.always_on_top);
}


/**
    @brief  callback when snapshot_boot clicked in option window
    @param  void
    @return void
    @date   2010/11/24
*/
void snapshot_boot_cb(void)
{
    preference_entrys.qemu_configuration.save_emulator_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(get_widget(OPTION_ID, OPTION_SNAPSHOT_BOOT)));
}


/**
 * @brief   callback when ok button clicked in option window
 * @param   widget: OK Button
 * @param   entry
 * @date    Apr 21. 2009
 * */
int ok_clicked_cb(GtkWidget *widget, gpointer entry)
{
    GtkWidget *option_win = get_window(OPTION_ID);

#if 0 // serial setting will not be supported. below code will be removed later
//  char *message_buf;

    /* 2. Serial Telnet Set */

    configuration.qemu_configuration.telnet_type = preference_entrys.qemu_configuration.telnet_type;
    configuration.qemu_configuration.serial_console_command_type = preference_entrys.qemu_configuration.serial_console_command_type;

    if (configuration.qemu_configuration.telnet_type == 0) {
        memset(configuration.qemu_configuration.telnet_port, 0x00, MAXBUF);
    }
    else if (configuration.qemu_configuration.telnet_type == 1) {
        GtkWidget *telnet_port_entry = get_widget(OPTION_ID, OPTION_TELNET_PORT_ENTRY);
        message_buf = (char *)gtk_entry_get_text(GTK_ENTRY(telnet_port_entry));
        snprintf(configuration.qemu_configuration.telnet_port, MAXBUF, "%s", message_buf);

        GtkWidget *serial_console_entry = get_widget(OPTION_ID, OPTION_SERIAL_CONSOLE_ENTRY);

        /* always save by okdear */
        message_buf = (char *)gtk_entry_get_text(GTK_ENTRY(serial_console_entry));
        if (strlen(message_buf) > 0)
            snprintf(configuration.qemu_configuration.serial_console_command, MAXBUF, "%s", message_buf);
    }
#endif

    // sd card option and snapshot boot option cannot be selected at the same time.
    if(virtual_target_info.sdcard_type != 0 && preference_entrys.qemu_configuration.save_emulator_state == 1)
    {
        show_message("Warning", "When virtual target has SD Card, Snapshot Boot option cannot be selected!");
        return 0;
    }


    /* 3. SD Card Set */

//  configuration.qemu_configuration.sdcard_type = preference_entrys.qemu_configuration.sdcard_type;

//  if (configuration.qemu_configuration.sdcard_type == 0) {
//      memset(configuration.qemu_configuration.sdcard_path, 0x00, MAXBUF);
//  }
//  else if (configuration.qemu_configuration.sdcard_type == 1) {
//      snprintf(configuration.qemu_configuration.sdcard_path, MAXBUF, "%s", preference_entrys.qemu_configuration.sdcard_path);
//  }

    /* 6. Network ip setting (proxy, dns server) */

    configuration.qemu_configuration.use_host_http_proxy = preference_entrys.qemu_configuration.use_host_http_proxy;
    configuration.qemu_configuration.use_host_dns_server = preference_entrys.qemu_configuration.use_host_dns_server;

    configuration.always_on_top = preference_entrys.always_on_top;
    GtkWidget *win = get_window(EMULATOR_ID);
    if(win != NULL)
        gtk_window_set_keep_above(GTK_WINDOW (win), configuration.always_on_top);

    /* 6.1 sanpshot boot option setting */
    configuration.qemu_configuration.save_emulator_state = preference_entrys.qemu_configuration.save_emulator_state;

    /* 7. Disk Image path select */

    configuration.qemu_configuration.diskimg_type = preference_entrys.qemu_configuration.diskimg_type;

    if (configuration.qemu_configuration.diskimg_type == 0) {
        memset(configuration.qemu_configuration.diskimg_path, 0x00, MAXBUF);
    }
    else if (configuration.qemu_configuration.diskimg_type == 1) {
        snprintf(configuration.qemu_configuration.diskimg_path, MAXBUF, "%s", preference_entrys.qemu_configuration.diskimg_path);
    }

    /* 19 option window widget destory */

    gtk_widget_destroy(option_win);

    /* 10. go to main fuction */

    gtk_main_quit();

    return 0;
}


/**
    @brief  in initial option
    @return true: for success
*/

gboolean emulator_deleted_callback (void)
{
    GtkWidget *win = NULL;
    GtkWidget *win1 = NULL;
    win = get_window(OPTION_ID);
    win1 = get_window(EMULATOR_ID);

    gtk_widget_destroy(win);

    if (win1 == NULL) {
        gtk_main_quit ();
        exit(0);
    }

    return TRUE;
}

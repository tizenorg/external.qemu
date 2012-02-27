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

#include "menu.h"
#include "debug_ch.h"
#include "sdb.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, menu);


/**
  * @brief  create popup advanced menu
  * @return void
  */
static void create_popup_advanced_menu(GtkWidget **pMenu, PHONEMODELINFO *device, CONFIGURATION *pconfiguration)
{
    GtkWidget *Item = NULL;
    GtkWidget *image_widget = NULL;
    GtkWidget *SubMenuItem = NULL;
    GtkWidget *SubMenuItem1 = NULL;
    GtkWidget *menu_item = NULL;
    GSList *pGroup = NULL;
    gchar icon_image[MAXPATH] = {0, };
    const gchar *skin_path;
    gchar *keyboard_menu[2] = {"On", "Off"};
    int i = 0;

    skin_path = get_skin_path();
    if (skin_path == NULL) {
        WARN("getting icon image path is failed!!\n");
    }

    /* 5. advanced */
    Item = gtk_image_menu_item_new_with_label(_("Advanced"));
    sprintf(icon_image, "%s/icons/02_ADVANCED.png", skin_path);
    image_widget = gtk_image_new_from_file (icon_image);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(Item), image_widget);
    if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(Item), TRUE);
    }
    gtk_container_add(GTK_CONTAINER(*pMenu), Item);

    /* submenu items */
    SubMenuItem = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(Item), SubMenuItem);

    /* 5.1 event injector menu */
    if (configuration.enable_telephony_emulator) {
        menu_item = gtk_image_menu_item_new_with_label(_("Telephony Emulator"));
        sprintf(icon_image, "%s/icons/03_TELEPHONY-eMULATOR.png", skin_path);
        image_widget = gtk_image_new_from_file (icon_image);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image_widget);
        if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
            gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
        }

        gtk_widget_set_tooltip_text(menu_item, "Emulate receiving and sending calls, SMSs, etc");
        gtk_container_add(GTK_CONTAINER(SubMenuItem), menu_item);
        if (UISTATE.is_ei_run == FALSE) {
            g_object_set(menu_item, "sensitive", TRUE, NULL);
        }

        g_signal_connect(menu_item, "activate", G_CALLBACK(menu_create_eiwidget_callback), menu_item);
        add_widget(EMULATOR_ID, MENU_EVENT_INJECTOR, menu_item);
        gtk_widget_show(menu_item);
    }

#ifdef ENABLE_TEST_EI
    menu_create_eiwidget_callback(NULL, menu_item);
#endif

    /* SaveVM Menu */
#if 0
    GtkWidget *savevm_menu_item = gtk_image_menu_item_new_with_label(_("Save Emulator State"));
    sprintf(icon_image, "%s/icons/05_GPS.png", skin_path);
    image_widget = gtk_image_new_from_file (icon_image);

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(savevm_menu_item), image_widget);
    if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(savevm_menu_item), TRUE);
    }

    if (UISTATE.is_gps_run == FALSE) {
        g_object_set(savevm_menu_item, "sensitive", TRUE, NULL);
    }

    //g_signal_connect(gps_menu_item, "activate", G_CALLBACK(menu_create_gps), NULL);
    g_signal_connect(savevm_menu_item, "activate", G_CALLBACK(save_emulator_state), NULL);

    gtk_container_add(GTK_CONTAINER(SubMenuItem), savevm_menu_item);
    add_widget(EMULATOR_ID, MENU_GPS, savevm_menu_item);
    gtk_widget_show(savevm_menu_item);
#endif

    /* 5.2 shell menu */
    if (configuration.enable_shell) {
        menu_item = gtk_image_menu_item_new_with_label(_("Shell"));
        sprintf(icon_image, "%s/icons/01_SHELL.png", skin_path);
        image_widget = gtk_image_new_from_file (icon_image);

        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image_widget);
        if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
            gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
        }
        gtk_widget_set_tooltip_text(menu_item, _("Run sdb shell"));
        g_signal_connect(menu_item, "activate", G_CALLBACK(create_cmdwindow), NULL);
        gtk_container_add(GTK_CONTAINER(SubMenuItem), menu_item);
        gtk_widget_show(menu_item);
    }

    /* 5.3 screen shot menu of advanced */
    menu_item = gtk_image_menu_item_new_with_label(_("Screen Shot"));
    sprintf(icon_image, "%s/icons/06_SCREEN-SHOT.png", skin_path);
    image_widget = gtk_image_new_from_file (icon_image);

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image_widget);
    if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
    }
    gtk_widget_set_tooltip_text(menu_item, "Capture and Save the present Screen Shot ");
    g_signal_connect(menu_item, "activate", G_CALLBACK(frame_buffer_handler), NULL);
    gtk_container_add(GTK_CONTAINER(SubMenuItem), menu_item);
    gtk_widget_show(menu_item);

    /* 5.4 USB keyboard menu */
    menu_item = gtk_image_menu_item_new_with_label(_("USB Keyboard"));
    sprintf(icon_image, "%s/icons/04_KEYPAD.png", skin_path);
    image_widget = gtk_image_new_from_file (icon_image);

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image_widget);
    if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
    }
    gtk_container_add(GTK_CONTAINER(SubMenuItem), menu_item);
    gtk_widget_show(menu_item);

    SubMenuItem1 = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), SubMenuItem1);

    for(i = 0; i < 2; i++)
    {
        menu_item = gtk_radio_menu_item_new_with_label(pGroup, keyboard_menu[i]);
        pGroup = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));

        gtk_container_add(GTK_CONTAINER(SubMenuItem1), menu_item);

        g_signal_connect(menu_item, "activate", G_CALLBACK(menu_keyboard_callback), keyboard_menu[i]);
        gtk_widget_show(menu_item);
    }

    /* 5.5 about menu */
    menu_item = gtk_image_menu_item_new_with_label(_("About"));
    sprintf(icon_image, "%s/icons/13_ABOUT.png", skin_path);
    image_widget = gtk_image_new_from_file (icon_image);

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image_widget);
    if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
    }

    g_signal_connect(menu_item, "activate", G_CALLBACK(menu_about_callback), NULL);
    gtk_widget_set_tooltip_text(menu_item, "Show license and version information");
    gtk_container_add(GTK_CONTAINER(SubMenuItem), menu_item);
    gtk_widget_show(menu_item);

    gtk_widget_show(Item);
}


/**
  * @brief  create popup menu
  * @return void
  */
void create_popup_menu(GtkWidget **pMenu, PHONEMODELINFO *device, CONFIGURATION *pconfiguration)
{
    GtkWidget *Item = NULL;
    GtkWidget *menu_item = NULL;
    GtkWidget *image_widget = NULL;
    GtkWidget *SubMenuItem = NULL;
    GSList *pGroup = NULL;
    gchar icon_image[MAXPATH] = {0, };
    const gchar *skin_path;
    char *emul_name = NULL;
    int i, j = 0;

    *pMenu = gtk_menu_new();
    add_widget(EMULATOR_ID, POPUP_MENU, *pMenu);

    skin_path = get_skin_path();
    if (skin_path == NULL) {
        WARN("getting icon image path is failed!!\n");
    }

    /* 1. emulator info menu */
    emul_name = g_strdup_printf("emulator-%d", get_sdb_base_port());
    Item = gtk_image_menu_item_new_with_label(_(emul_name));
    sprintf(icon_image, "%s/icons/Emulator_20x20.png", skin_path);
    image_widget = gtk_image_new_from_file (icon_image);

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(Item), image_widget);
    if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(Item), TRUE);
    }
    gtk_widget_set_tooltip_text(Item, _("Show Emulator infomation"));
    g_signal_connect(Item, "activate", G_CALLBACK(show_info_window), (gpointer*)startup_option.vtm);

    gtk_container_add(GTK_CONTAINER(*pMenu), Item);
    gtk_widget_show(Item);
    g_free(emul_name);

    MENU_ADD_SEPARTOR(*pMenu);

    /* 3. always on top  menu */
    Item = gtk_check_menu_item_new_with_label(_("Always On Top"));
    g_signal_connect(Item, "toggled", G_CALLBACK(always_on_top_cb), Item);
    int always_on_top = get_config_type(SYSTEMINFO.virtual_target_info_file, COMMON_GROUP, ALWAYS_ON_TOP_KEY);
    if (always_on_top == 1) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(Item), TRUE);
    } else {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(Item), FALSE);
    }

#ifdef _WIN32
    g_object_set_data((GObject *) *pMenu, "always_on_top", (GObject *) Item);
#endif

    gtk_widget_set_tooltip_text(Item, "Set keep above this window or not");
    gtk_container_add(GTK_CONTAINER(*pMenu), Item);
    gtk_widget_show(Item);

    /* 4. event for dbi file */
    if (device->event_menu_cnt > 0) {

        /* submenu items */
        for (i = 0; i < device->event_menu_cnt; i++) {

            Item = gtk_image_menu_item_new_with_label(device->event_menu[i].name);
            if (i == 0) {
                sprintf(icon_image, "%s/icons/09_ROTATE.png", skin_path);
            } else if (i == 1) {
                sprintf(icon_image, "%s/icons/10_PROPERTIES.png", skin_path);
            }

            image_widget = gtk_image_new_from_file (icon_image);

            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(Item), image_widget);
            if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
                gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(Item), TRUE);
            }
            gtk_container_add(GTK_CONTAINER(*pMenu), Item);
            gtk_widget_show(Item);

            SubMenuItem = gtk_menu_new();
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(Item), SubMenuItem);

            pGroup = NULL;
            for (j = 0; j < device->event_menu[i].event_list_cnt; j++) {
                Item = gtk_radio_menu_item_new_with_label(pGroup, device->event_menu[i].event_list[j].event_eid);
                pGroup = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(Item));

                if (g_strcmp0(device->event_menu[i].name, "Scale") == 0 &&
                    UISTATE.scale == atof(device->event_menu[i].event_list[j].event_evalue))
                {
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(Item), TRUE);
                }

                gtk_container_add(GTK_CONTAINER(SubMenuItem), Item);

                g_signal_connect(Item, "activate", G_CALLBACK(menu_event_callback), device->event_menu[i].event_list[j].event_evalue);
                gtk_widget_show(Item);

                g_object_set_data((GObject *) *pMenu, device->event_menu[i].event_list[j].event_evalue, (GObject *) Item);
                g_object_set(Item, "name", device->event_menu[i].event_list[j].event_evalue, NULL);
            }
        }
    }

    MENU_ADD_SEPARTOR(*pMenu);

    /* 5. advanced menu */
    create_popup_advanced_menu(pMenu, device, pconfiguration);
    //create_popup_properties_menu(pMenu, pconfiguration);

    /* 6. exit menu */
    Item = gtk_image_menu_item_new_with_label(_("Close"));
    sprintf(icon_image, "%s/icons/14_CLOSE.png", skin_path);
    image_widget = gtk_image_new_from_file (icon_image);

    //gtk_widget_add_accelerator (Item, "activate", group, GDK_C,
    //    GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(Item), image_widget);
    if (GTK_MAJOR_VERSION >=2 && GTK_MINOR_VERSION >= 16) {
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM(Item), TRUE);
    }

    g_signal_connect(Item, "activate", G_CALLBACK(exit_emulator), NULL);
    gtk_widget_set_tooltip_text(Item, "Exit Emulator");
    gtk_container_add(GTK_CONTAINER(*pMenu), Item);
    gtk_widget_show(Item);
}


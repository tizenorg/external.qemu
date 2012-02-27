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
    @file   dialog.c
    @brief  miscellaneous functions
*/

#include "dialog.h"
#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, dialog);

void show_message(const char *szTitle, const char *szMessage)
{
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *dialog_window;
    /* create Dialog */

    dialog_window = gtk_dialog_new();

    /* Widget Icon set */

    GdkPixbuf* tmppixbuf = NULL;
    tmppixbuf=gtk_widget_render_icon((GtkWidget *) (dialog_window), GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU, NULL);
    gtk_window_set_icon(GTK_WINDOW (dialog_window), tmppixbuf);

    /* set Title and Border */

    gtk_window_set_title(GTK_WINDOW(dialog_window), szTitle);
    gtk_container_set_border_width(GTK_CONTAINER(dialog_window), 0);

    /* set dialog not resizble */

    gtk_window_set_resizable(GTK_WINDOW(dialog_window), FALSE);

    /* create OK Button and Set Reponse */

    button = gtk_dialog_add_button(GTK_DIALOG(dialog_window), GTK_STOCK_OK, GTK_RESPONSE_OK);

    /* set OK Button to Default Button */
#if GTK_CHECK_VERSION(2,20,0)
    gtk_widget_set_can_default(button, TRUE);
#else
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
#endif
    gtk_widget_grab_default(button);

    /* create Label */

    label = gtk_label_new(szMessage);

    /* set Padding arround Label */

    gtk_misc_set_padding(GTK_MISC(label), 10, 10);

    /* pack Label to Dialog */

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->vbox), label, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog_window);

    gtk_dialog_run(GTK_DIALOG(dialog_window));

    gtk_widget_destroy(dialog_window);
}

/**
 * @brief   show sized dialog
 * @param   dialog title
 * @param   dialog message
 * @date    Nov 21. 2008
 * */

void show_sized_message(const char *szTitle, const char *szMessage, const int maxlength)
{
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *dialog_window;
    /* create Dialog */

    dialog_window = gtk_dialog_new();

    /* Widget Icon set */

    GdkPixbuf* tmppixbuf = NULL;
    tmppixbuf=gtk_widget_render_icon((GtkWidget *) (dialog_window), GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU, NULL);
    gtk_window_set_icon(GTK_WINDOW (dialog_window), tmppixbuf);

    /* set Title and Border */

    gtk_window_set_title(GTK_WINDOW(dialog_window), szTitle);
    gtk_container_set_border_width(GTK_CONTAINER(dialog_window), 0);

    /* set dialog not resizble */

    gtk_window_set_resizable(GTK_WINDOW(dialog_window), TRUE);

    /* create OK Button and Set Reponse */

    button = gtk_dialog_add_button(GTK_DIALOG(dialog_window), GTK_STOCK_OK, GTK_RESPONSE_OK);

    /* set OK Button to Default Button */
#if GTK_CHECK_VERSION(2,20,0)
    gtk_widget_set_can_default(button, TRUE);
#else
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
#endif
    gtk_widget_grab_default(button);

    /* create Label */

    label = gtk_label_new(szMessage);

    /* set line wrap mode */

//  gtk_label_set_width_chars(GTK_LABEL(label), 30);
    gtk_label_set_max_width_chars(GTK_LABEL(label), maxlength);
    gtk_label_set_line_wrap(GTK_LABEL(label),TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_CHAR);
    gtk_label_set_ellipsize(GTK_LABEL(label),PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(label),szMessage);
    /* set Padding arround Label */

    gtk_misc_set_padding(GTK_MISC(label), 10, 10);

    /* pack Label to Dialog */

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->vbox), label, TRUE, TRUE, 0);


    gtk_widget_show_all(dialog_window);

    gtk_dialog_run(GTK_DIALOG(dialog_window));

    gtk_widget_destroy(dialog_window);
}

gboolean show_ok_cancel_message(const char *szTitle, const char *szMessage)
{
    GtkWidget *label;
    GtkWidget *button_ok;
    GtkWidget *button_cancel;
    GtkWidget *dialog_window;

    /* create Dialog */

    dialog_window = gtk_dialog_new();

    /* Widget Icon set */

    GdkPixbuf* tmppixbuf = NULL;
    tmppixbuf=gtk_widget_render_icon((GtkWidget *) (dialog_window), GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU, NULL);
    gtk_window_set_icon(GTK_WINDOW (dialog_window), tmppixbuf);

    /* set Title and Border */

    gtk_window_set_title(GTK_WINDOW(dialog_window), szTitle);
    gtk_container_set_border_width(GTK_CONTAINER(dialog_window), 0);

    /* set dialog not resizble */

    gtk_window_set_resizable(GTK_WINDOW(dialog_window), FALSE);

    /* create OK Button and Set Reponse */

    button_ok = gtk_dialog_add_button(GTK_DIALOG(dialog_window), GTK_STOCK_OK, GTK_RESPONSE_OK);
    button_cancel = gtk_dialog_add_button(GTK_DIALOG(dialog_window), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

    /* set OK Button to Default Button */
#if GTK_CHECK_VERSION(2,20,0)
    gtk_widget_set_can_default(button_cancel, TRUE);
#else
    GTK_WIDGET_SET_FLAGS(button_cancel, GTK_CAN_DEFAULT);
#endif
    gtk_widget_grab_default(button_cancel);

    /* create Label */

    label = gtk_label_new(szMessage);

    /* set Padding arround Label */

    gtk_misc_set_padding(GTK_MISC(label), 10, 10);

    /* pack Label to Dialog */

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->vbox), label, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog_window);

    gint result = gtk_dialog_run(GTK_DIALOG(dialog_window));

    gtk_widget_destroy(dialog_window);

    switch (result)
    {
    case GTK_RESPONSE_OK:
        return TRUE;
    default:
        return FALSE;
    }

}


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
 * @file tools.c
 * @brief - implementation file for utility like skin parser, PLAY, REC, etc.
 */


#include "tools.h"
#include <glib.h>

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, tools);

#define MAX_FILENAME_LENGTH 64

/** 
  * @brief  get the filename from filebrowser
  * @param  str_title: titlebar name of filebrowser
  * @param  str_folder: if the folder is NULL, show the DEFAULT folder
  * @param  chooser_type: the way of save, open, folder open is same that of GtkFileChooserAction
  * @param  type: type of file dialog
  * @return success : choosen filename      
  *         failure : NULL
  */
char *get_file_name(char *str_title, char *str_folder, GtkFileChooserAction chooser_type, GtkFileFilter *file_filter, int type)
{
    GtkWidget *filew = NULL;
    char *filename = NULL;
    time_t rawtime;
    struct tm *timeinfo;
    char save_file_subname[MAX_FILENAME_LENGTH];
    char save_file_fullname[MAX_FILENAME_LENGTH];

    if (str_title == NULL) 
        return NULL;
    
    // after GTK 2.4 version, change to the gtk_file_chooser_dialog
    if (chooser_type == GTK_FILE_CHOOSER_ACTION_OPEN || chooser_type == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER || chooser_type == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER) 
        filew = gtk_file_chooser_dialog_new(str_title, NULL, chooser_type, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    else if (chooser_type == GTK_FILE_CHOOSER_ACTION_SAVE) 
        filew = gtk_file_chooser_dialog_new(str_title, NULL, chooser_type, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
    else 
        return NULL;

#if 0
    GdkPixbuf* tmppixbuf = NULL;
    tmppixbuf=gtk_widget_render_icon((GtkWidget *) (filew), GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_MENU, NULL);
    gtk_window_set_icon(GTK_WINDOW (filew), tmppixbuf);
#else
    gchar icon_image[MAXPATH] = {0, };

    const gchar *skin = get_skin_path();
    if (skin == NULL){
        WARN("getting icon image path is failed!!\n");
    }

    sprintf(icon_image, "%s/icons/07_EXECUTE_APP.png", skin);
    gtk_window_set_icon_from_file (GTK_WINDOW(filew), icon_image, NULL);
#endif

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(filew), TRUE);

    rawtime = time(NULL);
    timeinfo = localtime(&rawtime);
    strftime(save_file_subname, MAX_FILENAME_LENGTH, "-%Y-%m-%d-%H%M%S.png", timeinfo);

    if (strlen(startup_option.vtm) == 0 || MAX_FILENAME_LENGTH - (strlen(save_file_subname) + 1) < strlen(startup_option.vtm)) {
        snprintf(save_file_fullname, MAX_FILENAME_LENGTH - 1, "emulator%s", save_file_subname);
    } else {
        snprintf(save_file_fullname, MAX_FILENAME_LENGTH - 1, "%s%s", startup_option.vtm, save_file_subname);
    }

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filew), save_file_fullname);
    
    if (str_folder == NULL || strlen(str_folder) == 0) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filew), configuration.target_path);
    } else {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filew), str_folder);
    }

    // FileFilter
    if (file_filter != NULL) {
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filew), file_filter);

        {
            GtkFileFilter *file_filter = NULL;
            file_filter = gtk_file_filter_new();
            gtk_file_filter_set_name(file_filter, "All Files");
            gtk_file_filter_add_pattern(file_filter, "*");
            gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filew), file_filter);
        }
    }

    gtk_widget_show(filew);

    if (gtk_dialog_run(GTK_DIALOG(filew)) == GTK_RESPONSE_ACCEPT) {
        
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filew));

    }
    else {
        gtk_widget_destroy(filew);
        return NULL;
    }
    gtk_widget_destroy(filew);

    return filename;
}


/**
  * @brief  parse the skin info after getting the skin info file
  * @param  file_name: skin info filename
  * @param  pSkinData: point to structure saving the skin data
  * @return         success : 0
  *         failure : a) INVALID        : wrong argument value
  *               b) PARSE_ERROR    : parsing failure
  *                   c) the rest negative number is same the errno
  */
int skin_parser(char *file_name, PHONEMODELINFO *device)
{
    memset(device, 0x00, sizeof(PHONEMODELINFO));
    if (parse_dbi_file((gchar *) file_name, device) != 1) 
        return PARSE_ERROR;

    return 0;
}

/**
  * @brief  check in which region the coordinate is by getting the coordinate
  * @param  x: X coordinate
  * @param  y: Y coordinate
  * @param  pSkinData: pointer to structure containg region data
  * @return     success : region code   failure : -1
  */
int check_region_button(int x, int y, PHONEMODELINFO *device)
{
    int i = 0;
    if (UISTATE.last_index != -1) {
        if (INSIDE(x, y, PHONE.mode[UISTATE.current_mode].key_map_list[UISTATE.last_index].key_map_region, UISTATE.scale) == TRUE) {
            return UISTATE.last_index;
        }
    }

    for (i = 0; i < PHONE.mode[UISTATE.current_mode].key_map_list_cnt; i++) {
        if (INSIDE(x, y, PHONE.mode[UISTATE.current_mode].key_map_list[i].key_map_region, UISTATE.scale) == TRUE) {
            UISTATE.last_index = i;
            return UISTATE.last_index;
        }
    }   

    return NON_BUTTON_REGION;
}

/** 
  * @brief  analyze in which position some unique pointer is by getting the device info
  * @param  x: X coordinate
  * @param  y: Y coordinate
  * @param  device: pointer to structure containg device info
  * @return success : DUAL_LCD_REGION(2), LCD_REGION(1)  failure : NON_LCD_REGION(0)
  */
int check_region_lcd(int x, int y, PHONEMODELINFO * device)
{
    int i = 0;
    for (i = 0; i < device->mode[UISTATE.current_mode].lcd_list_cnt; i++) {
#if 0
        if(device->dual_display == 1){
            extern int intermediate_section;
            int curr_rotation = UISTATE.current_mode;

            /* 0 */
            if(curr_rotation == 0){
                int value = (device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.x * UISTATE.scale) +
                    ((int)(device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.w * UISTATE.scale)/2);
                /* If its within the middle bar then its not a LCD region */
                if(x >= value && x < (value + intermediate_section)){
                    return NON_LCD_REGION;
                }

                if (INSIDE_LCD_0_180(x, y, device->mode[UISTATE.current_mode].lcd_list[i].lcd_region) == TRUE) {
                    UISTATE.last_index = i;
                    if (i > 0)      return DUAL_LCD_REGION;
                    else            return LCD_REGION;
                }
            }
            /* 180 */
            else if(curr_rotation == 2){
                int value = (device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.x * UISTATE.scale) +
                    ((int)(device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.w * UISTATE.scale)/2);

                /* If its within the middle bar then its not a LCD region */
                if(x > value && x <= (value + intermediate_section)){
                    return NON_LCD_REGION;
                }

                if (INSIDE_LCD_0_180(x, y, device->mode[UISTATE.current_mode].lcd_list[i].lcd_region) == TRUE) {
                    UISTATE.last_index = i;
                    if (i > 0)      return DUAL_LCD_REGION;
                    else            return LCD_REGION;
                }
            }
            /* 90 */
            else if(curr_rotation == 1){
                int value = (device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.y * UISTATE.scale) +
                    ((int)(device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.h * UISTATE.scale)/2);

                /* If its within the middle bar then its not a LCD region */
                if(y > value && y <= (value + intermediate_section)){
                    return NON_LCD_REGION;
                }

                if (INSIDE_LCD_90(x, y, device->mode[UISTATE.current_mode].lcd_list[i].lcd_region) == TRUE) {
                    UISTATE.last_index = i;
                    if (i > 0)      return DUAL_LCD_REGION;
                    else            return LCD_REGION;
                }
            }
            /* 270 */
            else {
                int value = (device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.y * UISTATE.scale) +
                    ((int)(device->mode[UISTATE.current_mode].lcd_list[i].lcd_region.h * UISTATE.scale)/2);

                /* If its within the middle bar then its not a LCD region */
                if(y > value && y <= (value + intermediate_section)){
                    return NON_LCD_REGION;
                }

                if (INSIDE_LCD_270(x, y, device->mode[UISTATE.current_mode].lcd_list[i].lcd_region) == TRUE) {
                    UISTATE.last_index = i;
                    if (i > 0)      return DUAL_LCD_REGION;
                    else            return LCD_REGION;
                }
            }
        }
        else
        {
#endif
            if (INSIDE_LCD(x, y, device->mode[UISTATE.current_mode].lcd_list[i].lcd_region, UISTATE.scale) == TRUE) {
                UISTATE.last_index = i;
                if (i > 0)      return DUAL_LCD_REGION;
                else            return LCD_REGION;
            }

    }

    return NON_LCD_REGION;
}


/** 
  * @brief  load skin image with the information of parsing result
  * @param  device: pointer to structure containg phone model info
  * @return success : 0 failure : -1
  */
int load_skin_image(PHONEMODELINFO * device)
{
    GError *g_err = NULL;
    int i = 0;

    TRACE( "skin image call\n");

    /*normal mode*/
    for (i = 0; i < device->mode_cnt; i++) {

        device->mode_SkinImg[i].pPixImg = gdk_pixbuf_new_from_file(device->mode[i].image_list.main_image, &g_err);
        TRACE( "image = %s\n", device->mode[i].image_list.main_image);
        if (!device->mode_SkinImg[i].pPixImg) {
            WARN( "Image Generation failed!!=%s\n", device->mode[i].image_list.main_image);
            return -1;
        }

        device->mode_SkinImg[i].nImgWidth = gdk_pixbuf_get_width(device->mode_SkinImg[i].pPixImg);
        device->mode_SkinImg[i].nImgHeight = gdk_pixbuf_get_height(device->mode_SkinImg[i].pPixImg);
        device->mode_SkinImg[i].pPixImg_P = gdk_pixbuf_new_from_file(device->mode[i].image_list.keypressed_image, &g_err);
        if (!device->mode_SkinImg[i].pPixImg_P) {
            g_object_unref(device->mode_SkinImg[i].pPixImg);
            WARN( "Image Generation failed!!\n");
            return -1;
        }
        /*LED*/
        if (device->mode[i].image_list.led_main_image != NULL && strlen(device->mode[i].image_list.led_main_image) != 0) {
            TRACE( "led_main_image   Image Generation  %s \n", device->mode[i].image_list.led_main_image );
            device->mode_SkinImg[i].pPixImgLed = gdk_pixbuf_new_from_file(device->mode[i].image_list.led_main_image, &g_err);

            if (!device->mode_SkinImg[i].pPixImgLed) {
                g_object_unref(device->mode_SkinImg[i].pPixImg);
                g_object_unref(device->mode_SkinImg[i].pPixImg_P);
                WARN( "Image Generation failed!!\n");
                return -1;
            }
        }

        if (device->mode[i].image_list.led_keypressed_image != NULL && strlen(device->mode[i].image_list.led_keypressed_image) != 0) {
            TRACE( "led_keypressed_image Image Generation  %s \n", device->mode[i].image_list.led_keypressed_image);
            device->mode_SkinImg[i].pPixImgLed_P = gdk_pixbuf_new_from_file(device->mode[i].image_list.led_keypressed_image, &g_err);
            if (!device->mode_SkinImg[i].pPixImgLed_P) {
                g_object_unref(device->mode_SkinImg[i].pPixImg);
                g_object_unref(device->mode_SkinImg[i].pPixImg_P);
                g_object_unref(device->mode_SkinImg[i].pPixImgLed);
                WARN( "Image Generation failed!!\n");
                return -1;
            }
        }
    }

    /*cover mode*/
    if (device->cover_mode_cnt == 1) {
        device->cover_mode_SkinImg.pPixImg = gdk_pixbuf_new_from_file(device->cover_mode.image_list.main_image, &g_err);
        if (!device->cover_mode_SkinImg.pPixImg) {
            WARN( "Image Generation failed!!\n");
            return -1;
        }

        device->cover_mode_SkinImg.nImgWidth = gdk_pixbuf_get_width(device->cover_mode_SkinImg.pPixImg);
        device->cover_mode_SkinImg.nImgHeight = gdk_pixbuf_get_height(device->cover_mode_SkinImg.pPixImg);

        device->cover_mode_SkinImg.pPixImg_P = gdk_pixbuf_new_from_file(device->cover_mode.image_list.keypressed_image, &g_err);
        if (!device->cover_mode_SkinImg.pPixImg_P) {
            g_object_unref(device->cover_mode_SkinImg.pPixImg);
            WARN( "Image Generation failed!!\n");
            return -1;
        }
        /*LED*/
        if (device->cover_mode.image_list.led_main_image != NULL && strlen(device->cover_mode.image_list.led_main_image) != 0) {
            device->cover_mode_SkinImg.pPixImgLed = gdk_pixbuf_new_from_file(device->cover_mode.image_list.led_main_image, &g_err);

            if (!device->cover_mode_SkinImg.pPixImgLed) {
                g_object_unref(device->cover_mode_SkinImg.pPixImg);
                g_object_unref(device->cover_mode_SkinImg.pPixImg_P);
                WARN( "Image Generation failed!!\n");
                return -1;
            }
        }

        if (device->cover_mode.image_list.led_keypressed_image != NULL && strlen(device->cover_mode.image_list.led_keypressed_image) != 0) {

            device->cover_mode_SkinImg.pPixImgLed_P = gdk_pixbuf_new_from_file(device->cover_mode.image_list.led_keypressed_image, &g_err);

            if (!device->cover_mode_SkinImg.pPixImgLed_P) {
                g_object_unref(device->cover_mode_SkinImg.pPixImg);
                g_object_unref(device->cover_mode_SkinImg.pPixImg_P);
                g_object_unref(device->cover_mode_SkinImg.pPixImgLed);
                WARN( "Image Generation failed!!\n");
                return -1;
            }
        }
    }

        /* remember the skin image when 1.0 scale */
        for (i = 0; i < MODE_MAX; i++) {
            device->default_SkinImg[i].pPixImg = gdk_pixbuf_copy(device->mode_SkinImg[i].pPixImg);
            device->default_SkinImg[i].pPixImg_P = gdk_pixbuf_copy(device->mode_SkinImg[i].pPixImg_P);
            device->default_SkinImg[i].pPixImgLed = gdk_pixbuf_copy(device->mode_SkinImg[i].pPixImgLed);
            device->default_SkinImg[i].pPixImgLed_P = gdk_pixbuf_copy(device->mode_SkinImg[i].pPixImgLed_P);
        device->default_SkinImg[i].nImgWidth = device->mode_SkinImg[i].nImgWidth;
            device->default_SkinImg[i].nImgHeight = device->mode_SkinImg[i].nImgHeight;
        }

        return 0;
}



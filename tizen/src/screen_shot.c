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


#include "screen_shot.h"
#include "tools.h"

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, screen_shot);

static FBINFO g_fbinfo[MAX_EMULFB]={
    { DRAWWIDTH, DRAWHEIGHT, BPP, 0, FB_SIZE, 0, 0, 0, -1, NULL, 0, NULL },
    { DRAWWIDTH, DRAWHEIGHT, BPP, 0, FB_SIZE, 0, 0, 0, -1, NULL, 0, NULL },
    { DRAWWIDTH, DRAWHEIGHT, BPP, 0, FB_SIZE, 0, 0, 0, -1, NULL, 0, NULL },
};

static GtkWidget *create_frame_buffer(BUF_WIDGET *pWidget);
static gint frame_buffer_motion_notify_event_handler(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gint frame_buffer_motion_no_notify_event_handler(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean frame_buffer_key_press_event_handler(GtkWidget *widget, GdkEventKey *event_type, gpointer data);
static gboolean frame_buffer_key_release_event_handler(GtkWidget *widget, GdkEventKey *event_type, gpointer data);
static gboolean frame_buffer_scroll_event_handler(GtkWidget *widget, GdkEventScroll *event, gpointer data);
static void frame_buffer_destroy_event(GtkWidget *widget, gpointer data);
static void frame_buffer_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data);
static GtkWidget *frame_buffer_scale_window(gpointer *data, int nScale);
static void frame_buffer_magnify_zoomin_callback(GtkAction *action, gpointer data);
static void frame_buffer_magnify_zoomout_callback(GtkAction *action, gpointer data);
static void frame_buffer_magnify1x_callback(GtkAction *action, gpointer data);
static void frame_buffer_magnify2x_callback(GtkAction *action, gpointer data);
static void frame_buffer_magnify3x_callback(GtkAction *action, gpointer data);
static void frame_buffer_magnify4x_callback(GtkAction *action, gpointer data);
static void frame_buffer_magnify5x_callback(GtkAction *action, gpointer data);

#define MAX_ZOOM_IN_FACTOR 5
#define MAX_ZOOM_OUT_FACTOR 1

/**
  * @brief  create frame buffer window
  * @param  pBufInfo: pointer to structure describing frame buffer information
  * @return     pointer of the generated frame buffer window
  */
GtkWidget *create_frame_buffer_window(FBINFO *pBufInfo)
{
    GdkPixbuf *pImg = NULL;
    GtkWidget *widget =NULL;
    BUF_WIDGET *pWidget = NULL;
    GdkImage *image = NULL;
    pWidget = malloc(sizeof(BUF_WIDGET));

    /*
    buf = malloc(pBufInfo->nRGB_Size);
    memcpy(buf, pBufInfo->GdkRGBBuf, pBufInfo->nRGB_Size);
    printf("size = %d %d %d\n", qemu_state->surface_qemu->pitch , qemu_state->surface_qemu->h, qemu_state->surface_qemu->pitch * qemu_state->surface_qemu->h);
    */

    guchar *buf = NULL;
    buf = malloc(qemu_state->surface_qemu->pitch * qemu_state->surface_qemu->h);
    memcpy(buf, qemu_state->surface_qemu->pixels, qemu_state->surface_qemu->pitch * qemu_state->surface_qemu->h);

    pWidget->buf = buf;

    /* temporary modified for sdl widget size in 20100125 */

    image = gdk_image_new(GDK_IMAGE_NORMAL,
                            gdk_visual_get_system(),
                            qemu_state->surface_qemu->w,
                            qemu_state->surface_qemu->h);

    gdk_image_set_colormap(image, gdk_colormap_get_system());
    memcpy(image->mem,
        qemu_state->surface_qemu->pixels,
        qemu_state->surface_qemu->pitch * qemu_state->surface_qemu->h);

    pImg = gdk_pixbuf_get_from_image(NULL,  /* Destination pixbuf, or NULL if a new pixbuf should be created. */
                                image,      /* Source GdkImage. */
                                NULL,       /* A colormap, or NULL to use the one for src */
                                0,              /* Source X coordinate within drawable. */
                                0,              /*  Source Y coordinate within drawable. */
                                0,          /* Destination X coordinate in pixbuf, or 0 if dest is NULL. */
                                0,          /* Destination Y coordinate in pixbuf, or 0 if dest is NULL. */
                                qemu_state->surface_qemu->w,    /* Width in pixels of region to get. */
                                qemu_state->surface_qemu->h     /* Height in pixels of region to get */
                                );

    /* scale size */
        pWidget->nOrgWidth = pWidget->width = qemu_state->surface_qemu->w * qemu_state->scale;
        pWidget->nOrgHeight = pWidget->height = qemu_state->surface_qemu->h * qemu_state->scale;
        pImg = gdk_pixbuf_scale_simple(pImg, pWidget->width, pWidget->height, GDK_INTERP_NEAREST);

    pWidget->pPixBuf = pImg;
    pWidget->nCurDisplay = 1;

    /* rotate */
    int nMode = UISTATE.current_mode % 4;
    if (nMode  == 1) /*90*/
    {
        int temp;
        pImg = gdk_pixbuf_rotate_simple(pImg, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
        pWidget->pPixBuf = pImg;
        temp = pWidget->width;
        pWidget->width = pWidget->height;
        pWidget->height = temp;

        temp = pWidget->nOrgWidth;
        pWidget->nOrgWidth = pWidget->nOrgHeight;
        pWidget->nOrgHeight = temp;
    }
    if (nMode == 2) /*180*/
    {
        pImg = gdk_pixbuf_rotate_simple(pImg, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
        pWidget->pPixBuf = pImg;
    }
    if (nMode == 3) /*280*/
    {
        int temp;
        pImg = gdk_pixbuf_rotate_simple(pImg, GDK_PIXBUF_ROTATE_CLOCKWISE);
        pWidget->pPixBuf = pImg;
        temp = pWidget->width;
        pWidget->width = pWidget->height;
        pWidget->height = temp;

        temp = pWidget->nOrgWidth;
        pWidget->nOrgWidth = pWidget->nOrgHeight;
        pWidget->nOrgHeight = temp;
    }

    /* after getting the information, it is to generate the frame buffer window */

    widget = create_frame_buffer(pWidget);

    gdk_image_destroy(image);
    return (widget);
}


/**
  * @brief      handler to process it, when the frame buffer show menu is choosen
  * @param  widget: event generaton widget
  * @param  data: user data point
  * @return void
  */
void frame_buffer_handler(GtkWidget *widget, gpointer data)
{
    GtkWidget *frame_buffer_window = NULL;
    int x;
    gint frame_buffer_window_w;
    gint frame_buffer_window_h;

    frame_buffer_window = create_frame_buffer_window(&g_fbinfo[0]);

    add_window(frame_buffer_window, SCREEN_SHOT_ID);

    /* positioning */
    gtk_window_get_size(GTK_WINDOW(frame_buffer_window), &frame_buffer_window_w, &frame_buffer_window_h);

    x = configuration.main_x + (PHONE.mode_SkinImg[UISTATE.current_mode].nImgWidth * UISTATE.scale) + frame_buffer_window_w;
    if (x < gdk_screen_width()) { //right of emulator window
        gtk_window_move(GTK_WINDOW(frame_buffer_window), x - frame_buffer_window_w, configuration.main_y);
    } else { //left of emulator window
        gtk_window_move(GTK_WINDOW(frame_buffer_window), configuration.main_x -frame_buffer_window_w, configuration.main_y);
    }

    UISTATE.is_screenshot_run = TRUE;

    if (frame_buffer_window != NULL)
        gtk_widget_show_all(frame_buffer_window);

}


/**
  * @brief      callback function to destroy the frame buffer window
  * @param  widget: event generation widget
  * @param  data: user pointer data
  * @return void
  */
static void frame_buffer_destroy_event(GtkWidget *widget, gpointer data)
{
    BUF_WIDGET *pWidget = (BUF_WIDGET *) data;

    if (pWidget->pPixBuf != NULL)
        g_object_unref(pWidget->pPixBuf);

    UISTATE.is_screenshot_run = FALSE;

    free(pWidget);
}


/**
  * @brief  call the function when the expose event is generated in the draw area of frmae buffer window
  * @param  widget: event generated widget pointer
  * @param  event:  event type
  * @param  data: user information
  * @return void
  */
static void frame_buffer_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    GdkColor color;
    GdkGC *gc;

    gc = gdk_gc_new(widget->window);

    color.red = 0xaa << 8;
    color.green = 0xaa << 8;
    color.blue = 0xaa << 8;
    color.pixel = (color.red << 16) + (color.green << 8) + (color.blue);
    gdk_colormap_alloc_color(gdk_colormap_get_system(), &color, FALSE, TRUE);

    gdk_gc_set_foreground(gc, &color);
    gdk_draw_rectangle(widget->window,
                    gc,
                    TRUE,
                    event->area.x,
                    event->area.y,
                    event->area.width,
                    event->area.height);

    g_object_unref(gc);

    gdk_draw_pixbuf(widget->window,
                widget->style->fg_gc[GTK_STATE_NORMAL],
                (GdkPixbuf *) data,
                event->area.x,
                event->area.y,
                event->area.x,
                event->area.y,
                gdk_pixbuf_get_width((GdkPixbuf *) data) > event->area.width ? event->area.width : gdk_pixbuf_get_width((GdkPixbuf *) data),
                gdk_pixbuf_get_height((GdkPixbuf *) data) > event->area.height ? event->area.height : gdk_pixbuf_get_height((GdkPixbuf *) data),
                GDK_RGB_DITHER_MAX, 0, 0);

    gdk_flush();

    return;
}


/**
  * @brief      enlarge or reduce frame buffer
  * @param  data: event generation widget
  * @param  nScale: scale of enlargement or reduction
  * @return generated: widget pointer
  */
static GtkWidget *frame_buffer_scale_window(gpointer *data, int nScale)
{
    GdkPixbuf *pScaleImg = NULL;
    GtkWidget *buf = NULL;
    GtkWidget *widget = NULL;
    int x, y;
    BUF_WIDGET *pWidget = NULL;
    BUF_WIDGET *buf_widget = (BUF_WIDGET *) data;

    pWidget = malloc(sizeof(BUF_WIDGET));
    memcpy(pWidget, data, sizeof(BUF_WIDGET));

    /* 1. image scale function */

    pScaleImg = gdk_pixbuf_scale_simple(buf_widget->pPixBuf, pWidget->nOrgWidth * nScale, pWidget->nOrgHeight * nScale, GDK_INTERP_NEAREST);

    pWidget->pPixBuf = pScaleImg;
    pWidget->width = pWidget->nOrgWidth * nScale;
    pWidget->height = pWidget->nOrgHeight * nScale;
    pWidget->nCurDisplay = nScale;

    gtk_window_get_position(GTK_WINDOW(pWidget->pWindow), &x, &y);

    buf = pWidget->pWindow;
    widget = create_frame_buffer(pWidget);
    gtk_window_move(GTK_WINDOW(widget), x, y);
    gtk_widget_show_all(widget);

    gtk_widget_destroy(buf);

    return (widget);
}


/**
  * @brief  image sizeX1 event handler
  * @param  action: event-generated action
  * @param  data: user pointer
  * @return void
  */
static void frame_buffer_magnify_zoomin_callback(GtkAction *action, gpointer data)
{
    BUF_WIDGET *buf_widget = (BUF_WIDGET *) data;
    GtkWidget *widget = NULL;

    if (buf_widget->nCurDisplay < MAX_ZOOM_IN_FACTOR) {
        buf_widget->nCurDisplay++;
        widget = frame_buffer_scale_window(data, buf_widget->nCurDisplay);
        gtk_widget_show_all(widget);
    }
}


/**
  * @brief  image sizeX1 event handler
  * @param  action: event-generated action
  * @param  data: user pointer
  * @return void
  */
static void frame_buffer_magnify_zoomout_callback(GtkAction *action, gpointer data)
{
    BUF_WIDGET *buf_widget = (BUF_WIDGET *) data;
    GtkWidget *widget = NULL;

    if (buf_widget->nCurDisplay > MAX_ZOOM_OUT_FACTOR) {
        buf_widget->nCurDisplay--;
        widget = frame_buffer_scale_window(data, buf_widget->nCurDisplay);
        gtk_widget_show_all(widget);
    }
}


/**
  * @brief  image sizeX1 event handler
  * @param  action: event-generated action
  * @param  data: user pointer
  * @return void
  */
static void frame_buffer_magnify1x_callback(GtkAction *action, gpointer data)
{
    GtkWidget *widget = NULL;
    widget = frame_buffer_scale_window(data, 1);
    gtk_widget_show_all(widget);
}


/**
  * @brief  image sizeX2 event handler
  * @param  action: event-generated action
  * @param  data: user pointer
  * @return void
  */
static void frame_buffer_magnify2x_callback(GtkAction *action, gpointer data)
{
    GtkWidget *widget = NULL;
    widget = frame_buffer_scale_window(data, 2);
    gtk_widget_show_all(widget);
}


/**
  * @brief  image sizeX3 event handler
  * @param  action: event-generated action
  * @param  data: user pointer
  * @return void
  */
static void frame_buffer_magnify3x_callback(GtkAction *action, gpointer data)
{
    GtkWidget *widget = NULL;
    widget = frame_buffer_scale_window(data, 3);
    gtk_widget_show_all(widget);
}


/**
  * @brief  image sizeX4 event handler
  * @param  action: event-generated action
  * @param  data: user pointer
  * @return void
  */
static void frame_buffer_magnify4x_callback(GtkAction *action, gpointer data)
{
    GtkWidget *widget = NULL;
    widget = frame_buffer_scale_window(data, 4);
    gtk_widget_show_all(widget);
}


/**
  * @brief  image sizeX5 event handler
  * @param  action: event-generated action
  * @param  data: user pointer
  * @return void
  */
static void frame_buffer_magnify5x_callback(GtkAction *action, gpointer data)
{
    GtkWidget *widget = NULL;
    widget = frame_buffer_scale_window(data, 5);
    gtk_widget_show_all(widget);
}

#if 0
char * find_index(char *file_name, char search)
{
    int len = 0;

    len = strlen(file_name);

    while (len-- > 0) {
        if (*file_name == search) { //file_name + len
            return file_name;
        }
        file_name++;
    }
    return NULL;
}
#endif

/**
  * @brief      save frame buffer image as a file
  * @param toolbutton: event generated widget
  * @param user_data: user data pointer
  * @return void
  */
static void frame_buffer_save_image(GtkToolButton *toolbutton, gpointer user_data)
{
    char *file_name = NULL;
    char *str = NULL;
    GError *g_err = NULL;
    GdkPixbuf *pOriImg = NULL;
    BUF_WIDGET *buf_widget = (BUF_WIDGET *) user_data;
    GtkFileFilter *image_file_filter = NULL;

    pOriImg = gdk_pixbuf_scale_simple(buf_widget->pPixBuf, buf_widget->nOrgWidth, buf_widget->nOrgHeight, GDK_INTERP_NEAREST);
    buf_widget->pPixBuf = pOriImg;

    do {
        image_file_filter = gtk_file_filter_new();
        gtk_file_filter_set_name(image_file_filter, "Image Files");
        gtk_file_filter_add_pattern(image_file_filter, "*.[pP][nN][gG]");
        gtk_file_filter_add_pattern(image_file_filter, "*.[jJ][pP][gG]");
        gtk_file_filter_add_pattern(image_file_filter, "*.[jJ][pP][eE][gG]");
        gtk_file_filter_add_pattern(image_file_filter, "*.[bB][mM][pP]");

        gchar *screenshot_path =  (gchar *)g_get_home_dir();
        if (access(screenshot_path, 0) != 0) {
            screenshot_path = NULL;
        }

        file_name = get_file_name("Save Image...", screenshot_path, GTK_FILE_CHOOSER_ACTION_SAVE, image_file_filter, IMAGE_FILE_SAVE);
        if (file_name == NULL) {
            break; //cancle
        } else if (strlen(file_name) == 0) {
            g_free(file_name);
            continue;
        }

        str = strrchr(file_name, '.');
        if (str == NULL) {
            int len = strlen(file_name) + 5;
            char *png_file_name = qemu_mallocz(len);
            snprintf(png_file_name, len, "%s.png", file_name); //default format

            gdk_pixbuf_save(buf_widget->pPixBuf, png_file_name, "png", &g_err, "compression", "9", NULL);
            qemu_free(png_file_name);
            break;
        }

        if (strncmp(str + 1, "jpg", strlen("jpg") + 1) == 0 || strncmp(str + 1, "JPG", strlen("JPG") + 1) == 0 ||
            strncmp(str + 1, "jpeg", strlen("jpeg") + 1) == 0 || strncmp(str + 1, "JPEG", strlen("JPEG") + 1) == 0)
        {
            gdk_pixbuf_save(buf_widget->pPixBuf, file_name, "jpeg", &g_err, "quality", "100", NULL);
            break;
        } else if (strncmp(str + 1, "png", strlen("png") + 1) == 0 || strncmp(str + 1, "PNG", strlen("PNG") + 1) == 0) {
            gdk_pixbuf_save(buf_widget->pPixBuf, file_name, "png", &g_err, "compression", "9", NULL);
            break;
        } else if (strncmp(str + 1, "bmp", strlen("bmp") + 1) == 0 || strncmp(str + 1, "BMP", strlen("bmp") + 1) == 0) {
            gdk_pixbuf_save(buf_widget->pPixBuf, file_name, "bmp", &g_err, NULL);
            break;
        } else {
            show_message("Warning", "You must Add the Extension of File. (PNG/JPG/BMP)");
            g_free(file_name);
        }
    }while(1);

    g_free(file_name);
}


/**
 * @brief       copy the frame buffer image to clipboard
 * @param toolbutton: event generated widget
  * @param user_data: user data pointer
  * @return void
  */
static void copy_clip_board(GtkToolButton * toolbutton, gpointer user_data)
{
    GtkClipboard *clipboard = NULL;
    GdkPixbuf *pOriImg = NULL;
    BUF_WIDGET *buf_widget = (BUF_WIDGET *) user_data;
    clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    pOriImg = gdk_pixbuf_scale_simple(buf_widget->pPixBuf, buf_widget->nOrgWidth, buf_widget->nOrgHeight, GDK_INTERP_NEAREST);
    buf_widget->pPixBuf = pOriImg;

    gtk_clipboard_set_image(clipboard, buf_widget->pPixBuf);
}


/**
  * @brief      after get the information, generate the widget
  * @param  pWidget     structure pointer of widget info
  * @return     success : frmae buffer widget pointer
  *             failure : NULL
  */
static GtkWidget *create_frame_buffer(BUF_WIDGET * pWidget)
{
    GtkWidget *window1;
    GtkWidget *toolbar1;
    GtkIconSize tmp_toolbar_icon_size;
    GtkWidget *toolbutton1;
    GtkWidget *toolbutton2;
    GtkWidget *menutoolbutton1;
    GtkWidget *menutoolbutton2;
    GtkWidget *pStatusBar = NULL;
    GtkWidget *vBox;
    GtkWidget *scrolledwindow1;
    GtkWidget *viewport1;
    GtkWidget *drawingarea1;
    GtkAccelGroup *accel_group = NULL;

    accel_group = gtk_accel_group_new();

    /* 1. window create */

    window1 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position (GTK_WINDOW (window1), GTK_WIN_POS_CENTER);

    gtk_window_set_title(GTK_WINDOW(window1), ("Screen Shot"));
    gtk_window_set_resizable(GTK_WINDOW(window1), FALSE);
    pWidget->pWindow = window1;

    gchar icon_image[MAXPATH] = {0, };
    const gchar *skin = get_skin_path();
    if (skin == NULL){
        WARN("getting icon image path is failed!!\n");
    }

    sprintf(icon_image, "%s/icons/06_SCREEN-SHOT.png", skin);

    gtk_window_set_icon_from_file (GTK_WINDOW(window1), icon_image, NULL);

    vBox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window1), vBox);
    gtk_widget_show(vBox);

    gtk_window_add_accel_group(GTK_WINDOW(window1), accel_group);

    /* 2. toolbar create */

    toolbar1 = gtk_toolbar_new();
    gtk_widget_show(toolbar1);

    pWidget->pToolBar = toolbar1;

    gtk_box_pack_start(GTK_BOX(vBox), toolbar1, FALSE, FALSE, 0);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar1), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar1), FALSE);
    tmp_toolbar_icon_size = gtk_toolbar_get_icon_size(GTK_TOOLBAR(toolbar1));

    toolbutton1 = (GtkWidget *) gtk_tool_button_new_from_stock("gtk-justify-center");
    gtk_widget_set_tooltip_text(toolbutton1, "Copy into ClipBoard (Ctrl+c)");
    gtk_widget_show(toolbutton1);
    gtk_container_add(GTK_CONTAINER(toolbar1), toolbutton1);
    g_signal_connect(toolbutton1, "clicked", G_CALLBACK(copy_clip_board), pWidget);
    gtk_widget_add_accelerator(toolbutton1, "clicked", accel_group, GDK_c, (GdkModifierType) GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    toolbutton2 = (GtkWidget *) gtk_tool_button_new_from_stock("gtk-save-as");
    gtk_widget_set_tooltip_text(toolbutton2, "Save as (Ctrl+s)");
    gtk_widget_show(toolbutton2);
    gtk_container_add(GTK_CONTAINER(toolbar1), toolbutton2);
    g_signal_connect(toolbutton2, "clicked", G_CALLBACK(frame_buffer_save_image), pWidget);
    gtk_widget_add_accelerator(toolbutton2, "clicked", accel_group, GDK_s, (GdkModifierType) GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    menutoolbutton1 = (GtkWidget *)gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
    gtk_widget_set_tooltip_text(menutoolbutton1, "Zoom in (Mouse wheel up)");
    if (pWidget->nCurDisplay == MAX_ZOOM_IN_FACTOR) {
        gtk_widget_set_sensitive(menutoolbutton1, FALSE);
    }
    gtk_widget_show(menutoolbutton1);
    gtk_container_add(GTK_CONTAINER(toolbar1), menutoolbutton1);
    g_signal_connect(menutoolbutton1, "clicked", G_CALLBACK(frame_buffer_magnify_zoomin_callback), pWidget);

    menutoolbutton2 = (GtkWidget *)gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
    gtk_widget_set_tooltip_text(menutoolbutton2, "Zoom out (Mouse wheel down)");
    if (pWidget->nCurDisplay == MAX_ZOOM_OUT_FACTOR) {
        gtk_widget_set_sensitive(menutoolbutton2, FALSE);
    }
    gtk_widget_show(menutoolbutton2);
    gtk_container_add(GTK_CONTAINER(toolbar1), menutoolbutton2);
    g_signal_connect(menutoolbutton2, "clicked", G_CALLBACK(frame_buffer_magnify_zoomout_callback), pWidget);

    /* 4. in case of more than x2, scroll window create */

    if (pWidget->width > (pWidget->nOrgWidth * 2) || pWidget->height > (pWidget->nOrgHeight * 2)) {
        scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(GTK_WIDGET(scrolledwindow1), pWidget->nOrgWidth * 2, pWidget->nOrgHeight * 2);
        gtk_widget_show(scrolledwindow1);
        gtk_box_pack_start(GTK_BOX(vBox), scrolledwindow1, TRUE, TRUE, 0);

        viewport1 = gtk_viewport_new(NULL, NULL);
        gtk_widget_show(viewport1);
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwindow1), viewport1);

        drawingarea1 = gtk_drawing_area_new();
        gtk_widget_set_size_request(GTK_WIDGET(GTK_DRAWING_AREA(drawingarea1)), pWidget->width, pWidget->height);
        gtk_container_add(GTK_CONTAINER(viewport1), drawingarea1);
        gtk_widget_show(drawingarea1);
    }

    else {
        drawingarea1 = gtk_drawing_area_new();
        gtk_widget_set_size_request(GTK_WIDGET(GTK_DRAWING_AREA(drawingarea1)), pWidget->width, pWidget->height);
        gtk_box_pack_start(GTK_BOX(vBox), drawingarea1, TRUE, TRUE, 0);
        gtk_widget_show(drawingarea1);
    }

    pWidget->drawing_area = drawingarea1;

    /* 5. status bar */

    pStatusBar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vBox), pStatusBar, FALSE, FALSE, 0);

    pWidget->pStatusBar = pStatusBar;

    g_signal_connect(GTK_OBJECT(drawingarea1), "expose_event", G_CALLBACK(frame_buffer_expose_event), pWidget->pPixBuf);
    g_signal_connect(GTK_OBJECT(drawingarea1), "destroy", G_CALLBACK(frame_buffer_destroy_event), pWidget);
    g_signal_connect(GTK_OBJECT(drawingarea1), "scroll-event", G_CALLBACK(frame_buffer_scroll_event_handler), pWidget);

    g_signal_connect(GTK_OBJECT(drawingarea1), "motion_notify_event", G_CALLBACK(frame_buffer_motion_notify_event_handler), pWidget);
    g_signal_connect(GTK_OBJECT(drawingarea1), "button_release_event", G_CALLBACK(frame_buffer_motion_notify_event_handler), pWidget);
    g_signal_connect(GTK_OBJECT(drawingarea1), "button_press_event", G_CALLBACK(frame_buffer_motion_notify_event_handler), pWidget);

    g_signal_connect(GTK_OBJECT(drawingarea1), "leave_notify_event", G_CALLBACK(frame_buffer_motion_no_notify_event_handler), pWidget);

    /* 6. Ctrl key process */

    g_signal_connect(GTK_OBJECT(window1), "key_press_event", G_CALLBACK(frame_buffer_key_press_event_handler), NULL);
    g_signal_connect(GTK_OBJECT(window1), "key_release_event", G_CALLBACK(frame_buffer_key_release_event_handler), NULL);
    gtk_widget_set_events(drawingarea1, GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);

    return window1;
}


/**
  * @brief  the handler for dealing with mouse moving event
  * @param widget: event generate widget
  * @param event: structure of event info
  * @param data: user-defined data
  * @return         success : TRUE  failure : FALSE
  */
static gint frame_buffer_motion_notify_event_handler(GtkWidget * widget, GdkEventButton * event, gpointer data)
{
    BUF_WIDGET *buf_widget = data;
    char buf[MAXBUF];
    GdkCursor *cursor;
    guchar *pixel = NULL;
    int nCnt;
    int pixel_x = 0;
    int pixel_y = 0;
    GdkColor color;

    /* 1. user cursor */

    cursor = gdk_cursor_new(GDK_CROSSHAIR);
    gdk_window_set_cursor(widget->window, cursor);

    nCnt = buf_widget->width / buf_widget->nOrgWidth;

    pixel_x = (int)event->x / nCnt;
    pixel_y = (int)event->y / nCnt;

    pixel = buf_widget->buf + ((pixel_x * 4) + (pixel_y * buf_widget->nOrgWidth * 4));

    /* 2. after being colormap, transfer to the palette */

    color.blue = *(pixel) << 8;
    color.green = *(pixel + 1) << 8;
    color.red = *(pixel + 2) << 8;

    color.pixel = (*(pixel) << 16) + (*(pixel + 1) << 8) + (*(pixel + 2));

    snprintf(buf, sizeof(buf), "X:%d Y:%d ARGB(%d,%d,%d,%d)", pixel_x, pixel_y, *(pixel + 3) << 8 >> 8, color.red >> 8, color.green >> 8, color.blue >> 8);

    gtk_statusbar_push(GTK_STATUSBAR(buf_widget->pStatusBar), 0, buf);


    if (event->type == GDK_2BUTTON_PRESS) {
        static int ge_show_flag = 0;

        if (ge_show_flag == 0) {
            gtk_widget_hide(buf_widget->pToolBar);
            ge_show_flag = 1;
        }

        else {
            gtk_widget_show(buf_widget->pToolBar);
            ge_show_flag = 0;
        }

    }

    else if (event->type == GDK_BUTTON_PRESS) {
        if (event->button == 1 && UISTATE.frame_buffer_ctrl == 1) {
            if (buf_widget->nCurDisplay < 5) {
                buf_widget->nCurDisplay++;
                switch (buf_widget->nCurDisplay) {
                    case 1:
                        frame_buffer_magnify1x_callback(NULL, data);
                        break;
                    case 2:
                        frame_buffer_magnify2x_callback(NULL, data);
                        break;
                    case 3:
                        frame_buffer_magnify3x_callback(NULL, data);
                        break;
                    case 4:
                        frame_buffer_magnify4x_callback(NULL, data);
                        break;
                    case 5:
                        frame_buffer_magnify5x_callback(NULL, data);
                        break;
                }
            }
        }

        else if (event->button == 3 && UISTATE.frame_buffer_ctrl == 1) {
            if (buf_widget->nCurDisplay > 1) {
                buf_widget->nCurDisplay--;
                switch (buf_widget->nCurDisplay) {
                    case 1:
                        frame_buffer_magnify1x_callback(NULL, data);
                        break;
                    case 2:
                        frame_buffer_magnify2x_callback(NULL, data);
                        break;
                    case 3:
                        frame_buffer_magnify3x_callback(NULL, data);
                        break;
                    case 4:
                        frame_buffer_magnify4x_callback(NULL, data);
                        break;
                    case 5:
                        frame_buffer_magnify5x_callback(NULL, data);
                        break;
                }
            }
        }

        else {
            /* drag process */
            gtk_window_begin_move_drag(GTK_WINDOW(buf_widget->pWindow), event->button, event->x_root, event->y_root, event->time);
        }
    }
    return TRUE;
}


/**
  * @brief      fix cursor pointer when it locates out of the screen
  * @param widget: event generation widget
  * @param event: structure of event generation info
  * @param data: user define event data
  * @return     TRUE
  */
static gint frame_buffer_motion_no_notify_event_handler(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    BUF_WIDGET *buf_widget = (BUF_WIDGET *) data;

    /* cursor shape initialization */

    gdk_window_set_cursor(widget->window, NULL);
    gtk_statusbar_pop(GTK_STATUSBAR(buf_widget->pStatusBar), 0);
    gtk_statusbar_push(GTK_STATUSBAR(buf_widget->pStatusBar), 0, "");

    return TRUE;
}


/**
  * @brief      handler to process the pressed keyboard hot-key
  * @param widget:  event generation widget
  * @param event_type   event: type
  * @param data user: data pointer
  * @return FALSE
  */
static gboolean frame_buffer_key_press_event_handler(GtkWidget *widget, GdkEventKey *event_type, gpointer data)
{
    int key_buf = 0;
    GdkEventKey *key = (GdkEventKey *) event_type;
    key_buf = (int)key->keyval;
    if (GDK_Control_L == key_buf || GDK_Control_R == key_buf)
        UISTATE.frame_buffer_ctrl = 1;

    return FALSE;
}


/**
  * @brief      handler to process the released keyboard hot-key
  * @param widget   event generation widget
  * @param event_type       event type
  * @param data     user data pointer
  * @return FALSE
  */
static gboolean frame_buffer_key_release_event_handler(GtkWidget *widget, GdkEventKey *event_type, gpointer data)
{
    int key_buf = 0;
    GdkEventKey *key = (GdkEventKey *) event_type;
    key_buf = (int)key->keyval;
    if (GDK_Control_L == key_buf || GDK_Control_R == key_buf)
        UISTATE.frame_buffer_ctrl = 0;

    return FALSE;
}


/**
  * @brief      handler to process the mouse scroll button
  * @param widget:  event generation widget
  * @param event_type:  event type
  * @param data:    user data pointer
  * @return TRUE
  */
static gboolean frame_buffer_scroll_event_handler(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
    BUF_WIDGET *buf_widget = (BUF_WIDGET *) data;
    if (event->direction == GDK_SCROLL_UP) {

        if (buf_widget->nCurDisplay < 5) {
            buf_widget->nCurDisplay++;

            switch (buf_widget->nCurDisplay) {
                case 1:
                    frame_buffer_magnify1x_callback(NULL, data);
                    break;
                case 2:
                    frame_buffer_magnify2x_callback(NULL, data);
                    break;
                case 3:
                    frame_buffer_magnify3x_callback(NULL, data);
                    break;
                case 4:
                    frame_buffer_magnify4x_callback(NULL, data);
                    break;
                case 5:
                    frame_buffer_magnify5x_callback(NULL, data);
                    break;
            }
        }
    }

    else if (event->direction == GDK_SCROLL_DOWN) {
        if (buf_widget->nCurDisplay > 1) {
            buf_widget->nCurDisplay--;
            switch (buf_widget->nCurDisplay) {
                case 1:
                    frame_buffer_magnify1x_callback(NULL, data);
                    break;
                case 2:
                    frame_buffer_magnify2x_callback(NULL, data);
                    break;
                case 3:
                    frame_buffer_magnify3x_callback(NULL, data);
                    break;
                case 4:
                    frame_buffer_magnify4x_callback(NULL, data);
                    break;
                case 5:
                    frame_buffer_magnify5x_callback(NULL, data);
                    break;
            }
        }
    }

    return TRUE;
}

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

#include "qemu_gtk_widget.h"
#include "qemu_gtk_widget.h"
#include "utils.h"
#include <pthread.h>
#include "cursor_left_ptr.xpm"

#include "debug_ch.h"
#include "../ui/sdl_rotate.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, qemu_gtk_widget);

#ifndef _WIN32
//extern void opengl_exec_set_parent_window(Display* _dpy, Window _parent_window);
#endif

extern void sdl_display_set_window_size(int w, int h);
extern void sdl_display_force_refresh(void);

static void qemu_widget_class_init (gpointer g_class, gpointer class_data);
static void qemu_widget_init (GTypeInstance *instance, gpointer g_class);
static void qemu_widget_realize (GtkWidget  *widget);
static void qemu_widget_size_allocate   (GtkWidget  *widget, GtkAllocation *allocation);
static void qemu_update (qemu_state_t *qemu_state);
static void qemu_sdl_init(qemu_state_t *qemu_state);

qemu_state_t *qemu_state;
static int widget_exposed;
int qemu_state_initialized = 0;
static pthread_mutex_t sdl_mutex = PTHREAD_MUTEX_INITIALIZER;
static SDL_Cursor* sdl_cursor_normal;
multi_touch_state qemu_mts;

#define SDL_THREAD

#ifdef SDL_THREAD
static pthread_cond_t sdl_cond = PTHREAD_COND_INITIALIZER;
static int sdl_thread_initialized = 0;
#endif

int use_qemu_display = 0;

#ifdef SDL_THREAD
static void* run_qemu_update(void* arg)
{
    for(;;) {
        pthread_mutex_lock(&sdl_mutex);

        pthread_cond_wait(&sdl_cond, &sdl_mutex);

        qemu_update(qemu_state);

        pthread_mutex_unlock(&sdl_mutex);
    }

    return NULL;
}
#endif

static void qemu_ds_update(DisplayState *ds, int x, int y, int w, int h)
{
    //TRACE("qemu_ds_update\n");
    /* call sdl update */
#ifdef SDL_THREAD

    pthread_mutex_lock(&sdl_mutex);

    pthread_cond_signal(&sdl_cond);

    pthread_mutex_unlock(&sdl_mutex);

#else
    qemu_update(qemu_state);
#endif
}

static void qemu_ds_resize(DisplayState *ds)
{
    TRACE( "%d, %d\n",
            ds_get_width(qemu_state->ds),
            ds_get_height(qemu_state->ds));

    if (ds_get_width(qemu_state->ds) == 720 && ds_get_height(qemu_state->ds) == 400) {
        TRACE( "blanking BIOS\n");
        qemu_state->surface_qemu = NULL;
        return;
    }

    pthread_mutex_lock(&sdl_mutex);

    /* create surface_qemu */
    qemu_state->surface_qemu = SDL_CreateRGBSurfaceFrom(ds_get_data(qemu_state->ds),
            ds_get_width(qemu_state->ds),
            ds_get_height(qemu_state->ds),
            ds_get_bits_per_pixel(qemu_state->ds),
            ds_get_linesize(qemu_state->ds),
            qemu_state->ds->surface->pf.rmask,
            qemu_state->ds->surface->pf.gmask,
            qemu_state->ds->surface->pf.bmask,
            qemu_state->ds->surface->pf.amask);

    pthread_mutex_unlock(&sdl_mutex);

    if (!qemu_state->surface_qemu) {
        ERR( "Unable to set the RGBSurface: %s", SDL_GetError() );
        return;
    }

}


static void qemu_ds_refresh(DisplayState *ds)
{
    vga_hw_update();

    if (widget_exposed) {
#ifdef SDL_THREAD
        pthread_cond_signal(&sdl_cond);
#else
        qemu_update(qemu_state);
#endif
        widget_exposed--;
    }
}


static const GTypeInfo qemu_widget_info = {
    .class_size = sizeof (qemu_class_t),
    .base_init = NULL,
    .base_finalize = NULL,
    .class_init = qemu_widget_class_init,
    .class_finalize = NULL,
    .class_data = NULL,
    .instance_size = sizeof (qemu_state_t),
    .n_preallocs = 0,
    .instance_init = qemu_widget_init,
    .value_table = NULL,
};

static GType qemu_widget_type;

static GType qemu_get_type(void)
{
    if (!qemu_widget_type)
        qemu_widget_type = g_type_register_static(GTK_TYPE_WIDGET, "Qemu", &qemu_widget_info, 0);

    return qemu_widget_type;
}


static void qemu_widget_class_init(gpointer g_class, gpointer class_data)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (g_class);

    widget_class->realize = qemu_widget_realize;
    widget_class->size_allocate = qemu_widget_size_allocate;
    widget_class->expose_event = qemu_widget_expose;

    TRACE( "qemu class init\n");
}


static void qemu_widget_init(GTypeInstance *instance, gpointer g_class)
{
    TRACE( "qemu initialize\n");
}


void qemu_display_init (DisplayState *ds)
{
    WARN( "qemu_display_init\n");
    /*  graphics context information */
    DisplayChangeListener *dcl;

    while(1)
    {
        if(qemu_state_initialized == 1)
            break;
        else
            usleep(100000);
    }

    qemu_state->ds = ds;

    dcl = qemu_mallocz(sizeof(DisplayChangeListener));
    dcl->dpy_update = qemu_ds_update;
    dcl->dpy_resize = qemu_ds_resize;
    dcl->dpy_refresh = qemu_ds_refresh;

    register_displaychangelistener(qemu_state->ds, dcl);

#ifdef SDL_THREAD
    if(sdl_thread_initialized == 0 ){
        sdl_thread_initialized = 1;
        pthread_t thread_id;
        TRACE( "sdl update thread create \n");
        if (pthread_create(&thread_id, NULL, run_qemu_update, NULL) != 0) {
            ERR( "pthread_create fail \n");
            return;
        }
    }
#endif
}



int intermediate_section = 12;

gint qemu_widget_new (GtkWidget **widget)
{
    lcd_list_data *lcd = &PHONE.mode[UISTATE.current_mode].lcd_list[0];

    if (!qemu_state)
        qemu_state = g_object_new(qemu_get_type(), NULL);

    qemu_state->scale = UISTATE.scale;
    qemu_state->width = lcd->lcd_region.w * qemu_state->scale;
    qemu_state->height = lcd->lcd_region.h * qemu_state->scale;
    qemu_state->bpp = lcd->bitsperpixel;
    qemu_state->flags = SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL|SDL_NOFRAME;
    if(PHONE.dual_display == 1){
        intermediate_section = lcd->lcd_region.split;
        //printf("rotation intermediate_section %d\n", intermediate_section);
        if(UISTATE.current_mode ==0 || UISTATE.current_mode ==2)
        {
            qemu_state->width=qemu_state->width+intermediate_section;
            qemu_widget_size (qemu_state, qemu_state->width, qemu_state->height);
        }
        if(UISTATE.current_mode ==1 || UISTATE.current_mode ==3)
        {
            qemu_state->height=qemu_state->height+intermediate_section;
            qemu_widget_size (qemu_state, qemu_state->width, qemu_state->height);
        }
    }
    else
        qemu_widget_size (qemu_state, qemu_state->width, qemu_state->height);

    TRACE( "qemu widget size is width = %d, height = %d\n",
            qemu_state->width, qemu_state->height);

    *widget = GTK_WIDGET (qemu_state);

    qemu_state_initialized = 1;
    return TRUE;
}


void qemu_widget_size (qemu_state_t *qemu_state, gint width, gint height)
{
    GtkWidget *widget = GTK_WIDGET(qemu_state);

    g_return_if_fail (GTK_IS_QEMU (qemu_state));

    GTK_WIDGET (qemu_state)->requisition.width = width;
    GTK_WIDGET (qemu_state)->requisition.height = height;

    gtk_widget_queue_resize (GTK_WIDGET (qemu_state));

    TRACE("qemu_qemu_state size if width = %d, height = %d\n", width, height);

    sdl_display_set_rotation((8 - UISTATE.current_mode)%4);
    sdl_display_set_window_size(width, height);

#ifdef GTK_WIDGET_REALIZED
    if (GTK_WIDGET_REALIZED (widget))
#else
        if (gtk_widget_get_realized(widget))
#endif
        {
            pthread_mutex_lock(&sdl_mutex);
            qemu_sdl_init(GTK_QEMU(widget));
            pthread_mutex_unlock(&sdl_mutex);
        }
}


static void qemu_widget_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_QEMU (widget));
#if GTK_CHECK_VERSION(2,20,0)
    gtk_widget_set_realized(widget, TRUE);
#else
    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
#endif

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);
    attributes.event_mask = gtk_widget_get_events (widget) ;
    attributes.event_mask |= GDK_EXPOSURE_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gdk_window_set_user_data (widget->window, widget);

    widget->style = gtk_style_attach (widget->style, widget->window);
    gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

    pthread_mutex_lock(&sdl_mutex);
    qemu_sdl_init(GTK_QEMU(widget));
    pthread_mutex_unlock(&sdl_mutex);

    TRACE( "qemu realize success\n");
}


static void qemu_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_QEMU (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;
    /* FIXME, TODO-1.3: back out the MAX() statements */
    widget->allocation.width = MAX (1, widget->allocation.width);
    widget->allocation.height = MAX (1, widget->allocation.height);

#ifdef GTK_WIDGET_REALIZED
    if (GTK_WIDGET_REALIZED (widget))
#else
        if (gtk_widget_get_realized(widget))
#endif
        {
            gdk_window_move_resize (widget->window,
                    allocation->x, allocation->y,
                    allocation->width, allocation->height);
        }

    TRACE( "qemu_state size allocated\n");
}


static SDL_Cursor *sdl_cursor_init(const char *image[])
{
    int i, row, col;
        Uint8 data[4*32];
        Uint8 mask[4*32];
        int w, h;

        sscanf(image[0], "%d %d", &w, &h);

        i = -1;
        for ( row=0; row<32; ++row ) {
            for ( col=0; col<32; ++col ) {
                if ( col % 8 ) {
                    data[i] <<= 1;
                    mask[i] <<= 1;
                } else {
                    ++i;
                    data[i] = mask[i] = 0;
                }
                switch (image[4+row][col]) {
                    case 'X':
                        data[i] |= 0x01;
                        mask[i] |= 0x01;
                        break;
                    case '.':
                        mask[i] |= 0x01;
                        break;
                    case ' ':
                        break;
                }
            }
        }
        return SDL_CreateCursor(data, mask, 32, 32, 0, 0);
}

/* ===== Reference : http://content.gpwiki.org/index.php/SDL:Tutorials:Drawing_and_Filling_Circles ===== */
/*
 * This is a 32-bit pixel function created with help from this
* website: http://www.libsdl.org/intro.en/usingvideo.html
*
* You will need to make changes if you want it to work with
* 8-, 16- or 24-bit surfaces.  Consult the above website for
* more information.
*/
static void sdl_set_pixel(SDL_Surface *surface, int x, int y, Uint32 pixel) {
   Uint8 *target_pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
   *(Uint32 *)target_pixel = pixel;
}

/*
* This is an implementation of the Midpoint Circle Algorithm
* found on Wikipedia at the following link:
*
*   http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
*
* The algorithm elegantly draws a circle quickly, using a
* set_pixel function for clarity.
*/
static void sdl_draw_circle(SDL_Surface *surface, int cx, int cy, int radius, Uint32 pixel) {
   int error = -radius;
   int x = radius;
   int y = 0;

   while (x >= y)
   {
       sdl_set_pixel(surface, cx + x, cy + y, pixel);
       sdl_set_pixel(surface, cx + y, cy + x, pixel);

       if (x != 0)
       {
           sdl_set_pixel(surface, cx - x, cy + y, pixel);
           sdl_set_pixel(surface, cx + y, cy - x, pixel);
       }

       if (y != 0)
       {
           sdl_set_pixel(surface, cx + x, cy - y, pixel);
           sdl_set_pixel(surface, cx - y, cy + x, pixel);
       }

       if (x != 0 && y != 0)
       {
           sdl_set_pixel(surface, cx - x, cy - y, pixel);
           sdl_set_pixel(surface, cx - y, cy - x, pixel);
       }

       error += y;
       ++y;
       error += y;

       if (error >= 0)
       {
           --x;
           error -= x;
           error -= x;
       }
   }
}

/*
 * SDL_Surface 32-bit circle-fill algorithm without using trig
*
* While I humbly call this "Celdecea's Method", odds are that the
* procedure has already been documented somewhere long ago.  All of
* the circle-fill examples I came across utilized trig functions or
* scanning neighbor pixels.  This algorithm identifies the width of
* a semi-circle at each pixel height and draws a scan-line covering
* that width.
*
* The code is not optimized but very fast, owing to the fact that it
* alters pixels in the provided surface directly rather than through
* function calls.
*
* WARNING:  This function does not lock surfaces before altering, so
* use SDL_LockSurface in any release situation.
*/
static void sdl_fill_circle(SDL_Surface *surface, int cx, int cy, int radius, Uint32 pixel)
{
   // Note that there is more to altering the bitrate of this
   // method than just changing this value.  See how pixels are
   // altered at the following web page for tips:
   //   http://www.libsdl.org/intro.en/usingvideo.html
   const int bpp = 4;
   double dy;

   double r = (double)radius;

   for (dy = 1; dy <= r; dy += 1.0)
   {
       // This loop is unrolled a bit, only iterating through half of the
       // height of the circle.  The result is used to draw a scan line and
       // its mirror image below it.
       // The following formula has been simplified from our original.  We
       // are using half of the width of the circle because we are provided
       // with a center and we need left/right coordinates.
       double dx = floor(sqrt((2.0 * r * dy) - (dy * dy)));
       int x = cx - dx;
       // Grab a pointer to the left-most pixel for each half of the circle
       Uint8 *target_pixel_a = (Uint8 *)surface->pixels + ((int)(cy + r - dy)) * surface->pitch + x * bpp;
       Uint8 *target_pixel_b = (Uint8 *)surface->pixels + ((int)(cy - r + dy)) * surface->pitch + x * bpp;



       for (; x <= cx + dx; x++)
       {
           *(Uint32 *)target_pixel_a = pixel;
           *(Uint32 *)target_pixel_b = pixel;
           target_pixel_a += bpp;
           target_pixel_b += bpp;
       }
   }
}
/* ========================================================= */

#if 0
//TODO : call
static void qemu_sdl_cleanup(void)
{
    if (sdl_cursor_normal)
        SDL_FreeCursor(sdl_cursor_normal);

    SDL_FreeSurface(qemu_mts.finger_point);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
#endif

static void qemu_sdl_init(qemu_state_t *qemu_state)
{
    GtkWidget *qw = GTK_WIDGET(qemu_state);
    gchar SDL_windowhack[32];
    SDL_SysWMinfo info;
    long window;
    int temp;

    if (use_qemu_display)
        return;

#ifndef _WIN32
    window = GDK_WINDOW_XWINDOW(qw->window);;
#else
    window = (long)GDK_WINDOW_HWND(qw->window);;
#endif
    sprintf(SDL_windowhack, "%ld", window);
#ifndef _WIN32
    XSync(GDK_DISPLAY(), FALSE);
#endif
    g_setenv("SDL_WINDOWID", SDL_windowhack, 1);

    if (SDL_InitSubSystem (SDL_INIT_VIDEO) < 0 ) {
        ERR( "unable to init SDL: %s", SDL_GetError() );
        exit(1);
    }

    /* cursor init */
    sdl_cursor_normal = sdl_cursor_init(cursor_left_ptr_xpm);
    SDL_SetCursor(sdl_cursor_normal);

    /* finger point surface init */
    qemu_mts.multitouch_enable = 0;
    qemu_mts.finger_point_size = DEFAULT_FINGER_POINT_SIZE;
    temp = qemu_mts.finger_point_size / 2;
    qemu_mts.finger_point = SDL_CreateRGBSurface(SDL_SRCALPHA | SDL_HWSURFACE,
        qemu_mts.finger_point_size + 2, qemu_mts.finger_point_size + 2, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    sdl_fill_circle(qemu_mts.finger_point, temp, temp, temp, DEFAULT_FINGER_POINT_COLOR); //finger point
    sdl_draw_circle(qemu_mts.finger_point, temp, temp, temp, DEFAULT_FINGER_POINT_COLOR_OUTLINE); // finger point outline

    //atexit(qemu_sdl_cleanup); TODO:

    qemu_state->surface_screen = SDL_SetVideoMode(qemu_state->width,
            qemu_state->height, 0, qemu_state->flags);

#ifndef _WIN32
    SDL_VERSION(&info.version);
    SDL_GetWMInfo(&info);
    //  opengl_exec_set_parent_window(info.info.x11.display, info.info.x11.window);
#endif
}

gint qemu_widget_expose (GtkWidget *widget, GdkEventExpose *event)
{
    if (!qemu_state->ds) {
        sdl_display_force_refresh();
        return FALSE;
    }

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (GTK_IS_QEMU (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    TRACE("qemu_expose\n");
    widget_exposed++;

    return FALSE;
}

static void qemu_update (qemu_state_t *qemu_state)
{
    int i = 0;
    SDL_Rect r;
    SDL_Surface *surface  = NULL;

    if (!qemu_state->ds)
        return;

    g_return_if_fail (qemu_state != NULL);
    g_return_if_fail (GTK_IS_QEMU (qemu_state));

#ifndef SDL_THREAD
    pthread_mutex_lock(&sdl_mutex);
#endif

    surface = SDL_GetVideoSurface ();

    if (qemu_state->scale == 1) {
        if (UISTATE.current_mode % 4 != 0) { //rotation
            // work-around to remove afterimage on black color in Window and Ubuntu 11.10
            if( qemu_state->surface_qemu ) {
                // set color key 'magenta'
                qemu_state->surface_qemu->format->colorkey = 0xFF00FF;
            }

            SDL_Surface *rot_screen;
            rot_screen = rotozoomSurface(qemu_state->surface_qemu,
                    (UISTATE.current_mode % 4) * 90, 1, SMOOTHING_ON);
            SDL_BlitSurface(rot_screen, NULL, qemu_state->surface_screen, NULL);

            SDL_FreeSurface(rot_screen);
        } else {
            SDL_BlitSurface(qemu_state->surface_qemu, NULL, qemu_state->surface_screen, NULL);
        }

        /* draw finger points (multi-touch) */
        for (i = 0; i < qemu_mts.finger_cnt; i++) {
            r.x = qemu_mts.finger_slot[i].x - (qemu_mts.finger_point_size / 2);
            r.y = qemu_mts.finger_slot[i].y - (qemu_mts.finger_point_size / 2);
            r.w = r.h = qemu_mts.finger_point_size;

            SDL_BlitSurface(qemu_mts.finger_point, NULL, qemu_state->surface_screen, &r);
        }
    } else { //resize
        // work-around to remove afterimage on black color in Window and Ubuntu 11.10
        if( qemu_state->surface_qemu ) {
            // set color key 'magenta'
            qemu_state->surface_qemu->format->colorkey = 0xFF00FF;
        }

        SDL_Surface *down_screen;
        down_screen = rotozoomSurface(qemu_state->surface_qemu,
                (UISTATE.current_mode % 4) * 90, qemu_state->scale, SMOOTHING_ON);
        SDL_BlitSurface(down_screen, NULL, qemu_state->surface_screen, NULL);

        /* draw finger points (multi-touch) */
        for (i = 0; i < qemu_mts.finger_cnt; i++) {
            r.x = (qemu_mts.finger_slot[i].x * qemu_state->scale) - (qemu_mts.finger_point_size / 2);
            r.y = (qemu_mts.finger_slot[i].y * qemu_state->scale) - (qemu_mts.finger_point_size / 2);
            r.w = r.h = qemu_mts.finger_point_size;

            SDL_BlitSurface(qemu_mts.finger_point, NULL, qemu_state->surface_screen, &r);
        }

        SDL_FreeSurface(down_screen);
    }

    /* If 'x', 'y', 'w' and 'h' are all 0, SDL_UpdateRect will update the entire screen.*/

    SDL_UpdateRect(qemu_state->surface_screen, 0, 0, 0, 0);

#ifndef SDL_THREAD
    pthread_mutex_unlock(&sdl_mutex);
#endif
}

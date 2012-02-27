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
 * @file defines.h
 * @brief - header of file these are all structure defines here in emulator
 */

#ifndef DEFINES_H
#define DEFINES_H
#ifdef  __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <gtk/gtk.h>

#ifndef _WIN32
#else
#define ENABLE_LIMO
#endif

#define LOCALHOST   "127.0.0.1"
#define SENSOR_PORT 3581

/* define UNFS_THREAD, to enable the integrated NFS server */
#define UNFS_THREAD                 0
/* Multi boot */
#define ENABLE_MULTI        0

//#define _THREAD                   1
//#define _ROTATE   1
#define DISABLE_SMOTION
/**
 * GTK window size setting
 */
#define DRAWWIDTH                   240                     /* FrameBuffer Default LCD Width */
#define DRAWHEIGHT                  320                     /* FrameBuffer Default LCD Height */
#define BPP                         16                      /* FrameBuffer Default BPP */
#define FB_SIZE                     DRAWWIDTH * DRAWHEIGHT * (BPP /8)       /* FrameBuffer Byte Size */

#define DEFAULT_SKIN_FILE_FILTER        "*.dbi"                     /* Default SKIN Filter Filter */
#define DEFAULT_SKIN_FILE_FILTER_NAME   "SKIN(*.dbi)"               /* Default SKIN Filter Filter */

#define MAX_PID                     256                     /* maximum count of working processes */
#define MAX_ARGS                    10                      /* maxium argument count in executing process */
#define MAXPATH                     512                 /* maximum path length */

#define APPLICATION_DIALOG_OPEN     2
#define IMAGE_FILE_SAVE             3
#define LOG_FILE_OPEN               4
#define LOG_FILE_SAVE               5

/* Define for setting window menu */
#define WRITE_ERROR                 -3
#define INVALID                     -2
#define PARSE_ERROR                 -1
#define IMAGE_TAG                   "IMAGE"
#define IMAGE_TAG_P                 "IMAGE_P"
#define LCD_TAG                     "LCD"
#define LCDSUB_TAG                  "SUBLCD"
#define KEY_TAG                     "KEY"
#define X86                         "x86"
#define ARM                         "arm"
#define DEFAULT_TARGET              "default"
#define EMULATOR_X86                "emulator-x86"
#define EMULATOR_ARM                "emulator-arm"
#define EMUL_LOGFILE                "/emulator.log"
#define EMULMGR_LOGFILE             "/emulator-manager.log"
#define COMMON_GROUP                "COMMON"
#define EMULATOR_GROUP              "EMULATOR"
#define QEMU_GROUP                  "QEMU"
#define SKIN_PATH_KEY               "SKIN_PATH"
#define ALWAYS_ON_TOP_KEY           "ALWAYS_ON_TOP"
#define ENABLE_SHELL_KEY            "ENABLE_SHELL"
#define ENABLE_TELEPHONY_EMULATOR_KEY   "ENABLE_TELEPHONY_EMULATOR"
#define ENABLE_GPSD_KEY             "ENABLE_GPSD"
#define ENABLE_COMPASS_KEY          "ENABLE_COMPASS"
#define CMD_TYPE_KEY                "COMMAND_TYPE"
#define MAIN_X_KEY                  "MAIN_X"
#define MAIN_Y_KEY                  "MAIN_Y"
#define SCALE_KEY                   "SCALE"
#define TERMINAL_TYPE_KEY           "TERMINAL_TYPE"
#define EMULATOR_ARCH               "EMULATOR_ARCH"
#define HTTP_PROXY_KEY              "HTTP_PROXY"
#define DNS_SERVER_KEY              "DNS_SERVER"
#define TELNET_TYPE_KEY             "TELNET_TYPE"
#define TELNET_PORT_KEY             "TELNET_PORT"
#define TELNET_CONSOLE_COMMAND_TYPE_KEY "TELNET_CONSOLE_COMMAND_TYPE"
#define TELNET_CONSOLE_COMMAND_KEY      "TELNET_CONSOLE_COMMAND"
#define SDCARD_TYPE_KEY             "SDCARD_TYPE"
#define SDCARD_PATH_KEY             "SDCARD_PATH"
/* for saving emulator state */
#define SAVEVM_KEY                  "SAVEVM"
#define KVM_KEY                     "KVM"
#define BINARY_KEY                  "BINARY"
#define LOG_LEVEL_KEY               "LOG_LEVEL"
#define DEBUG_QEMU_KEY              "DEBUG_QEMU"
#define SNAPSHOT_SAVED_KEY          "SNAPSHOT_SAVED"
#define SNAPSHOT_SAVED_DATE_KEY     "SNAPSHOT_DATE"
#define DISK_TYPE_KEY               "DISK_TYPE"
#define DISK_PATH_KEY               "DISK_PATH"
#define BASEDISK_PATH_KEY           "BASEDISK_PATH"
#define EMULATOR_OPTION_KEY         "EMULATOR_OPTION"
#define QEMU_OPTION_KEY             "QEMU_OPTION"
#define MAJOR_VERSION_KEY           "MAJOR_VERSION"
#define MINOR_VERSION_KEY           "MINOR_VERSION"

#define TARGET_LIST_GROUP           "TARGET_LIST"
#define ETC_GROUP                   "ETC"
#define ADDITIONAL_OPTION_GROUP     "ADDITIONAL_OPTION"

#define CUSTOM_GROUP                "Custom"
#define HARDWARE_GROUP              "HARDWARE"
#define RESOLUTION_KEY              "RESOLUTION"
#define BUTTON_TYPE_KEY                         "BUTTON_TYPE"
#define RAM_SIZE_KEY                "RAM_SIZE"
#define DPI_KEY                     "DPI"
#define VERSION_GROUP               "VERSION"
#define FOLDER_CLOSE                "folder_close"
#define FOLDER_OPEN                 "folder_open"
#define LED_ON                      "led_on"
#define LED_OFF                     "led_off"
#define PORTRAIT                    "Portrait"
#define LANDSCAPE                   "Landscape"
#define REVERSE_PORTRAIT            "Reverse Portrait"
#define REVERSE_LANDSCAPE           "Reverse Landscape"
#define KEYBOARD_ON                 "keyboard_on"
#define KEYBOARD_OFF                "keyboard_off"
#define QUATER_SIZE                 "0.25"
#define HALF_SIZE                   "0.5"
#define THREE_QUATERS_SIZE              "0.75"
#define ACTUAL_SIZE                 "1.0"

#define PLATFORM_TYPE_KEY           "RELEASE"
#define DEFAULT_TARGET_KEY          "DEFAULT_TARGET"
#define SBOX_SKIN_KEY               "EmulatorSkinPath"
#define SBOX_EXEC_KEY               "PackagePath"
#define DEFAULT                     "default"

/* buffer generation */
#ifndef MAXBUF
#define MAXBUF                      512
#endif

#define MIDBUF                      128
#define QEMUARGC                    70
#define DIALOG_MAX_WIDTH            70
#define NON_BUTTON_REGION           -1

/* Tag for getting GTK OBJECT POINTER */
#define MAX_PROGRAM                 5                       /* program count of latest list */
#define MODE_MAX                    4                       /* maximum MODE count */
#define EVENT_INFO_MAX              2                       /* maximum EVENT count */
#define KEY_MAX_COUNT               256                     /* maximum KEY count */
#define LED_MAX_COUNT               4                       /* maximum LED count */
#define LCD_MAX                     3                       /* maximum  LCD count */
#define VALUE_MAX                   3                       /* maximum EVENT value count */

#define FILE_IN_USE                 1
#define FILE_NOT_USE                0

#define FILE_NOT_EXISTS             0
#define FILE_EXISTS                 1
#define FILE_EXISTS_IN_USE          2                       /* not using in qemu emulator */

#define NFS_TARGET                  0
#define DISK_TARGET                 1

#define REBOOT                      2
#define POWER_ON                    1
#define POWER_OFF                   0
#define NORMAL_BOOT                 -1

#define MAX_KEY_NUM 73

#define EMULATOR_DOMAIN         "Emulator"

#define SHUTDOWNTYPE_RELOAD 0x01
#define SHUTDOWNTYPE_EXIT 0x02
#define SHUTDOWNTYPE_SHUTDOWN 0x03

#define MAX_EMULFB 3
/* macro to find the position of skin */
/*#define INSIDE(_x, _y, _r)                                    \
        ((_x >= (_r).x) && (_x < ((_r).x + (_r).w)) &&  \
        (_y >= (_r).y) && (_y < ((_r).y + (_r).h)))*/
#define INSIDE(_x, _y, _r, _s)                                                                 \
               ((_x >= (_r).x * _s) && (_x < ((_r).x + (_r).w) * _s) &&        \
               (_y >= (_r).y * _s) && (_y < ((_r).y + (_r).h) * _s))


/*The below macros are for dual display */
extern int intermediate_section;
#define INSIDE_LCD_0_180(_x, _y, _r)                                    \
        ((_x >= (_r).x) && (_x < ((_r).x + intermediate_section+ (_r).w/(_r).s)) && \
        (_y >= (_r).y) && (_y < ((_r).y + (_r).h/(_r).s)))
#define INSIDE_LCD_90(_x, _y, _r)                                   \
        ((_x >= (_r).x) && (_x < ((_r).x + (_r).w/(_r).s)) &&   \
        (_y >= (_r).y) && (_y < ((_r).y + + intermediate_section + (_r).h/(_r).s)))
#define INSIDE_LCD_270(_x, _y, _r)                                  \
        ((_x >= (_r).x) && (_x < ((_r).x + (_r).w/(_r).s)) &&   \
        (_y >= (_r).y) && (_y < ((_r).y +  intermediate_section + (_r).h/(_r).s)))

/*The below macro is for single display */
/*#define INSIDE_LCD(_x, _y, _r)                                    \
        ((_x >= (_r).x) && (_x < ((_r).x  + (_r).w/(_r).s)) &&  \
        (_y >= (_r).y) && (_y < ((_r).y + (_r).h/(_r).s)))*/
#define INSIDE_LCD(_x, _y, _r, _s)                                                                     \
               ((_x >= (_r).x * _s) && (_x < (((_r).x + (_r).w) * _s)) &&      \
               (_y >= (_r).y * _s) && (_y < (((_r).y + (_r).h) * _s)))


/* macro to insert the delimiter into menu */
#define MENU_ADD_SEPARTOR(K) {\
            menu_item = gtk_separator_menu_item_new ();\
            gtk_container_add (GTK_CONTAINER (K), menu_item);\
            gtk_widget_show (menu_item);\
            }
/* macro for Emulator Manager */
#define SDCARD_SIZE_256     "256"
#define SDCARD_SIZE_512     "512"
#define SDCARD_SIZE_1024    "1024"
#define SDCARD_SIZE_1536    "1536"
#define SDCARD_DEFAULT_SIZE     1
# define VT_NAME_MAXBUF         21
#define RAM_SIZE_512    "512"
#define RAM_SIZE_768    "768"
#define RAM_SIZE_1024   "1024"
#define HVGA    "HVGA(320x480)"
#define WVGA    "WVGA(480x800)"
#define WSVGA   "WSVGA(600x1024)"
#define HD      "HD(720x1280)"
#define HVGA_VALUE  "320x480"
#define WVGA_VALUE  "480x800"
#define WSVGA_VALUE "600x1024"
#define HD_VALUE    "720x1280"
#define RAM_DEFAULT_SIZE    0
#define RAM_768_SIZE    1
#define RAM_1024_SIZE   2
#define RESOLUTION_DEFAULT_SIZE 1
#define RESOLUTION_HVGA 0
#define RESOLUTION_WVGA 1
#define RESOLUTION_WSVGA    2
#define RESOLUTION_HD   3
#define CREATE_MODE 1
#define DELETE_MODE 2
#define MODIFY_MODE 3
#define RESET_MODE 4
#define DELETE_GROUP_MODE 5


/* Front UI TYPE Enum */
enum {
    NON_LCD_REGION = -1,
    LCD_REGION,
    DUAL_LCD_REGION
};

/* PID Info List Column Index */
enum {
    PID_COLUMN = 0,
    TEXT_COLUMN,
    N_COLUMNS
};

/* event type of Touch pad */
enum {
    TOUCH_RELEASE = 0,
    TOUCH_PRESS = 1,
    TOUCH_DRAG = 1
};

/* structure to save EMULATOR Default Informations */
enum {
    STANDALONE_MODE=0,
    ISE_MODE
};

/* conf value mode */
enum {
    CONF_INIT_MODE=0,
    CONF_EXIST_STARTUP_MODE,
    CONF_DEFAULT_MODE
};

/* array index */
enum {
    REGION_0,
    REGION_1,
    REGION_2,
    REGION_3,
    REGION_4,
    REGION_5,
    REGION_6,
    REGION_7,
    REGION_8,
    REGION_9,
    REGION_ASTERISK,
    REGION_POUND,
    REGION_UP,
    REGION_DOWN,
    REGION_LEFT,
    REGION_RIGHT,
    REGION_OK,
    REGION_SEND,
    REGION_END,
    REGION_CLEAR,
    REGION_F1,
    REGION_F2,
    REGION_F3,
    REGION_F4,
    REGION_VOLUP,
    REGION_VOLDOWN,
    REGION_LCD,
    REGION_SUBLCD,
    REGION_CNT
};


/**
 *  @brief structure for command line option
 */
typedef struct _STARTUP_OPTION
{
    gchar       *disk;                  /**<Target name> */
    gchar       *vtm;                   /**<VTM name>**/
    int         run_level;              /**<run level> */
    gboolean    target_log;             /**<If ture, the target log of emulator is printed */
    gint        mountPort;
    gint        ssh_port;
    gint        telnet_port;
    gboolean    no_dump;
} STARTUP_OPTION;


typedef struct _keypad_data {
    char* image;        /* icon fiel */
    int width;          /* width of image file */
    int height;         /* heigth of image file */
    int keycode;        /* kernel keycode defined in /linux/input.h */
}keypad_data;


/**
 * @brief   structure to transcode GDK KEY
 */
typedef struct __GDK_KEY_STRING__ {
    char *pKeyString;
    int nGdkKeyCode;
} GDK_KEY_STRING_DATA;


/**
 * @brief   structure of image file info in skin
 */
typedef struct _SkinImgInfo {
    GdkPixbuf *pPixImg;         /*Front Image Button UP */
    GdkPixbuf *pPixImg_P;       /*Front Image Button DOWN */
    GdkPixbuf *pPixImgLed;      /*Front LED Image Button UP */
    GdkPixbuf *pPixImgLed_P;    /*Front LED Image Button Down */
    int nImgWidth;              /*Front Image Width */
    int nImgHeight;             /*Front Image Height */
} SkinImgInfo;


/**
 * @brief   structure of frame buffer informations
 */
typedef struct _FBINFO {
    int nDrawWidth;         /* FrameBuffer Width */
    int nDrawHeight;        /* FrameBuffer Height */
    int nBPP;               /* FrameBuffer BPP */
    int nonstd;             /* 0: RGB, 1: YUV422, 2:YUV420 */
    int nFB_Size;           /* FrameBuffer Byte Size */
    int nXoffset;           /* FrameBuffer X Offset */
    int nYoffset;           /* FrameBuffer Y Offset*/
    int nLineLen;           /* Framebuffer BPL */
    int nfbFD;              /* Framebuffer Open FD */
    char *LcdScreenBuf;     /* Driver FrameBuffer Pointer */
    int nRGB_Size;          /* RGB Buf Size */
    guchar *GdkRGBBuf;      /* GTK FrameBuffer Pointer */
} FBINFO;


/**
 * @brief   structure of showing the region
 */
typedef struct _region {
    int x;
    int y;
    int w;
    int h;
    float s;
    int split;
} region;


/* main, keypressed image filename char
 * @brief   Normal, Press image filename
 */
typedef struct _image_list {
    char *main_image;
    char *keypressed_image;
    char *led_main_image;
    char *led_keypressed_image;
    char *splitted_area_image;
} image_list_data;


/**
 * @brief       structre to save the LCD ID and information
 */
typedef struct _lcd_list {
    int id;
    int bitsperpixel;
    int nonstd;
    region lcd_region;
} lcd_list_data;


/**
 * @brief       structure to save LED id and name, etc. information
 */
typedef struct _led_list {
    char *id;
    char *name;
    char *imagepath;
    region led_region;
} led_list_data;


/**
 * @brief       structure to save the KEY EVENT value
 */
typedef struct _event_value {
    int key_code;
    char *key_name;
} event_value_data;


/**
 * @brief       structure to save the KEY EVENT value
 */
typedef struct _event_info {
    char *event_id;
    event_value_data event_value[VALUE_MAX];
    int event_value_cnt;
    char *keyboard;
    int gdk_key_code;
} event_info_data;


/**
 * @brief       structure to save the key position and keyboard event
 */
typedef struct _key_map {
    region key_map_region;
    event_info_data event_info[EVENT_INFO_MAX];
    char *tooltip;
    int event_info_cnt;
} key_map_list_data;


/**
 * @brief       structure to save the everything of Mode
 */
typedef struct _mode {
    int id;
    char *name;
    region REGION;      /* there is in XML, but is it really needed? */
    image_list_data image_list;
    lcd_list_data lcd_list[MAX_EMULFB];
    led_list_data led_list[LED_MAX_COUNT];
    key_map_list_data key_map_list[KEY_MAX_COUNT];
    int key_map_list_cnt;
    int lcd_list_cnt;
    int led_list_cnt;
} mode_list;


/**
 * @brief       structure to save the EVENT ID and VALUE
 */
typedef struct _event_prop {
    char event_eid[256];
    char event_evalue[256];
} event_prop;


/**
 * @brief       structure to save the EVENT
 */
typedef struct _event_menu {
    char name[256];
    event_prop event_list[10];
    int event_list_cnt;
} event_menu_list;


/**
 * @brief       structure to save the DBI parsing information
 */
typedef struct _PHONEMODELINFO{
    int mode_cnt;
    mode_list mode[MODE_MAX];
    SkinImgInfo mode_SkinImg[MODE_MAX];
    SkinImgInfo default_SkinImg[MODE_MAX];
    int cover_mode_cnt;
    mode_list cover_mode;
    SkinImgInfo cover_mode_SkinImg;

    event_menu_list event_menu[10];
    int event_menu_cnt;
    char model_name[64];
    int dual_display;

} PHONEMODELINFO;


/**
 *  @brief  structure to save close GTK flag list
 *
 */
typedef struct _EXITFLAG {
    int exit_flag;              /* Main Window EXIT FLAG */
    int sub_exit_flag;
    int reload_flag;
} EXITFLAG;


/**
 *  @brief  structure to save Time Out Event Tag *
 */
typedef struct _TIMERTAG {
    int timer_tag;              /* Main Window Refresh Timer Tag */
    int play_timer_tag;         /* Logging Timer Tag */
    int sub_timer_tag;          /* Sub Window Refresh Timer Tag */
} TIMERTAG;


/**
 *  @brief  structer to save GTK UI flags *
 */
typedef struct _UIFLAG {
    int last_index;
    int button_press_flag;
    int key_button_press_flag;
    int frame_buffer_ctrl;
    float scale;
    int current_mode;
    int config_flag;
    int PID_flag;
    gboolean is_ei_run;
    gboolean is_em_run;
    gboolean is_gps_run;
    gboolean is_compass_run;
    gboolean is_screenshot_run;
    int sub_window_flag;
    gboolean network_read_flag;
} UIFLAG;


/**
 * @brief   structure to save the latest list
 */
typedef struct _PROGRAMNAME{
    int nDate;
    char arName[MAXBUF];
} PROGRAMNAME;


/**
 * @brief   Structure to Save the Status of Emulator for Configuration of QEMU
 */
typedef struct _QEMUCONFIG
{
//  char initrd_path[MAXBUF];
    int use_host_http_proxy;
    int use_host_dns_server;
    char telnet_port[MAXBUF];
    int telnet_type;
    int sdcard_type;
    char sdcard_path[MAXBUF];
    int diskimg_type;
    char diskimg_path[MAXBUF];      /* Disk Image Path */
/* serial console command : start */
    int serial_console_command_type;
    char serial_console_command[MAXBUF];

/*      param to store user preference for savevm */
    int save_emulator_state;
    int snapshot_saved;
    char snapshot_saved_date[MAXBUF];
/* end */
}QEMUCONFIG;


/**
 * @brief   Structure to Save the Status of Emulator for Configuration of ISE
 */
typedef struct _CONFIGUATION
{
    int always_on_top;
    int enable_shell;
    int enable_telephony_emulator;
    int enable_gpsd;
    int enable_compass;
    int cmd_type;
    int main_x;
    int main_y;
    int scale;
    int mount_port;

    gchar target_path[MAXBUF];
    gchar skin_path[MAXBUF];

    QEMUCONFIG qemu_configuration;
} CONFIGURATION;


/**
 * @brief   structure to save EMULATOR Information
 */
typedef struct _SYSINFO {
    char conf_file[MAXPATH];
    char virtual_target_name[MAXBUF];
    char virtual_target_info_file[MAXPATH];
    char target_list_file[MAXPATH];
} SYSINFO;


/**
 * @brief   structure to save PLATFORM Information
 */
typedef struct _PLATINFO {
    char release_type[MAXBUF];
    char target[MAXBUF];
    char skin[MAXBUF];
} PLATINFO;

typedef struct _VIRTUALTARGETINFO {
    char virtual_target_name[MAXBUF];
    char major_version[MAXBUF];
    int minor_version;
    char resolution[MAXBUF];
    int button_type;
    int sdcard_type;
    int disk_type;
    char basedisk_path[MAXBUF];
    char sdcard_path[MAXBUF];
    int ram_size;
    char dpi[MAXBUF];
    char diskimg_path[MAXBUF];      /* Disk Image Path */
    int snapshot_saved;
    char snapshot_saved_date[MAXBUF];
} VIRTUALTARGETINFO;

#ifdef __cplusplus
}
#endif
#endif /* ifndef DEFINES_H */

/**
 * vim:set tabstop=4 shiftwidth=4 foldmethod=marker wrap:
 *
 */


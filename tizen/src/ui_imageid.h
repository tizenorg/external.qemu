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


#ifndef __UI_IMAGE_H__
#define __UI_IMAGE_H__

//============================== ISE Child Window IDs ============================== 
#define EMULATOR_ID                 0x0000
#define EVENT_INJECTOR_ID               0x0100
#define EVENT_MANAGER_ID                0x0200
#define SEARCH_POPUP_ID                 0x0201
#define COMMAND_WINDOW_ID               0x0300
#define OPTION_ID                       0x0400
#define KEYPAD_ID                       0x0500
#define SCREEN_SHOT_ID                  0x0600
#define QEMU_ID                         0x0700
#define VTM_MAIN_ID                     0x0800
#define VTM_CREATE_ID                   0x0900

//#define RESOURCE_MONITOR_ID           0x0600

#define EMULATOR_SDL_WIWDGET            "EMULATOR_SDL_WIWDGET"
#define SUB_MENU_ITEM_1                 "SUB_MENU_ITEM_1"
#define POPUP_MENU                      "POPUP_MENU"

#define MAIN_DRAWING_AREA               "main_drawing_area"
#define SUB_DRAWING_AREA                "sub_drawing_area"
#define PID_LIST_WIDGET                 "process_id_list_widget"
#define RECENT_WIDGET                   "Recent_Widget"
#define RECENT_SUB_WIDGET               "Recent_Sub_Menu"
#define RECENT_SUBSUB_WIDGET            "Recent_SubSub_Menu"    /* add SUBSUB Widget for Run App Menu */

#define MENU_POWER_ON                   "MENU_POWER_ON"
#define MENU_POWER_OFF                  "MENU_POWER_OFF"
#define MENU_EVENT_EVALUE               "MENU_EVENT_EVALUE"
#define MENU_RECENT_WIDGET              "MENU_RECENT_WIDGET"
#define MENU_EVENT_INJECTOR             "MENU_EVENT_INJECTOR"
#define MENU_EVENT_MANAGER              "MENU_EVENT_MANAGER"
#define MENU_VKEYPAD                    "MENU_VKEYPAD"
#define MENU_GPS                        "MENU_GPS"

#define MENU_COMPASS                    "MENU_COMPASS"
#define ENTRY_COMPASS_X                 "ENTRY_GPS_FILE_X"
#define ENTRY_COMPASS_Y                 "ENTRY_GPS_FILE_Y"
#define ENTRY_COMPASS_Z                 "ENTRY_GPS_FILE_Z"
#define ENTRY_COMPASS_TEMP              "ENTRY_GPS_FILE_TEMP"

#define MENU_GPS_FILE                   "MENU_GPS_FILE"
#define MENU_GPS_STATUS                 "MENU_GPS_STATUS"
#define MENU_GPS_ACTIVE                 "MENU_GPS_ACTIVE"
#define MENU_GPS_INACTIVE               "MENU_GPS_INACTIVE"

#define OPTION_TARGET                   "OPTION_TARGET"
#define OPTION_NFS                      "OPTION_NFS"
#define OPTION_DISKIMG                  "OPTION_DISKIMG"
#define OPTION_DISKIMG_BUTTON           "OPTION_DISKIMG_BUTTON"
#define OPTION_SKIN_BUTTON              "OPTION_SKIN_BUTTON"
#define OPTION_SKIN_ENTRY               "OPTION_SKIN_ENTRY"
#define OPTION_CONF_FRAME               "OPTION_CONF_FRAME"
#define OPTION_CONF_VBOX                "OPTION_CONF_VBOX"
#define OPTION_VIRTUAL_TARGET_COMBOBOX  "OPTION_VIRTUAL_TARGET_COMBOBOX"
#define OPTION_SCALE_HALF_BUTTON        "OPTION_SCALE_HALF_BUTTON"
#define OPTION_SCALE_ONE_BUTTON         "OPTION_SCALE_ONE_BUTTON"
#define OPTION_TELNET_PORT_ENTRY        "OPTION_TELNET_PORT_ENTRY"
#define OPTION_TELNET_NOWAIT_BUTTON         "OPTION_TELNET_NOWAIT_BUTTON"
#define  OPTION_TELNET_BUTTON   " OPTION_TELNET_BUTTON"
#define  OPTION_SERIAL_CONSOLE_BUTTON    " OPTION_SERIAL_CONSOLE_BUTTON"
#define OPTION_SERIAL_CONSOLE_ENTRY     "OPTION_SERIAL_CONSOLE_ENTRY"
#define OPTION_LAUNCH_BUTTON        "OPTION_LAUNCH_BUTTON"
#define OPTION_NETWORK_IFNAME_ENTRY     "OPTION_NETWORK_IFNAME_ENTRY"
#define OPTION_SERVER_IP_ENTRY          "OPTION_SERVER_IP_ENTRY"
#define OPTION_RFS_IP_ENTRY             "OPTION_RFS_IP_ENTRY"
#define OPTION_GATEWAY_ENTRY            "OPTION_GATEWAY_ENTRY"
#define OPTION_SUBNET_ENTRY             "OPTION_SUBNET_ENTRY"
#define OPTION_HTTP_PROXY_ENTRY         "OPTION_HTTP_PROXY_ENTRY"
#define OPTION_USE_HOST_PROXY           "OPTION_USE_HOST_PROXY"
#define OPTION_USE_HOST_DNS     "OPTION_USE_HOST_DNS"
#define OPTION_DNS_SERVER_ENTRY1            "OPTION_DNS_SERVER_ENTRY1"
#define OPTION_DNS_SERVER_ENTRY2            "OPTION_DNS_SERVER_ENTRY2"
#define OPTION_ALWAYS_ON_TOP_BUTTON     "OPTION_ALWAYS_ON_TOP_BUTTON"
#define OPTION_SDCARD_BUTTON            "OPTION_SDCARD_BUTTON"
#define OPTION_SDCARD_IMG_BUTTON        "OPTION_SDCARD_IMG_BUTTON"
#define OPTION_SNAPSHOT_BOOT        "OPTION_SNAPSHOT_BOOT"
#define OPTION_SNAPSHOT_SAVED_DATE_ENTRY        "OPTION_SNAPSHOT_SAVED_DATE_ENTRY"

#define VTM_CREATE_SDCARD_COMBOBOX      "VTM_CREATE_SDCARD_COMBOBOX"
#define VTM_CREATE_RAM_COMBOBOX         "VTM_CREATE_RAM_COMBOBOX"
#define VTM_CREATE_RESOLUTION_COMBOBOX          "VTM_CREATE_RESOLUTION_COMBOBOX"

//============================== ISE Window Menu IDs =============================
#define ID_MENU_FILE_OPEN               0x0001
#define ID_MENU_FILE_SAVE               0x0002
#define ID_MENU_FILE_SAVE_AS            0x0003
#define ID_MENU_ALWAYS_ON_TOP           0x0004
#define ID_MENU_EMULATOR                0x0005
#define ID_MENU_EVENT_INJECTOR          0x0006
#define ID_MENU_EVENT_MANAGER           0x0007
#define ID_MENU_COMMAND                 0x0009
#define ID_MENU_RESOURCE_MONITOR        0x0010

#endif

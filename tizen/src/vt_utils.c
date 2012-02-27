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


#include "vt_utils.h"
#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, vt_utils);

gchar **get_virtual_target_groups(gchar *filepath, int *num)
{
    GKeyFile *keyfile;
    GError *error = NULL;
    gchar **group_list = NULL;
    gsize length;

    keyfile = g_key_file_new();

    if(!g_key_file_load_from_file(keyfile, filepath, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        ERR("Loading file is failed : %s\n", filepath);
        show_message("Lodaing file failed", filepath);
        return NULL;
    }

    group_list = g_key_file_get_groups(keyfile, &length);

    if(group_list == NULL || length == 0) {
        ERR("no group under %s\n", filepath);
        return NULL;
    }

    *num = length;

    g_key_file_free(keyfile);
    return group_list;
}

gchar **get_virtual_target_list(gchar *filepath, const gchar *group, int *num)
{
    GKeyFile *keyfile;
    GError *error = NULL;
    gchar **target_list = NULL;
    gsize length;

    keyfile = g_key_file_new();

    if(!g_key_file_load_from_file(keyfile, filepath, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        ERR("Loading file is failed : %s\n", filepath);
        show_message("Lodaing file failed", filepath);
        return NULL;
    }

    target_list = g_key_file_get_keys(keyfile, group, &length, &error);

    if(target_list == NULL || length == 0) {
        ERR("no targets under group %s\n", group);
    }

    *num = length;

    g_key_file_free(keyfile);
    return target_list;
}


int is_valid_target_list_file(SYSINFO *pSYSTEMINFO)
{
    int status = 0;
    gchar *target_list_filepath = NULL;

    target_list_filepath = get_targetlist_filepath();

    status = is_exist_file(target_list_filepath);

    if (status != -1) {
        sprintf(pSYSTEMINFO->target_list_file, "%s", target_list_filepath);
    }

    g_free(target_list_filepath);

    return status;
}
/* used for target name check on create and modify window
    valid target name includes numbers, alphabet, -, _  */
void escapeStr(const char* str, char* dst)
{
    int i = 0;
    char ch;
    while(*str)
    {
        ch = *str++;
        
        if((ch >= 'a' && ch <= 'z') || 
                (ch >= 'A' && ch <= 'Z') || 
                (ch >='0' && ch <= '9') || 
                (ch == '-') || (ch == '_'))
            dst[i++] = ch;
    }
    dst[i] = '\0';
}

/* used for resolution setting
*   change like "HVGA(320x480)" -> "320x480" */
char* escape_resolution_str(const char* str)
{
    int i = 0;
    char ch;
    char *dst = malloc(20);
    while(*str)
    {
        ch = *str++;
        
        if((ch >='0' && ch <= '9') || (ch == 'x'))
            dst[i++] = ch;
    }
    dst[i] = '\0';      

    return dst;
}

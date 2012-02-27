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


#include <assert.h>
#include "fileio.h"

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen_sdk);
MULTI_DEBUG_CHANNEL(tizen_sdk, fileio);

extern STARTUP_OPTION startup_option;
#ifdef _WIN32
static void dos_path_to_unix_path(char *path)
{
    int i;

    for (i = 0; path[i]; i++)
        if (path[i] == '\\')
            path[i] = '/';
}

static void unix_path_to_dos_path(char *path)
{
    int i;

    for (i = 0; path[i]; i++)
        if (path[i] == '/')
            path[i] = '\\';
}
#endif

/**
 * @brief   check about file
 * @param   filename : file path (ex: /home/$(USER_ID)/.tizen_sdk/simulator/1/emulator.conf)
 * @return  exist normal(1), not exist(0), error case(-1))
 * @date    Oct. 22. 2009
 * */

int is_exist_file(gchar *filepath)
{
    if ((filepath == NULL) || (strlen(filepath) <= 0))  {
        ERR( "filepath is incorrect.\n");
        return -1;
    }

    if (strlen(filepath) >= MAXBUF) {
        ERR( "file path is too long. (%s)\n", filepath);
        return -1;
    }

    if (g_file_test(filepath, G_FILE_TEST_IS_DIR) == TRUE) {
        INFO( "%s: is not a file, is a directory!!\n", filepath);
        return -1;
    }

    if (g_file_test(filepath, G_FILE_TEST_EXISTS) == TRUE) {
        TRACE( "%s: exists normally!!\n", filepath);
        return FILE_EXISTS;
    }

    INFO( "%s: not exists!!\n", filepath);
    return FILE_NOT_EXISTS;
}

/* get_sdk_root = "~/tizen_sdk/" */
gchar *get_sdk_root(void)
{
    static gchar *sdk_path = NULL;
    gchar *info_file_subpath = NULL;
    gchar *info_file_fullpath = NULL;
    gchar *file_buf = NULL;

    if (!sdk_path)
    {
#ifdef __linux__
        const gchar *home_path = g_get_home_dir();
        info_file_subpath = "/.TizenSDK/tizensdkpath";

        int len = strlen(home_path) + strlen(info_file_subpath) + 1;
        info_file_fullpath = g_malloc0(len);
        snprintf(info_file_fullpath, len, "%s%s", home_path, info_file_subpath);
#elif _WIN32
        HKEY hKey;
        TCHAR szDefaultPath[_MAX_PATH] = {0};
        DWORD dwBufLen = MAX_PATH;

        RegOpenKeyEx(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
            0, KEY_QUERY_VALUE, &hKey);
        RegQueryValueEx(hKey, "Local AppData", NULL, NULL, (LPBYTE)szDefaultPath, &dwBufLen);
        RegCloseKey(hKey);

        info_file_subpath = "\\.TizenSDK\\tizensdkpath";

        int len = strlen(szDefaultPath) + strlen(info_file_subpath) + 1;
        info_file_fullpath = g_malloc0(len);
        snprintf(info_file_fullpath, len, "%s%s", szDefaultPath, info_file_subpath);
#endif

        if (!g_file_get_contents(info_file_fullpath, &file_buf, NULL, NULL)) {
            g_free(info_file_fullpath);
            return NULL;
        }

        sdk_path = strchr(file_buf, '=') + 1;

        g_free(info_file_fullpath);
    }

    return sdk_path;
}

/* get_sdb_path = "~/tizen_sdk/SDK/sdb/sdb" */
// After using this function, please call g_free().
gchar *get_sdb_path(void)
{
    gchar *sdb_fullpath = NULL;
    gchar *sdb_subpath = "/SDK/sdb/sdb";
    gchar *sdk_path = NULL;

    sdk_path = get_sdk_root();
    if (!sdk_path) {
        return NULL;
    }

#ifdef __linux__
    int len = strlen(sdk_path) + strlen(sdb_subpath) + 1;
    sdb_fullpath = g_malloc0(len);
    snprintf(sdb_fullpath, len, "%s%s", sdk_path, sdb_subpath);
#elif _WIN32
    int len = strlen(sdk_path) + strlen(sdb_subpath) + 5;
    sdb_fullpath = g_malloc0(len);
    snprintf(sdb_fullpath, len, "%s%s.exe", sdk_path, sdb_subpath);
    dos_path_to_unix_path(sdk_path);
#endif

    return sdb_fullpath;
}

/* exec_path = "~/tizen_sdk/Emulator/bin/emulator-manager" */
/*           = "~/tizen_sdk/Emulator/bin/emulator-x86" */
const gchar *get_exec_path(void)
{
    static gchar *exec_path = NULL;
    int len = 10;
    int r;

    /* allocate just once */
    if (exec_path)
        return exec_path;

    const char *env_path = getenv("EMULATOR_PATH");
    if (env_path) {
        exec_path = strdup(env_path);
        return exec_path;
    }

    while (1)
    {
        exec_path = malloc(len);
        if (!exec_path) {
            fprintf(stderr, "%s - %d: memory allocation failed!\n", __FILE__, __LINE__); exit(1);
        }

#ifndef _WIN32
        r = readlink("/proc/self/exe", exec_path, len);
        if (r < len) {
            exec_path[r] = 0;
            break;
        }
#else
        r = GetModuleFileName(NULL, exec_path, len);
        if (r < len) {
            exec_path[r] = 0;
            dos_path_to_unix_path(exec_path);
            break;
        }
#endif
        free(exec_path);
        len *= 2;
    }

    return exec_path;
}

/* get_root_path = "~/tizen_sdk/Emulator" */
const gchar *get_root_path(void)
{
    static gchar *root_path;
    static gchar *root_path_buf;

    if (!root_path)
    {
        const gchar *exec_path = get_exec_path();
        root_path_buf = g_path_get_dirname(exec_path);
        root_path = g_path_get_dirname(root_path_buf);
        g_free(root_path_buf);
    }

    return root_path;
}

/* get_bin_path = "~/tizen_sdk/Emulator/bin" */
const gchar *get_bin_path(void)
{
    static gchar *bin_path;

    if (!bin_path)
    {
        const gchar *exec_path = get_exec_path();
        bin_path = g_path_get_dirname(exec_path);
    }

    return bin_path;
}

/* get_baseimg_path = "~/tizen_sdk/Emulator/{ARCH}/emulimg{VERSION}.{ARCH}" */
const gchar *get_baseimg_path(void)
{
    const gchar *arch_path;
    static gchar *path;
    char* MAJOR_VERSION = NULL;
    char version_path[MAXPATH];
    gchar *target_list_filepath;
    char *arch = (char *)g_getenv(EMULATOR_ARCH);
    const gchar *exec_path = get_exec_path();
    target_list_filepath = get_targetlist_filepath();
    sprintf(version_path, "%s/version.ini",get_etc_path());
    MAJOR_VERSION = (char*)get_config_value(version_path, VERSION_GROUP, MAJOR_VERSION_KEY);
    char* subdir = NULL;
    if(!arch) /* for stand alone */
    {
        char *binary = g_path_get_basename(exec_path);
        if(strstr(binary, EMULATOR_X86))
            arch = g_strdup_printf(X86);
        else if(strstr(binary, EMULATOR_ARM))
            arch = g_strdup_printf(ARM);
        else
        {
            ERR( "binary setting failed\n");
            exit(1);
        }
        free(binary);
    }

    subdir = g_strdup_printf("/emulimg-%s.%s", MAJOR_VERSION, arch);

    arch_path = get_arch_path();
    path = malloc(strlen(arch_path) + strlen(subdir) + 2);
    strcpy(path, arch_path);
    strcat(path, subdir);

    free(MAJOR_VERSION);
    free(subdir);
    return path;
}

/* get_arch_path = "~/tizen_sdk/Emulator/{ARCH}" */
const gchar *get_arch_path(void)
{
    gchar *path_buf;
    static gchar *path;
    char *arch = (char *)g_getenv(EMULATOR_ARCH);

    const gchar *exec_path = get_exec_path();
    path_buf = g_path_get_dirname(g_path_get_dirname(exec_path));
    if(!arch) /* for stand alone */
    {
        char *binary = g_path_get_basename(exec_path);
        if(strstr(binary, EMULATOR_X86))
            arch = g_strdup_printf(X86);
        else if(strstr(binary, EMULATOR_ARM))
            arch = g_strdup_printf(ARM);
        else
        {
            ERR( "binary setting failed\n");
            exit(1);
        }
        free(binary);
    }

    path = malloc(strlen(path_buf) + strlen(arch) + 2);

    strcpy(path, path_buf);
    strcat(path, "/");
    strcat(path, arch);
    g_free(path_buf);

    return path;
}

/* get_etc_path = "~/tizen_sdk/simulator/etc" */
const gchar *get_etc_path(void)
{
    const char etcsubdir[] = "/etc";
    const char *path;
    static char *etc_path;

    if (etc_path)
        return etc_path;

    path = get_root_path();
    etc_path = malloc(strlen(path) + sizeof etcsubdir + 1);
    if (!etc_path) {
        ERR( "%s - %d: memory allocation failed!\n", __FILE__, __LINE__);
        exit(1);
    }

    strcpy(etc_path, path);
    strcat(etc_path, etcsubdir);

    if (g_file_test(etc_path, G_FILE_TEST_IS_DIR) == FALSE) {
        ERR( "no etc directory at %s\n", etc_path);
    }

    return etc_path;
}

/* get_skin_path = "~/tizen_sdk/simulator/skins" */
const gchar *get_skin_path(void)
{
    const char *skin_path_env;
    const char skinsubdir[] = "/skins";
    const char *path;

    static char *skin_path;

    if (skin_path)
        return skin_path;

    skin_path_env = getenv("EMULATOR_SKIN_PATH");
    if (!skin_path_env)
    {
        path = get_root_path();
        skin_path = malloc(strlen(path) + sizeof skinsubdir + 1);
        if (!skin_path) {
            fprintf(stderr, "%s - %d: memory allocation failed!\n", __FILE__, __LINE__);
            exit(1);
        }

        strcpy(skin_path, path);
        strcat(skin_path, skinsubdir);
    }
    else
        skin_path = strdup(skin_path_env);

    if (g_file_test(skin_path, G_FILE_TEST_IS_DIR) == FALSE) {
        fprintf(stderr, "no skin directory at %s\n", skin_path);
    }

    return skin_path;
}

/* get_tizen_tmp_path = "/tmp/tizen_sdk" (linux)                                      *
 * get env variables TMPDIR, TMP, and TEMP in that order (windows) */
const gchar *get_tizen_tmp_path(void)
{
    const char *tmp_path;
    const char subdir[] = "/tizen_sdk";
    static gchar *path;

    tmp_path = g_get_tmp_dir();
    path = malloc(strlen(tmp_path) + sizeof subdir + 1);
    if (!path) {
        fprintf(stderr, "%s - %d: memory allocation failed!\n", __FILE__, __LINE__);
        exit(1);
    }

    strcpy(path, tmp_path);
    strcat(path, subdir);

    return path;
}


/* get_data_path = "~/tizen_sdk/Emulator/{ARCH}/data" */
const gchar *get_data_path(void)
{
    static const char suffix[] = "/data";
    static gchar *data_path;

    if (!data_path)
    {
        const gchar *path = get_arch_path();

        data_path = malloc(strlen(path) + sizeof suffix + 1);
        assert(data_path != NULL);
        strcpy(data_path, path);
        strcat(data_path, suffix);
    }

    return data_path;
}

#ifdef _WIN32
/**
 * @brief   change_path_to_slash (\, \\ -> /)
 * @param   org path to change (C:\\test\\test\\test)
 * @return  changed path (C:/test/test/test)
 * @date    Nov 19. 2009
 * */
gchar *change_path_to_slash(gchar *org_path)
{
    gchar *path = strdup(org_path);

    dos_path_to_unix_path(path);

    return path;
}

gchar *change_path_from_slash(gchar *org_path)
{
    gchar *path = strdup(org_path);

    unix_path_to_dos_path(path);

    return path;
}
#endif


/* get_conf_path = "~/tizen_sdk/Emulator/{ARCH}/conf" */
const gchar *get_conf_path(void)
{
    static gchar *conf_path;

    static const char suffix[] = "/conf";

    const gchar *path = get_arch_path();
    conf_path = malloc(strlen(path) + sizeof suffix + 1);
    assert(conf_path != NULL);
    strcpy(conf_path, path);
    strcat(conf_path, suffix);

    return conf_path;
}
/* get_tizen_vms_arch_path = "/home/{USER}/.tizen_vms/{ARCH}" */
const gchar *get_tizen_vms_arch_path(void)
{
    char *tizen_vms_arch_path;
    char *tizen_vms = (char*)get_tizen_vms_path();
    char *arch = (char *)g_getenv(EMULATOR_ARCH);
    const gchar *exec_path = get_exec_path();
    if(!arch) /* for stand alone */
    {
        char *binary = g_path_get_basename(exec_path);
        if(strstr(binary, EMULATOR_X86))
            arch = g_strdup_printf(X86);
        else if(strstr(binary, EMULATOR_ARM))
            arch = g_strdup_printf(ARM);
        else
        {
            ERR( "binary setting failed\n");
            exit(1);
        }
        free(binary);
    }

    tizen_vms_arch_path = malloc(strlen(tizen_vms) + 1 + strlen(arch) + 1);
    assert(tizen_vms_arch_path != NULL);
    strcpy(tizen_vms_arch_path, tizen_vms);
    strcat(tizen_vms_arch_path, "/");
    strcat(tizen_vms_arch_path, arch);

    return tizen_vms_arch_path;
}

/* get_tizen_vms_path = "/home/{USER}/.tizen_vms" */
const gchar *get_tizen_vms_path(void)
{
    static const char tizen_vms[] = "/.tizen_vms";
#ifndef _WIN32
    char *homedir = (char*)g_getenv("HOME");
#else
    char *homedir = (char*)g_getenv("USERPROFILE");
#endif
    static gchar *tizen_vms_path;

    if(!homedir)
        homedir = (char*)g_get_home_dir();

    tizen_vms_path = malloc(strlen(homedir) + sizeof tizen_vms + 1);
    assert(tizen_vms_path != NULL);
    strcpy(tizen_vms_path, homedir);
    strcat(tizen_vms_path, tizen_vms);

    return tizen_vms_path;
}

/* get_screenshot_path = "/home/{USER}/.tizen_vms/screenshots" */
gchar *get_screenshots_path(void)
{
    const char subdir[] = "/screenshots";
    char *tizen_vms_path = (char*)get_tizen_vms_path();
    char *screenshots_path;

    screenshots_path = malloc(strlen(tizen_vms_path) + sizeof subdir + 1);
    assert(screenshots_path != NULL);
    strcpy(screenshots_path, tizen_vms_path);
    strcat(screenshots_path, subdir);

    return screenshots_path;
}

/*  get_targetlist_filepath  = " ~/tizen_VMs/{ARCH}/targetlist.ini" */
gchar *get_targetlist_filepath(void)
{
    gchar *targetlist_filepath = NULL;
    targetlist_filepath = calloc(1, 512);
    if(NULL == targetlist_filepath) {
        fprintf(stderr, "%s - %d: memory allocation failed!\n", __FILE__, __LINE__); exit(1);
    }

    const gchar *vms_path = get_tizen_vms_arch_path();
    sprintf(targetlist_filepath, "%s/targetlist.ini", vms_path);

    return targetlist_filepath;
}

/* get_virtual_target_path  "~/tizen_VMs/{ARCH}/virtual_target_name/" */
gchar *get_virtual_target_path(gchar *virtual_target_name)
{
    gchar *virtual_target_path = NULL;
    virtual_target_path = calloc(1,512);

    if(NULL == virtual_target_path) {
        fprintf(stderr, "%s - %d: memory allocation failed!\n", __FILE__, __LINE__); exit(1);
    }

    const gchar *conf_path = get_tizen_vms_arch_path();
    sprintf(virtual_target_path, "%s/%s/", conf_path, virtual_target_name);

    return virtual_target_path;
}

/* get_virtual_target_log_path  "~/tizen_VMs/{ARCH}/virtual_target_name/logs" */
gchar *get_virtual_target_log_path(gchar *virtual_target_name)
{
    gchar *virtual_target_log_path = NULL;
    virtual_target_log_path = calloc(1,512);

    if(NULL == virtual_target_log_path) {
        fprintf(stderr, "%s - %d: memory allocation failed!\n", __FILE__, __LINE__); exit(1);
    }

    const gchar *vms_path = get_tizen_vms_arch_path();
    sprintf(virtual_target_log_path, "%s/%s/logs", vms_path, virtual_target_name);

    return virtual_target_log_path;

}

int check_port(const char *ip_address, int port)
{
    struct sockaddr_in address;
    int sockfd, connect_status;

    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Socket Open error\n");
        return -2;
    }

    memset(&address, 0 , sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip_address);

    if(address.sin_addr.s_addr == INADDR_NONE)
    {
        fprintf(stderr, "Bad Address\n");
        close(sockfd);
        return -2;
    }
    connect_status = connect(sockfd, (struct sockaddr *)&address, sizeof(address));
    close(sockfd);
    return connect_status;
}


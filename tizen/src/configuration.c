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
#include "configuration.h"
#include "debug_ch.h"
#include "sdb.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, configuration);

#define EMUL_PIXELFORMAT_YUV422P           1
#define EMUL_PIXELFORMAT_YUV420P           2

#define RESOLUTION_COUNT                   4
#define BUTTON_TYPE_COUNT               4
/*
 *
 * *** config *** (target_path)
 *
 * If emulator.conf exists & target name is created by user, check platform config and so (set ld config and script path)
 * Else if emulator.conf exist & target is create by installer, read emulator.conf, nothing do by platform.conf
 * Else if emulator.conf no exist
 *    release type of platform exist ->
 *    release type of platform no exist -> default X2 -> create popup -> by select, change configuration & systeminfo
 *
 * A. configuration (char *)
 * init  (configuration.c init_structure - alloc) int init_configuration();
 * copy (sprintf, strcpy, pointer, strdup); -> sprintf
 * reload (by target path & skin path) (sprintf, strcpy, pointer, strdup) -> sprintf
 * destroy (configuration.c destroy_structure - free) destroy_structure(); -> remove
 *
 * B. systeminfo (platform_type - char *)
 * init
 * copy
 * reload -> X
 * destroy
 *
 * */


/**
 * @brief   set information to default init configuration structure
 * @param   status: CONF_INIT_MODE, CONF_DEFAULT_MODE, CONF_EXIST_STARTUP_MODE
 * @return  success(0)
 * @date    Nov 18. 2008
 * */

int fill_configuration(int status)
{
    int default_telnet_port = 1200;
    const gchar *data_path = get_data_path();

    /* 1. startup option parsing */

    /* 1.3 target setting */

    if (strlen(startup_option.disk) > 0) {
        configuration.qemu_configuration.diskimg_type = 1;
        if(strcmp(SYSTEMINFO.virtual_target_name, "default") == 0)
            strcpy(configuration.qemu_configuration.diskimg_path, startup_option.disk);
    }

    else {
#ifdef ENABLE_LIMO
        snprintf(configuration.qemu_configuration.diskimg_path, MAXBUF, "%s/coronado-simulator.img",data_path);
#else // inhouse
        snprintf(configuration.qemu_configuration.diskimg_path, MAXBUF, "%s/protector-simulator.img",data_path);
#endif
    }

    /* 2. CONF_INIT_MODE / CONF_EXIST_STARTUP_MODE*/

    if (status == CONF_INIT_MODE) {

        configuration.always_on_top = 0;
        /* 2.1 emulator config initial */
#ifndef _WIN32
        configuration.enable_shell = 1;
#else
        configuration.enable_shell = 0;
#endif
        configuration.enable_telephony_emulator = 0;
        configuration.enable_gpsd = 0;
        configuration.enable_compass = 0;

        configuration.main_x = 100;
        configuration.main_y = 100;

        /* 2.2 qemu config initial */

        configuration.qemu_configuration.use_host_http_proxy = 1;
        configuration.qemu_configuration.use_host_dns_server = 1;
//      snprintf(configuration.qemu_configuration.sdcard_path, MAXBUF, "%s/sdcard.img",data_path);

//      configuration.qemu_configuration.sdcard_type = 0;
        configuration.qemu_configuration.telnet_type = 1;

        if(ENABLE_MULTI)
        {
            while(check_port(LOCALHOST, default_telnet_port) == 0 )
                default_telnet_port += 3;
        }
        snprintf(configuration.qemu_configuration.telnet_port, MAXBUF, "%d", default_telnet_port);

        /* serial console command ==> start */
        configuration.qemu_configuration.serial_console_command_type = 0;
        snprintf(configuration.qemu_configuration.serial_console_command, MAXBUF, "/usr/bin/putty -telnet -P %s localhost",configuration.qemu_configuration.telnet_port);

//      configuration.qemu_configuration.snapshot_saved = 0;
/* end */
        return 0;
    }

    else if (status == CONF_EXIST_STARTUP_MODE) {
        return 0;
    }
    else
        INFO( "Configuration Structure fill error !\n");

    return 0;
}


/**
 * @brief   get config value list from emulator.conf file
 * @param   filename: configuration file path, group: EMULATOR_GROUP, field: RUNAPP_RECENT_LIST_KEY, CONFIGURATION
 * @return  fail(0), success(value)
 * @date    Nov 17. 2008
 * */
PROGRAMNAME *get_config_value_list(gchar *filepath, const gchar *group, const gchar *field)
{
    GKeyFile *keyfile;
    GError *error = NULL;
    gchar **value = NULL;
    gsize length;
    int i;

    static PROGRAMNAME runapp_info[5];

    keyfile = g_key_file_new();

    if (!g_key_file_load_from_file(keyfile, filepath, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        INFO("loading key file form %s is failed\n", filepath );
        g_error("%s", error->message);
        return NULL;
    }

    value = g_key_file_get_string_list(keyfile, group, field,  &length, &error);

    if ( value != NULL) {
        for (i = 0; i < (length / 2); i++) {
            runapp_info[i].nDate = atoi(*(value + (i * 2)));
            g_strlcpy(runapp_info[i].arName, *(value + (i * 2) + 1), sizeof runapp_info[i].arName);
        }
        g_free(value);
    }

    g_key_file_free(keyfile);

    return runapp_info;
}

/**
 * @brief   create configuration file if it doesn't exist and each field of configuration file sets default value
 * @param   filename : configuration file path (ex: /opt/xo-sim/conf/emulator.conf, emulator0.conf)
 * @return  success(0)
 * @date    Nov 17. 2008
 * */
int create_config_file(gchar* filepath)
{
    FILE *fp = g_fopen(filepath, "w+");

    if (fp != NULL) {
        g_fprintf (fp, "[%s]\n", COMMON_GROUP);
        g_fprintf (fp, "%s=\n",ALWAYS_ON_TOP_KEY);

        g_fprintf (fp, "\n[%s]\n", EMULATOR_GROUP);
        g_fprintf (fp, "%s=\n",TERMINAL_TYPE_KEY );
        g_fprintf (fp, "%s=\n", ENABLE_SHELL_KEY);
        g_fprintf (fp, "%s=\n", ENABLE_TELEPHONY_EMULATOR_KEY);
        g_fprintf (fp, "%s=\n", ENABLE_GPSD_KEY);
        g_fprintf (fp, "%s=\n", ENABLE_COMPASS_KEY);
        g_fprintf (fp, "%s=\n", CMD_TYPE_KEY);

        g_fprintf (fp, "%s=\n", MAIN_X_KEY);
        g_fprintf (fp, "%s=\n", MAIN_Y_KEY);
        g_fprintf (fp, "%s=\n", SCALE_KEY);

        g_fprintf (fp, "\n[%s]\n", QEMU_GROUP);
        g_fprintf (fp, "%s=\n", HTTP_PROXY_KEY);
        g_fprintf (fp, "%s=\n", DNS_SERVER_KEY);
        g_fprintf (fp, "%s=\n", TELNET_TYPE_KEY);
        g_fprintf (fp, "%s=\n", TELNET_PORT_KEY);
        g_fprintf (fp, "%s=\n", TELNET_CONSOLE_COMMAND_TYPE_KEY);
        g_fprintf (fp, "%s=\n", TELNET_CONSOLE_COMMAND_KEY);
//      g_fprintf (fp, "%s=\n", SDCARD_TYPE_KEY);
//      g_fprintf (fp, "%s=\n", SDCARD_PATH_KEY);
//      g_fprintf (fp, "%s=\n", DISK_TYPE_KEY);
//      g_fprintf (fp, "%s=\n", DISK_PATH_KEY);
        g_fprintf (fp, "%s=1\n", SAVEVM_KEY);
//      g_fprintf (fp, "%s=\n", SNAPSHOT_SAVED_KEY);
//      g_fprintf (fp, "%s=\n", SNAPSHOT_SAVED_DATE_KEY);

        fclose(fp);
    }
    else {
        ERR( "Can't open file path. (%s)\n", filepath);
        return -1;
    }

    g_chmod(filepath, 0666);

    return 0;
}


/**
 * @brief   write configuration structure value in conf file (emulator.conf)
 * @param   filename : configuration file path (ex: /opt/xo-sim/conf/emulator.conf), CONFIGURATION
 * @return  success(0)
 * @date    Nov 17. 2008
 * */
int write_config_file(gchar *filepath, CONFIGURATION *pconfiguration)
{
    set_config_type(filepath, COMMON_GROUP, ALWAYS_ON_TOP_KEY, pconfiguration->always_on_top);
    set_config_type(filepath, EMULATOR_GROUP, ENABLE_SHELL_KEY, pconfiguration->enable_shell);
    set_config_type(filepath, EMULATOR_GROUP, ENABLE_TELEPHONY_EMULATOR_KEY, pconfiguration->enable_telephony_emulator);
    set_config_type(filepath, EMULATOR_GROUP, ENABLE_GPSD_KEY, pconfiguration->enable_gpsd);
    set_config_type(filepath, EMULATOR_GROUP, ENABLE_COMPASS_KEY, pconfiguration->enable_compass);
    set_config_type(filepath, EMULATOR_GROUP, CMD_TYPE_KEY, pconfiguration->cmd_type);
    set_config_type(filepath, EMULATOR_GROUP, MAIN_X_KEY, pconfiguration->main_x);
    set_config_type(filepath, EMULATOR_GROUP, MAIN_Y_KEY, pconfiguration->main_y);
    set_config_type(filepath, EMULATOR_GROUP, SCALE_KEY, pconfiguration->scale);

    /*  QEMU option (09.05.26)*/

    set_config_type(filepath, QEMU_GROUP, HTTP_PROXY_KEY, pconfiguration->qemu_configuration.use_host_http_proxy);
    set_config_type(filepath, QEMU_GROUP, DNS_SERVER_KEY, pconfiguration->qemu_configuration.use_host_dns_server);
    set_config_type(filepath, QEMU_GROUP, TELNET_TYPE_KEY, pconfiguration->qemu_configuration.telnet_type);
    set_config_value(filepath, QEMU_GROUP, TELNET_PORT_KEY, pconfiguration->qemu_configuration.telnet_port);

    set_config_type(filepath, QEMU_GROUP, TELNET_CONSOLE_COMMAND_TYPE_KEY, pconfiguration->qemu_configuration.serial_console_command_type);
    set_config_value(filepath, QEMU_GROUP, TELNET_CONSOLE_COMMAND_KEY, pconfiguration->qemu_configuration.serial_console_command);

//  set_config_type(filepath, QEMU_GROUP, SDCARD_TYPE_KEY, pconfiguration->qemu_configuration.sdcard_type);
//  set_config_value(filepath, QEMU_GROUP, SDCARD_PATH_KEY, pconfiguration->qemu_configuration.sdcard_path);
    set_config_type(filepath, QEMU_GROUP, SAVEVM_KEY, pconfiguration->qemu_configuration.save_emulator_state);
//  set_config_type(filepath, QEMU_GROUP, SNAPSHOT_SAVED_KEY, pconfiguration->qemu_configuration.snapshot_saved);
//  set_config_value(filepath, QEMU_GROUP, SNAPSHOT_SAVED_DATE_KEY, pconfiguration->qemu_configuration.snapshot_saved_date);

    return 0;
}


/**
 * @brief   read conf value and save in configuration structure from config file (emulator.conf)
 * @param   filename : configuration file path, CONFIGURATION
 * @return  fail(-1), success(0)
 * @date    Nov 17. 2008
 * */
int read_config_file(gchar *filepath, CONFIGURATION *pconfiguration)
{
    char *buf = NULL;
    int status = 0;

    pconfiguration->always_on_top = get_config_type(filepath, COMMON_GROUP, ALWAYS_ON_TOP_KEY);

    pconfiguration->qemu_configuration.use_host_http_proxy = get_config_type(filepath, QEMU_GROUP, HTTP_PROXY_KEY);
    pconfiguration->qemu_configuration.use_host_dns_server = get_config_type(filepath, QEMU_GROUP, DNS_SERVER_KEY);

    /* option menu config parsed */
    gchar *sdb_path = get_sdb_path();
    if (access(sdb_path, 0) == 0) {
        pconfiguration->enable_shell = get_config_type(filepath, EMULATOR_GROUP, ENABLE_SHELL_KEY);
    }
    g_free(sdb_path);

    pconfiguration->enable_telephony_emulator = get_config_type(filepath, EMULATOR_GROUP, ENABLE_TELEPHONY_EMULATOR_KEY);
    pconfiguration->enable_gpsd = get_config_type(filepath, EMULATOR_GROUP, ENABLE_GPSD_KEY);
    pconfiguration->enable_compass = get_config_type(filepath, EMULATOR_GROUP, ENABLE_COMPASS_KEY);
    pconfiguration->cmd_type = get_config_type(filepath, EMULATOR_GROUP, CMD_TYPE_KEY);

    pconfiguration->main_x = get_config_type(filepath, EMULATOR_GROUP, MAIN_X_KEY);
    pconfiguration->main_y = get_config_type(filepath, EMULATOR_GROUP, MAIN_Y_KEY);
    pconfiguration->scale = get_config_type(filepath, EMULATOR_GROUP, SCALE_KEY);

    int telnet_port;
    buf = get_config_value(filepath, QEMU_GROUP, TELNET_PORT_KEY);
    if (buf != NULL) {
        telnet_port = atoi(buf);
        if(ENABLE_MULTI) {
            while(check_port(LOCALHOST, telnet_port) == 0)
                telnet_port += 3;
        }
        snprintf(pconfiguration->qemu_configuration.telnet_port, MAXBUF, "%d", telnet_port);
    }
    g_free(buf);

    pconfiguration->qemu_configuration.telnet_type = get_config_type(filepath, QEMU_GROUP, TELNET_TYPE_KEY);

    pconfiguration->qemu_configuration.serial_console_command_type = get_config_type(filepath, QEMU_GROUP, TELNET_CONSOLE_COMMAND_TYPE_KEY);

    buf = get_config_value(filepath, QEMU_GROUP, TELNET_CONSOLE_COMMAND_KEY);

    if (buf != NULL)
        snprintf(pconfiguration->qemu_configuration.serial_console_command, MAXBUF, "%s", buf);

    g_free(buf);

/*  pconfiguration->qemu_configuration.sdcard_type = get_config_type(filepath, QEMU_GROUP, SDCARD_TYPE_KEY);

    buf = get_config_value(filepath, QEMU_GROUP, SDCARD_PATH_KEY);
    if (buf != NULL)
        snprintf(pconfiguration->qemu_configuration.sdcard_path, MAXBUF, "%s", buf);
    g_free(buf); */

    pconfiguration->qemu_configuration.save_emulator_state = get_config_type(filepath, QEMU_GROUP, SAVEVM_KEY);

/*  pconfiguration->qemu_configuration.snapshot_saved = get_config_type(filepath, QEMU_GROUP, SNAPSHOT_SAVED_KEY);

    buf = get_config_value(filepath, QEMU_GROUP, SNAPSHOT_SAVED_DATE_KEY);
    if (buf != NULL)
        snprintf(pconfiguration->qemu_configuration.snapshot_saved_date, MAXBUF, "%s", buf);
    g_free(buf); */

    return status;
}


int read_virtual_target_info_file(gchar *virtual_target_name, VIRTUALTARGETINFO *pvirtual_target_info)
{
    int status = 0;
    int info_file_status;
    char *virtual_target_path;
    char *info_file;
    char *buf = NULL;

    virtual_target_path = get_virtual_target_path(virtual_target_name);
    info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
    info_file_status = is_exist_file(info_file);

    if(info_file_status == -1 || info_file_status == FILE_NOT_EXISTS)
    {
        ERR( "target info file not exists : %s\n", virtual_target_name);
        return -1;
    }

    snprintf(pvirtual_target_info->virtual_target_name, MAXBUF, "%s", virtual_target_name);

    buf = get_config_value(info_file, HARDWARE_GROUP, RESOLUTION_KEY);
    snprintf(pvirtual_target_info->resolution, MAXBUF, "%s", buf);
    g_free(buf);

    pvirtual_target_info->button_type = get_config_type(info_file, HARDWARE_GROUP, BUTTON_TYPE_KEY);

    pvirtual_target_info->sdcard_type = get_config_type(info_file, HARDWARE_GROUP, SDCARD_TYPE_KEY);
    if(pvirtual_target_info->sdcard_type == 0)
    {
        memset(pvirtual_target_info->sdcard_path, 0x00, MAXBUF);
    }
    else
    {
        buf = get_config_value(info_file, HARDWARE_GROUP, SDCARD_PATH_KEY);
        snprintf(pvirtual_target_info->sdcard_path, MAXBUF, "%s", buf);
        g_free(buf);
    }

    pvirtual_target_info->ram_size = get_config_type(info_file, HARDWARE_GROUP, RAM_SIZE_KEY);

    buf = get_config_value(info_file, HARDWARE_GROUP, DISK_PATH_KEY);
    //  buf = get_virtual_target_path(virtual_target_name);
    if(!buf && strlen(startup_option.disk) > 0)
        buf = startup_option.disk;
    snprintf(pvirtual_target_info->diskimg_path, MAXBUF, "%s", buf);
    g_free(buf);

    pvirtual_target_info->snapshot_saved = get_config_type(info_file, ETC_GROUP, SNAPSHOT_SAVED_KEY);
    if(pvirtual_target_info->snapshot_saved == 0)
    {
        memset(pvirtual_target_info->snapshot_saved_date, 0x00, MAXBUF);
    }
    else
    {
        buf = get_config_value(info_file, ETC_GROUP, SNAPSHOT_SAVED_DATE_KEY);
        snprintf(pvirtual_target_info->snapshot_saved_date, MAXBUF, "%s", buf);
        g_free(buf);
    }

    /* get DPI info from config.ini */
    buf = get_config_value(info_file, HARDWARE_GROUP, DPI_KEY);
    snprintf(pvirtual_target_info->dpi, MAXBUF, "%s", buf);
    //fprintf(stderr, "DPI = %s\n", pvirtual_target_info->dpi);

    g_free(virtual_target_path);
    g_free(info_file);

    return status;
}


/* sanity check on target path */
int is_valid_target(const gchar *path)
{
    if (g_file_test(path, G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS) == FALSE) {
        ERR( "target file: %s is not exist.\n", path);
        return -1;
    }

    return 0;
}


/**
 * @brief   check about skin
 * @param   filename : skin path (ex: /opt/xo-sim/skins/320x320/320x320_1FB.dbi)
 * @return  0 fail, 1 success
 * @date    Nov 17. 2008
 * */
int is_valid_skin (gchar *file)
{
    if (file == NULL)
        return FALSE;

    if ((g_file_test(file, G_FILE_TEST_EXISTS) != FALSE) && g_str_has_suffix (file, ".dbi"))
        return TRUE;

    return FALSE;
}


/**
 * @brief   get emulator config filepath for status
 * @return  exist normal(1), not exist(0), error(-1)
 * @date    Apr 19. 2009
 * */

int is_valid_targetlist_file()
{
    int status = 0;
    gchar *targetlist_filepath = NULL;

    targetlist_filepath = get_targetlist_filepath();

    status = is_exist_file(targetlist_filepath);

    if (status != -1) {
        sprintf(SYSTEMINFO.conf_file, "%s", targetlist_filepath);
    }

    g_free(targetlist_filepath);

    return status;
}


/**
 * @brief   load emulator.conf  and create config_window for conf file status
 * @param   SYSTEMINFO(SYSTEMINFO.conf_file)
 * @return  fail(-1), success(0)
 * @date    Apr 19. 2009
 * */
int load_targetlistig_file(SYSINFO *pSYSTEMINFO)
{
    int status = 0;
    int result = 0;
    char *virtual_target_path;
    char *info_file;

    int target_list_status = is_valid_target_list_file(pSYSTEMINFO);
    if(target_list_status == -1 || target_list_status == FILE_NOT_EXISTS)
    {
        ERR( "load target list file error\n");
        return -1;
    }

    int virtual_target_info_status = read_virtual_target_info_file(startup_option.vtm, &virtual_target_info);
    if(virtual_target_info_status == -1)
    {
        ERR( "load target info file error\n");
        return -1;
    }
    else
    {
        snprintf(pSYSTEMINFO->virtual_target_name, MAXBUF, "%s", startup_option.vtm);
//      virtual_target_path = get_virtual_target_path(startup_option.vtm);
        virtual_target_path = get_virtual_target_path(startup_option.vtm);
        info_file = g_strdup_printf("%sconfig.ini", virtual_target_path);
        snprintf(pSYSTEMINFO->virtual_target_info_file, MAXPATH, "%s", info_file);
    }

    g_free(virtual_target_path);
    g_free(info_file);

    status = is_valid_targetlist_file();

    switch ( status ) {
    case FILE_EXISTS:

        fill_configuration(CONF_EXIST_STARTUP_MODE);

        result = read_config_file(SYSTEMINFO.virtual_target_info_file, &configuration);

//      if (result == -1) {
//          fill_configuration(CONF_INIT_MODE);

        break;

    case FILE_NOT_EXISTS :
//      fill_configuration(CONF_INIT_MODE);
//      if ((strlen(configuration.target_path) < 2))
//      create_config_file(SYSTEMINFO.virtual_target_info_file);
        ERR( "load emulator config file \n");

        break;

    default:
        ERR( "Emulator config file not exists!\n");

        return -1;
    }

//  write_config_file(SYSTEMINFO.virtual_target_info_file, &configuration);
    startup_option_config_done = 1;

    return 0;
}


/**
 * @brief   load conf file
 * @param   SYSTEMINFO
 * @return  success  0,  fail    -1
 * @date    Nov 6. 2008
 * */
int load_config_file(SYSINFO *pSYSTEMINFO)
{
//  INFO( "default_target = %s \n", configuration.target_path);

    /* 2. emulator config file load (emulator.conf) */

    if (load_targetlistig_file(pSYSTEMINFO) == -1) {
        ERR( "load emulator.conf file error!!\n");
        return -1;
    }

    return 0;
}


int determine_skin(VIRTUALTARGETINFO *pvirtual_target_info, CONFIGURATION *pconfiguration)
{
    int i;
    int resolution_found = 0;
    int button_type         = 0;
    char *skin = NULL;
    char *resolution[RESOLUTION_COUNT] = {"320x480", "480x800", "600x1024", "720x1280"};
    char *button_types[BUTTON_TYPE_COUNT]   = {"","","not_use_","3keys_"};

    for(i = 0; i < RESOLUTION_COUNT; i++)
    {
        if(strcmp(pvirtual_target_info->resolution, resolution[i]) == 0)
        {
            resolution_found = 1;
            break;
        }
    }

    if(resolution_found == 0)
    {
        ERR( "unknown resolution\n");
        return -1;
    }

    button_type = pvirtual_target_info->button_type;
        if (button_type !=0 &&  button_type != 1 &&  button_type != 3)
        {
                ERR( "unknown button type : %d\n", button_type);
                return -1;
        }

    skin = g_strdup(resolution[i]);

    snprintf(pconfiguration->skin_path, MAXBUF, "%s/emul_%s%s/default.dbi", get_skin_path(), button_types[button_type], skin);

    if(is_valid_skin(pconfiguration->skin_path) == 0)
    {
        ERR( "skin file is invalid\n");
        g_free(skin);
        return -1;
    }

    g_free(skin);
    return 0;
}


/**
 * @brief   callback when ok button clicked in option window
 * @param   void
 * @param   result
 * @date    Apr 21. 2009
 * */
void qemu_option_set_to_config(arglist *al)
{
    gboolean userdata_exist = FALSE;

    const gchar *exec_path = get_exec_path();

    int width = (int)(PHONE.mode[UISTATE.current_mode].lcd_list[0].lcd_region.w);
    int height = (int)(PHONE.mode[UISTATE.current_mode].lcd_list[0].lcd_region.h);
    int bpp = (int)(PHONE.mode[UISTATE.current_mode].lcd_list[0].bitsperpixel);

    /* input kernel append*/

    char kernel_kappend[MAXBUF] = {0, };
    char fb1_format[MAXBUF] = {0, };

    if (PHONE.mode[UISTATE.current_mode].lcd_list[1].bitsperpixel == 16)
        sprintf(fb1_format, "rgb16");
    else if (PHONE.mode[UISTATE.current_mode].lcd_list[1].bitsperpixel == 24)
        sprintf(fb1_format, "rgb24");
    else if(PHONE.mode[UISTATE.current_mode].lcd_list[1].nonstd == EMUL_PIXELFORMAT_YUV420P)
        sprintf(fb1_format, "yuv420");
    else if(PHONE.mode[UISTATE.current_mode].lcd_list[1].nonstd == EMUL_PIXELFORMAT_YUV422P)
        sprintf(fb1_format, "yuv422");

    /* console */

    if (qemu_arch_is_arm()){
        /* duallcd command line is used in hw/s5pc1xx.c to change the board revision */
        if(PHONE.dual_display == 1)
            strcpy(kernel_kappend, "console=ttySAC2,115200n8 duallcd mem=80M mem=256M@0x40000000 mem=128@0x50000000 mtdparts=tizen-onenand:1m(bootloader),256k(params),2816k(config),8m(csa),7m(kernel),1m(log),12m(modem),60m(qboot),-(UBI) ");
        else
            strcpy(kernel_kappend, "console=ttySAC2,115200n8 mem=80M mem=256M@0x40000000 mem=128@0x50000000 mtdparts=tizen-onenand:1m(bootloader),256k(params),2816k(config),8m(csa),7m(kernel),1m(log),12m(modem),60m(qboot),-(UBI) ");
    }
    else
        strcpy(kernel_kappend, "console=ttyS0 ");

    /*  video overlay */

    if (!qemu_arch_is_arm()) {
        if (strlen(fb1_format) > 0)  /* Using Overlay */
            sprintf(&kernel_kappend[strlen(kernel_kappend)],
                    "video=uvesafb:ywrap,overlay:%s,%dx%d-%d@60 ", fb1_format, width, height, bpp);
        else
            sprintf(&kernel_kappend[strlen(kernel_kappend)],
                    "video=uvesafb:ywrap,%dx%d-%d@60 ", width, height, bpp);
    }

    char proxy[MIDBUF] ={0}, hostip[MIDBUF] = {0}, dns1[MIDBUF] = {0}, dns2[MIDBUF] = {0};

    gethostproxy(proxy);
    if (configuration.qemu_configuration.use_host_http_proxy == 1)
        sprintf(&kernel_kappend[strlen(kernel_kappend)], "proxy=%s ", proxy);

    gethostDNS(dns1, dns2);
    if (configuration.qemu_configuration.use_host_dns_server == 1) {
        if (strlen(dns1))   sprintf(&kernel_kappend[strlen(kernel_kappend)], "dns1=%s ", dns1);
        if (strlen(dns2))   sprintf(&kernel_kappend[strlen(kernel_kappend)], "dns2=%s ", dns2);
    }

    // handover Host IP address to kernel side
    /*
    gethostIP(hostip);
    if (strlen(hostip))
        sprintf(&kernel_kappend[strlen(kernel_kappend)], "openglip=%s ", hostip);
    */

    // sdb port
    sprintf(&kernel_kappend[strlen(kernel_kappend)], "sdb_port=%d ", get_sdb_base_port());

    // get DPI value
    if (strlen(virtual_target_info.dpi))
        sprintf(&kernel_kappend[strlen(kernel_kappend)], "dpi=%s ", virtual_target_info.dpi);

    /*
     * for QEMU user mode networking (with -net user),
     * our ip is 10.0.2.16, and the gateway is 10.0.2.2
     * These address are hardcoded in QEMU
     *
     * For NFS to work /etc/exports should contain something like:
     *
     * /scratchbox/users/mike/targets  127.0.0.1(rw,all_squash,insecure,async,insecure_locks)
     *
     */

#define GUEST_IP_ADDRESS "10.0.2.16"
#define HOST_QEMU_ADDRESS "10.0.2.2"
#define BOOT_DRIVE "c"
#define KERNEL_LOGFILE_NAME "logs/emulator.klog"

#define VIRTIO_DISK
    if(!configuration.qemu_configuration.diskimg_type) {
        sprintf(&kernel_kappend[strlen(kernel_kappend)], "root=/dev/nfs rw ");
        sprintf(&kernel_kappend[strlen(kernel_kappend)], "nfsroot=" HOST_QEMU_ADDRESS ":%s,udp,port=%d,v3 ",
                configuration.target_path, startup_option.mountPort);
    }
    else
    {
        if(qemu_arch_is_arm())
            strcat(kernel_kappend, "root=/dev/hda ");
        else
#ifdef VIRTIO_DISK
            strcat(kernel_kappend, "root=/dev/vda rw ");
#else
            strcat(kernel_kappend, "root=/dev/hda rw ");
#endif
    }

    /* user mode networking */

    if (qemu_arch_is_arm())
        sprintf(&kernel_kappend[strlen(kernel_kappend)], "ip=" GUEST_IP_ADDRESS "::" HOST_QEMU_ADDRESS ":255.255.255.0::usb0:none ");
    else
        sprintf(&kernel_kappend[strlen(kernel_kappend)], "ip=" GUEST_IP_ADDRESS "::" HOST_QEMU_ADDRESS ":255.255.255.0::eth0:none ");

    sprintf(&kernel_kappend[strlen(kernel_kappend)], "%d", startup_option.run_level);

    append_argvlist(al, "%s", exec_path);
    if (qemu_arch_is_arm()) {
        if(configuration.qemu_configuration.diskimg_type) {
            append_argvlist(al, "-hda");
            append_argvlist(al, "%s", virtual_target_info.diskimg_path);
        }
    }
    /*this is for i386*/
    else
    {
        if(configuration.qemu_configuration.diskimg_type) {
            append_argvlist(al, "-drive");
#ifdef VIRTIO_DISK
            append_argvlist(al, "file=%s,if=virtio", virtual_target_info.diskimg_path);
#else
            append_argvlist(al, "file=%s", virtual_target_info.diskimg_path);
#endif
            gchar userdata_buf[MAXBUF] = {0, };
            char *target_name = basename(configuration.target_path);
            snprintf(userdata_buf, MAXBUF, "%s/opt/%s-userdata.img", configuration.target_path, target_name);
            if (is_exist_file(userdata_buf) == 1) {
                append_argvlist(al, "-hdb");
                append_argvlist(al, "%s", userdata_buf);
                userdata_exist = TRUE;
            }
        }
    }

    append_argvlist(al, "-boot");
    append_argvlist(al, "%s", BOOT_DRIVE);

    /* snapshot boot set  -loadvm snapshot option */
    if(configuration.qemu_configuration.save_emulator_state) {
        append_argvlist(al,"-loadvm");
        append_argvlist(al,"snapshot");
    }

    append_argvlist(al, "-append");
    append_argvlist(al, "%s", kernel_kappend);

    if (!startup_option.no_dump) {
        gchar kernel_log_path[MAXBUF] = {0, };
        strcpy(kernel_log_path, get_virtual_target_path(startup_option.vtm));
        strcat(kernel_log_path, KERNEL_LOGFILE_NAME);
        append_argvlist(al, "-serial");
        append_argvlist(al, "file:%s", kernel_log_path);
    }
    else
    {
//  else if (configuration.qemu_configuration.telnet_type == 1) {
        append_argvlist(al, "-serial");
        append_argvlist(al, "telnet:localhost:%s,server,nowait,ipv4",
                        configuration.qemu_configuration.telnet_port);
    }

    if (!qemu_arch_is_arm()) {
        if(virtual_target_info.sdcard_type != 0) {
#ifdef VIRTIO_DISK
            append_argvlist(al, "-drive");
            append_argvlist(al, "file=%s,if=virtio", virtual_target_info.sdcard_path);
#else
            if (userdata_exist)
                append_argvlist(al, "-hdc");
            else
                append_argvlist(al, "-hdb");
            append_argvlist(al, "%s", virtual_target_info.sdcard_path);
#endif
        }
    }

    append_argvlist(al, "-m");
    append_argvlist(al, "%d", virtual_target_info.ram_size);

#if 0
    printf("startup option : \n\n");
    printf("target : %s\n", startup_option.target);
    printf("disk : %s\n", startup_option.disk);
    printf("skin : %s\n", startup_option.skin);
    printf("loglevel : %d\n", startup_option.log_level);
    printf("runlevel : %d\n", startup_option.run_level);
    printf("target_log : %d\n", startup_option.target_log);
    printf("mountport : %d\n", startup_option.mountPort);
    printf("sshport : %d\n", startup_option.ssh_port);
    printf("telnetport : %d\n", startup_option.telnet_port);
    printf("configuration : \n\n");
    printf("shell %d, telephony %d, gpsd %d, compass %d, cmd_type %d, ",configuration.enable_shell
            , configuration.enable_telephony_emulator, configuration.enable_gpsd, configuration.enable_compass
            , configuration.cmd_type);
    printf("mount_port %d, target_path : %s\n", configuration.mount_port, configuration.target_path);
    printf("qemu configuration : \n\n");
    printf("proxy %d, dns %d, telnetport %s, telnettype %d, sdcardtype %d\n",
            configuration.qemu_configuration.use_host_http_proxy, configuration.qemu_configuration.use_host_dns_server,
            configuration.qemu_configuration.telnet_port, configuration.qemu_configuration.telnet_type, configuration.qemu_configuration.sdcard_type);
    printf("sdcardpath : %s, diskimg_type %d, diskimg_path %s\n",
            configuration.qemu_configuration.sdcard_path, configuration.qemu_configuration.diskimg_type,
            configuration.qemu_configuration.diskimg_path);
    printf("console cmdtype %d, console cmd %s\n", configuration.qemu_configuration.serial_console_command_type,
            configuration.qemu_configuration.serial_console_command);
    printf("save state %d, saved %d, saved date %s\n\n", configuration.qemu_configuration.save_simulator_state,
            configuration.qemu_configuration.snapshot_saved,
            configuration.qemu_configuration.snapshot_saved_date);
#endif
}


const char *get_target_path(void)
{
    return configuration.target_path;
}

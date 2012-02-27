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


#ifndef __SENSOR_H_
#define __SENSOR_H_
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#include "sensor_server.h"
#include "emulator.h"
#include "menu_callback.h"
#define UDP

#include "debug_ch.h"
#include "sdb.h"
#include "nbd.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, sensor_server);

extern int sensor_update(uint16_t x, uint16_t y, uint16_t z);

int sensord_initialized = 0;
//int sent_start_value = 0;

int sensor_parser(char *buffer);
int parse_val(char *buff, unsigned char data, char *parsbuf);

void *init_sensor_server(void)
{
    int listen_s;
    uint16_t port;
    struct sockaddr_in servaddr;
    GIOChannel *channel = NULL;
    //GError *error;
    //GIOStatus status;

#ifdef __MINGW32__  
    WSADATA wsadata;
    if(WSAStartup(MAKEWORD(2,0), &wsadata) == SOCKET_ERROR) {
        ERR("Error creating socket.\n");
        return NULL;
    }
#endif

    /* ex: 26100 + 3/udp */
    port = get_sdb_base_port() + SDB_UDP_SENSOR_INDEX;

    if((listen_s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        ERR("Create listen socket error: ");
        perror("socket");
        goto cleanup;
    }

    memset(&servaddr, '\0', sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(port);

    INFO( "bind port[127.0.0.1:%d/udp] for sensor server in host \n", port);
    if(bind(listen_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
    {
        ERR("bind error: ");
        perror("bind");
        goto cleanup;
    }

    channel = g_io_channel_unix_new(listen_s);
    if(channel == NULL)
    {
        ERR("gnet_udp_socket_get_io_channel failed\n");
        goto cleanup;
    }

    //  status = g_io_channel_set_encoding(channel, NULL, NULL);
    //  if(status != G_IO_STATUS_NORMAL)
    //  {
    //      ERR("encoding error %d %s\n", status, error->message);
    //      goto cleanup;
    //  }
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);

    guint sourceid = g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP, sensor_server, NULL);

    if(sourceid <= 0)
    {
        ERR("g_io_add_watch() failed\n");
        g_io_channel_unref(channel);
        goto cleanup;
    }

    return NULL;

cleanup:
#ifdef __MINGW32__
    if(listen_s)
        closesocket(listen_s);
#else
    if(listen_s)
        close(listen_s);
#endif

    return NULL;
}

static int send_info_to_sensor_daemon(char *send_buf, int buf_size)
{
    int   s;  

    s = tcp_socket_outgoing("127.0.0.1", (uint16_t)(get_sdb_base_port() + SDB_TCP_EMULD_INDEX)); 
    if (s < 0) {
        TRACE( "can't create socket to talk to the sdb forwarding session \n");
        TRACE( "[127.0.0.1:%d/tcp] connect fail (%d:%s)\n"
                , get_sdb_base_port() + SDB_TCP_EMULD_INDEX
                , errno, strerror(errno)
             );
        return -1;
    }   

    socket_send(s, "sensor\n\n\n\n", 10);
    socket_send(s, &buf_size, 4); 
    socket_send(s, send_buf, buf_size);

    INFO( "send(size: %d) te 127.0.0.1:%d/tcp \n"
            , buf_size, get_sdb_base_port() + SDB_TCP_EMULD_INDEX);

#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif

    return 1;
}

#if 0
/* Not using sdb port forwarding => Using redir in sdb setup */
static void *create_fw_rota_init(void *arg)
{
    int s;
    int tries = 0;
    char fw_buf[64] = {0};
    char recv_buf[8] = {0};
    char send_buf[32] = {0};

    while(1)
    {
        s = tcp_socket_outgoing("127.0.0.1", SDB_HOST_PORT);
        if (s < 0) {

            ERR("[%d] can't create socket to talk to the SDB server \n", ++tries);
            usleep(1000000);

            if(tries > 9)
                break;
            else
                continue;
        }

        memset(fw_buf, 0, sizeof(fw_buf));

        /* length is hex: 0x35 = 53 */
        sprintf(fw_buf,"0035host-serial:emulator-%d:forward:tcp:%d;tcp:3577"
                ,get_sdb_base_port(),  get_sdb_base_port() + SDB_TCP_SENSOR_INDEX );

        /* Over 53+4 */
        socket_send(s, fw_buf, 60);

        memset(recv_buf, 0, sizeof(recv_buf));
        recv( s, recv_buf, 4, 0 );

        /* check OKAY */
        if(!memcmp(recv_buf, "OKAY", 4)) {
            INFO( "create forward [%s] success : [%s] \n", fw_buf, recv_buf);

            /* send init ratation info */
            sprintf(send_buf, "1\n3\n0\n-9.80665\n0\n");

            if( send_info_to_sensor_daemon(send_buf, 32) <= 0 ) {   
                ERR( "[%s][%d] send init rotaion info: error \n", __FUNCTION__, __LINE__);
            }else{
                INFO( "[%s][%d] send init rotation info: sucess \n", __FUNCTION__, __LINE__);
                /* all initialized */
                sensord_initialized = 1;
            }

            break;
        }else{
            /* not ready */
            //fprintf(stderr, "create forward [%s] fail : [%s] \n", fw_buf, recv_buf);
            usleep(1000000);
        }

#ifdef __WIN32
        closesocket(s);
#else
        close(s);
#endif

    }

    return NULL;
}
#endif

gboolean sensor_server(GIOChannel *channel, GIOCondition condition, gpointer data)
{
    int parse_result;

    //GError *error;
    //GIOStatus status;
    GIOError ioerror;

    gsize len;
    char recv_buf[32] = {0};
    char send_buf[32] = {0};

#ifdef __MINGW32__
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2,0), &wsadata) == SOCKET_ERROR) {
        ERR( "[%s][%d] Error creating socket  \n", __FUNCTION__, __LINE__);
        g_io_channel_unref(channel);
        g_io_channel_shutdown(channel, TRUE, NULL);
        return FALSE;
    }
#endif

    if((condition == G_IO_IN))
    {
        //      status = g_io_channel_read_chars(channel, recv_buf, 32, &len, &error);
        ioerror = g_io_channel_read(channel, recv_buf, 32, &len);
        //      if(status != G_IO_STATUS_NORMAL)
        if(ioerror != G_IO_ERROR_NONE)
        {
            ERR( "[%s][%d] recv() failed %d \n", __FUNCTION__, __LINE__, ioerror);
            goto clean_up;
        }
        else
        {
            parse_result = sensor_parser(recv_buf);

#if 0
            /* Not using sdb forwarding => Using redir in sdb setup */
            if(sent_start_value == 0) {

                /* new way with sdb */
                pthread_t taskid;

                INFO( "pthread_create for create_forward : \n");
                if( pthread_create( (pthread_t *)&taskid, NULL, (void *(*)(void *))create_fw_rota_init, NULL ) ){
                    ERR( "pthread_create for create_forward fail: \n");
                }   

                sent_start_value = 1;
            }
#endif

            if(sensord_initialized)
            {
                switch(parse_result)
                {
                    case 0:
                        sprintf(send_buf, "1\n3\n0\n-9.80665\n0\n");
                        break;
                    case 90:
                        sprintf(send_buf, "1\n3\n-9.80665\n0\n0\n");
                        break;
                    case 180:
                        sprintf(send_buf, "1\n3\n0\n9.80665\n0\n");
                        break;
                    case 270:
                        sprintf(send_buf, "1\n3\n9.80665\n0\n0\n");
                        break;
                    case 7:
                        sprintf(send_buf, "%s", recv_buf); 
                        break;
                    case 2: //reboot guest
                                        {
                        // menu_callback.h
                                                sprintf(send_buf, "7\n%1d\n", keyboard_state);
                                                break;
                                        }
                }

                if(parse_result != 1 && parse_result != -1)
                {
                    /* new way with sdb */
                    if( send_info_to_sensor_daemon(send_buf, 32) <= 0 )
                    {
                        ERR( "[%s][%d] send error \n", __FUNCTION__, __LINE__);
                        ERR( "[%s][%d] not clean_up \n", __FUNCTION__, __LINE__);
                        //goto clean_up;
                    }
                }
            }
            return TRUE;
        }
    }
    else if((condition == G_IO_ERR) || (condition == G_IO_HUP))
    {
        ERR("G_IO_ERR | G_IO_HUP received \n");
    }

clean_up:

    g_io_channel_unref(channel);
    g_io_channel_shutdown(channel, TRUE, NULL);

    return FALSE;
}


int sensor_parser(char *buffer)
{
    int len = 0;
    char tmpbuf[32];
    int rotation;
    int from_skin;
    int base_mode = 0;

#ifdef SENSOR_DEBUG
    TRACE("read data: %s\n", buffer);
#endif
    /* start */
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len = parse_val(buffer, 0x0a, tmpbuf);

    // packet from skin 
    if(strcmp(tmpbuf, "1\n") == 0)
    {
        from_skin = 1;
    }
    else if(strcmp(tmpbuf, "2\n") == 0) // packet from EI -> sensor plugin
    {
        from_skin = 0;
    }
    else if(strcmp(tmpbuf, "3\n") == 0) // packet from sensord
    {
        if (sensord_initialized == 1) // reboot guest
                        return 2;

        /* sensord_initialized will be initialized in create_fw_rota_init */
        sensord_initialized = 1;
        return 1;
    }
    else if(strcmp(tmpbuf, "7\n") == 0) // keyboard on/off
    {
        if(sensord_initialized == 0){
            /* can't send info to guest */
            show_message("Warning", "Not ready!\nYou cannot turn on/off the USB Keyboard.");
        }
        return 7;
    }
    else
    {
        ERR("bad data");
        return -1;
    }

    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len += parse_val(buffer+len, 0x0a, tmpbuf);

    rotation = atoi(tmpbuf);

    if(sensord_initialized == 1)
    {
        if(from_skin == 1) //skin-packet only
        {
            if(UISTATE.current_mode > 3)
                base_mode = 4;

            switch(rotation) //rotate emulator window
            {
                case 0:
                    if(UISTATE.current_mode %4 != 0)
                        rotate_event_callback(&PHONE, base_mode + 0);
                    break;
                case 90:
                    if(UISTATE.current_mode %4 != 1)
                        rotate_event_callback(&PHONE, base_mode + 1);
                    break;
                case 180:
                    if(UISTATE.current_mode %4 != 2)
                        rotate_event_callback(&PHONE, base_mode + 2);
                    break;
                case 270:
                    if(UISTATE.current_mode %4 != 3)
                        rotate_event_callback(&PHONE, base_mode + 3);
                    break;
                default:
                    assert(0);
            }
        }
    }
    else
    {
        if(from_skin)
            show_message("Warning", "Sensor server is not ready!\nYou cannot rotate the emulator until sensor server is ready.");
        return -1;
    }

    if(from_skin)
        return rotation;
    else
        return 1;
}

int parse_val(char *buff, unsigned char data, char *parsbuf)
{
    int count = 0;
    while(1)
    {
        if(count > 12)
            return -1;
        if(buff[count] == data)
        {
            count++;
            strncpy(parsbuf, buff, count);
            return count;   
        }
        count++;
    }

    return 0;
}
#if 0
int main(int argc, char *args[])
{
    init_sensor_server(0);
}
#endif
#endif

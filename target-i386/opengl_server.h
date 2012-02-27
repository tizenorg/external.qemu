/*
 * opengl_server.h
 *
 *  Created on: 2011. 6. 30.
 *      Author: dhhong
 */

#ifndef OPENGL_SERVER_H_
#define OPENGL_SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#ifndef _WIN32
 #include <arpa/inet.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <netinet/tcp.h>
 #include <netdb.h>
 #include <X11/Xlib.h>
 #include <X11/Xutil.h>
#else
 #include <windows.h>
 #include <winsock.h>
#endif	/* _WIN32 */

#ifdef _WIN32
#define socklen_t	int
typedef HDC Display;
typedef unsigned long Window;
#else
#define closesocket close
#endif

typedef struct {
	int port;
	int different_windows;
	int display_function_call;
	int must_save;
	int parent_xid;
	int refresh_rate;
	int timestamp; /* only valid if must_save == 1. include timestamps in the save file to enable real-time playback */
} OGLS_Opts;

typedef struct {
	int sock;
	int count_last_time, count_current;
	struct timeval last_time, current_time, time_stamp_start;
	struct timeval last_read_time, current_read_time;

	struct sockaddr_in clientname;
#ifdef _WIN32
	Display Display;
	HDC active_win; /* FIXME */
#else
	Display* parent_dpy;
	Window qemu_parent_window;

	Display* Display;
	Window active_win; /* FIXME */
#endif
	int active_win_x;
	int active_win_y;

	int local_connection;

	int last_assigned_internal_num;
	int last_active_internal_num;

	int  nTabAssocAttribListVisual ;
	void* tabAssocAttribListVisual ;
	void * processTab;

	OGLS_Opts *pOption;
} OGLS_Conn ;

/* opengl_server main function */
void *init_opengl_server(void *arg);

#endif /* OPENGL_SERVER_H_ */

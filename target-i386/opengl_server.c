/*
 *  TCP/IP OpenGL server
 *
 *  Copyright (c) 2007 Even Rouault
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* gcc -Wall -O2 -g opengl_server.c opengl_exec.c -o opengl_server -I../i386-softmmu -I. -I.. -lGL */
/* gcc -g -o opengl_server opengl_server.c opengl_exec.c -I../i386-softmmu -I. -I.. -I/c/mingw/include -lopengl32 -lws2_32 -lgdi32 -lpthread */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#ifdef _WIN32 // or __MINGW32__
	#include <windows.h>
	#include <winsock2.h>
#else // _WIN32
// #include <sys/types.h>
// #include <sys/time.h> 
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <errno.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netinet/tcp.h>
// #include <netdb.h> 
// #include <signal.h> 
// #include <X11/Xlib.h>
// #include <X11/Xutil.h>
#endif // _WIN32

#define ENABLE_GL_LOG

#include "opengl_func.h"
#include "opengl_utils.h"
#include "opengl_server.h"
#include "sdb.h"

#include "../tizen/src/debug_ch.h"

MULTI_DEBUG_CHANNEL(qemu,opengl_server);


extern int display_function_call;

#ifdef _WIN32
HWND		displayHWND;
static Display CreateDisplay(void)
{
	HWND        hWnd;
	WNDCLASS    wc;
	LPSTR       ClassName ="DISPLAY";
	HINSTANCE hInstance = 0;

	/* only register the window class once - use hInstance as a flag. */
	hInstance = GetModuleHandle(NULL);
	wc.style         = CS_OWNDC;
	wc.lpfnWndProc   = (WNDPROC)DefWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = ClassName;

	RegisterClass(&wc);

	displayHWND = CreateWindow(ClassName, ClassName, (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU ),
			0, 0, 10, 10, NULL, (HMENU)NULL, hInstance, NULL);


	ShowWindow(hWnd, SW_HIDE);

	return GetDC(displayHWND);
}
#endif // _WIN32

#ifdef ENABLE_GL_LOG
static FILE* f = NULL;

static const char* filename = "/tmp/debug_gl.bin";

#define write_gl_debug_init() do { if (f == NULL) f = fopen(filename, "wb"); } while(0)

inline static void  write_gl_debug_cmd_char(char my_int)
{
	write_gl_debug_init();
	fwrite(&my_int, sizeof(my_int), 1, f);
}

inline static void  write_gl_debug_cmd_short(short my_int)
{
	write_gl_debug_init();
	fwrite(&my_int, sizeof(my_int), 1, f);
}

inline static void write_gl_debug_cmd_int(int my_int)
{
	write_gl_debug_init();
	fwrite(&my_int, sizeof(my_int), 1, f);
}

inline static void  write_gl_debug_cmd_longlong(long long my_longlong)
{
	write_gl_debug_init();
	fwrite(&my_longlong, sizeof(my_longlong), 1, f);
}

inline static void  write_gl_debug_cmd_buffer_with_size(int size, void* buffer)
{
	write_gl_debug_init();
	fwrite(&size, sizeof(int), 1, f);
	if (size)
		fwrite(buffer, size, 1, f);
}

inline static void  write_gl_debug_cmd_buffer_without_size(int size, void* buffer)
{
	write_gl_debug_init();
	if (size)
		fwrite(buffer, size, 1, f);
}

inline static void  write_gl_debug_end(void)
{
	write_gl_debug_init();
	fclose(f);
	f = NULL;
}

#endif

static int write_sock_data(int sock, void* data, int len)
{
	int offset = 0;

	if (len && data)
	{
		while(offset < len)
		{
			int nwritten = send(sock, data + offset, len - offset, 0);
			if (nwritten == -1)
			{
#ifndef _WIN32
				if (errno == EINTR)
					continue;
#endif
				ERR("write error : %s(%d)\n", strerror(errno), errno);
				//assert(nwritten != -1);
				return -1;
			}
			offset += nwritten;
		}
	}

	return offset;
}

inline static int write_sock_int(int sock, int my_int)
{
	return write_sock_data(sock, &my_int, sizeof(int));
}

static int total_read = 0;
static void read_sock_data(int sock, void* data, int len)
{
	if (len)
	{
		int offset = 0;
		while(offset < len)
		{
			int nread = recv(sock, data + offset, len - offset, 0);
			if (nread == -1)
			{
#ifndef _WIN32
				if (errno == EINTR)
					continue;
#endif
				ERR("read(%d) error : %s(%d)\n", sock, strerror(errno), errno);
				//assert(nread != -1);
			}
			if (nread == 0)
			{
				//fprintf(stderr, "nread = 0\n");
			}
			//assert(nread > 0);
			if (nread > 0)
			{
				offset += nread;
				total_read += nread;
			}
		}
	}
}

inline static int read_sock_int(int sock)
{
	int ret;
	read_sock_data(sock, &ret, sizeof(int));
	return ret;
}

inline static short read_sock_short(int sock)
{
	short ret;
	read_sock_data(sock, &ret, sizeof(short));
	return ret;
}

static int OGLS_readConn( OGLS_Conn *pConn )
{
	int sock = pConn->sock;
	long args[50];
	int args_size[50];
	char ret_string[32768];
	char command_buffer[65536*16];

	if( pConn->Display == NULL )
	{
		create_process_tab(pConn);
#ifdef _WIN32
		pConn->Display = CreateDisplay();
#else
		pConn->Display = XOpenDisplay(NULL);
		if (pConn->pOption->parent_xid != -1)
		{
			opengl_exec_set_parent_window(pConn, pConn->pOption->parent_xid);
		}
#endif // _WIN32
	}

	int i;
	int func_number = read_sock_short(sock);

	TRACE("OGLS_readConn (%s)\n", tab_opengl_calls_name[func_number]);

	Signature* signature = (Signature*)tab_opengl_calls[func_number];
	int ret_type = signature->ret_type;
	int nb_args = signature->nb_args;
	int* args_type = signature->args_type;
	int pid = 0;

	if (func_number == _serialized_calls_func)
	{
		int command_buffer_size = read_sock_int(sock);
		int commmand_buffer_offset = 0;
		read_sock_data(sock, command_buffer, command_buffer_size);

#ifdef ENABLE_GL_LOG
		if (pConn->pOption->must_save) write_gl_debug_cmd_short(_serialized_calls_func);
#endif

		while(commmand_buffer_offset < command_buffer_size)
		{
			func_number = *(short*)(command_buffer + commmand_buffer_offset);
			if( ! (func_number >= 0 && func_number < GL_N_CALLS) )
			{
				fprintf(stderr, "func_number >= 0 && func_number < GL_N_CALLS failed at "
						"commmand_buffer_offset=%d (command_buffer_size=%d)\n",
						commmand_buffer_offset, command_buffer_size);
				//exit(-1);
				return -1;
			}

#ifdef ENABLE_GL_LOG
			if (pConn->pOption->must_save) write_gl_debug_cmd_short(func_number);
#endif
			TRACE("serialized call is %s\n", tab_opengl_calls_name[func_number]);

			commmand_buffer_offset += sizeof(short);


			signature = (Signature*)tab_opengl_calls[func_number];
			ret_type = signature->ret_type;
			assert(ret_type == TYPE_NONE);
			nb_args = signature->nb_args;
			args_type = signature->args_type;

			for(i=0;i<nb_args;i++)
			{
				switch(args_type[i])
				{
					case TYPE_UNSIGNED_CHAR:
					case TYPE_CHAR:
						{
							args[i] = *(int*)(command_buffer + commmand_buffer_offset);
#ifdef ENABLE_GL_LOG
							if (pConn->pOption->must_save) write_gl_debug_cmd_char(args[i]);
#endif
							commmand_buffer_offset += sizeof(int);
							break;
						}

					case TYPE_UNSIGNED_SHORT:
					case TYPE_SHORT:
						{
							args[i] = *(int*)(command_buffer + commmand_buffer_offset);
#ifdef ENABLE_GL_LOG
							if (pConn->pOption->must_save) write_gl_debug_cmd_short(args[i]);
#endif
							commmand_buffer_offset += sizeof(int);
							break;
						}

					case TYPE_UNSIGNED_INT:
					case TYPE_INT:
					case TYPE_FLOAT:
						{
							args[i] = *(int*)(command_buffer + commmand_buffer_offset);
#ifdef ENABLE_GL_LOG
							if (pConn->pOption->must_save) write_gl_debug_cmd_int(args[i]);
#endif
							commmand_buffer_offset += sizeof(int);
							break;
						}

					case TYPE_NULL_TERMINATED_STRING:
CASE_IN_UNKNOWN_SIZE_POINTERS:
						{
							args_size[i] = *(int*)(command_buffer + commmand_buffer_offset);
							commmand_buffer_offset += sizeof(int);

							if (args_size[i] == 0)
							{
								args[i] = 0;
							}
							else
							{
								args[i] = (long)(command_buffer + commmand_buffer_offset);
							}

							if (args[i] == 0)
							{
								if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number))
								{
									fprintf(stderr, "call %s arg %d pid=%d\n", tab_opengl_calls_name[func_number], i, pid);
									return 0;
								}
							}
#ifdef ENABLE_GL_LOG
							if (pConn->pOption->must_save) write_gl_debug_cmd_buffer_with_size(args_size[i], (void*)args[i]);
#endif
							commmand_buffer_offset += args_size[i];

							break;
						}

CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
						{
							args_size[i] = compute_arg_length(stderr, func_number, i, args);
							args[i] = (args_size[i]) ? (long)(command_buffer + commmand_buffer_offset) : 0;
#ifdef ENABLE_GL_LOG
							if (pConn->pOption->must_save) write_gl_debug_cmd_buffer_without_size(args_size[i], (void*)args[i]);
#endif
							commmand_buffer_offset += args_size[i];
							break;
						}

CASE_OUT_POINTERS:
						{
							fprintf(stderr, "shouldn't happen TYPE_OUT_xxxx : call %s arg %d pid=%d\n", tab_opengl_calls_name[func_number], i, pid);
							return 0;
							break;
						}

					case TYPE_DOUBLE:
CASE_IN_KNOWN_SIZE_POINTERS:
						args[i] = (long)(command_buffer + commmand_buffer_offset);
						args_size[i] = tab_args_type_length[args_type[i]];
#ifdef ENABLE_GL_LOG
						if (pConn->pOption->must_save) write_gl_debug_cmd_buffer_without_size(tab_args_type_length[args_type[i]], (void*)args[i]);
#endif
						commmand_buffer_offset += tab_args_type_length[args_type[i]];
						break;

					case TYPE_IN_IGNORED_POINTER:
						args[i] = 0;
						break;

					default:
						fprintf(stderr, "shouldn't happen : call %s arg %d pid=%d\n", tab_opengl_calls_name[func_number], i, pid);
						return 0;
						break;
				}
			}

			if (display_function_call) display_gl_call(stderr, func_number, args, args_size);
			do_function_call(pConn, func_number, 1, args, ret_string);
		}
	}
	else
	{
#ifdef ENABLE_GL_LOG
		if (pConn->pOption->must_save && func_number != _synchronize_func) write_gl_debug_cmd_short(func_number);
#endif

		for(i=0;i<nb_args;i++)
		{
			switch(args_type[i])
			{
				case TYPE_UNSIGNED_CHAR:
				case TYPE_CHAR:
					args[i] = read_sock_int(sock);
#ifdef ENABLE_GL_LOG
					if (pConn->pOption->must_save) write_gl_debug_cmd_char(args[i]);
#endif
					break;

				case TYPE_UNSIGNED_SHORT:
				case TYPE_SHORT:
					args[i] = read_sock_int(sock);
#ifdef ENABLE_GL_LOG
					if (pConn->pOption->must_save) write_gl_debug_cmd_short(args[i]);
#endif
					break;

				case TYPE_UNSIGNED_INT:
				case TYPE_INT:
				case TYPE_FLOAT:
					args[i] = read_sock_int(sock);
#ifdef ENABLE_GL_LOG
					if (pConn->pOption->must_save) write_gl_debug_cmd_int(args[i]);
#endif
					break;

				case TYPE_NULL_TERMINATED_STRING:
CASE_IN_UNKNOWN_SIZE_POINTERS:
					{
						args_size[i] = read_sock_int(sock);
						if (args_size[i])
						{
							args[i] = (long)malloc(args_size[i]);
							read_sock_data(sock, (void*)args[i], args_size[i]);
						}
						else
						{
							args[i] = 0;
							if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number))
							{
								fprintf(stderr, "call %s arg %d\n", tab_opengl_calls_name[func_number], i);
								return 0;
							}
						}
#ifdef ENABLE_GL_LOG
						if (pConn->pOption->must_save) write_gl_debug_cmd_buffer_with_size(args_size[i], (void*)args[i]);
#endif
						break;
					}

CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
					{
						args_size[i] = compute_arg_length(stderr, func_number, i, args);
						args[i] = (args_size[i]) ? (long)malloc(args_size[i]) : 0;
						read_sock_data(sock, (void*)args[i], args_size[i]);
#ifdef ENABLE_GL_LOG
						if (pConn->pOption->must_save) write_gl_debug_cmd_buffer_without_size(args_size[i], (void*)args[i]);
#endif
						break;
					}

CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
					{
						args_size[i] = compute_arg_length(stderr, func_number, i, args);
						args[i] = (long)malloc(args_size[i]);
						break;
					}

CASE_OUT_UNKNOWN_SIZE_POINTERS:
					{
						args_size[i] = read_sock_int(sock);
						if (func_number == glGetProgramLocalParameterdvARB_func)
						{
							fprintf(stderr, "size = %d\n", args_size[i]);
						}
						if (args_size[i])
						{
							args[i] = (long)malloc(args_size[i]);
						}
						else
						{
							if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number))
							{
								fprintf(stderr, "call %s arg %d pid=%d\n", tab_opengl_calls_name[func_number], i, pid);
								return 0;
							};
							args[i] = 0;
						}
						//fprintf(stderr, "%p %d\n", (void*)args[i], args_size[i]);
#ifdef ENABLE_GL_LOG
						if (pConn->pOption->must_save) write_gl_debug_cmd_int(args_size[i]);
#endif
						break;
					}

CASE_OUT_KNOWN_SIZE_POINTERS:
					{
						args_size[i] = tab_args_type_length[args_type[i]];
						assert(args_size[i]);
						args[i] = (long)malloc(args_size[i]);
						//fprintf(stderr, "%p %d\n", (void*)args[i], args_size[i]);
						break;
					}

				case TYPE_DOUBLE:
CASE_IN_KNOWN_SIZE_POINTERS:
					args_size[i] = tab_args_type_length[args_type[i]];
					args[i] = (long)malloc(args_size[i]);
					read_sock_data(sock, (void*)args[i], args_size[i]);
#ifdef ENABLE_GL_LOG
					if (pConn->pOption->must_save) write_gl_debug_cmd_buffer_without_size(tab_args_type_length[args_type[i]], (void*)args[i]);
#endif
					break;

				case TYPE_IN_IGNORED_POINTER:
					args[i] = 0;
					break;

				default:
					fprintf(stderr, "shouldn't happen : call %s arg %d\n", tab_opengl_calls_name[func_number], i);
					return 0;
					break;
			}
		}

		if (display_function_call) display_gl_call(stderr, func_number, args, args_size);

		if (getenv("ALWAYS_FLUSH")) fflush(f);

		int ret = do_function_call(pConn, func_number, 1, args, ret_string);
#ifdef ENABLE_GL_LOG
		if (pConn->pOption->must_save && func_number == glXGetVisualFromFBConfig_func)
		{
			write_gl_debug_cmd_int(ret);
		}
#endif

		for(i=0;i<nb_args;i++)
		{
			switch(args_type[i])
			{
				case TYPE_UNSIGNED_INT:
				case TYPE_INT:
				case TYPE_UNSIGNED_CHAR:
				case TYPE_CHAR:
				case TYPE_UNSIGNED_SHORT:
				case TYPE_SHORT:
				case TYPE_FLOAT:
					break;

				case TYPE_NULL_TERMINATED_STRING:
				case TYPE_DOUBLE:
CASE_IN_POINTERS:
					if (args[i]) free((void*)args[i]);
					break;

CASE_OUT_POINTERS:
					//fprintf(stderr, "%p %d\n", (void*)args[i], args_size[i]);
					if( 0> write_sock_data(sock, (void*)args[i], args_size[i]) )
					{
						perror( "write_sock_data" ) ;
						return -1 ;
					}

					if (display_function_call)
					{
						if (args_type[i] == TYPE_OUT_1INT)
						{
							fprintf(stderr, "out[%d] : %d\n", i, *(int*)args[i]);
						}
						else if (args_type[i] == TYPE_OUT_1FLOAT)
						{
							fprintf(stderr, "out[%d] : %f\n", i, *(float*)args[i]);
						}
					}
					if (args[i]) free((void*)args[i]);
					break;

				case TYPE_IN_IGNORED_POINTER:
					args[i] = 0;
					break;

				default:
					fprintf(stderr, "shouldn't happen : call %s arg %d\n", tab_opengl_calls_name[func_number], i);
					return 0;
					break;
			}
		}

		if (signature->ret_type == TYPE_CONST_CHAR)
		{
			if( 0 > write_sock_int(sock, strlen(ret_string) + 1) )
			{
				perror( "write_sock_int" ) ;
				return -1 ;
			}
			if( 0 > write_sock_data(sock, ret_string, strlen(ret_string) + 1) )
			{
				perror( "write_sock_data" ) ;
				return -1 ;
			}
		}
		else if (signature->ret_type != TYPE_NONE)
		{
			if( 0 > write_sock_int(sock, ret) )
			{
				perror( "write_sock_int" ) ;
				return -1 ;
			}
		}

#ifdef ENABLE_GL_LOG
		if (pConn->pOption->must_save && func_number == _exit_process_func)
		{
			write_gl_debug_end();
		}
#endif
		if (func_number == _exit_process_func)
		{
			return -1;
		}
		else if (func_number == glXSwapBuffers_func)
		{
			int diff_time;
			pConn->count_current++;
			gettimeofday(&pConn->current_time, NULL);
#ifdef ENABLE_GL_LOG
			if (pConn->pOption->must_save && pConn->pOption->timestamp)
			{
				long long ts = (pConn->current_time.tv_sec - pConn->time_stamp_start.tv_sec) * (long long)1000000 + pConn->current_time.tv_usec - pConn->time_stamp_start.tv_usec;
				/* -1 is special code that indicates time synchro */
				write_gl_debug_cmd_short(timesynchro_func);
				write_gl_debug_cmd_longlong(ts);
			}
#endif
			diff_time = (pConn->current_time.tv_sec - pConn->last_time.tv_sec) * 1000 + (pConn->current_time.tv_usec - pConn->last_time.tv_usec) / 1000;
			if (diff_time > pConn->pOption->refresh_rate)
			{
#ifdef ENABLE_GL_LOG
				fflush(f);
#endif
				printf("%d frames in %.1f seconds = %.3f FPS\n",
						pConn->count_current - pConn->count_last_time,
						diff_time / 1000.,
						(pConn->count_current - pConn->count_last_time) * 1000. / diff_time);
				pConn->last_time.tv_sec = pConn->current_time.tv_sec;
				pConn->last_time.tv_usec = pConn->current_time.tv_usec;
				pConn->count_last_time = pConn->count_current;
			}
		}
	}
	return 0;
}

static int OGLS_createListenSocket (uint16_t port)
{
	int sock;
	struct sockaddr_in name;

	/* Create the socket. */
	sock = socket (PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		ERR("socket error : %s(%d)\n", strerror(errno), errno);
		//exit (EXIT_FAILURE);
		return -1;
	}

	int flag = 1;
	if (setsockopt(sock, IPPROTO_IP, SO_REUSEADDR,(char *)&flag, sizeof(int)) != 0)
	{
		ERR("setsockopt error(SO_REUSEADDR) : %s(%d)\n", strerror(errno), errno);
	}
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(char *)&flag, sizeof(int)) != 0)
	{
		ERR("setsockopt error(TCP_NODELAY) : %s(%d)\n", strerror(errno), errno);
	}

	/* Give the socket a name. */
	name.sin_family = AF_INET;
	name.sin_port = htons (port);
	name.sin_addr.s_addr = htonl (INADDR_ANY);
	if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
	{
		fprintf(stderr, "bind(INADDR_ANY:%d): errorno = %d(%s)\n", port, errno, strerror(errno));
		ERR("bind error : %s(%d)\n", strerror(errno), errno);
		//exit (EXIT_FAILURE);
		//exit (EXIT_FAILURE);
		return -1;
	}

	//fprintf(stderr, "Port(%d/tcp) listen for opengl \n", port);

	return sock;
}

static OGLS_Conn * OGLS_createConn( OGLS_Opts *pOption )
{
	OGLS_Conn *pConn = malloc(sizeof(OGLS_Conn));
	if( !pConn )
	{
		return NULL ;
	}

	memset( pConn, 0, sizeof(*pConn) );
	pConn->pOption = pOption;
	return pConn;
}

static void OGLS_removeConn( OGLS_Conn *pConn )
{
	if( !pConn ) return ;

	if( pConn->sock > 0 )
	{
		closesocket(pConn->sock);
		pConn->sock = 0 ;
	}

	if( pConn->Display )
	{
#ifdef _WIN32
		//		ReleaseDC(pConn->Display);
#else
		XCloseDisplay( pConn->Display );
		pConn->Display = NULL;
#endif // _WIN32
	}

	remove_process_tab(pConn);

	free( pConn );
}

static void OGLS_loop( OGLS_Conn *pConn )
{
	//fprintf (stderr, "Server: connect from host %s, port %hd.\n",
	//		inet_ntoa (pConn->clientname.sin_addr),
	//		ntohs (pConn->clientname.sin_port));

	gettimeofday(&pConn->last_time, NULL);
	gettimeofday(&pConn->last_read_time, NULL);

#ifndef _WIN32
	if (strcmp(inet_ntoa(pConn->clientname.sin_addr), "127.0.0.1") == 0 &&
			pConn->pOption->different_windows == 0)
	{
		pConn->local_connection = 1;
	}
#endif // _WIN32

	if( pConn->pOption->timestamp )
	{
		gettimeofday(&pConn->time_stamp_start, NULL);
	}

	while( OGLS_readConn(pConn) >= 0 );

	do_function_call(pConn, _exit_process_func, 1, NULL, NULL);

	//fprintf (stderr, "Server: disconnect from host %s, port %hd.\n",
	//		inet_ntoa (pConn->clientname.sin_addr),
	//		ntohs (pConn->clientname.sin_port));

	OGLS_removeConn( pConn );
}

#ifndef _WIN32
int has_x_error = 0;
static int OGLS_x_error_handler(Display *display, XErrorEvent *error)
{
	char buf[64];
	XGetErrorText(display, error->error_code, buf, 63);
	fprintf (stderr, "The program received an X Window System error.\n"
			"This probably reflects a bug in the program.\n"
			"The error was '%s'.\n"
			"  (Details: serial %ld error_code %d request_code %d minor_code %d)\n",
			buf,
			error->serial,
			error->error_code,
			error->request_code,
			error->minor_code);
	has_x_error = 1;
	return 0;
}
#endif // _WIN32

static void OGLS_main( OGLS_Opts *pOption )
{
	int sock;
	fd_set active_fd_set, read_fd_set;

	socklen_t size;

#ifndef _WIN32
	XSetErrorHandler(OGLS_x_error_handler);
#endif // _WIN32

	/* Create the socket and set it up to accept connections. */
	sock = OGLS_createListenSocket( pOption->port );
	if (sock < 0)
		return;

	if (listen (sock, 1) < 0)
	{
		ERR("listen error : %s(%d)\n", strerror(errno), errno);
		//exit (EXIT_FAILURE);
		return;
	}

	FD_ZERO (&active_fd_set);
	FD_SET (sock, &active_fd_set);

	while(1)
	{
		OGLS_Conn *pConn = NULL;
		pthread_t taskid;

		read_fd_set = active_fd_set;
		if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
		{
#ifndef _WIN32
			if (errno == EINTR) {
				continue;
			}
#endif
			ERR("select error : %s(%d)\n", strerror(errno), errno);
			//exit (EXIT_FAILURE);
			break;
		}

		pConn = OGLS_createConn( pOption );
		if( !pConn )
		{
			perror( "OGLS_createConn" );
			continue;
		}

		size = sizeof(pConn->clientname);
		pConn->sock = accept (sock, (struct sockaddr *) &pConn->clientname, &size);
		if (pConn->sock < 0)
		{
			ERR("accept error : %s(%d)\n", strerror(errno), errno);
			OGLS_removeConn( pConn );
			continue;
		}

		if( pthread_create( (pthread_t *)&taskid, NULL, (void *(*)(void *))OGLS_loop, (void *)pConn ) )
		{
			ERR("pthread_create error : %s(%d)\n", strerror(errno), errno);
			OGLS_removeConn( pConn );
			continue;
		}
	}

	closesocket( sock );
}

#if 0
static void usage(void)
{
	printf("Usage : opengl_server [OPTION]\n\n");
	printf("The following options are available :\n");
	printf("--port=XXXX         : set XXX as the port number for the TCP/IP server \n");
	printf("--debug             : output debugging trace on stderr\n");
	printf("--save              : dump the serialialized OpenGL flow in a file (default : /tmp/debug_gl.bin)\n");
	printf("--filename=X        : the file where to write the serailized OpenGL flow\n");
	printf("--different-windows : even if the client is on 127.0.0.1, display OpenGL on a new X window\n");
	printf("--parent-xid=XXX    : use XXX as the XID of the parent X window where to display the OpenGL flow\n");
	printf("                     This is useful if you want to run accelerated OpenGL inside a non-patched QEMU\n");
	printf("                     or from another emulator, through TCP/IP\n");
	printf("--h or --help       : display this help\n");
}
#endif

//int main (int argc, char* argv[])
void *init_opengl_server(void *arg)
{
	OGLS_Opts option;

	memset( &option, 0, sizeof(option) );

	// set default values
	option.port = get_sdb_base_port() + SDB_TCP_OPENGL_INDEX;
	option.parent_xid = -1;
	option.refresh_rate = 1000;
	option.timestamp = 1; /* only valid if must_save == 1. include timestamps in the save file to enable real-time playback */

#if 0
	for(i=1;i<argc;i++)
	{
		if (argv[i][0] == '-' && argv[i][1] == '-')
			argv[i] = argv[i]+1;

		if (strcmp(argv[i], "-debug") == 0)
		{
			option.display_function_call = 1;
		}
		else if (strcmp(argv[i], "-save") == 0)
		{
			option.must_save = 1;
		}
		else if (strncmp(argv[i], "-port=",6) == 0)
		{
			option.port = atoi(argv[i] + 6);
		}
		else if (strncmp(argv[i], "-filename=",strlen("-filename=")) == 0)
		{
			filename = argv[i] + strlen("-filename=");
		}
		else if (strncmp(argv[i], "-parent-xid=",strlen("-parent-xid=")) == 0)
		{
			char* c = argv[i] + strlen("-parent-xid=");
			option.parent_xid = strtol(c, NULL, 0);
			option.different_windows = 1;
		}
		else if (strcmp(argv[i], "-different-windows") == 0)
		{
			option.different_windows = 1;
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0)
		{
			usage();
			return 0;
		}
		else
		{
			fprintf(stderr, "unknown parameter : %s\n", argv[i]);
			usage();
			return -1;
		}
	}
#endif	/* 0 */

	OGLS_main(&option);
	return 0;
}

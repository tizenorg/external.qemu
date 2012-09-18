/*
 * Management of the debugging channels
 *
 * Copyright 2000 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * @file	debug_ch.c
 * @brief	Management of the debugging channels
 *
 * @author
 * @date
 * @attention
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "qemu-common.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

#include "emulator.h"
#include "debug_ch.h"

// DEBUGCH file is located in binary directory.
char bin_dir[256] = {0,};

static char logpath[512] = {0,};
static char debugchfile[512] = {0, };
#ifdef _WIN32
static HANDLE handle;
#endif

void set_log_path(char *path)
{
    strcpy(logpath, path);
}

char *get_log_path(void)
{
    return logpath;
}

static inline int interlocked_xchg_add( int *dest, int incr )
{
	int ret;
	__asm__ __volatile__( "lock; xaddl %0,(%1)"
			: "=r" (ret) : "r" (dest), "0" (incr) : "memory" );
	return ret;
}

static const char * const debug_classes[] = {"fixme", "err", "warn", "trace", "info"};

#define MAX_DEBUG_OPTIONS 256

//static unsigned char default_flags = (1 << __DBCL_ERR) | (1 << __DBCL_FIXME) | (1 << __DBCL_INFO);
static unsigned char default_flags = (1 << __DBCL_ERR)  | (1 << __DBCL_INFO);
static int nb_debug_options = -1;
static struct _debug_channel debug_options[MAX_DEBUG_OPTIONS];

static void debug_init(void);

static int cmp_name( const void *p1, const void *p2 )
{
	const char *name = p1;
	const struct _debug_channel *chan = p2;
	return strcmp( name, chan->name );
}

/* get the flags to use for a given channel, possibly setting them too in case of lazy init */
unsigned char _dbg_get_channel_flags( struct _debug_channel *channel )
{
	if (nb_debug_options == -1)
		debug_init();

	if(nb_debug_options){
        		
		struct _debug_channel *opt;

		/* first check for multi channel */
		opt = bsearch( channel->multiname,
				debug_options,
				nb_debug_options,
				sizeof(debug_options[0]), cmp_name );
		if (opt)
			return opt->flags;

		opt = bsearch( channel->name,
				debug_options,
				nb_debug_options,
				sizeof(debug_options[0]), cmp_name );
		if (opt){
			return opt->flags;
		}
	}

	/* no option for this channel */
	if (channel->flags & (1 << __DBCL_INIT))
		channel->flags = default_flags;

	return default_flags;
}

/* set the flags to use for a given channel; return 0 if the channel is not available to set */
int _dbg_set_channel_flags( struct _debug_channel *channel,
		unsigned char set, unsigned char clear )
{
	if (nb_debug_options == -1)
		debug_init();

	if (nb_debug_options){
		struct _debug_channel *opt;

		/* first set for multi channel */
		opt = bsearch( channel->multiname,
				debug_options,
				nb_debug_options,
				sizeof(debug_options[0]), cmp_name );
		if (opt){
			opt->flags = (opt->flags & ~clear) | set;
			return 1;
		}

		opt = bsearch( channel->name,
				debug_options,
				nb_debug_options,
				sizeof(debug_options[0]), cmp_name );
		if (opt){
			opt->flags = (opt->flags & ~clear) | set;
			return 1;
		}
	}
	return 0;
}

/* add a new debug option at the end of the option list */
static void add_option( const char *name, unsigned char set, unsigned char clear )
{
	int min = 0, max = nb_debug_options - 1, pos, res;

	if (!name[0])  /* "all" option */
	{
		default_flags = (default_flags & ~clear) | set;
		return;
	}

	if (strlen(name) >= sizeof(debug_options[0].name))
		return;

	while (min <= max)
	{
		pos = (min + max) / 2;
		res = strcmp( name, debug_options[pos].name );
		if (!res)
		{
			debug_options[pos].flags = (debug_options[pos].flags & ~clear) | set;
			return;
		}
		if (res < 0)
			max = pos - 1;
		else
			min = pos + 1;
	}
	if (nb_debug_options >= MAX_DEBUG_OPTIONS)
		return;

	pos = min;
	if (pos < nb_debug_options) {
		memmove( &debug_options[pos + 1],
				&debug_options[pos],
				(nb_debug_options - pos) * sizeof(debug_options[0]) );
	}

	strcpy( debug_options[pos].name, name );
	debug_options[pos].flags = (default_flags & ~clear) | set;
	nb_debug_options++;
}

/* parse a set of debugging option specifications and add them to the option list */
static void parse_options( const char *str )
{
	char *opt, *next, *options;
	unsigned int i;

	if (!(options = strdup(str)))
		return;
	for (opt = options; opt; opt = next)
	{
		const char *p;
		unsigned char set = 0, clear = 0;

		if ((next = strchr( opt, ',' )))
			*next++ = 0;

		p = opt + strcspn( opt, "+-" );
		if (!p[0])
			p = opt;  /* assume it's a debug channel name */

		if (p > opt)
		{
			for (i = 0; i < sizeof(debug_classes)/sizeof(debug_classes[0]); i++)
			{
				int len = strlen(debug_classes[i]);
				if (len != (p - opt))
					continue;
				if (!memcmp( opt, debug_classes[i], len ))  /* found it */
				{
					if (*p == '+')
						set |= 1 << i;
					else
						clear |= 1 << i;
					break;
				}
			}
			if (i == sizeof(debug_classes)/sizeof(debug_classes[0])) /* bad class name, skip it */
				continue;
		}
		else
		{
			if (*p == '-')
				clear = ~0;
			else
				set = ~0;
		}
		if (*p == '+' || *p == '-')
			p++;
		if (!p[0])
			continue;

		if (!strcmp( p, "all" ))
			default_flags = (default_flags & ~clear) | set;
		else
			add_option( p, set, clear );
	}
	free( options );
}

/* print the usage message */
static void debug_usage(void)
{
	static const char usage[] =
		"Syntax of the DEBUGCH variable:\n"
		"  DEBUGCH=[class]+xxx,[class]-yyy,...\n\n"
		"Example: DEBUGCH=+all,warn-heap\n"
		"    turns on all messages except warning heap messages\n"
		"Available message classes: err, warn, fixme, trace\n";
	const int ret = write( 2, usage, sizeof(usage) - 1 );
	assert(ret >= 0);
	exit(1);
}

/* initialize all options at startup */
static void debug_init(void)
{
	char *debug = NULL;
	FILE *fp = NULL;
	char *tmp = NULL;
    int open_flags;
    int fd;

    if (nb_debug_options != -1)
		return;  /* already initialized */

	nb_debug_options = 0;

#if 0
	strcpy(debugchfile, get_etc_path());
	strcat(debugchfile, "/DEBUGCH");
#endif

    if ( 0 == strlen( bin_dir ) ) {
        strcpy( debugchfile, "DEBUGCH" );
    } else {
        strcat( debugchfile, bin_dir );
#ifdef _WIN32
        strcat( debugchfile, "\\" );
#else
        strcat( debugchfile, "/" );
#endif
        strcat( debugchfile, "DEBUGCH" );
    }

	fp= fopen(debugchfile, "r");
	if( fp == NULL){
		debug = getenv("DEBUGCH");
	}else{
		if ((tmp= (char *)malloc(1024 + 1)) == NULL){
			fclose(fp);
			return;
		}

		fseek(fp, 0, SEEK_SET);
		const char* str = fgets(tmp, 1024, fp);
		if (str) {
			tmp[strlen(tmp)-1] = 0;
			debug = tmp;
		}

		fclose(fp);
	}

	if( debug != NULL ){
		if (!strcmp( debug, "help" ))
			debug_usage();
		parse_options( debug );
	}

	if( tmp != NULL ){
		free(tmp);
	}
	
	open_flags = O_BINARY | O_RDWR | O_CREAT | O_TRUNC;
	fd = qemu_open(logpath, open_flags, 0666);
    if(fd < 0) {
        fprintf(stderr, "Can't open logfile: %s\n", logpath);
    	exit(1);
    }
    close(fd);
}

/* allocate some tmp string space */
/* FIXME: this is not 100% thread-safe */
char *get_dbg_temp_buffer( size_t size )
{
	static char *list[32];
	static int pos;
	char *ret;
	int idx;

	idx = interlocked_xchg_add( &pos, 1 ) % (sizeof(list)/sizeof(list[0]));

	if ((ret = realloc( list[idx], size )))
		list[idx] = ret;

	return ret;
}

/* release unused part of the buffer */
void release_dbg_temp_buffer( char *buffer, size_t size )
{
	/* don't bother doing anything */
	(void)( buffer );
	(void)( size );
}

static int dbg_vprintf( const char *format, va_list args )
{
	char tmp[MSGSIZE_MAX] = { 0, };
	char txt[MSGSIZE_MAX] = { 0, };

	FILE *fp;
	// lock

	int ret = vsnprintf( tmp, MSGSIZE_MAX, format, args );

	tmp[MSGSIZE_MAX - 2] = '\n';
	tmp[MSGSIZE_MAX - 1] = 0;

	sprintf(txt, "%s", tmp);

	// unlock
	if ((fp = fopen(logpath, "a+")) == NULL) {
		fprintf(stdout, "Emulator can't open.\n"
				"Please check if "
				"this binary file is running on the right path.\n");
		exit(1);
	}

	fputs(txt, fp);
	fclose(fp);
	return ret;
}

int dbg_printf( const char *format, ... )
{
	int ret;
	va_list valist;

	va_start(valist, format);
	ret = dbg_vprintf( format, valist );
	va_end(valist);

	return ret;
}

int dbg_printf_nonewline( const char *format, ... )
{
	int ret;
	va_list valist;

	va_start(valist, format);
	ret = dbg_vprintf( format, valist );
	va_end(valist);

	return ret;
}

/* printf with temp buffer allocation */
const char *dbg_sprintf( const char *format, ... )
{
	static const int max_size = 200;
	char *ret;
	int len;
	va_list valist;

	va_start(valist, format);
	ret = get_dbg_temp_buffer( max_size );
	len = vsnprintf( ret, max_size, format, valist );
	if (len == -1 || len >= max_size)
		ret[max_size-1] = 0;
	else
		release_dbg_temp_buffer( ret, len + 1 );
	va_end(valist);
	return ret;
}

int dbg_log( enum _debug_class cls, struct _debug_channel *channel,
		const char *format, ... )
{
	int ret = 0;
	char buf[2048];
	va_list valist;
	int open_flags;
	int fd;
    
    if (!(_dbg_get_channel_flags( channel ) & (1 << cls)))
		return -1;

	ret += snprintf(buf, sizeof(buf),"[%s:%s", debug_classes[cls], channel->name);

	if (*channel->multiname)
		ret += snprintf(buf + ret, sizeof(buf) - ret, ":%s]", channel->multiname);
	else
		ret += snprintf(buf + ret, sizeof(buf) - ret, "]");

 	va_start(valist, format);
	ret += vsnprintf(buf + ret, sizeof(buf) - ret, format, valist );
	va_end(valist);
   
    open_flags = O_RDWR | O_APPEND | O_BINARY ;
	fd = qemu_open(logpath, open_flags, 0666);
	if(fd < 0) {
        fprintf(stderr, "Can't open logfile: %s\n", logpath);
    	exit(1);
    }
    qemu_write_full(fd, buf, ret);
    close(fd);

	return ret;
}

void assert_fail(char *exp, const char *file, int line)
{
	fprintf(stderr, "[%s][%d] Assert(%s) failed \n"
			, file, line, exp);
	fprintf(stdout, "[%s][%d] Assert(%s) failed \n"
			, file, line, exp);
	exit(0);
}

/* end of debug_ch.c */

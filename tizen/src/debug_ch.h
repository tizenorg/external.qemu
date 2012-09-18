/*
 * Wine debugging interface
 *
 * Copyright 1999 Patrik Stridvall
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
 * @file	debug_ch.h
 * @brief	Management of the debugging channels
 *
 * @author	
 * @date
 * @attention 
 */

#ifndef __DEBUGCHANNEL_H
#define __DEBUGCHANNEL_H

#include <sys/types.h>

// #define NO_DEBUG

#ifdef __cplusplus
extern "C" {
#endif

enum _debug_class
{
	__DBCL_FIXME,
	__DBCL_ERR,
	__DBCL_WARN,
	__DBCL_TRACE,
	__DBCL_INFO,

	__DBCL_INIT = 7  /* lazy init flag */
};

struct _debug_channel
{
	unsigned char flags;
	char name[15];
	char multiname[15];
};

void set_log_path(char *path);
char *get_log_path(void);

#ifndef NO_DEBUG
#define MSGSIZE_MAX 2048
#define __GET_DEBUGGING_FIXME(dbch) ((dbch)->flags & (1 << __DBCL_FIXME))
#define __GET_DEBUGGING_ERR(dbch)   ((dbch)->flags & (1 << __DBCL_ERR))
#define __GET_DEBUGGING_WARN(dbch)  ((dbch)->flags & (1 << __DBCL_WARN))
#define __GET_DEBUGGING_TRACE(dbch) ((dbch)->flags & (1 << __DBCL_TRACE))
#define __GET_DEBUGGING_INFO(dbch)  ((dbch)->flags & (1 << __DBCL_INFO))
#else
#define __GET_DEBUGGING_FIXME(dbch) 0
#define __GET_DEBUGGING_ERR(dbch)   0
#define __GET_DEBUGGING_WARN(dbch)  0
#define __GET_DEBUGGING_TRACE(dbch) 0
#define __GET_DEBUGGING_INFO(dbch)  0
#endif

#define __GET_DEBUGGING(dbcl,dbch)  __GET_DEBUGGING##dbcl(dbch)

#define __IS_DEBUG_ON(dbcl,dbch) \
	(__GET_DEBUGGING##dbcl(dbch) && \
	 (_dbg_get_channel_flags(dbch) & (1 << __DBCL##dbcl)))

#define __DPRINTF(dbcl,dbch) \
	do{ if(__GET_DEBUGGING(dbcl,(dbch))){ \
		struct _debug_channel * const __dbch =(struct _debug_channel *)(dbch); \
		const enum _debug_class __dbcl = __DBCL##dbcl; \
		_DBG_LOG

#define _DBG_LOG(args...) \
		dbg_log(__dbcl, (struct _debug_channel *)(__dbch), args); } }while(0)
/*

#define __DPRINTF(dbcl,dbch) \
	(!__GET_DEBUGGING(dbcl,(dbch)) || \
	 (dbg_log(__DBCL##dbcl,(dbch), "") == -1)) ? \
(void)0 : (void)dbg_printf
*/

extern char bin_dir[256];

extern unsigned char _dbg_get_channel_flags( struct _debug_channel *channel );
extern int _dbg_set_channel_flags( struct _debug_channel *channel,
		unsigned char set, unsigned char clear );

extern const char *dbg_sprintf( const char *format, ... );
extern int dbg_printf( const char *format, ... );
extern int dbg_printf_nonewline( const char *format, ... );
extern int dbg_log( enum _debug_class cls, struct _debug_channel *ch,
		const char *format, ... );

extern char *get_dbg_temp_buffer( size_t size );
extern void release_dbg_temp_buffer( char *buffer, size_t size );

#ifndef TRACE
#define TRACE                 __DPRINTF(_TRACE,_dbch___default)
#define TRACE_(ch)            __DPRINTF(_TRACE,&_dbch_##ch)
#endif
#define TRACE_ON(ch)          __IS_DEBUG_ON(_TRACE,&_dbch_##ch)

#ifndef WARN
#define WARN                  __DPRINTF(_WARN,_dbch___default)
#define WARN_(ch)             __DPRINTF(_WARN,&_dbch_##ch)
#endif
#define WARN_ON(ch)           __IS_DEBUG_ON(_WARN,&_dbch_##ch)

#ifndef FIXME
#define FIXME                 __DPRINTF(_FIXME,_dbch___default)
#define FIXME_(ch)            __DPRINTF(_FIXME,&_dbch_##ch)
#endif
#define FIXME_ON(ch)          __IS_DEBUG_ON(_FIXME,&_dbch_##ch)

#define ERR                   __DPRINTF(_ERR,_dbch___default)
#define ERR_(ch)              __DPRINTF(_ERR,&_dbch_##ch)
#define ERR_ON(ch)            __IS_DEBUG_ON(_ERR,&_dbch_##ch)

#ifndef INFO
#define INFO                  __DPRINTF(_INFO,_dbch___default)
#define INFO_(ch)             __DPRINTF(_INFO,&_dbch_##ch)
#endif
#define INFO_ON(ch)           __IS_DEBUG_ON(_INFO,&_dbch_##ch)

#define DECLARE_DEBUG_CHANNEL(ch) \
	static struct _debug_channel _dbch_##ch = { ~0, #ch, ""};
#define DEFAULT_DEBUG_CHANNEL(ch) \
	static struct _debug_channel _dbch_##ch = { ~0, #ch, ""}; \
static struct _debug_channel * const _dbch___default = &_dbch_##ch

#define MULTI_DEBUG_CHANNEL(ch, chm) \
	static struct _debug_channel _dbch_##ch = { ~0, #ch , #chm}; \
static struct _debug_channel * const _dbch___default = &_dbch_##ch

#define DPRINTF               dbg_printf
#define MESSAGE               dbg_printf

void assert_fail(char *exp, const char *file, int line);

#define ASSERT(exp) if (exp) ;                                      \
	else assert_fail( (char *)#exp, __FILE__, __LINE__ )

#ifdef __cplusplus
}
#endif

#endif  /* __DEBUGCHANNEL_H */

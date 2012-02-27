/* Copyright (C) 2006-2010 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#define SDB_HOST_PORT 26099

#ifdef _WIN32
#  ifndef EINTR
#    define EINTR        10004
#  endif
#  ifndef EAGAIN
#    define EAGAIN       10035
#  endif
#  ifndef EWOULDBLOCK
#    define EWOULDBLOCK  EAGAIN
#  endif
#  ifndef EINPROGRESS
#    define EINPROGRESS  10036
#  endif
#  ifndef EALREADY
#    define EALREADY     10037
#  endif
#  ifndef EDESTADDRREQ
#    define EDESTADDRREQ 10039
#  endif
#  ifndef EMSGSIZE
#    define EMSGSIZE     10040
#  endif
#  ifndef EPROTOTYPE
#    define EPROTOTYPE   10041
#  endif
#  ifndef ENOPROTOOPT
#    define ENOPROTOOPT  10042
#  endif
#  ifndef EAFNOSUPPORT
#    define EAFNOSUPPORT 10047
#  endif
#  ifndef EADDRINUSE
#    define EADDRINUSE   10048
#  endif
#  ifndef EADDRNOTAVAIL
#    define EADDRNOTAVAIL 10049
#  endif
#  ifndef ENETDOWN
#    define ENETDOWN     10050
#  endif
#  ifndef ENETUNREACH
#    define ENETUNREACH  10051
#  endif
#  ifndef ENETRESET
#    define ENETRESET    10052
#  endif
#  ifndef ECONNABORTED
#    define ECONNABORTED 10053
#  endif
#  ifndef ECONNRESET
#    define ECONNRESET   10054
#  endif
#  ifndef ENOBUFS
#    define ENOBUFS      10055
#  endif
#  ifndef EISCONN
#    define EISCONN      10056
#  endif
#  ifndef ENOTCONN
#    define ENOTCONN     10057
#  endif
#  ifndef ESHUTDOWN
#    define ESHUTDOWN     10058
#  endif
#  ifndef ETOOMANYREFS
#    define ETOOMANYREFS  10059
#  endif
#  ifndef ETIMEDOUT
#    define ETIMEDOUT     10060
#  endif
#  ifndef ECONNREFUSED
#    define ECONNREFUSED  10061
#  endif
#  ifndef ELOOP
#    define ELOOP         10062
#  endif
#  ifndef EHOSTDOWN
#    define EHOSTDOWN     10064
#  endif
#  ifndef EHOSTUNREACH
#    define EHOSTUNREACH  10065
#  endif
#endif /* _WIN32 */

#define SDB_TCP_EMULD_INDEX  3    /* emulator daemon port */
#define SDB_TCP_OPENGL_INDEX  4   /* opengl server port */

#define SDB_UDP_SENSOR_INDEX  3   /* sensor server port */
extern int tizen_base_port;
void sdb_setup(void);
int get_sdb_base_port(void);
int inet_strtoip(const char*  str, uint32_t  *ip);
int socket_send(int fd, const void*  buf, int  buflen);
void socket_close(int fd);

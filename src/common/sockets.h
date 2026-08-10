/*
 *  LibVNCServer/LibVNCClient common platform socket defines, includes
 *  and internal socket helper functions.
 *
 *  Copyright (C) 2022 Christian Beier <dontmind@freeshell.org>
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#ifndef _RFB_COMMON_SOCKETS_H
#define _RFB_COMMON_SOCKETS_H

#ifdef WIN32
/*
  Windows sockets
 */
#include <winsock2.h>
#include <ws2tcpip.h>

#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK

#undef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT

#undef EINTR
#define EINTR WSAEINTR

#undef EINVAL
#define EINVAL WSAEINVAL

/* MinGW has those, but MSVC not */
#ifdef _MSC_VER
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#define SHUT_RDWR SD_BOTH
#endif

#define read(sock,buf,len) recv(sock,buf,len,0)
#define write(sock,buf,len) send(sock,buf,len,0)

#else
/*
  Unix sockets
 */
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif

/*
  Common internal socket functions
 */
#include "rfb/rfbproto.h"

/*
  select() cannot handle file descriptors >= FD_SETSIZE (1024 on most systems),
  where FD_SET() is undefined behaviour. Use poll() instead where both the
  function and its header are available; the code falls back to select()
  otherwise. RFB_USE_POLL is internal to LibVNCServer, do not use it in
  installed headers.
 */
#if defined(LIBVNCSERVER_HAVE_POLL) && defined(LIBVNCSERVER_HAVE_POLL_H)
#include <poll.h>
#define RFB_USE_POLL 1
#endif

/*
   Set (non)blocking mode for a socket.
   Returns TRUE on succcess, FALSE on failure.
 */
rfbBool sock_set_nonblocking(rfbSocket sock, rfbBool non_blocking, void (*log)(const char *format, ...));


/*
   Wait for a socket to become connected.
   Returns TRUE if socket connected in time, FALSE if otherwise.
*/
rfbBool sock_wait_for_connected(int socket, unsigned int timeout_seconds);


#endif /* _RFB_COMMON_SOCKETS_H */

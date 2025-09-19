/*
 *  Copyright (C) 2011-2012 Christian Beier <dontmind@freeshell.org>
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
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

/*
 * sockets.c - functions to deal with sockets.
 */

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#ifdef __linux__
/* Setting this on other systems hides definitions such as INADDR_LOOPBACK.
 * The check should be for __GLIBC__ in fact. */
# define _POSIX_SOURCE
#endif
#endif
#if LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <rfb/rfbclient.h>
#include "sockets.h"
#include "tls.h"
#include "sasl.h"

void PrintInHex(rfbClient* client, char *buf, int len);

rfbBool errorMessageOnReadFailure = TRUE;

/*
 * ReadFromRFBServer is called whenever we want to read some data from the RFB
 * server.  It is non-trivial for two reasons:
 *
 * 1. For efficiency it performs some intelligent buffering, avoiding invoking
 *    the read() system call too often.  For small chunks of data, it simply
 *    copies the data out of an internal buffer.  For large amounts of data it
 *    reads directly into the buffer provided by the caller.
 *
 * 2. Whenever read() would block, it invokes the Xt event dispatching
 *    mechanism to process X events.  In fact, this is the only place these
 *    events are processed, as there is no XtAppMainLoop in the program.
 */

rfbBool
ReadFromRFBServer(rfbClient* client, char *out, unsigned int n)
{
  const int USECS_WAIT_PER_RETRY = 100000;
  int retries = 0;
#undef DEBUG_READ_EXACT
#ifdef DEBUG_READ_EXACT
	char* oout=out;
	unsigned int nn=n;
	rfbClientLog2(client, "ReadFromRFBServer %d bytes\n",n);
#endif

  /* Handle attempts to write to NULL out buffer that might occur
     when an outside malloc() fails. For instance, memcpy() to NULL
     results in undefined behaviour and probably memory corruption.*/
  if(!out)
    return FALSE;

  if (client->serverPort==-1) {
    /* vncrec playing */
    rfbVNCRec* rec = client->vncRec;
    struct timeval tv;

    if (rec->readTimestamp) {
      rec->readTimestamp = FALSE;
      if (!fread(&tv,sizeof(struct timeval),1,rec->file))
        return FALSE;

      tv.tv_sec = rfbClientSwap32IfLE (tv.tv_sec);
      tv.tv_usec = rfbClientSwap32IfLE (tv.tv_usec);

      if (rec->tv.tv_sec!=0 && !rec->doNotSleep) {
        struct timeval diff;
        diff.tv_sec = tv.tv_sec - rec->tv.tv_sec;
        diff.tv_usec = tv.tv_usec - rec->tv.tv_usec;
        if(diff.tv_usec<0) {
	  diff.tv_sec--;
	  diff.tv_usec+=1000000;
        }
#ifndef WIN32
        sleep (diff.tv_sec);
        usleep (diff.tv_usec);
#else
	Sleep (diff.tv_sec * 1000 + diff.tv_usec/1000);
#endif
      }

      rec->tv=tv;
    }
    
    return (fread(out,1,n,rec->file) != n ? FALSE : TRUE);
  }
  
  if (n <= client->buffered) {
    memcpy(out, client->bufoutptr, n);
    client->bufoutptr += n;
    client->buffered -= n;
#ifdef DEBUG_READ_EXACT
    goto hexdump;
#endif
    return TRUE;
  }

  memcpy(out, client->bufoutptr, client->buffered);

  out += client->buffered;
  n -= client->buffered;

  client->bufoutptr = client->buf;
  client->buffered = 0;

  if (n <= RFB_BUF_SIZE) {

    while (client->buffered < n) {
      int i;
      if (client->tlsSession)
        i = ReadFromTLS(client, client->buf + client->buffered, RFB_BUF_SIZE - client->buffered);
      else
#ifdef LIBVNCSERVER_HAVE_SASL
      if (client->saslconn)
        i = ReadFromSASL(client, client->buf + client->buffered, RFB_BUF_SIZE - client->buffered);
      else {
#endif /* LIBVNCSERVER_HAVE_SASL */
        i = read(client->sock, client->buf + client->buffered, RFB_BUF_SIZE - client->buffered);
#ifdef WIN32
	if (i < 0) errno=WSAGetLastError();
#endif
#ifdef LIBVNCSERVER_HAVE_SASL
      }
#endif
  
      if (i <= 0) {
	if (i < 0) {
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    if (client->readTimeout > 0 &&
		++retries > (client->readTimeout * 1000 * 1000 / USECS_WAIT_PER_RETRY))
	    {
	      rfbClientLog2(client, "Connection timed out\n");
	      return FALSE;
	    }
	    /* TODO:
	       ProcessXtEvents();
	    */
	    WaitForMessage(client, USECS_WAIT_PER_RETRY);
	    i = 0;
	  } else {
	    rfbClientErr2(client, "read (%d: %s)\n",errno,strerror(errno));
	    return FALSE;
	  }
	} else {
	  if (errorMessageOnReadFailure) {
	    rfbClientLog2(client, "VNC server closed connection\n");
	  }
	  return FALSE;
	}
      }
      client->buffered += i;
    }

    memcpy(out, client->bufoutptr, n);
    client->bufoutptr += n;
    client->buffered -= n;

  } else {

    while (n > 0) {
      int i;
      if (client->tlsSession)
        i = ReadFromTLS(client, out, n);
      else
#ifdef LIBVNCSERVER_HAVE_SASL
      if (client->saslconn)
        i = ReadFromSASL(client, out, n);
      else
#endif
        i = read(client->sock, out, n);

      if (i <= 0) {
	if (i < 0) {
#ifdef WIN32
	  errno=WSAGetLastError();
#endif
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    if (client->readTimeout > 0 &&
		++retries > (client->readTimeout * 1000 * 1000 / USECS_WAIT_PER_RETRY))
	    {
		rfbClientLog2(client, "Connection timed out\n");
		return FALSE;
	    }
	    /* TODO:
	       ProcessXtEvents();
	    */
	    WaitForMessage(client, USECS_WAIT_PER_RETRY);
	    i = 0;
	  } else {
	    rfbClientErr2(client, "read (%s)\n",strerror(errno));
	    return FALSE;
	  }
	} else {
	  if (errorMessageOnReadFailure) {
	    rfbClientLog2(client, "VNC server closed connection\n");
	  }
	  return FALSE;
	}
      }
      out += i;
      n -= i;
    }
  }

#ifdef DEBUG_READ_EXACT
hexdump:
  { unsigned int ii;
    for(ii=0;ii<nn;ii++)
      fprintf(stderr,"%02x ",(unsigned char)oout[ii]);
    fprintf(stderr,"\n");
  }
#endif

  return TRUE;
}


/*
 * Write an exact number of bytes, and don't return until you've sent them.
 */

rfbBool
WriteToRFBServer(rfbClient* client, const char *buf, unsigned int n)
{
  fd_set fds;
  int i = 0;
  int j;
  const char *obuf = buf;
#ifdef LIBVNCSERVER_HAVE_SASL
  const char *output;
  unsigned int outputlen;
  int err;
#endif /* LIBVNCSERVER_HAVE_SASL */

  if (client->serverPort==-1)
    return TRUE; /* vncrec playing */

  if (client->tlsSession) {
    /* WriteToTLS() will guarantee either everything is written, or error/eof returns */
    i = WriteToTLS(client, buf, n);
    if (i <= 0) return FALSE;

    return TRUE;
  }
#ifdef LIBVNCSERVER_HAVE_SASL
  if (client->saslconn) {
    err = sasl_encode(client->saslconn,
                      buf, n,
                      &output, &outputlen);
    if (err != SASL_OK) {
      rfbClientLog2(client, "Failed to encode SASL data %s",
                   sasl_errstring(err, NULL, NULL));
      return FALSE;
    }
    obuf = output;
    n = outputlen;
  }
#endif /* LIBVNCSERVER_HAVE_SASL */

  while (i < n) {
    j = write(client->sock, obuf + i, (n - i));
    if (j <= 0) {
      if (j < 0) {
#ifdef WIN32
	 errno=WSAGetLastError();
#endif
	if (errno == EWOULDBLOCK ||
#ifdef LIBVNCSERVER_ENOENT_WORKAROUND
		errno == ENOENT ||
#endif
		errno == EAGAIN) {
          if(client->sock == RFB_INVALID_SOCKET) {
              errno = EBADF;
              rfbClientErr2(client, "socket invalid\n");
              return FALSE;
          }

	  FD_ZERO(&fds);
	  FD_SET(client->sock,&fds);

	  if (select(client->sock+1, NULL, &fds, NULL, NULL) <= 0) {
	    rfbClientErr2(client, "select\n");
	    return FALSE;
	  }
	  j = 0;
	} else {
	  rfbClientErr2(client, "write\n");
	  return FALSE;
	}
      } else {
	rfbClientLog2(client, "write failed\n");
	return FALSE;
      }
    }
    i += j;
  }
  return TRUE;
}


rfbSocket
ConnectClientToTcpAddr(rfbClient* client, unsigned int host, int port)
{
  rfbSocket sock = ConnectClientToTcpAddrWithTimeout(client, host, port, DEFAULT_CONNECT_TIMEOUT);
  /* put socket back into blocking mode for compatibility reasons */
  if (sock != RFB_INVALID_SOCKET) {
    SetBlocking(client, sock);
  }
  return sock;
}

rfbSocket
ConnectClientToTcpAddrWithTimeout(rfbClient* client, unsigned int host, int port, unsigned int timeout)
{
  rfbSocket sock;
  struct sockaddr_in addr;
  int one = 1;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = host;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == RFB_INVALID_SOCKET) {
#ifdef WIN32
    errno=WSAGetLastError();
#endif
    rfbClientErr2(client, "ConnectToTcpAddr: socket (%s)\n",strerror(errno));
    return RFB_INVALID_SOCKET;
  }

  if (!SetNonBlocking(client, sock))
    return FALSE;

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef WIN32
    errno=WSAGetLastError();
#endif
    if (!((errno == EWOULDBLOCK || errno == EINPROGRESS) && sock_wait_for_connected(sock, timeout))) {
      rfbClientErr2(client, "ConnectToTcpAddr: connect\n");
      rfbCloseSocket(sock);
      return RFB_INVALID_SOCKET;
    }
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		 (char *)&one, sizeof(one)) < 0) {
    rfbClientErr2(client, "ConnectToTcpAddr: setsockopt\n");
    rfbCloseSocket(sock);
    return RFB_INVALID_SOCKET;
  }

  return sock;
}

rfbSocket
ConnectClientToTcpAddr6(rfbClient* client, const char *hostname, int port)
{
  rfbSocket sock = ConnectClientToTcpAddr6WithTimeout(client, hostname, port, DEFAULT_CONNECT_TIMEOUT);
  /* put socket back into blocking mode for compatibility reasons */
  if (sock != RFB_INVALID_SOCKET) {
    SetBlocking(client, sock);
  }
  return sock;
}

rfbSocket
ConnectClientToTcpAddr6WithTimeout(rfbClient* client, const char *hostname, int port, unsigned int timeout)
{
#ifdef LIBVNCSERVER_IPv6
  rfbSocket sock;
  int n;
  struct addrinfo hints, *res, *ressave;
  char port_s[10];
  int one = 1;

  snprintf(port_s, 10, "%d", port);
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if ((n = getaddrinfo(strcmp(hostname,"") == 0 ? "localhost": hostname, port_s, &hints, &res)))
  {
    rfbClientErr2(client, "ConnectClientToTcpAddr6: getaddrinfo (%s)\n", gai_strerror(n));
    return RFB_INVALID_SOCKET;
  }

  ressave = res;
  sock = RFB_INVALID_SOCKET;
  while (res)
  {
    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock != RFB_INVALID_SOCKET)
    {
      if (SetNonBlocking(client, sock)) {
        if (connect(sock, res->ai_addr, res->ai_addrlen) == 0) {
          break;
        } else {
#ifdef WIN32
          errno=WSAGetLastError();
#endif
          if ((errno == EWOULDBLOCK || errno == EINPROGRESS) && sock_wait_for_connected(sock, timeout))
            break;
          rfbCloseSocket(sock);
          sock = RFB_INVALID_SOCKET;
        }
      } else {
        rfbCloseSocket(sock);
        sock = RFB_INVALID_SOCKET;
      }
    }
    res = res->ai_next;
  }
  freeaddrinfo(ressave);

  if (sock == RFB_INVALID_SOCKET)
  {
    rfbClientErr2(client, "ConnectClientToTcpAddr6: connect\n");
    return RFB_INVALID_SOCKET;
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		 (char *)&one, sizeof(one)) < 0) {
    rfbClientErr2(client, "ConnectToTcpAddr: setsockopt\n");
    rfbCloseSocket(sock);
    return RFB_INVALID_SOCKET;
  }

  return sock;

#else

  rfbClientErr2(client, "ConnectClientToTcpAddr6: IPv6 disabled\n");
  return RFB_INVALID_SOCKET;

#endif
}

rfbSocket
ConnectClientToUnixSock(rfbClient* client, const char *sockFile)
{
  rfbSocket sock = ConnectClientToUnixSockWithTimeout(client, sockFile, DEFAULT_CONNECT_TIMEOUT);
  /* put socket back into blocking mode for compatibility reasons */
  if (sock != RFB_INVALID_SOCKET) {
    SetBlocking(client, sock);
  }
  return sock;
}

rfbSocket
ConnectClientToUnixSockWithTimeout(rfbClient* client, const char *sockFile, unsigned int timeout)
{
#ifdef WIN32
  rfbClientErr2(client, "Windows doesn't support UNIX sockets\n");
  return RFB_INVALID_SOCKET;
#else
  rfbSocket sock;
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  if(strlen(sockFile) + 1 > sizeof(addr.sun_path)) {
      rfbClientErr2(client, "ConnectToUnixSock: socket file name too long\n");
      return RFB_INVALID_SOCKET;
  }
  strcpy(addr.sun_path, sockFile);

  sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == RFB_INVALID_SOCKET) {
    rfbClientErr2(client, "ConnectToUnixSock: socket (%s)\n",strerror(errno));
    return RFB_INVALID_SOCKET;
  }

  if (!SetNonBlocking(client, sock))
    return RFB_INVALID_SOCKET;

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr.sun_family) + strlen(addr.sun_path)) < 0 &&
      !(errno == EINPROGRESS && sock_wait_for_connected(sock, timeout))) {
    rfbClientErr2(client, "ConnectToUnixSock: connect\n");
    rfbCloseSocket(sock);
    return RFB_INVALID_SOCKET;
  }

  return sock;
#endif
}



/*
 * FindFreeTcpPort tries to find unused TCP port in the range
 * (TUNNEL_PORT_OFFSET, TUNNEL_PORT_OFFSET + 99]. Returns 0 on failure.
 */

int
FindFreeTcpPort(rfbClient* client)
{
  rfbSocket sock;
  int port;
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == RFB_INVALID_SOCKET) {
    rfbClientErr2(client, ": FindFreeTcpPort: socket\n");
    return 0;
  }

  for (port = TUNNEL_PORT_OFFSET + 99; port > TUNNEL_PORT_OFFSET; port--) {
    addr.sin_port = htons((unsigned short)port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
      rfbCloseSocket(sock);
      return port;
    }
  }

  rfbCloseSocket(sock);
  return 0;
}


/*
 * ListenAtTcpPort starts listening at the given TCP port.
 */

rfbSocket
ListenAtTcpPort(rfbClient* client, int port)
{
  return ListenAtTcpPortAndAddress(client, port, NULL);
}



/*
 * ListenAtTcpPortAndAddress starts listening at the given TCP port on
 * the given IP address
 */

rfbSocket
ListenAtTcpPortAndAddress(rfbClient* client, int port, const char *address)
{
  rfbSocket sock = RFB_INVALID_SOCKET;
  int one = 1;
#ifndef LIBVNCSERVER_IPv6
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (address) {
    addr.sin_addr.s_addr = inet_addr(address);
  } else {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  }

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == RFB_INVALID_SOCKET) {
    rfbClientErr2(client, "ListenAtTcpPort: socket\n");
    return RFB_INVALID_SOCKET;
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		 (const char *)&one, sizeof(one)) < 0) {
    rfbClientErr2(client, "ListenAtTcpPort: setsockopt\n");
    rfbCloseSocket(sock);
    return RFB_INVALID_SOCKET;
  }

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    rfbClientErr2(client, "ListenAtTcpPort: bind\n");
    rfbCloseSocket(sock);
    return RFB_INVALID_SOCKET;
  }

#else
  int rv;
  struct addrinfo hints, *servinfo, *p;
  char port_str[8];

  snprintf(port_str, 8, "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; /* fill in wildcard address if address == NULL */

  if ((rv = getaddrinfo(address, port_str, &hints, &servinfo)) != 0) {
    rfbClientErr2(client, "ListenAtTcpPortAndAddress: error in getaddrinfo: %s\n", gai_strerror(rv));
    return RFB_INVALID_SOCKET;
  }

  /* loop through all the results and bind to the first we can */
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == RFB_INVALID_SOCKET) {
      continue;
    }

#ifdef IPV6_V6ONLY
    /* we have separate IPv4 and IPv6 sockets since some OS's do not support dual binding */
    if (p->ai_family == AF_INET6 && setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&one, sizeof(one)) < 0) {
      rfbClientErr2(client, "ListenAtTcpPortAndAddress: error in setsockopt IPV6_V6ONLY: %s\n", strerror(errno));
      rfbCloseSocket(sock);
      freeaddrinfo(servinfo);
      return RFB_INVALID_SOCKET;
    }
#endif

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) < 0) {
      rfbClientErr2(client, "ListenAtTcpPortAndAddress: error in setsockopt SO_REUSEADDR: %s\n", strerror(errno));
      rfbCloseSocket(sock);
      freeaddrinfo(servinfo);
      return RFB_INVALID_SOCKET;
    }

    if (bind(sock, p->ai_addr, p->ai_addrlen) < 0) {
      rfbCloseSocket(sock);
      continue;
    }

    break;
  }

  if (p == NULL)  {
    rfbClientErr2(client, "ListenAtTcpPortAndAddress: error in bind: %s\n", strerror(errno));
    return RFB_INVALID_SOCKET;
  }

  /* all done with this structure now */
  freeaddrinfo(servinfo);
#endif

  if (listen(sock, 5) < 0) {
    rfbClientErr2(client, "ListenAtTcpPort: listen\n");
    rfbCloseSocket(sock);
    return RFB_INVALID_SOCKET;
  }

  return sock;
}


/*
 * AcceptTcpConnection accepts a TCP connection.
 */

rfbSocket
AcceptTcpConnection(rfbClient* client, rfbSocket listenSock)
{
  rfbSocket sock;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int one = 1;

  sock = accept(listenSock, (struct sockaddr *) &addr, &addrlen);
  if (sock == RFB_INVALID_SOCKET) {
    rfbClientErr2(client, "AcceptTcpConnection: accept\n");
    return RFB_INVALID_SOCKET;
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		 (char *)&one, sizeof(one)) < 0) {
    rfbClientErr2(client, "AcceptTcpConnection: setsockopt\n");
    rfbCloseSocket(sock);
    return RFB_INVALID_SOCKET;
  }

  return sock;
}


/*
 * SetNonBlocking sets a socket into non-blocking mode.
 */

rfbBool
SetNonBlocking(rfbClient* client, rfbSocket sock)
{
    return sock_set_nonblocking(client, sock, TRUE, rfbClientErr2);
}


rfbBool SetBlocking(rfbClient* client, rfbSocket sock)
{
    return sock_set_nonblocking(client, sock, FALSE, rfbClientErr2);
}


/*
 * SetDSCP sets a socket's IP QoS parameters aka Differentiated Services Code Point field
 */

rfbBool
SetDSCP(rfbClient* client, rfbSocket sock, int dscp)
{
#ifdef WIN32
  rfbClientErr2(client, "Setting of QoS IP DSCP not implemented for Windows\n");
  return TRUE;
#else
  int level, cmd;
  struct sockaddr addr;
  socklen_t addrlen = sizeof(addr);

  if(getsockname(sock, &addr, &addrlen) != 0) {
    rfbClientErr2(client, "Setting socket QoS failed while getting socket address: %s\n",strerror(errno));
    return FALSE;
  }

  switch(addr.sa_family)
    {
#if defined LIBVNCSERVER_IPv6 && defined IPV6_TCLASS
    case AF_INET6:
      level = IPPROTO_IPV6;
      cmd = IPV6_TCLASS;
      break;
#endif
    case AF_INET:
      level = IPPROTO_IP;
      cmd = IP_TOS;
      break;
    default:
      rfbClientErr2(client, "Setting socket QoS failed: Not bound to IP address");
      return FALSE;
    }

  if(setsockopt(sock, level, cmd, (void*)&dscp, sizeof(dscp)) != 0) {
    rfbClientErr2(client, "Setting socket QoS failed: %s\n", strerror(errno));
    return FALSE;
  }

  return TRUE;
#endif
}



/*
 * StringToIPAddr - convert a host string to an IP address.
 */

rfbBool
StringToIPAddr(const char *str, unsigned int *addr)
{
  struct addrinfo hints, *res;

  if (strcmp(str,"") == 0) {
    *addr = htonl(INADDR_LOOPBACK); /* local */
    return TRUE;
  }

  *addr = inet_addr(str);

  if (*addr != -1)
    return TRUE;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(str, NULL, &hints, &res) == 0) {
    *addr = (((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
    freeaddrinfo(res);

    return TRUE;
  }

  return FALSE;
}

/*
 * Test if the other end of a socket is on the same machine.
 */

rfbBool
SameMachine(rfbSocket sock)
{
  struct sockaddr_in peeraddr, myaddr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  getpeername(sock, (struct sockaddr *)&peeraddr, &addrlen);
  getsockname(sock, (struct sockaddr *)&myaddr, &addrlen);

  return (peeraddr.sin_addr.s_addr == myaddr.sin_addr.s_addr);
}


/*
 * Print out the contents of a packet for debugging.
 */

void
PrintInHex(rfbClient* client, char *buf, int len)
{
  int i, j;
  char c, str[17];

  str[16] = 0;

  rfbClientLog2(client, "ReadExact: ");

  for (i = 0; i < len; i++)
    {
      if ((i % 16 == 0) && (i != 0)) {
	rfbClientLog2(client, "           ");
      }
      c = buf[i];
      str[i % 16] = (((c > 31) && (c < 127)) ? c : '.');
      rfbClientLog2(client, "%02x ",(unsigned char)c);
      if ((i % 4) == 3)
	rfbClientLog2(client, " ");
      if ((i % 16) == 15)
	{
	  rfbClientLog2(client, "%s\n",str);
	}
    }
  if ((i % 16) != 0)
    {
      for (j = i % 16; j < 16; j++)
	{
	  rfbClientLog2(client, "   ");
	  if ((j % 4) == 3) rfbClientLog2(client, " ");
	}
      str[i % 16] = 0;
      rfbClientLog2(client, "%s\n",str);
    }

  fflush(stderr);
}

int WaitForMessage(rfbClient* client,unsigned int usecs)
{
  fd_set fds;
  struct timeval timeout;
  int num;

  if (client->serverPort==-1)
    /* playing back vncrec file */
    return 1;
  
  timeout.tv_sec=(usecs/1000000);
  timeout.tv_usec=(usecs%1000000);

  if(client->sock == RFB_INVALID_SOCKET) {
      errno = EBADF;
      return -1;
  }

  FD_ZERO(&fds);
  FD_SET(client->sock,&fds);

  num=select(client->sock+1, &fds, NULL, NULL, &timeout);
  if(num<0) {
#ifdef WIN32
    errno=WSAGetLastError();
#endif
    rfbClientLog2(client, "Waiting for message failed: %d (%s)\n",errno,strerror(errno));
  }

  return num;
}



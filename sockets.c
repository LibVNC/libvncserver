/*
 * sockets.c - deal with TCP & UDP sockets.
 *
 * This code should be independent of any changes in the RFB protocol.  It just
 * deals with the X server scheduling stuff, calling rfbNewClientConnection and
 * rfbProcessClientMessage to actually deal with the protocol.  If a socket
 * needs to be closed for any reason then rfbCloseClient should be called, and
 * this in turn will call rfbClientConnectionGone.  To make an active
 * connection out, call rfbConnect - note that this does _not_ call
 * rfbNewClientConnection.
 *
 * This file is divided into two types of function.  Those beginning with
 * "rfb" are specific to sockets using the RFB protocol.  Those without the
 * "rfb" prefix are more general socket routines (which are used by the http
 * code).
 *
 * Thanks to Karl Hakimian for pointing out that some platforms return EAGAIN
 * not EWOULDBLOCK.
 */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.
 *  All Rights Reserved.
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

#include <stdio.h>
#include <sys/types.h>
#ifdef WIN32
#pragma warning (disable: 4018 4761)
#define close closesocket
#define read(sock,buf,len) recv(sock,buf,len,0)
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ETIMEDOUT WSAETIMEDOUT
#define write(sock,buf,len) send(sock,buf,len,0)
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif
#if defined(__osf__)
typedef int socklen_t;
#endif
#if defined(__linux__) && defined(NEED_TIMEVAL)
struct timeval
{
   long int tv_sec,tv_usec;
}
;
#endif
#include <fcntl.h>
#include <errno.h>

#ifdef USE_LIBWRAP
#include <syslog.h>
#include <tcpd.h>
int allow_severity=LOG_INFO;
int deny_severity=LOG_WARNING;
#endif

#include "rfb.h"

/*#ifndef WIN32
int max(int i,int j) { return(i<j?j:i); }
#endif
*/

int rfbMaxClientWait = 20000;   /* time (ms) after which we decide client has
                                   gone away - needed to stop us hanging */

/*
 * rfbInitSockets sets up the TCP and UDP sockets to listen for RFB
 * connections.  It does nothing if called again.
 */

void
rfbInitSockets(rfbScreenInfoPtr rfbScreen)
{
    if (rfbScreen->socketInitDone)
	return;

    rfbScreen->socketInitDone = TRUE;

    if (rfbScreen->inetdSock != -1) {
	const int one = 1;

#ifndef WIN32
	if (fcntl(rfbScreen->inetdSock, F_SETFL, O_NONBLOCK) < 0) {
	    rfbLogPerror("fcntl");
	    exit(1);
	}
#endif

	if (setsockopt(rfbScreen->inetdSock, IPPROTO_TCP, TCP_NODELAY,
		       (char *)&one, sizeof(one)) < 0) {
	    rfbLogPerror("setsockopt");
	    exit(1);
	}

    	FD_ZERO(&(rfbScreen->allFds));
    	FD_SET(rfbScreen->inetdSock, &(rfbScreen->allFds));
    	rfbScreen->maxFd = rfbScreen->inetdSock;
	return;
    }

    if(rfbScreen->autoPort) {
        int i;
        rfbLog("Autoprobing TCP port \n");

        for (i = 5900; i < 6000; i++) {
            if ((rfbScreen->rfbListenSock = ListenOnTCPPort(i)) >= 0) {
		rfbScreen->rfbPort = i;
		break;
	    }
        }

        if (i >= 6000) {
	    rfbLogPerror("Failure autoprobing");
	    exit(1);
        }

        rfbLog("Autoprobing selected port %d\n", rfbScreen->rfbPort);
        FD_ZERO(&(rfbScreen->allFds));
        FD_SET(rfbScreen->rfbListenSock, &(rfbScreen->allFds));
        rfbScreen->maxFd = rfbScreen->rfbListenSock;
    }
    else if(rfbScreen->rfbPort>0) {
      rfbLog("Listening for VNC connections on TCP port %d\n", rfbScreen->rfbPort);

      if ((rfbScreen->rfbListenSock = ListenOnTCPPort(rfbScreen->rfbPort)) < 0) {
	rfbLogPerror("ListenOnTCPPort");
	exit(1);
      }

      FD_ZERO(&(rfbScreen->allFds));
      FD_SET(rfbScreen->rfbListenSock, &(rfbScreen->allFds));
      rfbScreen->maxFd = rfbScreen->rfbListenSock;
    }

    if (rfbScreen->udpPort != 0) {
	rfbLog("rfbInitSockets: listening for input on UDP port %d\n",rfbScreen->udpPort);

	if ((rfbScreen->udpSock = ListenOnUDPPort(rfbScreen->udpPort)) < 0) {
	    rfbLogPerror("ListenOnUDPPort");
	    exit(1);
	}
	FD_SET(rfbScreen->udpSock, &(rfbScreen->allFds));
	rfbScreen->maxFd = max((int)rfbScreen->udpSock,rfbScreen->maxFd);
    }
}


/*
 * rfbCheckFds is called from ProcessInputEvents to check for input on the RFB
 * socket(s).  If there is input to process, the appropriate function in the
 * RFB server code will be called (rfbNewClientConnection,
 * rfbProcessClientMessage, etc).
 */

void
rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec)
{
    int nfds;
    fd_set fds;
    struct timeval tv;
    struct sockaddr_in addr;
    size_t addrlen = sizeof(addr);
    char buf[6];
    const int one = 1;
    int sock;
    rfbClientIteratorPtr i;
    rfbClientPtr cl;

    if (!rfbScreen->inetdInitDone && rfbScreen->inetdSock != -1) {
	rfbNewClientConnection(rfbScreen,rfbScreen->inetdSock);
	rfbScreen->inetdInitDone = TRUE;
    }

    memcpy((char *)&fds, (char *)&(rfbScreen->allFds), sizeof(fd_set));
    tv.tv_sec = 0;
    tv.tv_usec = usec;
    nfds = select(rfbScreen->maxFd + 1, &fds, NULL, NULL /* &fds */, &tv);
    if (nfds == 0) {
	return;
    }
    if (nfds < 0) {
#ifdef WIN32
		errno = WSAGetLastError();
#endif
	rfbLogPerror("rfbCheckFds: select");
	return;
    }

    if (rfbScreen->rfbListenSock != -1 && FD_ISSET(rfbScreen->rfbListenSock, &fds)) {

	if ((sock = accept(rfbScreen->rfbListenSock,
			   (struct sockaddr *)&addr, &addrlen)) < 0) {
	    rfbLogPerror("rfbCheckFds: accept");
	    return;
	}

#ifndef WIN32
	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
	    rfbLogPerror("rfbCheckFds: fcntl");
	    close(sock);
	    return;
	}
#endif

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		       (char *)&one, sizeof(one)) < 0) {
	    rfbLogPerror("rfbCheckFds: setsockopt");
	    close(sock);
	    return;
	}

#ifdef USE_LIBWRAP
	if(!hosts_ctl("vnc",STRING_UNKNOWN,inet_ntoa(addr.sin_addr),
		      STRING_UNKNOWN)) {
	  rfbLog("Rejected connection from client %s\n",
		 inet_ntoa(addr.sin_addr));
	  close(sock);
	  return;
	}
#endif

	rfbLog("Got connection from client %s\n", inet_ntoa(addr.sin_addr));

	rfbNewClient(rfbScreen,sock);

	FD_CLR(rfbScreen->rfbListenSock, &fds);
	if (--nfds == 0)
	    return;
    }

    if ((rfbScreen->udpSock != -1) && FD_ISSET(rfbScreen->udpSock, &fds)) {
        if(!rfbScreen->udpClient)
	    rfbNewUDPClient(rfbScreen);
	if (recvfrom(rfbScreen->udpSock, buf, 1, MSG_PEEK,
		     (struct sockaddr *)&addr, &addrlen) < 0) {
	    rfbLogPerror("rfbCheckFds: UDP: recvfrom");
	    rfbDisconnectUDPSock(rfbScreen);
	    rfbScreen->udpSockConnected = FALSE;
	} else {
	    if (!rfbScreen->udpSockConnected ||
		(memcmp(&addr, &rfbScreen->udpRemoteAddr, addrlen) != 0))
	    {
		/* new remote end */
		rfbLog("rfbCheckFds: UDP: got connection\n");

		memcpy(&rfbScreen->udpRemoteAddr, &addr, addrlen);
		rfbScreen->udpSockConnected = TRUE;

		if (connect(rfbScreen->udpSock,
			    (struct sockaddr *)&addr, addrlen) < 0) {
		    rfbLogPerror("rfbCheckFds: UDP: connect");
		    rfbDisconnectUDPSock(rfbScreen);
		    return;
		}

		rfbNewUDPConnection(rfbScreen,rfbScreen->udpSock);
	    }

	    rfbProcessUDPInput(rfbScreen);
	}

	FD_CLR(rfbScreen->udpSock, &fds);
	if (--nfds == 0)
	    return;
    }

    i = rfbGetClientIterator(rfbScreen);
    while((cl = rfbClientIteratorNext(i))) {
      if (cl->onHold)
	continue;
      if (FD_ISSET(cl->sock, &fds) && FD_ISSET(cl->sock, &(rfbScreen->allFds)))
	rfbProcessClientMessage(cl);
    }
    rfbReleaseClientIterator(i);
}


void
rfbDisconnectUDPSock(rfbScreenInfoPtr rfbScreen)
{
  rfbScreen->udpSockConnected = FALSE;
}



void
rfbCloseClient(cl)
     rfbClientPtr cl;
{
    LOCK(cl->updateMutex);
    if (cl->sock != -1) {
      FD_CLR(cl->sock,&(cl->screen->allFds));
      shutdown(cl->sock,SHUT_RDWR);
      close(cl->sock);
      cl->sock = -1;
    }
    TSIGNAL(cl->updateCond);
    UNLOCK(cl->updateMutex);
}


/*
 * rfbConnect is called to make a connection out to a given TCP address.
 */

int
rfbConnect(rfbScreen, host, port)
    rfbScreenInfoPtr rfbScreen;
    char *host;
    int port;
{
    int sock;
    int one = 1;

    rfbLog("Making connection to client on host %s port %d\n",
	   host,port);

    if ((sock = ConnectToTcpAddr(host, port)) < 0) {
	rfbLogPerror("connection failed");
	return -1;
    }

#ifndef WIN32
    if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
	rfbLogPerror("fcntl failed");
	close(sock);
	return -1;
    }
#endif

    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		   (char *)&one, sizeof(one)) < 0) {
	rfbLogPerror("setsockopt failed");
	close(sock);
	return -1;
    }

    /* AddEnabledDevice(sock); */
    FD_SET(sock, &rfbScreen->allFds);
    rfbScreen->maxFd = max(sock,rfbScreen->maxFd);

    return sock;
}

/*
 * ReadExact reads an exact number of bytes from a client.  Returns 1 if
 * those bytes have been read, 0 if the other end has closed, or -1 if an error
 * occurred (errno is set to ETIMEDOUT if it timed out).
 * timeout is the timeout in ms, 0 for no timeout.
 */

int
ReadExactTimeout(rfbClientPtr cl, char* buf, int len, int timeout)
{
    int sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;
    int to = 20000;
    if (timeout)
	to = timeout;

    while (len > 0) {
        n = read(sock, buf, len);

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            return 0;

        } else {
#ifdef WIN32
			errno = WSAGetLastError();
#endif
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                return n;
            }

            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = to / 1000;
            tv.tv_usec = (to % 1000) * 1000;
            n = select(sock+1, &fds, NULL, &fds, &tv);
            if (n < 0) {
                rfbLogPerror("ReadExact: select");
                return n;
            }
            if ((n == 0) && timeout) {
                errno = ETIMEDOUT;
                return -1;
            }
        }
    }
    return 1;
}

int ReadExact(rfbClientPtr cl,char* buf,int len)
{
    return ReadExactTimeout(cl, buf, len, 0);
}

/*
 * WriteExact writes an exact number of bytes to a client.  Returns 1 if
 * those bytes have been written, or -1 if an error occurred (errno is set to
 * ETIMEDOUT if it timed out).
 */

int
WriteExact(cl, buf, len)
     rfbClientPtr cl;
     const char *buf;
     int len;
{
    int sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;
    int totalTimeWaited = 0;

    LOCK(cl->outputMutex);
    while (len > 0) {
        n = write(sock, buf, len);

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            rfbLog("WriteExact: write returned 0?\n");
            exit(1);

        } else {
#ifdef WIN32
			errno = WSAGetLastError();
#endif
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
	        UNLOCK(cl->outputMutex);
                return n;
            }

            /* Retry every 5 seconds until we exceed rfbMaxClientWait.  We
               need to do this because select doesn't necessarily return
               immediately when the other end has gone away */

            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            n = select(sock+1, NULL, &fds, NULL /* &fds */, &tv);
            if (n < 0) {
                rfbLogPerror("WriteExact: select");
                UNLOCK(cl->outputMutex);
                return n;
            }
            if (n == 0) {
                totalTimeWaited += 5000;
                if (totalTimeWaited >= rfbMaxClientWait) {
                    errno = ETIMEDOUT;
                    UNLOCK(cl->outputMutex);
                    return -1;
                }
            } else {
                totalTimeWaited = 0;
            }
        }
    }
    UNLOCK(cl->outputMutex);
    return 1;
}

int
ListenOnTCPPort(port)
    int port;
{
    struct sockaddr_in addr;
    int sock;
    int one = 1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    /* addr.sin_addr.s_addr = interface.s_addr; */
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		   (char *)&one, sizeof(one)) < 0) {
	close(sock);
	return -1;
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	close(sock);
	return -1;
    }
    if (listen(sock, 5) < 0) {
	close(sock);
	return -1;
    }

    return sock;
}

int
ConnectToTcpAddr(host, port)
    char *host;
    int port;
{
    struct hostent *hp;
    int sock;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if ((addr.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
    {
	if (!(hp = gethostbyname(host))) {
	    errno = EINVAL;
	    return -1;
	}
	addr.sin_addr.s_addr = *(unsigned long *)hp->h_addr;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, (sizeof(addr))) < 0) {
	close(sock);
	return -1;
    }

    return sock;
}

int
ListenOnUDPPort(port)
    int port;
{
    struct sockaddr_in addr;
    int sock;
    int one = 1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    /* addr.sin_addr.s_addr = interface.s_addr; */
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		   (char *)&one, sizeof(one)) < 0) {
	return -1;
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	return -1;
    }

    return sock;
}

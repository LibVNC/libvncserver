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
#include <sys/time.h>
#if defined(__linux__) && defined(NEED_TIMEVAL)
struct timeval 
{
   long int tv_sec,tv_usec;
}
;
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "rfb.h"

int max(int i,int j) { return(i<j?j:i); }

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

	if (fcntl(rfbScreen->inetdSock, F_SETFL, O_NONBLOCK) < 0) {
	    rfbLogPerror("fcntl");
	    exit(1);
	}

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

    rfbLog("Listening for VNC connections on TCP port %d\n", rfbScreen->rfbPort);

    if ((rfbScreen->rfbListenSock = ListenOnTCPPort(rfbScreen->rfbPort)) < 0) {
	rfbLogPerror("ListenOnTCPPort");
	exit(1);
    }

    FD_ZERO(&(rfbScreen->allFds));
    FD_SET(rfbScreen->rfbListenSock, &(rfbScreen->allFds));
    rfbScreen->maxFd = rfbScreen->rfbListenSock;

    if (rfbScreen->udpPort != 0) {
	rfbLog("rfbInitSockets: listening for input on UDP port %d\n",rfbScreen->udpPort);

	if ((rfbScreen->udpSock = ListenOnUDPPort(rfbScreen->udpPort)) < 0) {
	    rfbLogPerror("ListenOnUDPPort");
	    exit(1);
	}
	FD_SET(rfbScreen->udpSock, &(rfbScreen->allFds));
	rfbScreen->maxFd = max(rfbScreen->udpSock,rfbScreen->maxFd);
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
    int addrlen = sizeof(addr);
    char buf[6];
    const int one = 1;
    int sock;
    rfbClientPtr cl;

    if (!rfbScreen->inetdInitDone && rfbScreen->inetdSock != -1) {
	rfbNewClientConnection(rfbScreen,rfbScreen->inetdSock); 
	rfbScreen->inetdInitDone = TRUE;
    }

    memcpy((char *)&fds, (char *)&(rfbScreen->allFds), sizeof(fd_set));
    tv.tv_sec = 0;
    tv.tv_usec = usec;
    nfds = select(rfbScreen->maxFd + 1, &fds, NULL, NULL, &tv);
    if (nfds == 0) {
	return;
    }
    if (nfds < 0) {
	rfbLogPerror("rfbCheckFds: select");
	return;
    }

    if (rfbScreen->rfbListenSock != -1 && FD_ISSET(rfbScreen->rfbListenSock, &fds)) {

	if ((sock = accept(rfbScreen->rfbListenSock,
			   (struct sockaddr *)&addr, &addrlen)) < 0) {
	    rfbLogPerror("rfbCheckFds: accept");
	    return;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
	    rfbLogPerror("rfbCheckFds: fcntl");
	    close(sock);
	    return;
	}

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		       (char *)&one, sizeof(one)) < 0) {
	    rfbLogPerror("rfbCheckFds: setsockopt");
	    close(sock);
	    return;
	}

	fprintf(stderr,"\n");
	rfbLog("Got connection from client %s\n", inet_ntoa(addr.sin_addr));

	FD_SET(sock, &(rfbScreen->allFds));
	rfbScreen->maxFd = max(sock,rfbScreen->maxFd);

	rfbNewClient(rfbScreen,sock);

	FD_CLR(rfbScreen->rfbListenSock, &fds);
	if (--nfds == 0)
	    return;
    }

    if ((rfbScreen->udpSock != -1) && FD_ISSET(rfbScreen->udpSock, &fds)) {

	if (recvfrom(rfbScreen->udpSock, buf, 1, MSG_PEEK,
		     (struct sockaddr *)&addr, &addrlen) < 0) {

	    rfbLogPerror("rfbCheckFds: UDP: recvfrom");
	    rfbDisconnectUDPSock(rfbScreen);

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

	    //TODO: UDP also needs a client
	    //rfbProcessUDPInput(rfbScreen,rfbScreen->udpSock);
	}

	FD_CLR(rfbScreen->udpSock, &fds);
	if (--nfds == 0)
	    return;
    }

    for (cl = rfbScreen->rfbClientHead; cl; cl=cl->next) {
	if (FD_ISSET(cl->sock, &fds) && FD_ISSET(cl->sock, &(rfbScreen->allFds))) {
	    rfbProcessClientMessage(cl);
	}
    }
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
    pthread_mutex_lock(&cl->updateMutex);
    FD_CLR(cl->sock,&(cl->screen->allFds));
    close(cl->sock);
    cl->sock = -1;
    pthread_cond_signal(&cl->updateCond);
    pthread_mutex_lock(&cl->updateMutex);
    rfbClientConnectionGone(cl);
    pthread_mutex_unlock(&cl->updateMutex);
}


/*
 * ReadExact reads an exact number of bytes from a client.  Returns 1 if
 * those bytes have been read, 0 if the other end has closed, or -1 if an error
 * occurred (errno is set to ETIMEDOUT if it timed out).
 */

int
ReadExact(cl, buf, len)
     rfbClientPtr cl;
     char *buf;
     int len;
{
    int sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;

    while (len > 0) {
        n = read(sock, buf, len);

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            return 0;

        } else {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                return n;
            }

            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = rfbMaxClientWait / 1000;
            tv.tv_usec = (rfbMaxClientWait % 1000) * 1000;
            n = select(sock+1, &fds, NULL, NULL, &tv);
            if (n < 0) {
                rfbLogPerror("ReadExact: select");
                return n;
            }
            if (n == 0) {
                errno = ETIMEDOUT;
                return -1;
            }
        }
    }
    return 1;
}



/*
 * WriteExact writes an exact number of bytes to a client.  Returns 1 if
 * those bytes have been written, or -1 if an error occurred (errno is set to
 * ETIMEDOUT if it timed out).
 */

int
WriteExact(cl, buf, len)
     rfbClientPtr cl;
     char *buf;
     int len;
{
    int sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;
    int totalTimeWaited = 0;

#ifdef HAVE_PTHREADS
    pthread_mutex_lock(&cl->outputMutex);
#endif
    while (len > 0) {
        n = write(sock, buf, len);

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            rfbLog("WriteExact: write returned 0?\n");
            exit(1);

        } else {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
#ifdef HAVE_PTHREADS
                pthread_mutex_unlock(&cl->outputMutex);
#endif
                return n;
            }

            /* Retry every 5 seconds until we exceed rfbMaxClientWait.  We
               need to do this because select doesn't necessarily return
               immediately when the other end has gone away */

            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            n = select(sock+1, NULL, &fds, NULL, &tv);
            if (n < 0) {
                rfbLogPerror("WriteExact: select");
#ifdef HAVE_PTHREADS
                pthread_mutex_unlock(&cl->outputMutex);
#endif
                return n;
            }
            if (n == 0) {
                totalTimeWaited += 5000;
                if (totalTimeWaited >= rfbMaxClientWait) {
                    errno = ETIMEDOUT;
#ifdef HAVE_PTHREADS
                    pthread_mutex_unlock(&cl->outputMutex);
#endif
                    return -1;
                }
            } else {
                totalTimeWaited = 0;
            }
        }
    }
#ifdef HAVE_PTHREADS
    pthread_mutex_unlock(&cl->outputMutex);
#endif
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
    //addr.sin_addr.s_addr = interface.s_addr;
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
ListenOnUDPPort(port)
    int port;
{
    struct sockaddr_in addr;
    int sock;
    int one = 1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    //addr.sin_addr.s_addr = interface.s_addr;
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

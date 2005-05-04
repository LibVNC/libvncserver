/*
 * httpd.c - a simple HTTP server
 */

/*
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

#include <stdio.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock.h>
#define close closesocket
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <pwd.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>
#ifdef __osf__
typedef int socklen_t;
#endif

#ifdef USE_LIBWRAP
#include <tcpd.h>
#endif

#include "rfb.h"

#define NOT_FOUND_STR "HTTP/1.0 404 Not found\n\n" \
    "<HEAD><TITLE>File Not Found</TITLE></HEAD>\n" \
    "<BODY><H1>File Not Found</H1></BODY>\n"

#define OK_STR "HTTP/1.0 200 OK\nContent-Type: text/html\n\n"

static void httpProcessInput();
static Bool compareAndSkip(char **ptr, const char *str);

/*
int httpPort = 0;
char *httpDir = NULL;

int httpListenSock = -1;
int httpSock = -1;
FILE* httpFP = NULL;
*/

#define BUF_SIZE 32768

static char buf[BUF_SIZE];
static size_t buf_filled=0;


/*
 * httpInitSockets sets up the TCP socket to listen for HTTP connections.
 */

void
httpInitSockets(rfbScreenInfoPtr rfbScreen)
{
    if (rfbScreen->httpInitDone)
	return;

    rfbScreen->httpInitDone = TRUE;

    if (!rfbScreen->httpDir)
	return;

    if (rfbScreen->httpPort == 0) {
	rfbScreen->httpPort = rfbScreen->rfbPort-100;
    }

    rfbLog("Listening for HTTP connections on TCP port %d\n", rfbScreen->httpPort);

    rfbLog("  URL http://%s:%d\n",rfbScreen->rfbThisHost,rfbScreen->httpPort);

    if ((rfbScreen->httpListenSock = ListenOnTCPPort(rfbScreen->httpPort)) < 0) {
	rfbLogPerror("ListenOnTCPPort");
	exit(1);
    }

   /*AddEnabledDevice(httpListenSock);*/
}


/*
 * httpCheckFds is called from ProcessInputEvents to check for input on the
 * HTTP socket(s).  If there is input to process, httpProcessInput is called.
 */

void
httpCheckFds(rfbScreenInfoPtr rfbScreen)
{
    int nfds;
    fd_set fds;
    struct timeval tv;
    struct sockaddr_in addr;
    size_t addrlen = sizeof(addr);

    if (!rfbScreen->httpDir)
	return;

    FD_ZERO(&fds);
    FD_SET(rfbScreen->httpListenSock, &fds);
    if (rfbScreen->httpSock >= 0) {
	FD_SET(rfbScreen->httpSock, &fds);
    }
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    nfds = select(max(rfbScreen->httpSock,rfbScreen->httpListenSock) + 1, &fds, NULL, NULL, &tv);
    if (nfds == 0) {
	return;
    }
    if (nfds < 0) {
#ifdef WIN32
		errno = WSAGetLastError();
#endif
	rfbLogPerror("httpCheckFds: select");
	return;
    }

    if ((rfbScreen->httpSock >= 0) && FD_ISSET(rfbScreen->httpSock, &fds)) {
	httpProcessInput(rfbScreen);
    }

    if (FD_ISSET(rfbScreen->httpListenSock, &fds)) {
        int flags;
	if (rfbScreen->httpSock >= 0) close(rfbScreen->httpSock);

	if ((rfbScreen->httpSock = accept(rfbScreen->httpListenSock,
			       (struct sockaddr *)&addr, &addrlen)) < 0) {
	    rfbLogPerror("httpCheckFds: accept");
	    return;
	}
#ifdef USE_LIBWRAP
	if(!hosts_ctl("vnc",STRING_UNKNOWN,inet_ntoa(addr.sin_addr),
		      STRING_UNKNOWN)) {
	  rfbLog("Rejected connection from client %s\n",
		 inet_ntoa(addr.sin_addr));
#else
	if ((rfbScreen->httpFP = fdopen(rfbScreen->httpSock, "r+")) == NULL) {
	    rfbLogPerror("httpCheckFds: fdopen");
#endif
	    close(rfbScreen->httpSock);
	    rfbScreen->httpSock = -1;
	    return;
	}
	flags=fcntl(rfbScreen->httpSock,F_GETFL);
	if(flags==-1 ||
	   fcntl(rfbScreen->httpSock,F_SETFL,flags|O_NONBLOCK)==-1) {
	  rfbLogPerror("httpCheckFds: fcntl");
	  close(rfbScreen->httpSock);
	  rfbScreen->httpSock=-1;
	  return;
	}

	/*AddEnabledDevice(httpSock);*/
    }
}


static void
httpCloseSock(rfbScreenInfoPtr rfbScreen)
{
    fclose(rfbScreen->httpFP);
    rfbScreen->httpFP = NULL;
    /*RemoveEnabledDevice(httpSock);*/
    rfbScreen->httpSock = -1;
}

static rfbClientRec cl;

/*
 * httpProcessInput is called when input is received on the HTTP socket.
 */

static void
httpProcessInput(rfbScreenInfoPtr rfbScreen)
{
    struct sockaddr_in addr;
    size_t addrlen = sizeof(addr);
    char fullFname[256];
    char *fname;
    unsigned int maxFnameLen;
    FILE* fd;
    Bool performSubstitutions = FALSE;
    char str[256];
#ifndef WIN32
    struct passwd *user = getpwuid(getuid());
#endif
   
    cl.sock=rfbScreen->httpSock;

    if (strlen(rfbScreen->httpDir) > 200) {
	rfbLog("-httpd directory too long\n");
	httpCloseSock(rfbScreen);
	return;
    }
    strcpy(fullFname, rfbScreen->httpDir);
    fname = &fullFname[strlen(fullFname)];
    maxFnameLen = 255 - strlen(fullFname);

    /* Read data from the HTTP client until we get a complete request. */
    while (1) {
	ssize_t got = read (rfbScreen->httpSock, buf + buf_filled,
			    sizeof (buf) - buf_filled - 1);

	if (got <= 0) {
	    if (got == 0) {
		rfbLog("httpd: premature connection close\n");
	    } else {
		if (errno == EAGAIN) {
		    return;
		}
		rfbLogPerror("httpProcessInput: read");
	    }
	    httpCloseSock(rfbScreen);
	    return;
	}

	buf_filled += got;
	buf[buf_filled] = '\0';

	/* Is it complete yet (is there a blank line)? */
	if (strstr (buf, "\r\r") || strstr (buf, "\n\n") ||
	    strstr (buf, "\r\n\r\n") || strstr (buf, "\n\r\n\r"))
	    break;
    }


    /* Process the request. */
    if (strncmp(buf, "GET ", 4)) {
	rfbLog("no GET line\n");
	httpCloseSock(rfbScreen);
	return;
    } else {
	/* Only use the first line. */
	buf[strcspn(buf, "\n\r")] = '\0';
    }

    if (strlen(buf) > maxFnameLen) {
	rfbLog("GET line too long\n");
	httpCloseSock(rfbScreen);
	return;
    }

    if (sscanf(buf, "GET %s HTTP/1.0", fname) != 1) {
	rfbLog("couldn't parse GET line\n");
	httpCloseSock(rfbScreen);
	return;
    }

    if (fname[0] != '/') {
	rfbLog("filename didn't begin with '/'\n");
	WriteExact(&cl, NOT_FOUND_STR, strlen(NOT_FOUND_STR));
	httpCloseSock(rfbScreen);
	return;
    }

    if (strchr(fname+1, '/') != NULL) {
	rfbLog("asking for file in other directory\n");
	WriteExact(&cl, NOT_FOUND_STR, strlen(NOT_FOUND_STR));
	httpCloseSock(rfbScreen);
	return;
    }

    getpeername(rfbScreen->httpSock, (struct sockaddr *)&addr, &addrlen);
    rfbLog("httpd: get '%s' for %s\n", fname+1,
	   inet_ntoa(addr.sin_addr));

    /* If we were asked for '/', actually read the file index.vnc */

    if (strcmp(fname, "/") == 0) {
	strcpy(fname, "/index.vnc");
	rfbLog("httpd: defaulting to '%s'\n", fname+1);
    }

    /* Substitutions are performed on files ending .vnc */

    if (strlen(fname) >= 4 && strcmp(&fname[strlen(fname)-4], ".vnc") == 0) {
	performSubstitutions = TRUE;
    }

    /* Open the file */

    if ((fd = fopen(fullFname, "r")) <= 0) {
        rfbLogPerror("httpProcessInput: open");
        WriteExact(&cl, NOT_FOUND_STR, strlen(NOT_FOUND_STR));
        httpCloseSock(rfbScreen);
        return;
    }

    WriteExact(&cl, OK_STR, strlen(OK_STR));

    while (1) {
	int n = fread(buf, 1, BUF_SIZE-1, fd);
	if (n < 0) {
	    rfbLogPerror("httpProcessInput: read");
	    fclose(fd);
	    httpCloseSock(rfbScreen);
	    return;
	}

	if (n == 0)
	    break;

	if (performSubstitutions) {

	    /* Substitute $WIDTH, $HEIGHT, etc with the appropriate values.
	       This won't quite work properly if the .vnc file is longer than
	       BUF_SIZE, but it's reasonable to assume that .vnc files will
	       always be short. */

	    char *ptr = buf;
	    char *dollar;
	    buf[n] = 0; /* make sure it's null-terminated */

	    while ((dollar = strchr(ptr, '$'))!=NULL) {
		WriteExact(&cl, ptr, (dollar - ptr));

		ptr = dollar;

		if (compareAndSkip(&ptr, "$WIDTH")) {

		    sprintf(str, "%d", rfbScreen->width);
		    WriteExact(&cl, str, strlen(str));

		} else if (compareAndSkip(&ptr, "$HEIGHT")) {

		    sprintf(str, "%d", rfbScreen->height);
		    WriteExact(&cl, str, strlen(str));

		} else if (compareAndSkip(&ptr, "$APPLETWIDTH")) {

		    sprintf(str, "%d", rfbScreen->width);
		    WriteExact(&cl, str, strlen(str));

		} else if (compareAndSkip(&ptr, "$APPLETHEIGHT")) {

		    sprintf(str, "%d", rfbScreen->height + 32);
		    WriteExact(&cl, str, strlen(str));

		} else if (compareAndSkip(&ptr, "$PORT")) {

		    sprintf(str, "%d", rfbScreen->rfbPort);
		    WriteExact(&cl, str, strlen(str));

		} else if (compareAndSkip(&ptr, "$DESKTOP")) {

		    WriteExact(&cl, rfbScreen->desktopName, strlen(rfbScreen->desktopName));

		} else if (compareAndSkip(&ptr, "$DISPLAY")) {

		    sprintf(str, "%s:%d", rfbScreen->rfbThisHost, rfbScreen->rfbPort-5900);
		    WriteExact(&cl, str, strlen(str));

		} else if (compareAndSkip(&ptr, "$USER")) {
#ifndef WIN32
		    if (user) {
			WriteExact(&cl, user->pw_name,
				   strlen(user->pw_name));
		    } else
#endif
			WriteExact(&cl, "?", 1);
		} else {
		    if (!compareAndSkip(&ptr, "$$"))
			ptr++;

		    if (WriteExact(&cl, "$", 1) < 0) {
			fclose(fd);
			httpCloseSock(rfbScreen);
			return;
		    }
		}
	    }
	    if (WriteExact(&cl, ptr, (&buf[n] - ptr)) < 0)
		break;

	} else {

	    /* For files not ending .vnc, just write out the buffer */

	    if (WriteExact(&cl, buf, n) < 0)
		break;
	}
    }

    fclose(fd);
    httpCloseSock(rfbScreen);
}


static Bool
compareAndSkip(char **ptr, const char *str)
{
    if (strncmp(*ptr, str, strlen(str)) == 0) {
	*ptr += strlen(str);
	return TRUE;
    }

    return FALSE;
}

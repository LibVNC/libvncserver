/*
 * websockets.c - deal with WebSockets clients.
 *
 * This code should be independent of any changes in the RFB protocol. It is
 * an additional handshake and framing of normal sockets:
 *   http://www.whatwg.org/specs/web-socket-protocol/
 *
 */

/*
 *  Copyright (C) 2010 Joel Martin
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

#include <rfb/rfb.h>
#include <resolv.h> /* __b64_ntop */
/* errno */
#include <errno.h>

#include <md5.h>

#define FLASH_POLICY_RESPONSE "<cross-domain-policy><allow-access-from domain=\"*\" to-ports=\"*\" /></cross-domain-policy>\n"
#define SZ_FLASH_POLICY_RESPONSE 93

#define WEBSOCKETS_HANDSHAKE_RESPONSE "HTTP/1.1 101 Web Socket Protocol Handshake\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
%sWebSocket-Origin: %s\r\n\
%sWebSocket-Location: %s://%s%s\r\n\
%sWebSocket-Protocol: %s\r\n\
\r\n%s"

#define WEBSOCKETS_CLIENT_CONNECT_WAIT_MS 100
#define WEBSOCKETS_CLIENT_SEND_WAIT_MS 100
#define WEBSOCKETS_MAX_HANDSHAKE_LEN 4096

#if defined(__linux__) && defined(NEED_TIMEVAL)
struct timeval
{
   long int tv_sec,tv_usec;
}
;
#endif

static rfbBool webSocketsHandshake(rfbClientPtr cl, char *scheme);
void webSocketsGenMd5(char * target, char *key1, char *key2, char *key3);

static int
min (int a, int b) {
    return a < b ? a : b;
}

/*
 * rfbWebSocketsHandshake is called to handle new WebSockets connections
 */

rfbBool
webSocketsCheck (rfbClientPtr cl)
{
    char bbuf[4], *scheme;
    int ret;

    ret = rfbPeekExactTimeout(cl, bbuf, 4,
                                   WEBSOCKETS_CLIENT_CONNECT_WAIT_MS);
    if ((ret < 0) && (errno == ETIMEDOUT)) {
      rfbLog("Normal socket connection\n");
      return TRUE;
    } else if (ret <= 0) {
      rfbErr("webSocketsHandshake: unknown connection error\n");
      return FALSE;
    }

    if (strncmp(bbuf, "<", 1) == 0) {
        rfbLog("Got Flash policy request, sending response\n");
        if (rfbWriteExact(cl, FLASH_POLICY_RESPONSE,
                          SZ_FLASH_POLICY_RESPONSE) < 0) {
            rfbErr("webSocketsHandshake: failed sending Flash policy response");
        }
        return FALSE;
    } else if (strncmp(bbuf, "\x16", 1) == 0) {
        cl->webSocketsSSL = TRUE;
        rfbLog("Got TLS/SSL WebSockets connection\n");
        scheme = "wss";
        /* TODO */
        /* bbuf = ... */
        return FALSE;
    } else {
        cl->webSocketsSSL = FALSE;
        scheme = "ws";
    }

    if (strncmp(bbuf, "GET ", 4) != 0) {
      rfbErr("webSocketsHandshake: invalid client header\n");
      return FALSE;
    }

    rfbLog("Got '%s' WebSockets handshake\n", scheme);

    if (!webSocketsHandshake(cl, scheme)) {
        return FALSE;
    }
    cl->webSockets    = TRUE;   /* Start WebSockets framing */
    return TRUE;
}

static rfbBool
webSocketsHandshake(rfbClientPtr cl, char *scheme)
{
    char *buf, *response, *line;
    int n, linestart = 0, len = 0, llen;
    char prefix[5], trailer[17];
    char *path = NULL, *host = NULL, *origin = NULL, *protocol = NULL;
    char *key1 = NULL, *key2 = NULL, *key3 = NULL;

    buf = (char *) malloc(WEBSOCKETS_MAX_HANDSHAKE_LEN);
    if (!buf) {
        rfbLogPerror("webSocketsHandshake: malloc");
        return FALSE;
    }
    response = (char *) malloc(WEBSOCKETS_MAX_HANDSHAKE_LEN);
    if (!response) {
        free(buf);
        rfbLogPerror("webSocketsHandshake: malloc");
        return FALSE;
    }

    while (len < WEBSOCKETS_MAX_HANDSHAKE_LEN-1) {
        if ((n = rfbReadExactTimeout(cl, buf+len, 1,
                                     WEBSOCKETS_CLIENT_SEND_WAIT_MS)) <= 0) {
            if ((n < 0) && (errno == ETIMEDOUT)) {
                break;
            }
            if (n == 0)
                rfbLog("webSocketsHandshake: client gone\n");
            else
                rfbLogPerror("webSocketsHandshake: read");
            return FALSE;
        }

        len += 1;
        llen = len - linestart;
        if (((llen >= 2)) && (buf[len-1] == '\n')) {
            line = buf+linestart;
            if ((llen == 2) && (strncmp("\r\n", line, 2) == 0)) {
                if (key1 && key2) {
                    if ((n = rfbReadExact(cl, buf+len, 8)) <= 0) {
                        if ((n < 0) && (errno == ETIMEDOUT)) {
                            break;
                        }
                        if (n == 0)
                            rfbLog("webSocketsHandshake: client gone\n");
                        else
                            rfbLogPerror("webSocketsHandshake: read");
                        return FALSE;
                    }
                    rfbLog("Got key3\n");
                    key3 = buf+len;
                    len += 8;
                } else {
                    buf[len] = '\0';
                }
                break;
            } else if ((llen >= 16) && ((strncmp("GET ", line, min(llen,4))) == 0)) {
                /* 16 = 4 ("GET ") + 1 ("/.*") + 11 (" HTTP/1.1\r\n") */
                path = line+4;
                buf[len-11] = '\0'; /* Trim trailing " HTTP/1.1\r\n" */
                cl->webSocketsBase64 = TRUE;
                /* rfbLog("Got path: %s\n", path); */
            } else if ((strncasecmp("host: ", line, min(llen,6))) == 0) {
                host = line+6;
                buf[len-2] = '\0';
                /* rfbLog("Got host: %s\n", host); */
            } else if ((strncasecmp("origin: ", line, min(llen,8))) == 0) {
                origin = line+8;
                buf[len-2] = '\0';
                /* rfbLog("Got origin: %s\n", origin); */
            } else if ((strncasecmp("sec-websocket-key1: ", line, min(llen,20))) == 0) {
                key1 = line+20;
                buf[len-2] = '\0';
                /* rfbLog("Got key1: %s\n", key1); */
            } else if ((strncasecmp("sec-websocket-key2: ", line, min(llen,20))) == 0) {
                key2 = line+20;
                buf[len-2] = '\0';
                /* rfbLog("Got key2: %s\n", key2); */
            } else if ((strncasecmp("sec-websocket-protocol: ", line, min(llen,24))) == 0) {
                protocol = line+24;
                buf[len-2] = '\0';
                /* rfbLog("Got protocol: %s\n", protocol); */
            }
            linestart = len;
        }
    }

    if (!(path && host && origin)) {
        rfbErr("webSocketsHandshake: incomplete client handshake\n");
        free(response);
        free(buf);
        return FALSE;
    }

    /*
    if ((!protocol) || (!strcasestr(protocol, "base64"))) {
        rfbErr("webSocketsHandshake: base64 subprotocol not supported by client\n");
        free(response);
        free(buf);
        return FALSE;
    }
    */

    /*
     * Generate the WebSockets server response based on the the headers sent
     * by the client.
     */

    if (!(key1 && key2 && key3)) {
        rfbLog("  - WebSockets client version 75\n");
        prefix[0] = '\0';
        trailer[0] = '\0';
    } else {
        rfbLog("  - WebSockets client version 76\n");
        snprintf(prefix, 5, "Sec-");
        webSocketsGenMd5(trailer, key1, key2, key3);
    }

    snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
             WEBSOCKETS_HANDSHAKE_RESPONSE, prefix, origin, prefix, scheme,
             host, path, prefix, protocol, trailer);

    if (rfbWriteExact(cl, response, strlen(response)) < 0) {
        rfbErr("webSocketsHandshake: failed sending WebSockets response\n");
        free(response);
        free(buf);
        return FALSE;
    }
    /* rfbLog("webSocketsHandshake: handshake complete\n"); */
    return TRUE;
}

void
webSocketsGenMd5(char * target, char *key1, char *key2, char *key3)
{
    unsigned int i, spaces1 = 0, spaces2 = 0;
    unsigned long num1 = 0, num2 = 0;
    unsigned char buf[17];
    for (i=0; i < strlen(key1); i++) {
        if (key1[i] == ' ') {
            spaces1 += 1;
        }
        if ((key1[i] >= 48) && (key1[i] <= 57)) {
            num1 = num1 * 10 + (key1[i] - 48);
        }
    }
    num1 = num1 / spaces1;

    for (i=0; i < strlen(key2); i++) {
        if (key2[i] == ' ') {
            spaces2 += 1;
        }
        if ((key2[i] >= 48) && (key2[i] <= 57)) {
            num2 = num2 * 10 + (key2[i] - 48);
        }
    }
    num2 = num2 / spaces2;

    /* Pack it big-endian */
    buf[0] = (num1 & 0xff000000) >> 24;
    buf[1] = (num1 & 0xff0000) >> 16;
    buf[2] = (num1 & 0xff00) >> 8;
    buf[3] =  num1 & 0xff;

    buf[4] = (num2 & 0xff000000) >> 24;
    buf[5] = (num2 & 0xff0000) >> 16;
    buf[6] = (num2 & 0xff00) >> 8;
    buf[7] =  num2 & 0xff;

    strncpy((char *)buf+8, key3, 8);
    buf[16] = '\0';

    md5_buffer((char *)buf, 16, target);
    target[16] = '\0';

    return;
}

int
webSocketsEncode(rfbClientPtr cl, const char *src, int len)
{
    int i, sz = 0;
    unsigned char chr;
    cl->encodeBuf[sz++] = '\x00';
    if (cl->webSocketsBase64) {
        len = __b64_ntop((unsigned char *)src, len, cl->encodeBuf+sz, UPDATE_BUF_SIZE*2);
        if (len < 0) {
            return len;
        }
        sz += len;
    } else {
        for (i=0; i < len; i++) {
            chr = src[i];
            if (chr < 128) {
                if (chr == 0x00) {
                    cl->encodeBuf[sz++] = '\xc4';
                    cl->encodeBuf[sz++] = '\x80';
                } else {
                    cl->encodeBuf[sz++] = chr;
                }
            } else {
                if (chr < 192) {
                    cl->encodeBuf[sz++] = '\xc2';
                    cl->encodeBuf[sz++] = chr;
                } else {
                    cl->encodeBuf[sz++] = '\xc3';
                    cl->encodeBuf[sz++] = chr - 64;
                }
            }
        }
    }
    cl->encodeBuf[sz++] = '\xff';
    /* rfbLog("<< webSocketsEncode: %d\n", len); */
    return sz;
}

int
webSocketsDecode(rfbClientPtr cl, char *dst, int len)
{
    int retlen = 0, n, i, avail, modlen, needlen, actual;
    char *buf, *end = NULL;
    unsigned char chr, chr2;

    buf = cl->decodeBuf;

    n = recv(cl->sock, buf, len*2+2, MSG_PEEK);

    if (n <= 0) {
        rfbLog("recv of %d\n", n);
        return n;
    }


    if (cl->webSocketsBase64) {
        /* Base64 encoded WebSockets stream */

        if (buf[0] == '\xff') {
            i = read(cl->sock, buf, 1); /* Consume marker */
            buf++;
            n--;
        }
        if (n == 0) {
            errno = EAGAIN;
            return -1;
        }
        if (buf[0] == '\x00') {
            i = read(cl->sock, buf, 1); /* Consume marker */
            buf++;
            n--;
        }
        if (n == 0) {
            errno = EAGAIN;
            return -1;
        }

        /* end = memchr(buf, '\xff', len*2+2); */
        end = memchr(buf, '\xff', n);
        if (!end) {
            end = buf + n;
        }
        avail = end - buf;

        len -= cl->carrylen;

        /* Determine how much base64 data we need */
        modlen = len + (len+2)/3;
        needlen = modlen;
        if (needlen % 4) {
            needlen += 4 - (needlen % 4);
        }

        if (needlen > avail) {
            /* rfbLog("Waiting for more base64 data\n"); */
            errno = EAGAIN;
            return -1;
        }

        /* Any carryover from previous decode */
        for (i=0; i < cl->carrylen; i++) {
	    /* rfbLog("Adding carryover %d\n", cl->carryBuf[i]); */
            dst[i] = cl->carryBuf[i];
            retlen += 1;
        }

        /* Decode the rest of what we need */
        buf[needlen] = '\x00';  /* Replace end marker with end of string */
        /* rfbLog("buf: %s\n", buf); */
        n = __b64_pton(buf, (unsigned char *)dst+retlen, 2+len);
        if (n < len) {
            rfbErr("Base64 decode error\n");
            errno = EIO;
            return -1;
        }
        retlen += n;

        /* Consume the data from socket */
        i = read(cl->sock, buf, needlen);

        cl->carrylen = n - len;
        retlen -= cl->carrylen;
        for (i=0; i < cl->carrylen; i++) {
            /* rfbLog("Saving carryover %d\n", dst[retlen + i]); */
            cl->carryBuf[i] = dst[retlen + i];
        }
    } else {
        /* UTF-8 encoded WebSockets stream */

        actual = 0;
        for (needlen = 0; needlen < n && actual < len; needlen++) {
            chr = buf[needlen];
            if ((chr > 0) && (chr < 128)) {
                actual++;
            } else if ((chr > 127) && (chr < 255)) {
                if (needlen + 1 >= n) {
                    break;
                }
                needlen++;
                actual++;
            }
        }

        if (actual < len) {
            errno = EAGAIN;
            return -1;
        }

        /* Consume what we need */
        if ((n = read(cl->sock, buf, needlen)) < needlen) {
            return n;
        }

        while (retlen < len) {
            chr = buf[0];
            buf += 1;
            if (chr == 0) {
                /* Begin frame marker, just skip it */
            } else if (chr == 255) {
                /* Begin frame marker, just skip it */
	    } else if (chr < 128) {
                dst[retlen++] = chr;
            } else {
                chr2 = buf[0];
                buf += 1;
                switch (chr) {
                case (unsigned char) '\xc2':
                    dst[retlen++] = chr2;
                    break;
                case (unsigned char) '\xc3':
                    dst[retlen++] = chr2 + 64;
                    break;
                case (unsigned char) '\xc4':
                    dst[retlen++] = 0;
                    break;
                default:
                    rfbErr("Invalid UTF-8 encoding\n");
                    errno = EIO;
                    return -1;
                }
            }
        }
    }

    /* rfbLog("<< webSocketsDecode, retlen: %d\n", retlen); */
    return retlen;
}

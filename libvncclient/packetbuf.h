/*
  packetbuf.h: packet buffer header

  Copyright (C) 2010 Christian Beier <dontmind@freeshell.org>
 
  This is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this software; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
  USA.
*/


#ifndef PACKETBUF_H
#define PACKETBUF_H

#include <stdlib.h>
#include "rfb/rfbint.h"


/*
   Data structure containing a received UDP packet.
*/
typedef struct _packet {
  char *data;
  size_t datalen;
  uint32_t id;
  struct _packet *next;
} packet;


/*
  the buffer holding packets
*/
typedef struct _packetBuf {
  packet *head;
  packet *tail;
  size_t maxlen;
  size_t len;
  size_t count;
} packetBuf;



/* creates a new empty packetBuf buffer of maximum size max_length in byte */
packetBuf* packetBufCreate(size_t max_length);

/* destroys a packetBuf, deallocating all internal data */
void packetBufDestroy(packetBuf* b);

/* Add a packet to the buffer. 
   Returns 1 on success, 0 when buffer at max length.*/
int packetBufPush(packetBuf* b, packet *packet);

/* Removes the first packet from buffer. */
void packetBufPop(packetBuf* b);

/* Get packet at index i. */
packet* packetBufAt(packetBuf* b, size_t i);

/* get number of buffered elements */
size_t packetBufCount(packetBuf* b);


#endif

/*
  packetbuf.c: packet buffer implementation

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

#include <string.h>
#include "packetbuf.h"



/* creates a new empty packetBuf buffer of maximum size max_length */
packetBuf* packetBufCreate(size_t max_length)
{
  packetBuf* b = calloc(sizeof(packetBuf), 1);
  b->maxlen = max_length;
  return b;
}



/* destroys a packetBuf, deallocating all internal data */
void packetBufDestroy(packetBuf* b)
{
  if(b) {
    while(b->head) {
      packet *tmp = b->head->next;
      free(b->head->data);
      free(b->head);
      b->head = tmp;
    }
  }
}



/* Add a packet to the buffer. 
   Returns 1 on success, 0 when buffer at max length.*/
int packetBufPush(packetBuf* b, packet *packet)
{
  if(b->len + packet->datalen >= b->maxlen)
    return 0;

  if(!b->head) { /* is empty */
    b->head = packet;
    b->tail = packet;
    packet->next = NULL;
  }
  else {
    b->tail->next = packet;
    b->tail = packet;
    b->tail->next = NULL;
  }
  b->len += packet->datalen;
  b->count++;
  return 1;
}



/* Removes the first packet from buffer. */
void packetBufPop(packetBuf* b)
{
  if(b->head) {
    /* remove head */
    packet *newhead = b->head->next;
    b->len -= b->head->datalen;;
    b->count--;
    free(b->head->data);
    free(b->head);
    b->head = newhead;
  }
}


/* Get packet at index i. */
packet* packetBufAt(packetBuf* b, size_t i)
{
 if(b) {
   packet *r = b->head;
   while(r && i) {
     r = r->next;
     --i;
   }
   return r;
 }
 else
   return NULL;
}


/* get number of buffered elements */
size_t packetBufCount(packetBuf* b)
{
  return b->count;
}






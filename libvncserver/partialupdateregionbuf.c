/*
  partialupdateregionbuf.c: partial update region ringbuffer 
                            implementation

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


#include "partialupdateregionbuf.h"
#include "rfb/rfbregion.h"


partUpdRgnBuf* partUpdRgnBufCreate(size_t length)
{
  partUpdRgnBuf* b = malloc(sizeof(partUpdRgnBuf));
	
  b->len = length;
  b->nextInsertAt = 0;
  b->wraparound = FALSE;  
  b->partUpdRgns = calloc(sizeof(partialUpdRegion), b->len);

  return b;
}



void partUpdRgnBufDestroy(partUpdRgnBuf* b)
{
  /* free all associated regions */
  size_t i;
  for(i = 0; i < b->len; ++i) 
    if(b->partUpdRgns[i].region)
      sraRgnDestroy(b->partUpdRgns[i].region);

  /* free internal array */
  free(b->partUpdRgns);

  /* free the buffer object itself */
  free(b);
}



void partUpdRgnBufInsert(partUpdRgnBuf* b, partialUpdRegion partUpdRgn)
{
  /* clean up old data if it exists */
  if(b->partUpdRgns[b->nextInsertAt].region)
    sraRgnDestroy(b->partUpdRgns[b->nextInsertAt].region);

  /* insert new */
  b->partUpdRgns[b->nextInsertAt] = partUpdRgn;

  b->nextInsertAt++;

  /* if at the end, wrap around */
  if(b->nextInsertAt >= b->len) {
    b->nextInsertAt = 0;
    b->wraparound = TRUE;
  }
}



partialUpdRegion* partUpdRgnBufAt(partUpdRgnBuf* b, size_t index)
{
  if(index > partUpdRgnBufCount(b)-1)
    return NULL;

  if(!b->wraparound) { 
    return b->partUpdRgns + index;
  }
  else {
    /* real position is first element 
       (which is at nextInsertAt after wrap-around)
       plus the index offset */
    size_t realpos = b->nextInsertAt + index;
    if(realpos >= b->len)
      realpos -= b->len;

    return b->partUpdRgns + realpos;
  }
}



size_t partUpdRgnBufCount(partUpdRgnBuf* b)
{
  if(!b->wraparound)
    return b->nextInsertAt;
  else
    return b->len;
}

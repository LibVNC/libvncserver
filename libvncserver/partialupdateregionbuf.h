/*
  partialupdateregionbuf.h: partial update region ringbuffer header

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


#ifndef PARTIALUPDATEREGIONBUF_H
#define PARTIALUPDATEREGIONBUF_H

#include "rfb/rfb.h"


/*
   a data structure associating 
   partial update ids to rfb regions 
*/
typedef struct _partialUpdRegion {
  uint16_t idWhole;
  uint32_t idPartial;
  sraRegionPtr region;
  rfbBool pending;
} partialUpdRegion;


/*
  the ringbuffer holding partialUpdRegions  
*/
typedef struct _partUpdRgnBuf {
  partialUpdRegion* partUpdRgns;
  size_t len;
  size_t nextInsertAt;
  rfbBool wraparound;
} partUpdRgnBuf;



/* creates a new empty partUpdRgnBuf ringbuffer of size length */
partUpdRgnBuf* partUpdRgnBufCreate(size_t length);

/* destroys a partUpdRgnBuf, deallocating all internal data */
void partUpdRgnBufDestroy(partUpdRgnBuf* b);

/* insert a partialUpdRegion into a ringbuffer */
void partUpdRgnBufInsert(partUpdRgnBuf* b, partialUpdRegion partUpdRgn);

/* get partialUpdRegion at index */
partialUpdRegion* partUpdRgnBufAt(partUpdRgnBuf* b, size_t index);

/* get number of buffered elements */
size_t partUpdRgnBufCount(partUpdRgnBuf* b);


#endif

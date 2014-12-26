/*
  partialupdateregion.h: partial update region for use with generic ring buffer

  Copyright (C) 2014 Christian Beier <dontmind@freeshell.org>
 
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


#ifndef PARTIALUPDATEREGION_H
#define PARTIALUPDATEREGION_H

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
  uint32_t sendrate;
  rfbBool sendrate_decreased;
} partialUpdRegion;


/*
  cleaner function for the ring buffer
*/ 
static void clean_partialUpdRegion(void *p)
{
    partialUpdRegion *pur = (partialUpdRegion*)p;
    if(pur->region)
	sraRgnDestroy(pur->region);
}



#endif

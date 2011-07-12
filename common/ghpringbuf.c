/* 
   Generic High Performance Ring Buffer

   Copyright (c) 2011 Christian Beier <dontmind@freeshell.org>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <string.h>
#include "ghpringbuf.h"


ghpringbuf* ghpringbuf_create(size_t capacity, size_t item_size, int is_overwriting, void (*item_cleaner)(void*))
{
  ghpringbuf* b = calloc(1, sizeof(ghpringbuf));
  if(!b)
    return NULL;
  b->capacity = capacity;
  b->item_sz = item_size;
  b->is_overwriting = is_overwriting;
  b->clean_item = item_cleaner;
  b->items = calloc(capacity, item_size);
  if(!b->items)
    {
      free(b);
      return NULL;
    }
  return b;
}


void ghpringbuf_destroy(ghpringbuf* b)
{
  if(!b)
    return;

  if(b->clean_item)
    {
      size_t i, count = ghpringbuf_count(b); /* pop decrements count */
      for(i=0; i < count; ++i)
	ghpringbuf_pop(b);
    }
  free(b->items);
  free(b);
}


int ghpringbuf_put(ghpringbuf* b, void* item)
{
  b->lock = 1;
  if (b->count < b->capacity)
    {
      char* it = b->items;
      it += b->item_sz * b->iput;
      memcpy(it, item, b->item_sz);
      b->iput = (b->iput+1) == b->capacity ? 0 : b->iput+1; /* advance or wrap around */
      b->count++;
    }
  else
    {
      if(b->is_overwriting)
	{
	  char* it = b->items;
	  it += b->item_sz * b->iput;

	  if(b->clean_item) /* clean out item that we will overwrite */
	    b->clean_item(it);

	  memcpy(it, item, b->item_sz);
	  b->iget = b->iput = (b->iput+1) == b->capacity ? 0 : b->iput+1; /* advance or wrap around */
	}
      else
	{
	  b->lock = 0;
	  return 0; /* buffer full */
	}
    }
  b->lock = 0;
  return 1;
}


int ghpringbuf_at(ghpringbuf* b, size_t index, void* dst)
{
  b->lock = 1;
  if (b->count > 0 && index < b->count) 
    {
      size_t pos = b->iget + index;
      if(pos >= b->capacity)
	pos -= b->capacity;

      char* it = b->items;
      it += b->item_sz * pos;
      memcpy(dst, it, b->item_sz);
      b->lock = 0;
      return 1;
    }
  b->lock = 0;
  return 0;
}


int ghpringbuf_pop(ghpringbuf* b)
{
  b->lock = 1;
  if (b->count > 0) 
    {
      if(b->clean_item)
	{
	  char* it = b->items;
	  it += b->item_sz * b->iget; /* go to iget index */
	  if(b->clean_item) /* clean out item that we will abandon */
	    b->clean_item(it);
	}

      b->iget = (b->iget+1) == b->capacity ? 0 : b->iget+1; /* advance or wrap around */
      b->count--;
      b->lock = 0;
      return 1;
    }
  b->lock = 0;
  return 0;
}


size_t ghpringbuf_count(ghpringbuf* b)
{
  return b->count;
}



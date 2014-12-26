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

#ifndef GHPRINGBUF_H
#define GHPRINGBUF_H

#include <stdlib.h>


/**
  the ringbuffer struct
*/
typedef struct _ghpringbuf {
  void* items;
  size_t iput;      /* index for next item to be put */
  size_t iget;      /* index for next item to be got */
  size_t item_sz;      /* size of one item */
  void (*clean_item)(void *item); /* item cleaner callback. set in case item contains pointers to other stuff. */
  size_t count;        /* number of items in buffer */
  size_t capacity;     /* max item count */
  int lock;            /* internal lock */
  int is_overwriting;  /* if this is an overwriting buffer or not */
  int flags;           /*< general purpose flags to mark buffer */
} ghpringbuf;



/** creates a new empty ringbuffer of capacity capacity, item size item_size,
    if this is an overwriting buffer or not, and a item cleaner callback or NULL */
ghpringbuf* ghpringbuf_create(size_t capacity, size_t item_size, int is_overwriting, void (*item_cleaner)(void*));

/** destroys a ghpringbuf, deallocating all internal data */
void ghpringbuf_destroy(ghpringbuf* b);

/** copy item at item_ptr to end of ringbuffer. returns 1 on success, 0 if buffer full */
int ghpringbuf_put(ghpringbuf* b, void* item_ptr);

/** insert item at index, will be copied from src. returns 1 on success, 0 if index out of bounds */
int ghpringbuf_insert(ghpringbuf* b, size_t index, void* src);

/** access item at index, returns pointer to item on success, NULL if index out of bounds */
void* ghpringbuf_at(ghpringbuf* b, size_t index);

/** remove first item. returns 1 on success, 0 if buffer empty */
int ghpringbuf_pop(ghpringbuf* b);

/** get number of buffered items */
size_t ghpringbuf_count(ghpringbuf* b);


#endif

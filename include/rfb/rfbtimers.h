/*
 * Copyright (C) 2023 AnatoScope SA.  All Rights Reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 *
 * Copyright 1987, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from The Open Group.
 *
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Originally derived from TurboVNC ff35d99e9aebb3905c2d90bea7c3305b63c853cd
 */

#ifndef _RFB_TIMERS_H_
#define _RFB_TIMERS_H_

typedef struct _rfbTimersRec rfbTimers;
typedef rfbTimers* rfbTimersPtr;

typedef struct _rfbTimerRec rfbTimer;
typedef rfbTimer* rfbTimerPtr;

/**
 * @param timer timer that invoked the callback
 * @param time current time
 * @param arg user data to pass
 * @return if non-zero then set the timer to fire again after that time
 */
typedef unsigned int (*rfbTimerCallback) (rfbTimerPtr timer, unsigned int time, void *arg);

/**
 * Create a list for timers.
 */
rfbTimersPtr rfbTimersCreate();

/**
 * Destroy a list of timers.
 */
void rfbTimersDestroy(rfbTimersPtr timers);

/**
 * Invoke callbacks that were scheduled to run after specified time.
 */
void rfbTimerCheck(rfbTimersPtr timers);

/**
 * Schedule a callback to run after specified time, create the timer if needed.
 *
 * @param timers timer list
 * @param timer timer to use (NULL creates a new one)
 * @param millis time in milliseconds after which the callback is allowed to be invoked
 * @param arg user data to associate with the timer
 * @return pointer to a timer that has to be freed by calling rfbTimerFree() (no need to recreate the timer for every rfbTimerSet())
 */
rfbTimerPtr rfbTimerSet(rfbTimersPtr timers, rfbTimerPtr timer, unsigned int millis, rfbTimerCallback func, void *arg);

/**
 * Stop timer.
 *
 * @param timers timer list
 * @param timer timer to stop
 */
void rfbTimerCancel(rfbTimersPtr timers, rfbTimerPtr timer);

/**
 * Cancel and delete a timer.
 *
 * @param timers timer list
 * @param timer timer to delete
 */
void rfbTimerFree(rfbTimersPtr timers, rfbTimerPtr timer);

#endif /* _RFB_TIMERS_H_ */

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

#include <rfb/rfbconfig.h>

#ifdef LIBVNCSERVER_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <stddef.h>
#include <stdlib.h>

#include <rfb/rfblist.h>
#include <rfb/rfbtimers.h>
#include <rfb/threading.h>

struct _rfbTimerRec {
    struct rfb_list list;
    unsigned int expires;
    unsigned int delta;
    rfbTimerCallback callback;
    void *arg;
};

struct _rfbTimersRec {
    MUTEX(listmutex);
    struct rfb_list timers;
};

static inline rfbTimerPtr first_timer(rfbTimersPtr timers_ctx)
{
    /* inline rfb_list_is_empty which can't handle volatile */
    if (timers_ctx->timers.next == &timers_ctx->timers)
        return NULL;
    return rfb_list_first_entry(&timers_ctx->timers, struct _rfbTimerRec, list);
}

#if !defined LIBVNCSERVER_HAVE_GETTIMEOFDAY && defined WIN32
#include <fcntl.h>
#include <conio.h>
#include <sys/timeb.h>

static void gettimeofday(struct timeval* tv,char* dummy)
{
   SYSTEMTIME t;
   GetSystemTime(&t);
   tv->tv_sec=t.wHour*3600+t.wMinute*60+t.wSecond;
   tv->tv_usec=t.wMilliseconds*1000;
}
#endif

#if (defined WIN32 && defined __MINGW32__) || defined(__CYGWIN__)
static unsigned int GetTimeInMillis(void)
{
    return GetTickCount();
}
#else
static unsigned int GetTimeInMillis(void)
{
    struct timeval tv;

#ifdef MONOTONIC_CLOCK
    struct timespec tp;

    if (!clockid) {
#ifdef CLOCK_MONOTONIC_COARSE
        if (clock_getres(CLOCK_MONOTONIC_COARSE, &tp) == 0 &&
            (tp.tv_nsec / 1000) <= 1000 &&
            clock_gettime(CLOCK_MONOTONIC_COARSE, &tp) == 0)
            clockid = CLOCK_MONOTONIC_COARSE;
        else
#endif
        if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
            clockid = CLOCK_MONOTONIC;
        else
            clockid = ~0L;
    }
    if (clockid != ~0L && clock_gettime(clockid, &tp) == 0)
        return (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000L);
#endif

    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}
#endif

static void timers_lock(rfbTimersPtr timers_ctx)
{
    LOCK(timers_ctx->listmutex);
}

static void timers_unlock(rfbTimersPtr timers_ctx)
{
    UNLOCK(timers_ctx->listmutex);
}

static inline int timer_pending(rfbTimerPtr timer) {
    return !rfb_list_is_empty(&timer->list);
}

static void DoTimer(rfbTimersPtr timers_ctx, rfbTimerPtr timer, unsigned int now)
{
    unsigned int newTime;

    rfb_list_del(&timer->list);
    newTime = (*timer->callback)(timer, now, timer->arg);
    if (newTime)
        rfbTimerSet(timers_ctx, timer, newTime, timer->callback, timer->arg);
}

static void DoTimers(rfbTimersPtr timers_ctx, unsigned int now)
{
    rfbTimerPtr timer;

    timers_lock(timers_ctx);
    while ((timer = first_timer(timers_ctx))) {
        if ((int) (timer->expires - now) > 0)
            break;
        rfb_list_del(&timer->list);
        timers_unlock(timers_ctx);
        unsigned int newTime = (*timer->callback)(timer, now, timer->arg);
        if (newTime)
            rfbTimerSet(timers_ctx, timer, newTime, timer->callback, timer->arg);
        timers_lock(timers_ctx);
    }
    timers_unlock(timers_ctx);
}

rfbTimersPtr rfbTimersCreate()
{
    rfbTimersPtr timers_ctx = (rfbTimersPtr) malloc(sizeof(rfbTimers));
    if (!timers_ctx) {
        return NULL;
    }

    rfbTimerPtr timer, tmp;

    INIT_MUTEX_RECURSIVE(timers_ctx->listmutex);
    rfb_list_init((struct rfb_list*) &timers_ctx->timers);

    rfb_list_for_each_entry_safe(timer, tmp, &timers_ctx->timers, list)
    {
        rfb_list_del(&timer->list);
        free(timer);
    }

    return timers_ctx;
}

void rfbTimersDestroy(rfbTimersPtr timers_ctx)
{
    rfbTimerPtr timer, tmp;

    if (!timers_ctx) {
        return;
    }

    TINI_MUTEX(timers_ctx->listmutex);

    rfb_list_for_each_entry_safe(timer, tmp, &timers_ctx->timers, list)
    {
        rfb_list_del(&timer->list);
        free(timer);
    }

    free(timers_ctx);
}

void rfbTimerCheck(rfbTimersPtr timers_ctx)
{
    DoTimers(timers_ctx, GetTimeInMillis());
}

rfbTimerPtr rfbTimerSet(rfbTimersPtr timers_ctx, rfbTimerPtr timer, unsigned int millis, rfbTimerCallback func, void *arg)
{
    rfbTimerPtr existing, tmp;
    unsigned int now = GetTimeInMillis();

    if (!timer) {
        timer = calloc(1, sizeof(struct _rfbTimerRec));
        if (!timer)
            return NULL;
        rfb_list_init(&timer->list);
    } else {
        timers_lock(timers_ctx);
        if (timer_pending(timer)) {
            rfb_list_del(&timer->list);
        }
        timers_unlock(timers_ctx);
    }
    if (!millis)
        return timer;
    timer->delta = millis;
    millis += now;
    timer->expires = millis;
    timer->callback = func;
    timer->arg = arg;
    timers_lock(timers_ctx);

    /* Sort into list */
    rfb_list_for_each_entry_safe(existing, tmp, &timers_ctx->timers, list)
        if ((int) (existing->expires - millis) > 0)
            break;
    /* This even works at the end of the list -- existing->list will be timers */
    rfb_list_add(&timer->list, existing->list.prev);

    /* Check to see if the timer is ready to run now */
    if ((int) (millis - now) <= 0)
        DoTimer(timers_ctx, timer, now);

    timers_unlock(timers_ctx);
    return timer;
}

void rfbTimerCancel(rfbTimersPtr timers_ctx, rfbTimerPtr timer)
{
    if (!timer)
        return;
    timers_lock(timers_ctx);
    rfb_list_del(&timer->list);
    timers_unlock(timers_ctx);
}

void rfbTimerFree(rfbTimersPtr timers_ctx, rfbTimerPtr timer)
{
    if (!timer)
        return;
    rfbTimerCancel(timers_ctx, timer);
    free(timer);
}

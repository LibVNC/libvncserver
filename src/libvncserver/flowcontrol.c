/*
 * flowcontrol.c - implement RFB flow control extensions
 */

/*
 * Copyright (C) 2023 AnatoScope SA.  All Rights Reserved.
 * Copyright (C) 2012, 2014, 2017-2018, 2021, 2023 D. R. Commander.
 *                                                 All Rights Reserved.
 * Copyright (C) 2018 Peter Åstrand for Cendio AB.  All Rights Reserved.
 * Copyright (C) 2011, 2015 Pierre Ossman for Cendio AB.  All Rights Reserved.
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
 */

/*
 * This code implements congestion control in the same manner as TCP, in order
 * to avoid excessive latency in the transport.  This is needed because "buffer
 * bloat" is unfortunately still a very real problem.
 *
 * The basic principle is that described in RFC 5681 (TCP Congestion Control),
 * with the addition of using the TCP Vegas algorithm.  The reason we use Vegas
 * is that we run on top of a reliable transport, so we need a latency-based
 * algorithm rather than a loss-based one.  There is also a lot of
 * interpolation in our algorithm, because our measurements have poor
 * granularity.
 *
 * We use a simplistic form of slow start in order to ramp up quickly from an
 * idle state.  We do not have any persistent threshold, though, as there is
 * too much noise for it to be reliable.
 */

/*
 * Originally derived from TurboVNC ff35d99e9aebb3905c2d90bea7c3305b63c853cd
 */

#include <rfb/rfbconfig.h>

#ifdef LIBVNCSERVER_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <rfb/rfb.h>
#include <rfb/rfbregion.h>
#include <limits.h>

#include "flowcontrol.h"

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

/* #define CONGESTION_DEBUG */


/* This window should get us going fairly quickly on a network with decent
   bandwidth.  If it's too high, then it will rapidly be reduced and stay
   low. */
static const unsigned INITIAL_WINDOW = 16384;

/* TCP's minimum window is 3 * MSS, but since we don't know the MSS, we
   make a guess at 4 KB (it's probably a bit higher.) */
static const unsigned MINIMUM_WINDOW = 4096;

/* The current default maximum window for Linux (4 MB.)  This should be a good
   limit for now... */
static const unsigned MAXIMUM_WINDOW = 4194304;


static rfbBool IsCongested(rfbClientPtr);
static int GetUncongestedETA(rfbClientPtr);
static unsigned GetExtraBuffer(rfbClientPtr);
static unsigned GetInFlight(rfbClientPtr);
static uint32_t congestionCallback(rfbTimerPtr, unsigned int, void*);
static void UpdateCongestion(rfbClientPtr);


#ifndef min
inline static unsigned min(unsigned a, unsigned b)
{
    return a > b ? b : a;
}
#endif

#ifndef max
inline static unsigned max(unsigned a, unsigned b)
{
    return a > b ? a : b;
}
#endif

static time_t msBetween(const struct timeval *first,
                        const struct timeval *second)
{
  time_t diff;

  diff = (second->tv_sec - first->tv_sec) * 1000;

  diff += second->tv_usec / 1000;
  diff -= first->tv_usec / 1000;

  return diff;
}

static time_t msSince(const struct timeval *then)
{
  struct timeval now;

  gettimeofday(&now, NULL);

  return msBetween(then, &now);
}

static rfbBool isBefore(const struct timeval *first, const struct timeval *second)
{
  if (first->tv_sec < second->tv_sec)
    return TRUE;
  if (first->tv_sec > second->tv_sec)
    return FALSE;
  if (first->tv_usec < second->tv_usec)
    return TRUE;
  return FALSE;
}

/* Compare two positions, even if integer wraparound has occurred. */
static inline rfbBool isAfter(unsigned a, unsigned b)
{
  return a != b && a - b <= UINT_MAX / 2;
}


void rfbInitFlowControl(rfbClientPtr cl)
{
  cl->congWindow = INITIAL_WINDOW;
  cl->inSlowStart = TRUE;
  gettimeofday(&cl->lastUpdate, NULL);
  gettimeofday(&cl->lastSent, NULL);
  gettimeofday(&cl->lastPongArrival, NULL);
  gettimeofday(&cl->lastAdjustment, NULL);
}


/*
 * rfbUpdatePosition() registers the current stream position.  It can and
 * should be called often.
 */

void rfbUpdatePosition(rfbClientPtr cl, unsigned pos)
{
  struct timeval now;
  unsigned delta, consumed;

  gettimeofday(&now, NULL);

  delta = pos - cl->lastPosition;
  if ((delta > 0) || (cl->extraBuffer > 0))
    cl->lastSent = now;

  /* Idle for too long?
     We use a very crude RTO calculation in order to keep things simple.

     FIXME: Implement RFC 2861. */
  if (msBetween(&cl->lastSent, &now) > max(cl->baseRTT * 2, 100)) {

#ifdef CONGESTION_DEBUG
    rfbLog("Connection idle for %d ms.  Resetting congestion control.\n",
           msBetween(&cl->lastSent, &now));
#endif

    /* Close congestion window and redo wire latency measurement. */
    cl->congWindow = min(INITIAL_WINDOW, cl->congWindow);
    cl->baseRTT = (unsigned)-1;
    cl->measurements = 0;
    gettimeofday(&cl->lastAdjustment, NULL);
    cl->minRTT = cl->minCongestedRTT = (unsigned)-1;
    cl->inSlowStart = TRUE;
  }

  /* Commonly we will be in a state of overbuffering.  We need to estimate the
     extra delay that this causes, so we can separate it from the delay caused
     by an incorrect congestion window.  (We cannot do this until we have a RTT
     measurement, though.) */
  if (cl->baseRTT != (unsigned)-1) {
    cl->extraBuffer += delta;
    consumed = msBetween(&cl->lastUpdate, &now) * cl->congWindow / cl->baseRTT;
    if (cl->extraBuffer < consumed)
      cl->extraBuffer = 0;
    else
      cl->extraBuffer -= consumed;
  }

  cl->lastPosition = pos;
  cl->lastUpdate = now;
}


rfbBool rfbSendRTTPing(rfbClientPtr cl)
{
  rfbRTTInfo *rttInfo;
  char type;

  if (!cl->enableFence)
    return TRUE;

  rfbUpdatePosition(cl, cl->sockOffset);

  /* We need to make sure that any old updates are already processed by the
     time we get the response back.  This allows us to reliably throttle
     back if the client or the network overloads. */
  type = 1;
  if (!rfbSendFence(cl, rfbFenceFlagRequest | rfbFenceFlagBlockBefore,
                    sizeof(type), &type))
    return FALSE;

  rttInfo = (rfbRTTInfo*) calloc(sizeof(rfbRTTInfo), 1);

  gettimeofday(&rttInfo->tv, NULL);
  rttInfo->pos = cl->lastPosition;
  rttInfo->extra = GetExtraBuffer(cl);
  rttInfo->congested = IsCongested(cl);

  rfb_list_append(&rttInfo->entry, &cl->pings);

  return TRUE;
}


static void HandleRTTPong(rfbClientPtr cl)
{
  struct timeval now;
  rfbRTTInfo *rttInfo;
  unsigned rtt, delay;

  if (rfb_list_is_empty(&cl->pings))
    return;

  gettimeofday(&now, NULL);

  rttInfo = rfb_list_first_entry(&cl->pings, rfbRTTInfo, entry);
  rfb_list_del(&rttInfo->entry);

  cl->lastPong = *rttInfo;
  cl->lastPongArrival = now;

  rtt = msBetween(&rttInfo->tv, &now);
  if (rtt < 1)
    rtt = 1;

  /* Try to estimate wire latency by tracking the lowest observed latency. */
  if (rtt < cl->baseRTT)
    cl->baseRTT = rtt;

  /* Pings sent before the last adjustment aren't interesting, as they aren't a
     measure of the current congestion window. */
  if (isBefore(&rttInfo->tv, &cl->lastAdjustment))
    return;

  /* Estimate added delay because of overtaxed buffers (see above.) */
  delay = rttInfo->extra * cl->baseRTT / cl->congWindow;
  if (delay < rtt)
    rtt -= delay;
  else
    rtt = 1;

  /* An observed latency less than the wire latency means that we've
     understimated the congestion window.  We can't really determine by how
     much, though, so we pretend that we observed no buffer latency at all. */
  if (rtt < cl->baseRTT)
    rtt = cl->baseRTT;

  /* Record the minimum observed delay (hopefully ignoring jitter), and let the
     congestion control algorithm do its thing.

     NOTE: Our algorithm is delay-based rather than loss-based, which means
     that we need to look at pongs even if they weren't limited by the current
     window ("congested").  Otherwise we will fail to detect increasing
     congestion until the application exceeds the congestion window. */
  if (rtt < cl->minRTT)
    cl->minRTT = rtt;
  if (rttInfo->congested) {
    if (rtt < cl->minCongestedRTT)
      cl->minCongestedRTT = rtt;
  }

  cl->measurements++;
  UpdateCongestion(cl);

  free(rttInfo);
}


static rfbBool IsCongested(rfbClientPtr cl)
{
  if (GetInFlight(cl) < cl->congWindow)
    return FALSE;

  return TRUE;
}


/*
 * rfbIsCongested() determines if the transport is currently congested or if
 * more data can be sent.
 */

rfbBool rfbIsCongested(rfbClientPtr cl)
{
  int eta;

  if (!cl->enableFence)
    return FALSE;

  rfbTimerCancel(cl->timers, cl->congestionTimer);

  rfbUpdatePosition(cl, cl->sockOffset);
  if (!IsCongested(cl))
    return FALSE;

  eta = GetUncongestedETA(cl);
  cl->congestionTimer = rfbTimerSet(cl->timers, cl->congestionTimer, eta <= 0 ? 1 : eta,
                                    congestionCallback, cl);
  return TRUE;
}


/*
 * GetUncongestedETA() estimates the number of milliseconds until the transport
 * will no longer be congested.  It returns 0 if there is no congestion and -1
 * if it is unknown when the transport will no longer be congested.
 */

static int GetUncongestedETA(rfbClientPtr cl)
{
  unsigned targetAcked;

  const rfbRTTInfo *prevPing;
  unsigned eta, elapsed;
  unsigned etaNext, delay;

  rfbRTTInfo *iter;

  targetAcked = cl->lastPosition - cl->congWindow;

  /* Simple case? */
  if (isAfter(cl->lastPong.pos, targetAcked))
    return 0;

  /* No measurements yet? */
  if (cl->baseRTT == (unsigned)-1)
    return -1;

  prevPing = &cl->lastPong;
  eta = 0;
  elapsed = msSince(&cl->lastPongArrival);

  /* Walk the ping queue and figure out which ping we are waiting for in order
     to get to an uncongested state. */
  for (iter = NULL, iter = __container_of(cl->pings.next, iter, entry);;
       iter = __container_of(iter->entry.next, iter, entry)) {
    rfbRTTInfo curPing;

    /* If we aren't waiting for a pong that will clear the congested state,
       then we have to estimate the final bit by pretending that we had a ping
       just after the last position update. */
    if (&iter->entry == &cl->pings) {
      curPing.tv = cl->lastUpdate;
      curPing.pos = cl->lastPosition;
      curPing.extra = cl->extraBuffer;
    } else {
      curPing = *iter;
    }

    etaNext = msBetween(&prevPing->tv, &curPing.tv);
    /* Compensate for buffering delays. */
    delay = curPing.extra * cl->baseRTT / cl->congWindow;
    etaNext += delay;
    delay = prevPing->extra * cl->baseRTT / cl->congWindow;
    if (delay >= etaNext)
      etaNext = 0;
    else
      etaNext -= delay;

    /* Found it? */
    if (isAfter(curPing.pos, targetAcked)) {
      eta += etaNext * (curPing.pos - targetAcked) /
                       (curPing.pos - prevPing->pos);
      if (elapsed > eta)
        return 0;
      else
        return eta - elapsed;
    }

    eta += etaNext;
    prevPing = &*iter;
  }

  return -1;
}


static unsigned GetExtraBuffer(rfbClientPtr cl)
{
  unsigned elapsed;
  unsigned consumed;

  if (cl->baseRTT == (unsigned)-1)
    return 0;

  elapsed = msSince(&cl->lastUpdate);
  consumed = elapsed * cl->congWindow / cl->baseRTT;

  if (consumed >= cl->extraBuffer)
    return 0;
  else
    return cl->extraBuffer - consumed;
}


static unsigned GetInFlight(rfbClientPtr cl)
{
  rfbRTTInfo nextPong;
  unsigned etaNext, delay, elapsed, acked;

  /* Simple case? */
  if (cl->lastPosition == cl->lastPong.pos)
    return 0;

  /* No measurements yet? */
  if (cl->baseRTT == (unsigned)-1) {
    if (!rfb_list_is_empty(&cl->pings)) {
      rfbRTTInfo *rttInfo =
        rfb_list_first_entry(&cl->pings, rfbRTTInfo, entry);
      return cl->lastPosition - rttInfo->pos;
    }
    return 0;
  }

  /* If we aren't waiting for a pong, then we have to estimate things by
     pretending that we had a ping just after the last position update. */
  if (rfb_list_is_empty(&cl->pings)) {
    nextPong.tv = cl->lastUpdate;
    nextPong.pos = cl->lastPosition;
    nextPong.extra = cl->extraBuffer;
  } else {
    rfbRTTInfo *rttInfo = rfb_list_first_entry(&cl->pings, rfbRTTInfo, entry);
    nextPong = *rttInfo;
  }

  /* First, we need to estimate how many bytes have made it through completely.
     To do this, we look at the next ping that should arrive, figure out how
     far behind it should be, and interpolate the positions. */

  etaNext = msBetween(&cl->lastPong.tv, &nextPong.tv);
  /* Compensate for buffering delays. */
  delay = nextPong.extra * cl->baseRTT / cl->congWindow;
  etaNext += delay;
  delay = cl->lastPong.extra * cl->baseRTT / cl->congWindow;
  if (delay >= etaNext)
    etaNext = 0;
  else
    etaNext -= delay;

  elapsed = msSince(&cl->lastPongArrival);

  /* The pong should be here very soon.  Be optimistic and assume we can
     already use its value. */
  if (etaNext <= elapsed)
    acked = nextPong.pos;
  else {
    acked = cl->lastPong.pos;
    acked += (nextPong.pos - cl->lastPong.pos) * elapsed / etaNext;
  }

  return cl->lastPosition - acked;
}


static uint32_t congestionCallback(rfbTimerPtr timer, uint32_t time, void *arg)
{
  rfbClientPtr cl = (rfbClientPtr)arg;

  LOCK(cl->updateMutex);
  sraRegionPtr updateRegion = sraRgnCreateRgn(cl->modifiedRegion);
  UNLOCK(cl->updateMutex);
  LOCK(cl->sendMutex);
  rfbSendFramebufferUpdate(cl, updateRegion);
  UNLOCK(cl->sendMutex);
  sraRgnDestroy(updateRegion);

  return 0;
}


static void UpdateCongestion(rfbClientPtr cl)
{
  unsigned diff;
#if defined(CONGESTION_DEBUG) && defined(TCP_INFO)
  struct tcp_info tcp_info;
  socklen_t tcp_info_length;
#endif

  /* In order to avoid noise, we want at least three measurements. */
  if (cl->measurements < 3)
    return;

  /* The goal is to have a congestion window that is slightly too large, since
     a "perfect" congestion window cannot be distinguished from one that is too
     small.  This translates to a goal of a few extra milliseconds of delay. */

  diff = cl->minRTT - cl->baseRTT;

  if (diff > max(100, cl->baseRTT / 2)) {
    /* We have no way of detecting loss, so assume that a massive latency spike
       means packet loss.  Adjust the window and go directly to congestion
       avoidance. */
#ifdef CONGESTION_DEBUG
    rfbLog("Latency spike!  Backing off...\n");
#endif
    cl->congWindow = cl->congWindow * cl->baseRTT / cl->minRTT;
    cl->inSlowStart = FALSE;
  }

  if (cl->inSlowStart) {
    /* Slow start-- aggressive growth until we see congestion */

    if (diff > 25) {
      /* If we observe increased latency, then we assume we've hit the limit
         and it's time to leave slow start and switch to congestion
         avoidance. */
      cl->congWindow = cl->congWindow * cl->baseRTT / cl->minRTT;
      cl->inSlowStart = FALSE;
    } else {
      /* It's not safe to increase the congestion window unless we actually
         used all of it, so we look at minCongestedRTT and not minRTT. */

      diff = cl->minCongestedRTT - cl->baseRTT;
      if (diff < 25)
        cl->congWindow *= 2;
    }
  } else {
    /* Congestion avoidance (VEGAS) */

    if (diff > 50) {
      /* Slightly too fast */
      cl->congWindow -= 4096;
    } else {
      /* Only the "congested" pongs are checked to see if the window is too
         small. */

      diff = cl->minCongestedRTT - cl->baseRTT;

      if (diff < 5) {
        /* Way too slow */
        cl->congWindow += 8192;
      } else if (diff < 25) {
        /* Too slow */
        cl->congWindow += 4096;
      }
    }
  }

  if (cl->congWindow < MINIMUM_WINDOW)
    cl->congWindow = MINIMUM_WINDOW;
  if (cl->congWindow > MAXIMUM_WINDOW)
    cl->congWindow = MAXIMUM_WINDOW;

#ifdef CONGESTION_DEBUG
  rfbLog("RTT: %d/%d ms (%d ms), Window: %d KB, Offset: %d KB, Bandwidth: %g Mbps%s\n",
         cl->minRTT, cl->minCongestedRTT, cl->baseRTT, cl->congWindow / 1024,
         cl->sockOffset / 1024, cl->congWindow * 8.0 / cl->baseRTT / 1000.0,
         cl->inSlowStart ? " (slow start)" : "");

#ifdef TCP_INFO
  tcp_info_length = sizeof(tcp_info);
  if (getsockopt(cl->sock, SOL_TCP, TCP_INFO, (void *)&tcp_info,
                 &tcp_info_length) == 0) {
    rfbLog("Socket: RTT: %d ms (+/- %d ms) Window %d KB\n",
           tcp_info.tcpi_rtt / 1000, tcp_info.tcpi_rttvar / 1000,
           tcp_info.tcpi_snd_mss * tcp_info.tcpi_snd_cwnd / 1024);
  }
#endif

#endif

  cl->measurements = 0;
  gettimeofday(&cl->lastAdjustment, NULL);
  cl->minRTT = cl->minCongestedRTT = (unsigned)-1;
}


/*
 * rfbSendFence sends a fence message to a specific client
 */
rfbBool rfbSendFence(rfbClientPtr cl, uint32_t flags, unsigned len,
                  const char *data)
{
  rfbFenceMsg f;

  if (!cl->enableFence) {
    rfbLog("ERROR in rfbSendFence: Client does not support fence extension\n");
    return FALSE;
  }
  if (len > 64) {
    rfbLog("ERROR in rfbSendFence: Fence payload is too large\n");
    return FALSE;
  }
  if ((flags & ~rfbFenceFlagsSupported) != 0) {
    rfbLog("ERROR in rfbSendFence: Unknown fence flags\n");
    return FALSE;
  }

  memset(&f, 0, sz_rfbFenceMsg);
  f.type = rfbFence;
  f.flags = Swap32IfLE(flags);
  f.length = len;

  if (rfbWriteExact(cl, (char *)&f, sz_rfbFenceMsg) < 0) {
    rfbLogPerror("rfbSendFence: write");
    rfbCloseClient(cl);
    return FALSE;
  }

  if (len > 0) {
    if (rfbWriteExact(cl, (char *)data, len) < 0) {
      rfbLogPerror("rfbSendFence: write");
      rfbCloseClient(cl);
      return FALSE;
    }
  }
  return TRUE;
}


/*
 * This is called whenever a client fence message is received.
 */
void rfbHandleFence(rfbClientPtr cl, uint32_t flags, unsigned len, const char *data)
{
  unsigned char type;

  if (flags & rfbFenceFlagRequest) {

    if (flags & rfbFenceFlagSyncNext) {
      cl->pendingSyncFence = TRUE;
      cl->fenceFlags = flags & (rfbFenceFlagBlockBefore |
                                rfbFenceFlagBlockAfter |
                                rfbFenceFlagSyncNext);
      cl->fenceDataLen = len;
      if (len > 0)
        memcpy(cl->fenceData, data, len);
      return;
    }

    /* We handle everything synchronously, so we trivially honor these
       modes */
    flags = flags & (rfbFenceFlagBlockBefore | rfbFenceFlagBlockAfter);

    rfbSendFence(cl, flags, len, data);
    return;
  }

  if (len < 1)
    rfbLog("Fence of unusual size received\n");

  type = data[0];

  switch (type) {
    case 0:
      /* Initial dummy fence */
      break;

    case 1:
      HandleRTTPong(cl);
      break;

    default:
      rfbLog("Fence of unusual size received\n");
  }
}


/*
 * rfbSendEndOfCU sends an end of Continuous Updates message to a specific
 * client
 */
rfbBool rfbSendEndOfCU(rfbClientPtr cl)
{
  uint8_t type = rfbEndOfContinuousUpdates;

  if (!cl->enableCU) {
    rfbLog("ERROR in rfbSendEndOfCU: Client does not support Continuous Updates\n");
    return FALSE;
  }

  if (rfbWriteExact(cl, (char *)&type, 1) < 0) {
    rfbLogPerror("rfbSendEndOfCU: write");
    rfbCloseClient(cl);
    return FALSE;
  }

  return TRUE;
}

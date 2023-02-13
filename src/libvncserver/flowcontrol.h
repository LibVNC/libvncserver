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

#ifndef LIBVNCSERVER_FLOWCONTROL_H_
#define LIBVNCSERVER_FLOWCONTROL_H_

#include <rfb/rfbproto.h>

void rfbInitFlowControl(rfbClientPtr cl);

/*
 * rfbUpdatePosition() registers the current stream position.  It can and
 * should be called often.
 */

void rfbUpdatePosition(rfbClientPtr cl, unsigned pos);

rfbBool rfbSendRTTPing(rfbClientPtr cl);

/*
 * rfbIsCongested() determines if the transport is currently congested or if
 * more data can be sent.
 */

rfbBool rfbIsCongested(rfbClientPtr cl);

/*
 * Sends a fence message to a specific client.
 */
rfbBool rfbSendFence(rfbClientPtr cl, uint32_t flags, unsigned len, const char *data);

/*
 * This is to call for a received client fence message.
 */
void rfbHandleFence(rfbClientPtr cl, uint32_t flags, unsigned len, const char *data);

/*
 * rfbSendEndOfCU sends an end of Continuous Updates message to a specific
 * client
 */
rfbBool rfbSendEndOfCU(rfbClientPtr cl);

#endif /* LIBVNCSERVER_FLOWCONTROL_H_ */

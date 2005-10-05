/*
 * cutpaste.c - routines to deal with cut & paste buffers / selection.
 */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 *  USA.
 */

#include <stdio.h>
#include "rfb.h"


/*
 * rfbSetXCutText sets the cut buffer to be the given string.  We also clear
 * the primary selection.  Ideally we'd like to set it to the same thing, but I
 * can't work out how to do that without some kind of helper X client.
 */

void rfbGotXCutText(rfbScreenInfoPtr rfbScreen, char *str, int len)
{
   rfbSendServerCutText(rfbScreen, str, len);
}

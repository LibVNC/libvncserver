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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <stdio.h>
#include <rfb/rfb.h>

static void usage(void)
{
    printf("\nusage:  storepasswd <password> <filename>\n\n");

    printf("Stores a password in encrypted format.\n");
    printf("The resulting file can be used with the -rfbauth argument to OSXvnc.\n\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 3) 
        usage();

    if (rfbEncryptAndStorePasswd(argv[1], argv[2]) != 0) {
        printf("storing password failed.\n");
        return 1;
    } else {
        printf("storing password succeeded.\n");
        return 0;
    }
}

/*
 *  This parses the command line arguments. It was seperated from main.c by 
 *  Justin Dearing <jdeari01@longisland.poly.edu>.
 */

/*
 *  LibVNCServer (C) 2001 Johannes E. Schindelin <Johannes.Schindelin@gmx.de>
 *  Original OSXvnc (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  see GPL (latest version) for full details
 */

#include "rfb.h"

void
rfbUsage(void)
{
    fprintf(stderr, "-rfbport port          TCP port for RFB protocol\n");
    fprintf(stderr, "-rfbwait time          max time in ms to wait for RFB client\n");
    fprintf(stderr, "-rfbauth passwd-file   use authentication on RFB protocol\n"
                    "                       (use 'storepasswd' to create a password file)\n");
    fprintf(stderr, "-passwd plain-password use authentication \n"
                    "                       (use plain-password as password, USE AT YOUR RISK)\n");
    fprintf(stderr, "-deferupdate time      time in ms to defer updates "
                                                             "(default 40)\n");
    fprintf(stderr, "-desktop name          VNC desktop name (default \"LibVNCServer\")\n");
    fprintf(stderr, "-alwaysshared          always treat new clients as shared\n");
    fprintf(stderr, "-nevershared           never treat new clients as shared\n");
    fprintf(stderr, "-dontdisconnect        don't disconnect existing clients when a "
                                                             "new non-shared\n"
                    "                       connection comes in (refuse new connection "
                                                                "instead)\n");
    exit(1);
}

/* purges COUNT arguments from ARGV at POSITION and decrements ARGC.
   POSITION points to the first non purged argument afterwards. */
void rfbPurgeArguments(int* argc,int* position,int count,char *argv[])
{
  int amount=(*argc)-(*position)-count;
  if(amount)
    memmove(argv+(*position),argv+(*position)+count,sizeof(char*)*amount);
  (*argc)-=count;
  (*position)--;
}

void 
rfbProcessArguments(rfbScreenInfoPtr rfbScreen,int* argc, char *argv[])
{
    int i,i1;

    if(!argc) return;
    
    for (i = i1 = 1; i < *argc; i++) {
        if (strcmp(argv[i], "-help") == 0) {
	    rfbUsage();
	    exit(1);
	} else if (strcmp(argv[i], "-rfbport") == 0) { /* -rfbport port */
            if (i + 1 >= *argc) rfbUsage();
	   rfbScreen->rfbPort = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbwait") == 0) {  /* -rfbwait ms */
            if (i + 1 >= *argc) rfbUsage();
	   rfbScreen->rfbMaxClientWait = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbauth") == 0) {  /* -rfbauth passwd-file */
            if (i + 1 >= *argc) rfbUsage();
            rfbScreen->rfbAuthPasswdData = argv[++i];
	} else if (strcmp(argv[i], "-passwd") == 0) {  /* -passwd password */
	    char **passwds = malloc(sizeof(char**)*2);
	    if (i + 1 >= *argc) rfbUsage();
	    passwds[0] = argv[++i];
	    passwds[1] = 0;
	    rfbScreen->rfbAuthPasswdData = (void*)passwds;
	    rfbScreen->passwordCheck = rfbCheckPasswordByList;
        } else if (strcmp(argv[i], "-deferupdate") == 0) {  /* -desktop desktop-name */
            if (i + 1 >= *argc) rfbUsage();
            rfbScreen->rfbDeferUpdateTime = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-desktop") == 0) {  /* -desktop desktop-name */
            if (i + 1 >= *argc) rfbUsage();
            rfbScreen->desktopName = argv[++i];
        } else if (strcmp(argv[i], "-alwaysshared") == 0) {
	    rfbScreen->rfbAlwaysShared = TRUE;
        } else if (strcmp(argv[i], "-nevershared") == 0) {
            rfbScreen->rfbNeverShared = TRUE;
        } else if (strcmp(argv[i], "-dontdisconnect") == 0) {
            rfbScreen->rfbDontDisconnect = TRUE;
        } else if (strcmp(argv[i], "-width") == 0) {
               rfbScreen->width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-height") == 0) {
               rfbScreen->height = atoi(argv[++i]);
        } else {
	    /* we just remove the processed arguments from the list */
	    if(i != i1)
	        rfbPurgeArguments(argc,&i,i1-i,argv);
	    i1++;
	    i++;
        }
    }
    *argc -= i-i1;
}

void rfbSizeUsage()
{
    fprintf(stderr, "-width                 sets the width of the framebuffer\n");
    fprintf(stderr, "-height                sets the height of the framebuffer\n");
    exit(1);
}

void 
rfbProcessSizeArguments(int* width,int* height,int* bpp,int* argc, char *argv[])
{
    int i,i1;

    if(!argc) return;
    for (i = i1 = 1; i < *argc-1; i++) {
        if (strcmp(argv[i], "-bpp") == 0) {
               *bpp = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-width") == 0) {
               *width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-height") == 0) {
               *height = atoi(argv[++i]);
        } else {
	    /* we just remove the processed arguments from the list */
	    if(i != i1) {
	        memmove(argv+i1,argv+i,sizeof(char*)*(*argc-i));
		*argc -= i-i1;
	    }
	    i1++;
	    i = i1-1;
        }
    }
    *argc -= i-i1;
}


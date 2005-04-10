/*
 * blockdpy.c
 *
 * Copyright (c) 2004 Karl J. Runge <runge@karlrunge.com>
 * All rights reserved.
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
 *
 *-----------------------------------------------------------------------
 *
 * This tool is intended for use with x11vnc.  It is a kludge to try to 
 * "block" access via the physical display while x11vnc is running.
 *
 * The expected application is that of a user who screen-locks his
 * workstation before leaving and then later unlocks it remotely via
 * x11vnc.  The user is concerned people with physical access to the
 * machine will be watching, etc.
 *
 * Of course if people have physical access to the machine there are
 * much larger potential security problems, but the idea here is to put
 * up a larger barrier than simply turning on the monitor and tapping
 * the mouse (i.e. to wake up the monitor from DPMS and then observe
 * the x11vnc activity).
 *
 * This program requires DPMS support in the video card and monitor,
 * and the DPMS extension in the X server and the corresponding
 * library with the DPMS API (libXext).
 *
 * It starts off by forcing the state to be DPMSModeOff (lowest power).
 * Then it periodically (a few times a second) checks if the system is
 * still in that state.  If it discovers it to be in another state, it
 * immediately runs, as a separate command, a screen-lock program, "xlock"
 * by default.  The environment variable XLOCK_CMD or -lock option can
 * override this default.  "xscreensaver-command" might be another choice.
 *
 * It is up to the user to make sure the screen-lock command works
 * and PATH is set up correctly, etc.  The command can do anything,
 * it doesn't have to lock the screen.  It could make the sound of a
 * dog barking, for example :-)
 *
 * The option '-grab' causes the program to additionally call
 * XGrabServer() to try to prevent physical mouse or keyboard input to get
 * to any applications on the screen.  NOTE: do NOT use, not working yet!
 * Freezes everything.
 *
 * The options: -display and -auth can be used to set the DISPLAY and
 * XAUTHORITY environment variables via the command line.
 *
 * The options -standby and -suspend change the desired DPMS level
 * to be DPMSModeStandby and DPMSModeSuspend, respectively. 
 *
 * The option '-f flagfile' indicates a flag file to watch for to cause
 * the program to clean up and exit once it exists.  No screen locking is
 * done when the file appears: it is an 'all clear' flag.  Presumably the
 * x11vnc user has relocked the screen before the flagfile is created.
 * See below for coupling this behavior with the -gone command.
 *
 * The option '-bg' causes the program to fork into the background and
 * return 0 if everything looks ok.  If there was an error up to that
 * point the return value would be 1.
 * 
 * Option '-v' prints more info out, useful for testing and debugging.
 * 
 * 
 * These options allow this sort of x11vnc usage:
 *
 * x11vnc ... -accept "blockdpy -bg -f $HOME/.bdpy" -gone "touch $HOME/.bdpy"
 *
 * (this may also work for gone: -gone "killall blockdpy")
 *
 * In the above, once a client connects this program starts up in the
 * background and monitors the DPMS level.  When the client disconnects
 * (he relocked the screen before doing so) the flag file is created and
 * so this program exits normally.  On the other hand, if the physical
 * mouse or keyboard was used during the session, this program would
 * have locked the screen as soon as it noticed the DPMS change.
 *
 * One could create shell scripts for -accept and -gone that do much
 * more sophisticated things.  This would be needed if more than one
 * client connects at a time.
 *
 * It is important to remember once this program locks the screen
 * it *exits*, so nothing will be watching the screen at that point.
 * Don't immediately unlock the screen from in x11vnc!!  Best to think
 * about what might have happened, disconnect the VNC viewer, and then
 * restart x11vnc (thereby having this monitoring program started again).
 *
 *
 * To compile on Linux or Solaris:

 cc -o blockdpy blockdpy.c -L /usr/X11R6/lib -L /usr/openwin/lib -lX11 -lXext

 * (may also need -I /usr/.../include on older machines).
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>

Display *dpy = NULL;
CARD16 standby, suspend, off;
int grab = 0;
int verbose = 0;
int bg = 0;

/* for sleeping some number of millisecs */
struct timeval _mysleep;
#define msleep(x) \
        _mysleep.tv_sec  = ((x)*1000) / 1000000; \
        _mysleep.tv_usec = ((x)*1000) % 1000000; \
        select(0, NULL, NULL, NULL, &_mysleep); 

/* called on signal or if DPMS changed, or other problem */
void reset(int sig) {
	if (grab) {
		if (verbose) {
			fprintf(stderr, "calling XUngrabServer()\n");
		}
		XUngrabServer(dpy);
	}
	if (verbose) {
		fprintf(stderr, "resetting original DPMS values.\n"); 
	}
	fprintf(stderr, "blockdpy: reset sig=%d called\n", sig);
	DPMSEnable(dpy);
	DPMSSetTimeouts(dpy, standby, suspend, off);
	XFlush(dpy);
	if (sig) {
		XCloseDisplay(dpy);
		exit(0);
	}
}

int main(int argc, char** argv) {

	int verbose = 0, bg = 0;
	int i, ev, er;
	char *lock_cmd = "xlock";
	char *flag_file = NULL;
	char estr[100], cmd[500];
	struct stat sbuf;
	CARD16 power;
	CARD16 desired = DPMSModeOff;
	BOOL state;
	

	/* setup the lock command. it may be reset by -lock below. */
	if (getenv("XLOCK_CMD")) {
		lock_cmd = (char *) getenv("XLOCK_CMD");
	}

	/* process cmd line: */
	for (i=1; i<argc; i++) {
		if (!strcmp(argv[i], "-display")) {
			sprintf(estr, "DISPLAY=%s", argv[++i]);
			putenv(strdup(estr));
		} else if (!strcmp(argv[i], "-auth")) {
			sprintf(estr, "XAUTHORITY=%s", argv[++i]);
			putenv(strdup(estr));
		} else if (!strcmp(argv[i], "-lock")) {
			lock_cmd = argv[++i];
		} else if (!strcmp(argv[i], "-f")) {
			flag_file = argv[++i];
			unlink(flag_file);
		} else if (!strcmp(argv[i], "-grab")) {
			grab = 1;
		} else if (!strcmp(argv[i], "-bg")) {
			bg = 1;
		} else if (!strcmp(argv[i], "-v")) {
			verbose = 1;
		} else if (!strcmp(argv[i], "-standby")) {
			desired = DPMSModeStandby;
		} else if (!strcmp(argv[i], "-suspend")) {
			desired = DPMSModeSuspend;
		} else if (!strcmp(argv[i], "-off")) {
			desired = DPMSModeOff;
		}
	}

	/* we want it to go into background to avoid blocking, so add '&'. */
	strcpy(cmd, lock_cmd);
	strcat(cmd, " &");
	lock_cmd = cmd;

	/* close any file descriptors we may have inherited (e.g. port 5900) */
	for (i=3; i<=100; i++) {
		close(i);
	}

	/* open DISPLAY */
	dpy = XOpenDisplay(NULL);
	if (! dpy) {
		fprintf(stderr, "XOpenDisplay failed.\n");
		exit(1);
	}

	/* check for DPMS extension */
	if (! DPMSQueryExtension(dpy, &ev, &er)) {
		fprintf(stderr, "DPMSQueryExtension failed.\n");
		exit(1);
	}
	if (! DPMSCapable(dpy)) {
		fprintf(stderr, "DPMSCapable failed.\n");
		exit(1);
	}
	/* make sure DPMS is enabled */
	if (! DPMSEnable(dpy)) {
		fprintf(stderr, "DPMSEnable failed.\n");
		exit(1);
	}

	/* retrieve the timeouts for later resetting */
	if (! DPMSGetTimeouts(dpy, &standby, &suspend, &off)) {
		fprintf(stderr, "DPMSGetTimeouts failed.\n");
		exit(1);
	}
	if (! standby || ! suspend || ! off) {
		/* if none, set to some reasonable values */
		standby = 900;
		suspend = 1200;
		off = 1800;
	}
	if (verbose) {
		fprintf(stderr, "DPMS timeouts: %d %d %d\n", standby,
		    suspend, off);
	}

	/* now set them to very small values */
	if (desired == DPMSModeOff) {
		if (! DPMSSetTimeouts(dpy, 1, 1, 1)) {
			fprintf(stderr, "DPMSSetTimeouts failed.\n");
			exit(1);
		}
	} else if (desired == DPMSModeSuspend) {
		if (! DPMSSetTimeouts(dpy, 1, 1, 0)) {
			fprintf(stderr, "DPMSSetTimeouts failed.\n");
			exit(1);
		}
	} else if (desired == DPMSModeStandby) {
		if (! DPMSSetTimeouts(dpy, 1, 0, 0)) {
			fprintf(stderr, "DPMSSetTimeouts failed.\n");
			exit(1);
		}
	}
	XFlush(dpy);

	/* set handlers for clean up in case we terminate via signal */
	signal(SIGHUP,  reset);
	signal(SIGINT,  reset);
	signal(SIGQUIT, reset);
	signal(SIGABRT, reset);
	signal(SIGTERM, reset);

	/* force state into DPMS Off (lowest power) mode */
	if (! DPMSForceLevel(dpy, desired)) {
		fprintf(stderr, "DPMSForceLevel failed.\n");
		exit(1);
	}
	XFlush(dpy);

	/* read state */
	msleep(500);
	if (! DPMSInfo(dpy, &power, &state)) {
		fprintf(stderr, "DPMSInfo failed.\n");
		exit(1);
	}
	fprintf(stderr, "power: %d state: %d\n", power, state); 

	/* grab display if desired. NOT WORKING */
	if (grab) {
		if (verbose) {
			fprintf(stderr, "calling XGrabServer()\n");
		}
		XGrabServer(dpy);
	}

	/* go into background if desired. */
	if (bg) {
		pid_t p;
		if ((p = fork()) != 0) {
			if (p < 0) {
				fprintf(stderr, "problem forking.\n");
				exit(1);
			} else {
				/* XXX no fd closing */
				exit(0);
			}
		}
	}

	/* main loop: */
	while (1) {
		/* reassert DPMSModeOff (desired) */
		if (verbose) fprintf(stderr, "reasserting desired DPMSMode\n");
		DPMSForceLevel(dpy, desired);
		XFlush(dpy);

		/* wait a bit */
		msleep(200);

		/* check for flag file appearence */
		if (flag_file && stat(flag_file, &sbuf) == 0) {
			if (verbose) {
				fprintf(stderr, "flag found: %s\n", flag_file);
			}
			unlink(flag_file);
			reset(0);
			exit(0);
		}

		/* check state and power level */
		if (! DPMSInfo(dpy, &power, &state)) {
			fprintf(stderr, "DPMSInfo failed.\n");
			reset(0);
			exit(1);
		}
		if (verbose) {
			fprintf(stderr, "power: %d state: %d\n", power, state); 
		}
		if (!state || power != desired) {
			/* Someone (or maybe a cat) is evidently watching...  */
			fprintf(stderr, "DPMS CHANGE: power: %d state: %d\n",
			    power, state); 
			break;
		}
	}
	reset(0);
	fprintf(stderr, "locking screen with command: \"%s\"\n", lock_cmd); 
	system(lock_cmd);
	exit(0);
}

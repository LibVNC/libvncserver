/* -- selection.c -- */

#include "x11vnc.h"
#include "cleanup.h"
#include "connections.h"
#include "unixpw.h"

/*
 * Selection/Cutbuffer/Clipboard handlers.
 */

int own_selection = 0;	/* whether we currently own PRIMARY or not */
int set_cutbuffer = 0;	/* to avoid bouncing the CutText right back */
int sel_waittime = 15;	/* some seconds to skip before first send */
Window selwin;		/* special window for our selection */

/*
 * This is where we keep our selection: the string sent TO us from VNC
 * clients, and the string sent BY us to requesting X11 clients.
 */
char *xcut_str = NULL;


void selection_request(XEvent *ev);
int check_sel_direction(char *dir, char *label, char *sel, int len);
void cutbuffer_send(void);
void selection_send(XEvent *ev);


/*
 * Our callbacks instruct us to check for changes in the cutbuffer
 * and PRIMARY selection on the local X11 display.
 *
 * We store the new cutbuffer and/or PRIMARY selection data in this
 * constant sized array selection_str[].
 * TODO: check if malloc does not cause performance issues (esp. WRT
 * SelectionNotify handling).
 */
static char selection_str[PROP_MAX+1];

/*
 * An X11 (not VNC) client on the local display has requested the selection
 * from us (because we are the current owner).
 *
 * n.b.: our caller already has the X_LOCK.
 */
void selection_request(XEvent *ev) {
	XSelectionEvent notify_event;
	XSelectionRequestEvent *req_event;
	XErrorHandler old_handler;
	unsigned int length;
	unsigned char *data;
#ifndef XA_LENGTH
	unsigned long XA_LENGTH = XInternAtom(dpy, "LENGTH", True);
#endif

	req_event = &(ev->xselectionrequest);
	notify_event.type 	= SelectionNotify;
	notify_event.display	= req_event->display;
	notify_event.requestor	= req_event->requestor;
	notify_event.selection	= req_event->selection;
	notify_event.target	= req_event->target;
	notify_event.time	= req_event->time;

	if (req_event->property == None) {
		notify_event.property = req_event->target;
	} else {
		notify_event.property = req_event->property;
	}
	if (xcut_str) {
		length = strlen(xcut_str);
	} else {
		length = 0;
	}

	/* the window may have gone away, so trap errors */
	trapped_xerror = 0;
	old_handler = XSetErrorHandler(trap_xerror);

	if (ev->xselectionrequest.target == XA_LENGTH) {
		/* length request */

		XChangeProperty(ev->xselectionrequest.display,
		    ev->xselectionrequest.requestor,
		    ev->xselectionrequest.property,
		    ev->xselectionrequest.target, 32, PropModeReplace,
		    (unsigned char *) &length, sizeof(unsigned int));

	} else {
		/* data request */

		data = (unsigned char *)xcut_str;

		XChangeProperty(ev->xselectionrequest.display,
		    ev->xselectionrequest.requestor,
		    ev->xselectionrequest.property,
		    ev->xselectionrequest.target, 8, PropModeReplace,
		    data, length);
	}

	if (! trapped_xerror) {
		XSendEvent(req_event->display, req_event->requestor, False, 0,
		    (XEvent *)&notify_event);
	} 
	if (trapped_xerror) {
		rfbLog("selection_request: ignored XError while sending "
		    "PRIMARY selection to 0x%x.\n", req_event->requestor);
	}
	XSetErrorHandler(old_handler);
	trapped_xerror = 0;

	XFlush(dpy);
}

int check_sel_direction(char *dir, char *label, char *sel, int len) {
	int db = 0, ok = 1;
	if (sel_direction) {
		if (strstr(sel_direction, "debug")) {
			db = 1;
		}
		if (strcmp(sel_direction, "debug")) {
			if (strstr(sel_direction, dir) == NULL) {
				ok = 0;
			}
		}
	}
	if (db) {
		char str[40];
		int n = 40;
		strncpy(str, sel, n);
		str[n-1] = '\0';
		if (len < n) {
			str[len] = '\0';
		}
		rfbLog("%s: %s...\n", label, str);
		if (ok) {
			rfbLog("%s: %s-ing it.\n", label, dir);
		} else {
			rfbLog("%s: NOT %s-ing it.\n", label, dir);
		}
	}
	return ok;
}

/*
 * CUT_BUFFER0 property on the local display has changed, we read and
 * store it and send it out to any connected VNC clients.
 *
 * n.b.: our caller already has the X_LOCK.
 */
void cutbuffer_send(void) {
	Atom type;
	int format, slen, dlen, len;
	unsigned long nitems = 0, bytes_after = 0;
	unsigned char* data = NULL;

	selection_str[0] = '\0';
	slen = 0;

	/* read the property value into selection_str: */
	do {
		if (XGetWindowProperty(dpy, DefaultRootWindow(dpy),
		    XA_CUT_BUFFER0, nitems/4, PROP_MAX/16, False,
		    AnyPropertyType, &type, &format, &nitems, &bytes_after,
		    &data) == Success) {

			dlen = nitems * (format/8);
			if (slen + dlen > PROP_MAX) {
				/* too big */
				rfbLog("warning: truncating large CUT_BUFFER0"
				   " selection > %d bytes.\n", PROP_MAX);
				XFree(data);
				break;
			}
			memcpy(selection_str+slen, data, dlen);
			slen += dlen;
			selection_str[slen] = '\0';
			XFree(data);
		}
	} while (bytes_after > 0);

	selection_str[PROP_MAX] = '\0';

	if (! all_clients_initialized()) {
		rfbLog("cutbuffer_send: no send: uninitialized clients\n");
		return; /* some clients initializing, cannot send */ 
	}
	if (unixpw_in_progress) {
		return;
	}

	/* now send it to any connected VNC clients (rfbServerCutText) */
	if (!screen) {
		return;
	}
	len = strlen(selection_str);
	if (check_sel_direction("send", "cutbuffer_send", selection_str, len)) {
		rfbSendServerCutText(screen, selection_str, len);
	}
}

/* 
 * "callback" for our SelectionNotify polling.  We try to determine if
 * the PRIMARY selection has changed (checking length and first CHKSZ bytes)
 * and if it has we store it and send it off to any connected VNC clients.
 *
 * n.b.: our caller already has the X_LOCK.
 *
 * TODO: if we were willing to use libXt, we could perhaps get selection
 * timestamps to speed up the checking... XtGetSelectionValue().
 *
 * Also: XFIXES has XFixesSelectSelectionInput().
 */
#define CHKSZ 32
void selection_send(XEvent *ev) {
	Atom type;
	int format, slen, dlen, oldlen, newlen, toobig = 0, len;
	static int err = 0, sent_one = 0;
	char before[CHKSZ], after[CHKSZ];
	unsigned long nitems = 0, bytes_after = 0;
	unsigned char* data = NULL;

	/*
	 * remember info about our last value of PRIMARY (or CUT_BUFFER0)
	 * so we can check for any changes below.
	 */
	oldlen = strlen(selection_str);
	strncpy(before, selection_str, CHKSZ);

	selection_str[0] = '\0';
	slen = 0;

	/* read in the current value of PRIMARY: */
	do {
		if (XGetWindowProperty(dpy, ev->xselection.requestor,
		    ev->xselection.property, nitems/4, PROP_MAX/16, True,
		    AnyPropertyType, &type, &format, &nitems, &bytes_after,
		    &data) == Success) {

			dlen = nitems * (format/8);
			if (slen + dlen > PROP_MAX) {
				/* too big */
				toobig = 1;
				XFree(data);
				if (err) {	/* cut down on messages */
					break;
				} else {
					err = 5;
				}
				rfbLog("warning: truncating large PRIMARY"
				   " selection > %d bytes.\n", PROP_MAX);
				break;
			}
			memcpy(selection_str+slen, data, dlen);
			slen += dlen;
			selection_str[slen] = '\0';
			XFree(data);
		}
	} while (bytes_after > 0);

	if (! toobig) {
		err = 0;
	} else if (err) {
		err--;
	}

	if (! sent_one) {
		/* try to force a send first time in */
		oldlen = -1;
		sent_one = 1;
	}

	/* look for changes in the new value */
	newlen = strlen(selection_str);
	strncpy(after, selection_str, CHKSZ);

	if (oldlen == newlen && strncmp(before, after, CHKSZ) == 0) {
		/* evidently no change */
		return;
	}
	if (newlen == 0) {
		/* do not bother sending a null string out */
		return;
	}

	if (! all_clients_initialized()) {
		rfbLog("selection_send: no send: uninitialized clients\n");
		return; /* some clients initializing, cannot send */ 
	}

	if (unixpw_in_progress) {
		return;
	}

	/* now send it to any connected VNC clients (rfbServerCutText) */
	if (!screen) {
		return;
	}

	len = newlen;
	if (check_sel_direction("send", "selection_send", selection_str, len)) {
		rfbSendServerCutText(screen, selection_str, len);
	}
}



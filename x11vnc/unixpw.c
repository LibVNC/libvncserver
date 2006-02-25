/* -- unixpw.c -- */

#ifdef __linux__
/* some conflict with _XOPEN_SOURCE */
extern int grantpt(int);
extern int unlockpt(int);
extern char *ptsname(int);
#endif

#include "x11vnc.h"
#include "scan.h"
#include "cleanup.h"
#include "xinerama.h"
#include <rfb/default8x16.h>

/* much to do for it to work on *BSD ... */

#if LIBVNCSERVER_HAVE_FORK
#if LIBVNCSERVER_HAVE_SETSID
#if LIBVNCSERVER_HAVE_SYS_WAIT_H
#if LIBVNCSERVER_HAVE_PWD_H
#if LIBVNCSERVER_HAVE_SETUID
#if LIBVNCSERVER_HAVE_WAITPID
#if LIBVNCSERVER_HAVE_TERMIOS_H
#if LIBVNCSERVER_HAVE_SYS_IOCTL_H
#if LIBVNCSERVER_HAVE_GRANTPT
#define UNIXPW
#include <sys/ioctl.h>
#include <termios.h>
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

void unixpw_screen(int init);
void unixpw_keystroke(rfbBool down, rfbKeySym keysym, int init);
void unixpw_accept(void);
void unixpw_deny(void);

static int white(void);
static int text_x(void);
static int text_y(void);
static int su_verify(char *user, char *pass);
static void unixpw_verify(char *user, char *pass);

int unixpw_in_progress = 0;
time_t unixpw_last_try_time = 0;
rfbClientPtr unixpw_client = NULL;

static int in_login = 0, in_passwd = 0, tries = 0;
static int char_row = 0, char_col = 0;
static int char_x = 0, char_y = 0, char_w = 8, char_h = 16;

static int white(void) {
	static unsigned long black_pix = 0, white_pix = 1, set = 0;

	if (depth <= 8 && ! set) {
		X_LOCK;
		black_pix = BlackPixel(dpy, scr);
		white_pix = WhitePixel(dpy, scr);
		X_UNLOCK;
		set = 1;
	}
	if (depth <= 8) {
		return (int) white_pix;
	} else if (depth < 24) {
		return 0xffff;
	} else {
		return 0xffffff;
	}
}

static int text_x(void) {
	return char_x + char_col * char_w;
}

static int text_y(void) {
	return char_y + char_row * char_h;
}

void unixpw_screen(int init) {
#ifndef UNIXPW
	rfbLog("-unixpw is not supported on this OS/machine\n");
	clean_up_exit(1);
#endif
	if (init) {
		int x, y;
		char log[] = "login: ";

		zero_fb(0, 0, dpy_x, dpy_y);

		x = nfix(dpy_x / 2 -  strlen(log) * char_w, dpy_x);
		y = dpy_y / 4;

		rfbDrawString(screen, &default8x16Font, x, y, log, white());

		char_x = x;
		char_y = y;
		char_col = strlen(log);
		char_row = 0;
	}

	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);
}

static int su_verify(char *user, char *pass) {
#ifdef UNIXPW
	int status, fd, sfd;
	char *slave;
	pid_t pid, pidw;
	struct stat sbuf;

	if (unixpw_list) {
		char *p, *str = strdup(unixpw_list);
		int ok = 0;

		p = strtok(str, ",");
		while (p) {
			if (!strcmp(user, p)) {
				ok = 1;
				break;
			}
			p = strtok(NULL, ",");
		}
		free(str);
		if (! ok) {
			return 0;
		}
	}
	if (stat("/bin/su", &sbuf) != 0) {
		rfbLogPerror("existence /bin/su");
		return 0;
	}
	if (stat("/bin/true", &sbuf) != 0) {
		rfbLogPerror("existence /bin/true");
		return 0;
	}
	
	fd = open("/dev/ptmx", O_RDWR|O_NOCTTY);

	if (fd < 0) {
		rfbLogPerror("open /dev/ptmx");
		return 0;
	}

	if (grantpt(fd) != 0) {
		rfbLogPerror("grantpt");
		close(fd);
		return 0;
	}
	if (unlockpt(fd) != 0) {
		rfbLogPerror("unlockpt");
		close(fd);
		return 0;
	}

	slave = ptsname(fd);
	if (! slave)  {
		rfbLogPerror("ptsname");
		close(fd);
		return 0;
	}

	pid = fork();
	if (pid < 0) {
		rfbLogPerror("fork");
		close(fd);
		return 0;
	}

	if (pid == 0) {
		int ttyfd;
		struct passwd *pw;

		close(fd);

		pw = getpwnam("nobody");

		if (pw) {
			setuid(pw->pw_uid);
#if LIBVNCSERVER_HAVE_SETEUID
			seteuid(pw->pw_uid);
#endif
			setgid(pw->pw_gid);
#if LIBVNCSERVER_HAVE_SETEGID
			setegid(pw->pw_gid);
#endif
		}

		if (getuid() == 0 || geteuid() == 0) {
			fprintf(stderr, "could not switch to user nobody.\n");
			exit(1);
		}

		if (setsid() == -1) {
			perror("setsid");
			exit(1);
		}

#ifdef TIOCNOTTY
		ttyfd = open("/dev/tty", O_RDWR);
		if (ttyfd >= 0) {
			(void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
			close(ttyfd);
		}
#endif

		close(0);
		close(1);
		close(2);

		sfd = open(slave, O_RDWR);
		if (sfd < 0) {
			fprintf(stderr, "failed: %s\n", slave); 
			perror("open");
			exit(1);
		}
#ifdef TIOCSCTTY
		if (ioctl(sfd, TIOCSCTTY, (char *) 0) != 0) {
			perror("ioctl");
			exit(1);
		}
#endif
		execlp("/bin/su", "/bin/su", user, "-c", "/bin/true",
		    (char *) NULL);
		exit(1);
	}

	usleep( 500 * 1000 );
	write(fd, pass, strlen(pass)); 

	pidw = waitpid(pid, &status, 0); 
	close(fd);

	if (pid != pidw) {
		return 0;
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		return 1;
	} else {
		return 0;
	}
#else
	return 0;
#endif
}

static int db = 0;

static void unixpw_verify(char *user, char *pass) {
	int x, y;
	char li[] = "Login incorrect";
	char log[] = "login: ";

if (db) fprintf(stderr, "unixpw_verify: '%s' '%s'\n", user, db > 1 ? pass : "********");

	if (su_verify(user, pass)) {
		unixpw_accept();
		return;
	}

	if (tries < 2) {
		char_row++;
		char_col = 0;

		x = text_x();
		y = text_y();
		rfbDrawString(screen, &default8x16Font, x, y, li, white());

		char_row += 2;

		x = text_x();
		y = text_y();
		rfbDrawString(screen, &default8x16Font, x, y, log, white());

		char_col = strlen(log);

		mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);

		unixpw_last_try_time = time(0);
		unixpw_keystroke(0, 0, 2);
		tries++;
	} else {
		unixpw_deny();
	}
}

void unixpw_keystroke(rfbBool down, rfbKeySym keysym, int init) {
	int x, y, i, nmax = 100;
	static char user[100], pass[100];
	static int  u_cnt = 0, p_cnt = 0, first = 1;
	char str[100];

	if (first) {
		if (getenv("DEBUG_UNIXPW")) {
			db = atoi(getenv("DEBUG_UNIXPW"));
		}
		first = 0;
	}

	if (init) {
		in_login = 1;
		in_passwd = 0;
		if (init == 1) {
			tries = 0;
		}

		u_cnt = 0;
		p_cnt = 0;
		for (i=0; i<nmax; i++) {
			user[i] = '\0';
			pass[i] = '\0';
		}
		return;
	}

	if (down) {
		return;
	}

	if (in_login) {
		if (keysym == XK_BackSpace || keysym == XK_Delete) {
			if (u_cnt > 0) {
				user[u_cnt-1] = '\0';
				x = text_x();
				y = text_y();
				zero_fb(x - char_w, y - char_h, x, y);
				mark_rect_as_modified(x - char_w, y - char_h,
				    x, y, 0);
				char_col--;
				u_cnt--;
			}
			return;
		}
		if (keysym == XK_Return || keysym == XK_Linefeed) {
			char pw[] = "Password: ";

			in_login = 0;
			in_passwd = 1;

			char_row++;
			char_col = 0;

			x = text_x();
			y = text_y();
			rfbDrawString(screen, &default8x16Font, x, y, pw,
			    white());

			char_col = strlen(pw);
			mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);
			return;
		}
		if (keysym <= ' ' || keysym >= 0x7f) {
			return;
		}
		if (u_cnt >= nmax - 1) {
			rfbLog("unixpw_deny: username too long\n");
			unixpw_deny();
			return;
		}

		X_LOCK;
		sprintf(str, "%s", XKeysymToString(keysym));
		X_UNLOCK;

		user[u_cnt++] = str[0];

		x = text_x();
		y = text_y();

if (db) fprintf(stderr, "u_cnt: %d %d/%d ks: 0x%x  %s\n", u_cnt, x, y, keysym, str);

		str[1] = '\0';
		rfbDrawString(screen, &default8x16Font, x, y, str, white());

		mark_rect_as_modified(x, y-char_h, x+char_w, y, 0);
		char_col++;

	} else if (in_passwd) {
		if (keysym == XK_BackSpace || keysym == XK_Delete) {
			if (p_cnt > 0) {
				pass[p_cnt-1] = '\0';
				p_cnt--;
			}
			return;
		}
		if (keysym == XK_Return || keysym == XK_Linefeed) {
			in_login = 0;
			in_passwd = 0;
			pass[p_cnt++] = '\n';
			unixpw_verify(user, pass);
			return;
		}
		if (keysym <= ' ' || keysym >= 0x7f) {
			return;
		}
		if (p_cnt >= nmax - 2) {
			rfbLog("unixpw_deny: password too long\n");
			unixpw_deny();
			return;
		}
		pass[p_cnt++] = (char) keysym;
	}
}

void unixpw_accept(void) {
	unixpw_in_progress = 0;
	unixpw_client = NULL;
	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);
}

void unixpw_deny(void) {
	int x, y, i;
	char pd[] = "Permission denied.";

	char_row += 2;
	char_col = 0;
	x = char_x + char_col * char_w;
	y = char_y + char_row * char_h;

	rfbDrawString(screen, &default8x16Font, x, y, pd, white());
	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);

	for (i=0; i<5; i++) {
		rfbPE(-1);
		usleep(500 * 1000);
	}

	rfbCloseClient(unixpw_client);
	rfbClientConnectionGone(unixpw_client);
	rfbPE(-1);

	unixpw_in_progress = 0;
	unixpw_client = NULL;
	copy_screen();
}


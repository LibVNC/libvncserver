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

#if LIBVNCSERVER_HAVE_FORK
#if LIBVNCSERVER_HAVE_SYS_WAIT_H
#if LIBVNCSERVER_HAVE_WAITPID
#define UNIXPW
#endif
#endif
#endif

#if LIBVNCSERVER_HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if LIBVNCSERVER_HAVE_TERMIOS_H
#include <termios.h>
#endif
#if 0
#include <sys/stropts.h>
#endif
#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
#define IS_BSD
#endif

void unixpw_screen(int init);
void unixpw_keystroke(rfbBool down, rfbKeySym keysym, int init);
void unixpw_accept(char *user);
void unixpw_deny(void);
int su_verify(char *user, char *pass);

static int white(void);
static int text_x(void);
static int text_y(void);
static void set_db(void);
static void unixpw_verify(char *user, char *pass);

int unixpw_in_progress = 0;
time_t unixpw_last_try_time = 0;
rfbClientPtr unixpw_client = NULL;

static int in_login = 0, in_passwd = 0, tries = 0;
static int char_row = 0, char_col = 0;
static int char_x = 0, char_y = 0, char_w = 8, char_h = 16;

static int db = 0;

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

	
#ifdef MAXPATHLEN
static char slave_str[MAXPATHLEN];
#else
static char slave_str[4096];
#endif

char *get_pty_ptmx(int *fd_p) {
	char *slave;
	int fd = -1, i, ndevs = 4, tmp;
	char *devs[] = { 
		"/dev/ptmx",
		"/dev/ptm/clone",
		"/dev/ptc",
		"/dev/ptmx_bsd"
	};

	*fd_p = -1;

#if LIBVNCSERVER_HAVE_GRANTPT

	for (i=0; i < ndevs; i++) {

#ifdef O_NOCTTY
		fd = open(devs[i], O_RDWR|O_NOCTTY);
#else
		fd = open(devs[i], O_RDWR);
#endif
		if (fd >= 0) {
			break;
		}
	}

	if (fd < 0) {
		rfbLogPerror("open /dev/ptmx");
		return NULL;
	}

#if 0
#if defined(FIONBIO) 
	tmp = 1;
	ioctl(fd, FIONBIO, &tmp);
#endif
#endif

#if LIBVNCSERVER_HAVE_SYS_IOCTL_H && defined(TIOCPKT)
	tmp = 0;
	ioctl(fd, TIOCPKT, (char *) &tmp);
#endif

	if (grantpt(fd) != 0) {
		rfbLogPerror("grantpt");
		close(fd);
		return NULL;
	}
	if (unlockpt(fd) != 0) {
		rfbLogPerror("unlockpt");
		close(fd);
		return NULL;
	}

	slave = ptsname(fd);
	if (! slave)  {
		rfbLogPerror("ptsname");
		close(fd);
		return NULL;
	}

#if LIBVNCSERVER_HAVE_SYS_IOCTL_H && defined(TIOCFLUSH)
	ioctl(fd, TIOCFLUSH, (char *) 0);
#endif



	strcpy(slave_str, slave);
	*fd_p = fd;
	return slave_str;

#else
	return NULL;

#endif /* GRANTPT */
}


char *get_pty_loop(int *fd_p) {
	char *slave;
	char master_str[16];
	int fd = -1, i;
	char c;

	*fd_p = -1;

	/* for *BSD loop over /dev/ptyXY */

	for (c = 'p'; c <= 'z'; c++) {
		for (i=0; i < 16; i++) {
			sprintf(master_str, "/dev/pty%c%x", c, i);
#ifdef O_NOCTTY
			fd = open(master_str, O_RDWR|O_NOCTTY);
#else
			fd = open(master_str, O_RDWR);
#endif
			if (fd >= 0) {
				break;
			}
		}
		if (fd >= 0) {
			break;
		}
	}
	if (fd < 0) {
		return NULL;
	}

#if LIBVNCSERVER_HAVE_SYS_IOCTL_H && defined(TIOCFLUSH)
	ioctl(fd, TIOCFLUSH, (char *) 0);
#endif

	sprintf(slave_str, "/dev/tty%c%x", c, i);
	*fd_p = fd;
	return slave_str;
}

char *get_pty(int *fd_p) {
	if (getenv("BSD_PTY")) {
		return get_pty_loop(fd_p);
	}
#ifdef IS_BSD
	return get_pty_loop(fd_p);
#else
#if LIBVNCSERVER_HAVE_GRANTPT
	return get_pty_ptmx(fd_p);
#else
	return get_pty_loop(fd_p);
#endif
#endif
}

void try_to_be_nobody(void) {

#if LIBVNCSERVER_HAVE_PWD_H
	struct passwd *pw;
	pw = getpwnam("nobody");

	if (pw) {
#if LIBVNCSERVER_HAVE_SETUID
		setuid(pw->pw_uid);
#endif
#if LIBVNCSERVER_HAVE_SETEUID
		seteuid(pw->pw_uid);
#endif
#if LIBVNCSERVER_HAVE_SETGID
		setgid(pw->pw_gid);
#endif
#if LIBVNCSERVER_HAVE_SETEGID
		setegid(pw->pw_gid);
#endif
	}

#endif	/* PWD_H */
}


static int slave_fd = -1;
static void close_alarm (int sig) {
	if (slave_fd >= 0) {
		close(slave_fd);
	}
}

int su_verify(char *user, char *pass) {
#ifndef UNIXPW
	return 0;
#else
	int i, j, status, fd = -1, sfd, tfd;
	char *slave, *bin_true = NULL, *bin_su = NULL;
	pid_t pid, pidw;
	struct stat sbuf;
	static int first = 1;
	char instr[16];

	if (first) {
		set_db();
		first = 0;
	}

	if (unixpw_list) {
		char *p, *q, *str = strdup(unixpw_list);
		int ok = 0;

		p = strtok(str, ",");
		while (p) {
			if ( (q = strchr(p, ':')) != NULL ) {
				*q = '\0';	/* get rid of options. */
			}
			if (!strcmp(user, p) || !strcmp("*", p)) {
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

	if (stat("/bin/su", &sbuf) == 0) {
		bin_su = "/bin/su";
	} else if (stat("/usr/bin/su", &sbuf) == 0) {
		bin_su = "/usr/bin/su";
	}
	if (bin_su == NULL) {
		rfbLogPerror("existence /bin/su");
		return 0;
	}

	if (stat("/bin/true", &sbuf) == 0) {
		bin_true = "/bin/true";
	} if (stat("/usr/bin/true", &sbuf) == 0) {
		bin_true = "/usr/bin/true";
	}
	if (bin_true == NULL) {
		rfbLogPerror("existence /bin/true");
		return 0;
	}

	slave = get_pty(&fd);
	if (slave == NULL) {
		rfbLogPerror("get_pty failed.");
		return 0;
	}
if (db) fprintf(stderr, "slave is: %s fd=%d\n", slave, fd);

	if (fd < 0) {
		rfbLogPerror("get_pty fd < 0");
		return 0;
	}

	fcntl(fd, F_SETFD, 1);

	pid = fork();
	if (pid < 0) {
		rfbLogPerror("fork");
		close(fd);
		return 0;
	}

	if (pid == 0) {
		int ttyfd;
		char tmp[256];

#if LIBVNCSERVER_HAVE_SETSID
		if (setsid() == -1) {
			perror("setsid");
			exit(1);
		}
#else
		if (setpgrp() == -1) {
			perror("setpgrp");
			exit(1);
		}

#if LIBVNCSERVER_HAVE_SYS_IOCTL_H && defined(TIOCNOTTY)
		ttyfd = open("/dev/tty", O_RDWR);
		if (ttyfd >= 0) {
			(void) ioctl(ttyfd, TIOCNOTTY, (char *) 0);
			close(ttyfd);
		}
#endif	

#endif	/* SETSID */

		close(0);
		close(1);
		close(2);

		sfd = open(slave, O_RDWR);
		if (sfd < 0) {
			exit(1);
		}
		/* sfd should be 0 since we closed 0. */

#ifdef F_SETFL
		fcntl (sfd, F_SETFL, O_NONBLOCK);
#endif
		if (fcntl(sfd, F_DUPFD, 1) == -1) {
			exit(1);
		}
		if (fcntl(sfd, F_DUPFD, 2) == -1) {
			exit(1);
		}

		unlink("/tmp/isatty");
		unlink("/tmp/isastream");
#if LIBVNCSERVER_HAVE_SYS_IOCTL_H
#if 0
	if (isastream(sfd)) {
tfd = open("/tmp/isastream", O_CREAT|O_WRONLY, 0600);
close(tfd);
		ioctl(sfd, I_PUSH, "ptem");
		ioctl(sfd, I_PUSH, "ldterm");
		ioctl(sfd, I_PUSH, "ttcompat");
	}
#endif
#if 1
#if defined(TIOCSCTTY) && !defined(sun) && !defined(hpux)
		ioctl(sfd, TIOCSCTTY, (char *) 0);
#endif
#endif
		if (isatty(sfd)) {
			char nam[256];
tfd = open("/tmp/isatty", O_CREAT|O_WRONLY, 0600);
close(tfd);
			sprintf(nam, "stty -a < %s > /tmp/isatty 2>&1", slave);
			system(nam);
		}

#endif	/* SYS_IOCTL_H */

		chdir("/");

		try_to_be_nobody();
#if LIBVNCSERVER_HAVE_GETUID
		if (getuid() == 0 || geteuid() == 0) {
			exit(1);
		}
#else
		exit(1);
#endif

		set_env("LC_ALL", "C");
		set_env("LANG", "C");
		set_env("SHELL", "/bin/sh");

		execlp(bin_su, bin_su, user, "-c", bin_true, (char *) NULL);
		exit(1);
	}

	if (db)	fprintf(stderr, "pid: %d\n", pid);
	if (db > 3) {
		char cmd[32];
		usleep( 100 * 1000 );
		sprintf(cmd, "ps wu %d", pid);
		system(cmd);
		sprintf(cmd, "stty -a < %s", slave);
		system(cmd);
	}

	usleep( 500 * 1000 );

	/* send the password "early" (i.e. before we drain) */
if (0) {
	int k;
	for (k = 0; k < strlen(pass); k++) {
		write(fd, pass+k, 1); 
		usleep(100 * 1000);
	}
} else {
	write(fd, pass, strlen(pass)); 
}

	/*
	 * set an alarm for blocking read() to close the master
	 * (presumably terminating the child.  we avoid SIGTERM for now)
	 */
	slave_fd = fd;
	signal(SIGALRM, close_alarm);
	alarm(10);

	/*
	 * In addition to checking exit code below, we watch for the
	 * appearance of the string "Password:".  BSD does not seem to
	 * ask for a password trying to su to yourself.
	 */
	for (i=0; i<16; i++) {
		instr[i] = '\0';
	}
	j = 0;
	for (i=0; i < strlen("Password:"); i++) {
		char pstr[] = "password:";
		char buf[2];
		int n;	

		buf[0] = '\0';
		buf[1] = '\0';

		n = read(fd, buf, 1);

if (db == 1) fprintf(stderr, "%d ", n, db > 1 ? buf : "");
if (db > 1)  fprintf(stderr, "%s", buf);

		if (db > 3 && n == 1 && buf[0] == ':') {
			char cmd[32];
			usleep( 100 * 1000 );
			sprintf(cmd, "ps wu %d", pid);
			system(cmd);
			sprintf(cmd, "stty -a < %s", slave);
			system(cmd);
		}

		if (n == 1) {
			if (isspace(buf[0])) {
				continue;
			}
			instr[j++] = tolower(buf[0]);
		}
		if (n <= 0 || strstr(pstr, instr) != pstr) {
			rfbLog("\"Password:\" did not appear: '%s' n=%d\n",
			    instr, n);
			if (db > 3 && n == 1) {
				continue;
			}
			alarm(0);
			signal(SIGALRM, SIG_DFL);
			slave_fd = -1;
			close(fd);
			kill(pid, SIGTERM);
			waitpid(pid, &status, WNOHANG); 
			return 0;
		}
	}
	alarm(0);
	signal(SIGALRM, SIG_DFL);

	usleep( 250 * 1000 );

#if 0
	tcdrain(fd);
#endif

	signal(SIGALRM, close_alarm);
	alarm(15);

	/*
	 * try to drain the output, hopefully never as much as 4096 (motd?)
	 * if we don't drain we may block at waitpid.  If we close(fd), the
	 * make cause child to die by signal.
	 */
	for (i = 0; i<4096; i++) {
		char buf[2];
		int n;	
		
		buf[0] = '\0';
		buf[1] = '\0';

		n = read(fd, buf, 1);

if (db == 1) fprintf(stderr, "%d ", n, db > 1 ? buf : "");
if (db > 1)  fprintf(stderr, "%s", buf);

		if (n <= 0) {
			break;
		}
	}

if (db) fprintf(stderr, "\n");

	alarm(0);
	signal(SIGALRM, SIG_DFL);
	slave_fd = -1;
	
	pidw = waitpid(pid, &status, 0); 
	close(fd);

	if (pid != pidw) {
		return 0;
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		return 1; /* this is the only return of success. */
	} else {
		return 0;
	}
#endif	/* UNIXPW */
}

static void unixpw_verify(char *user, char *pass) {
	int x, y;
	char li[] = "Login incorrect";
	char log[] = "login: ";

if (db) fprintf(stderr, "unixpw_verify: '%s' '%s'\n", user, db > 1 ? pass : "********");
	rfbLog("unixpw_verify: %s\n", user);

	if (su_verify(user, pass)) {
		unixpw_accept(user);
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

static void set_db(void) {
	if (getenv("DEBUG_UNIXPW")) {
		db = atoi(getenv("DEBUG_UNIXPW"));
	}
}

void unixpw_keystroke(rfbBool down, rfbKeySym keysym, int init) {
	int x, y, i, nmax = 100;
	static char user[100], pass[100];
	static int  u_cnt = 0, p_cnt = 0, first = 1;
	char keystr[100];

	if (first) {
		set_db();
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

	X_LOCK;
	sprintf(keystr, "%s", XKeysymToString(keysym));
	X_UNLOCK;

	if (db > 2) {
		fprintf(stderr, "%s / %s  0x%x %s\n", in_login ? "login":"pass ",
		    down ? "down":"up  ", keysym, keystr);
	}

	if (keysym == XK_Return || keysym == XK_Linefeed) {
		;	/* let "up" pass down below for Return case */
	} else if (! down) {
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

			if (down) {
				/*
				 * require Up so the Return Up is not processed
				 * by the normal session after login.
				 */
				return;
			}

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
			for (i=0; i<nmax; i++) {
				user[i] = '\0';
				pass[i] = '\0';
			}
			unixpw_deny();
			return;
		}

		user[u_cnt++] = keystr[0];

		x = text_x();
		y = text_y();

if (db && db <= 2) fprintf(stderr, "u_cnt: %d %d/%d ks: 0x%x  %s\n", u_cnt, x, y, keysym, keystr);

		keystr[1] = '\0';
		rfbDrawString(screen, &default8x16Font, x, y, keystr, white());

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
			if (down) {
				/*
				 * require Up so the Return Up is not processed
				 * by the normal session after login.
				 */
				return;
			}
			in_login = 0;
			in_passwd = 0;
			pass[p_cnt++] = '\n';
			unixpw_verify(user, pass);
			for (i=0; i<nmax; i++) {
				user[i] = '\0';
				pass[i] = '\0';
			}
			return;
		}
		if (keysym <= ' ' || keysym >= 0x7f) {
			return;
		}
		if (p_cnt >= nmax - 2) {
			rfbLog("unixpw_deny: password too long\n");
			for (i=0; i<nmax; i++) {
				user[i] = '\0';
				pass[i] = '\0';
			}
			unixpw_deny();
			return;
		}
		pass[p_cnt++] = (char) keysym;
	} else {
		/* should not happen... clean up a bit. */
		u_cnt = 0;
		p_cnt = 0;
		for (i=0; i<nmax; i++) {
			user[i] = '\0';
			pass[i] = '\0';
		}
	}
}

static void apply_opts (char *user) {
	char *p, *q, *str, *opts = NULL, *opts_star = NULL;
	ClientData *cd = (ClientData *) unixpw_client->clientData;
	rfbClientPtr cl = unixpw_client;
	int i;
	
	if (! unixpw_list) {
		return;
	}
	str = strdup(unixpw_list);

	/* apply any per-user options. */
	p = strtok(str, ",");
	while (p) {
		if ( (q = strchr(p, ':')) != NULL ) {
			*q = '\0';	/* get rid of options. */
		} else {
			p = strtok(NULL, ",");
			continue;
		}
		if (!strcmp(user, p)) {
			opts = strdup(q+1);
		}
		if (!strcmp("*", p)) {
			opts_star = strdup(q+1);
		}
		p = strtok(NULL, ",");
	}
	free(str);

	for (i=0; i < 2; i++) {
		char *s = (i == 0) ? opts_star : opts;
		if (s == NULL) {
			continue;
		}
		p = strtok(s, "+");
		while (p) {
			if (!strcmp(p, "viewonly")) {
				cl->viewOnly = TRUE;
				strncpy(cd->input, "-", CILEN);
			} else if (!strcmp(p, "fullaccess")) {
				cl->viewOnly = FALSE;
				strncpy(cd->input, "-", CILEN);
			} else if ((q = strstr(p, "input=")) == p) {
				q += strlen("input=");
				strncpy(cd->input, q, CILEN);
			} else if (!strcmp(p, "deny")) {
				cl->viewOnly = TRUE;
				unixpw_deny();
				break;
			}
			p = strtok(NULL, "+");
		}
		free(s);
	}
}

void unixpw_accept(char *user) {

	apply_opts(user);

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


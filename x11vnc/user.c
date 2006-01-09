/* -- user.c -- */

#include "x11vnc.h"
#include "solid.h"
#include "cleanup.h"
#include "scan.h"
#include "screen.h"

void check_switched_user(void);
void lurk_loop(char *str);
int switch_user(char *user, int fb_mode);
int read_passwds(char *passfile);
void install_passwds(void);
void check_new_passwds(void);


static void switch_user_task_dummy(void);
static void switch_user_task_solid_bg(void);
static char *get_login_list(int with_display);
static char **user_list(char *user_str);
static void user2uid(char *user, uid_t *uid, char **name, char **home);
static int lurk(char **users);
static int guess_user_and_switch(char *str, int fb_mode);
static int try_user_and_display(uid_t uid, char *dpystr);
static int switch_user_env(uid_t uid, char *name, char *home, int fb_mode);
static void try_to_switch_users(void);


/* tasks for after we switch */
static void switch_user_task_dummy(void) {
	;	/* dummy does nothing */
}
static void switch_user_task_solid_bg(void) {
	/* we have switched users, some things to do. */
	if (use_solid_bg && client_count) {
		solid_bg(0);
	}
}

void check_switched_user(void) {
	static time_t sched_switched_user = 0;
	static int did_solid = 0;
	static int did_dummy = 0;
	int delay = 15;
	time_t now = time(0);

	if (started_as_root == 1 && users_list) {
		try_to_switch_users();
		if (started_as_root == 2) {
			/*
			 * schedule the switch_user_tasks() call
			 * 15 secs is for piggy desktops to start up.
			 * might not be enough for slow machines...
			 */
			sched_switched_user = now;
			did_dummy = 0;
			did_solid = 0;
			/* add other activities */
		}
	}
	if (! sched_switched_user) {
		return;
	}

	if (! did_dummy) {
		switch_user_task_dummy();
		did_dummy = 1;
	}
	if (! did_solid) {
		int doit = 0;
		char *ss = solid_str;
		if (now >= sched_switched_user + delay) {
			doit = 1;
		} else if (ss && strstr(ss, "root:") == ss) {
		    	if (now >= sched_switched_user + 3) {
				doit = 1;
			}
		} else if (strcmp("root", guess_desktop())) {
			usleep(1000 * 1000);
			doit = 1;
		}
		if (doit) {
			switch_user_task_solid_bg();
			did_solid = 1;
		}
	}

	if (did_dummy && did_solid) {
		sched_switched_user = 0;
	}
}

/* utilities for switching users */
static char *get_login_list(int with_display) {
	char *out;
#if LIBVNCSERVER_HAVE_UTMPX_H
	int i, cnt, max = 200, ut_namesize = 32;
	int dpymax = 1000, sawdpy[1000];
	struct utmpx *utx;

	/* size based on "username:999," * max */
	out = (char *) malloc(max * (ut_namesize+1+3+1) + 1);
	out[0] = '\0';

	for (i=0; i<dpymax; i++) {
		sawdpy[i] = 0;
	}

	setutxent();
	cnt = 0;
	while (1) {
		char *user, *line, *host, *id;
		char tmp[10];
		int d = -1;
		utx = getutxent();
		if (! utx) {
			break;
		}
		if (utx->ut_type != USER_PROCESS) {
			continue;
		}
		user = lblanks(utx->ut_user);
		if (*user == '\0') {
			continue;
		}
		if (strchr(user, ',')) {
			continue;	/* unlikely, but comma is our sep. */
		}

		line = lblanks(utx->ut_line);
		host = lblanks(utx->ut_host);
		id   = lblanks(utx->ut_id);

		if (with_display) {
			if (0 && line[0] != ':' && strcmp(line, "dtlocal")) {
				/* XXX useful? */
				continue;
			}

			if (line[0] == ':') {
				if (sscanf(line, ":%d", &d) != 1)  {
					d = -1;
				}
			}
			if (d < 0 && host[0] == ':') {
				if (sscanf(host, ":%d", &d) != 1)  {
					d = -1;
				}
			}
			if (d < 0 && id[0] == ':') {
				if (sscanf(id, ":%d", &d) != 1)  {
					d = -1;
				}
			}

			if (d < 0 || d >= dpymax || sawdpy[d]) {
				continue;
			}
			sawdpy[d] = 1;
			sprintf(tmp, ":%d", d);
		} else {
			/* try to eliminate repeats */
			int repeat = 0;
			char *q;

			q = out;
			while ((q = strstr(q, user)) != NULL) {
				char *p = q + strlen(user) + strlen(":DPY");
				if (q == out || *(q-1) == ',') {
					/* bounded on left. */
					if (*p == ',' || *p == '\0') {
						/* bounded on right. */
						repeat = 1;
						break;
					}
				}
				q = p;
			}
			if (repeat) {
				continue;
			}
			sprintf(tmp, ":DPY");
		}

		if (*out) {
			strcat(out, ",");
		}
		strcat(out, user);
		strcat(out, tmp);

		cnt++;
		if (cnt >= max) {
			break;
		}
	}
	endutxent();
#else
	out = strdup("");
#endif
	return out;
}

static char **user_list(char *user_str) {
	int n, i;
	char *p, **list;
	
	p = user_str;
	n = 1;
	while (*p++) {
		if (*p == ',') {
			n++;
		}
	}
	list = (char **) malloc((n+1)*sizeof(char *));

	p = strtok(user_str, ",");
	i = 0;
	while (p) {
		list[i++] = p;
		p = strtok(NULL, ",");
	}
	list[i] = NULL;
	return list;
}

static void user2uid(char *user, uid_t *uid, char **name, char **home) {
	int numerical = 1;
	char *q;

	*uid = (uid_t) -1;
	*name = NULL;
	*home = NULL;

	q = user;
	while (*q) {
		if (! isdigit(*q++)) {
			numerical = 0;
			break;
		}
	}

	if (numerical) {
		int u = atoi(user);

		if (u < 0) {
			return;
		}
		*uid = (uid_t) u;
	}

#if LIBVNCSERVER_HAVE_PWD_H
	if (1) {
		struct passwd *pw;
		if (numerical) {
			pw = getpwuid(*uid);
		} else {
			pw = getpwnam(user);
		}
		if (pw) {
			*uid  = pw->pw_uid;
			*name = pw->pw_name;	/* n.b. use immediately */
			*home = pw->pw_dir;
		}
	}
#endif
}


static int lurk(char **users) {
	uid_t uid;
	int success = 0, dmin = -1, dmax = -1;
	char *p, *logins, **u;

	if ((u = users) != NULL && *u != NULL && *(*u) == ':') {
		int len;
		char *tmp;

		/* extract min and max display numbers */
		tmp = *u;
		if (strchr(tmp, '-')) {
			if (sscanf(tmp, ":%d-%d", &dmin, &dmax) != 2) {
				dmin = -1;
				dmax = -1;
			}
		}
		if (dmin < 0) {
			if (sscanf(tmp, ":%d", &dmin) != 1) {
				dmin = -1;
				dmax = -1;
			} else {
				dmax = dmin;
			}
		}
		if ((dmin < 0 || dmax < 0) || dmin > dmax || dmax > 10000) {
			dmin = -1;
			dmax = -1;
		}

		/* get user logins regardless of having a display: */
		logins = get_login_list(0);

		/*
		 * now we append the list in users (might as well try
		 * them) this will probably allow weird ways of starting
		 * xservers to work.
		 */
		len = strlen(logins);
		u++;
		while (*u != NULL) {
			len += strlen(*u) + strlen(":DPY,");
			u++;
		}
		tmp = (char *) malloc(len+1);
		strcpy(tmp, logins);

		/* now concatenate them: */
		u = users+1;
		while (*u != NULL) {
			char *q, chk[100];
			snprintf(chk, 100, "%s:DPY", *u);
			q = strstr(tmp, chk);
			if (q) {
				char *p = q + strlen(chk);
				
				if (q == tmp || *(q-1) == ',') {
					/* bounded on left. */
					if (*p == ',' || *p == '\0') {
						/* bounded on right. */
						u++;
						continue;
					}
				}
			}
			
			if (*tmp) {
				strcat(tmp, ",");
			}
			strcat(tmp, *u);
			strcat(tmp, ":DPY");
			u++;
		}
		free(logins);
		logins = tmp;
		
	} else {
		logins = get_login_list(1);
	}
	
	p = strtok(logins, ",");
	while (p) {
		char *user, *name, *home, dpystr[10];
		char *q, *t;
		int ok = 1, dn;
		
		t = strdup(p);	/* bob:0 */
		q = strchr(t, ':'); 
		if (! q) {
			free(t);
			break;
		}
		*q = '\0';
		user = t;
		snprintf(dpystr, 10, ":%s", q+1);

		if (users) {
			u = users;
			ok = 0;
			while (*u != NULL) {
				if (*(*u) == ':') {
					u++;
					continue;
				}
				if (!strcmp(user, *u++)) {
					ok = 1;
					break;
				}
			}
		}

		user2uid(user, &uid, &name, &home);
		free(t);

		if (! uid) {
			ok = 0;
		}

		if (! ok) {
			p = strtok(NULL, ",");
			continue;
		}
		
		for (dn = dmin; dn <= dmax; dn++) {
			if (dn >= 0) {
				sprintf(dpystr, ":%d", dn);
			}
			if (try_user_and_display(uid, dpystr)) {
				if (switch_user_env(uid, name, home, 0)) {
					rfbLog("lurk: now user: %s @ %s\n",
					    name, dpystr);
					started_as_root = 2;
					success = 1;
				}
				set_env("DISPLAY", dpystr);
				break;
			}
		}
		if (success) {
			 break;
		}

		p = strtok(NULL, ",");
	}
	free(logins);
	return success;
}

void lurk_loop(char *str) {
	char *tstr = NULL, **users = NULL;

	if (strstr(str, "lurk=") != str) {
		exit(1);
	}
	rfbLog("lurking for logins using: '%s'\n", str);
	if (strlen(str) > strlen("lurk=")) {
		char *q = strchr(str, '=');
		tstr = strdup(q+1);
		users = user_list(tstr);
	}

	while (1) {
		if (lurk(users)) {
			break;
		}
		sleep(3);
	}
	if (tstr) {
		free(tstr);
	}
	if (users) {
		free(users);
	}
}

static int guess_user_and_switch(char *str, int fb_mode) {
	char *dstr, *d = DisplayString(dpy);
	char *p, *tstr = NULL, *allowed = NULL, *logins, **users = NULL;
	int dpy1, ret = 0;

	/* pick out ":N" */
	dstr = strchr(d, ':');
	if (! dstr) {
		return 0;
	}
	if (sscanf(dstr, ":%d", &dpy1) != 1) {
		return 0;
	}
	if (dpy1 < 0) {
		return 0;
	}

	if (strstr(str, "guess=") == str && strlen(str) > strlen("guess=")) {
		allowed = strchr(str, '=');
		allowed++;

		tstr = strdup(allowed);
		users = user_list(tstr);
	}

	/* loop over the utmpx entries looking for this display */
	logins = get_login_list(1);
	p = strtok(logins, ",");
	while (p) {
		char *user, *q, *t;
		int dpy2, ok = 1;

		t = strdup(p);
		q = strchr(t, ':'); 
		if (! q) {
			free(t);
			break;
		}
		*q = '\0';
		user = t;
		dpy2 = atoi(q+1);

		if (users) {
			char **u = users;
			ok = 0;
			while (*u != NULL) {
				if (!strcmp(user, *u++)) {
					ok = 1;
					break;
				}
			}
		}
		if (dpy1 != dpy2) {
			ok = 0;
		}

		if (! ok) {
			free(t);
			p = strtok(NULL, ",");
			continue;
		}
		if (switch_user(user, fb_mode)) {
			rfbLog("switched to guessed user: %s\n", user);
			free(t);
			ret = 1;
			break;
		}

		p = strtok(NULL, ",");
	}
	if (tstr) {
		free(tstr);
	}
	if (users) {
		free(users);
	}
	if (logins) {
		free(logins);
	}
	return ret;
}

static int try_user_and_display(uid_t uid, char *dpystr) {
	/* NO strtoks */
#if LIBVNCSERVER_HAVE_FORK && LIBVNCSERVER_HAVE_SYS_WAIT_H && LIBVNCSERVER_HAVE_PWD_H
	pid_t pid, pidw;
	char *home, *name;
	int st;
	struct passwd *pw;
	
	pw = getpwuid(uid);
	if (pw) {
		name = pw->pw_name;
		home = pw->pw_dir;
	} else {
		return 0;
	}

	/* 
	 * We fork here and try to open the display again as the
	 * new user.  Unreadable XAUTHORITY could be a problem...
	 * This is not really needed since we have DISPLAY open but:
	 * 1) is a good indicator this user owns the session and  2)
	 * some activities do spawn new X apps, e.g.  xmessage(1), etc.
	 */
	if ((pid = fork()) > 0) {
		;
	} else if (pid == -1) {
		fprintf(stderr, "could not fork\n");
		rfbLogPerror("fork");
		return 0;
	} else {
		/* child */
		Display *dpy2 = NULL;
		int rc;

		rc = switch_user_env(uid, name, home, 0); 
		if (! rc) {
			exit(1);
		}

		fclose(stderr);
		dpy2 = XOpenDisplay(dpystr);
		if (dpy2) {
			XCloseDisplay(dpy2);
			exit(0);	/* success */
		} else {
			exit(2);	/* fail */
		}
	}

	/* see what the child says: */
	pidw = waitpid(pid, &st, 0);
	if (pidw == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0) {
		return 1;
	}
#endif	/* LIBVNCSERVER_HAVE_FORK ... */
	return 0;
}

int switch_user(char *user, int fb_mode) {
	/* NO strtoks */
	int doit = 0;
	uid_t uid = 0;
	char *name, *home;

	if (*user == '+') {
		doit = 1;
		user++;
	}

	if (strstr(user, "guess=") == user) {
		return guess_user_and_switch(user, fb_mode);
	}

	user2uid(user, &uid, &name, &home);

	if (uid == (uid_t) -1 || uid == 0) {
		return 0;
	}

	if (! doit && dpy) {
		/* see if this display works: */
		char *dstr = DisplayString(dpy);
		doit = try_user_and_display(uid, dstr);
	}

	if (doit) {
		int rc = switch_user_env(uid, name, home, fb_mode);
		if (rc) {
			started_as_root = 2;
		}
		return rc;
	} else {
		return 0;
	}
}

static int switch_user_env(uid_t uid, char *name, char *home, int fb_mode) {
	/* NO strtoks */
	char *xauth;
	int reset_fb = 0;

#if !LIBVNCSERVER_HAVE_SETUID
	return 0;
#else
	/*
	 * OK tricky here, we need to free the shm... otherwise
	 * we won't be able to delete it as the other user...
	 */
	if (fb_mode == 1 && using_shm) {
		reset_fb = 1;
		clean_shm(0);
		free_tiles();
	}
	if (setuid(uid) != 0) {
		if (reset_fb) {
			/* 2 means we did clean_shm and free_tiles */
			do_new_fb(2);
		}
		return 0;
	}
#endif
	if (reset_fb) {
		do_new_fb(2);
	}

	xauth = getenv("XAUTHORITY");
	if (xauth && access(xauth, R_OK) != 0) {
		*(xauth-2) = '_';	/* yow */
	}
	
	set_env("USER", name);
	set_env("LOGNAME", name);
	set_env("HOME", home);
	return 1;
}

static void try_to_switch_users(void) {
	static time_t last_try = 0;
	time_t now = time(0);
	char *users, *p;

	if (getuid() && geteuid()) {
		rfbLog("try_to_switch_users: not root\n");
		started_as_root = 2;
		return;
	}
	if (!last_try) {
		last_try = now;
	} else if (now <= last_try + 2) {
		/* try every 3 secs or so */
		return;
	}
	last_try = now;

	users = strdup(users_list);

	if (strstr(users, "guess=") == users) {
		if (switch_user(users, 1)) {
			started_as_root = 2;
		}
		free(users);
		return;
	}

	p = strtok(users, ",");
	while (p) {
		if (switch_user(p, 1)) {
			started_as_root = 2;
			rfbLog("try_to_switch_users: now %s\n", p);
			break;
		}
		p = strtok(NULL, ",");
	}
	free(users);
}

int read_passwds(char *passfile) {
	char line[1024];
	char *filename;
	char **old_passwd_list = passwd_list;
	int remove = 0;
	int read_mode = 0;
	int begin_vo = -1;
	struct stat sbuf;
	int linecount = 0, i, max;
	FILE *in;
	static time_t last_read = 0;
	static int read_cnt = 0;
	int db_passwd = 0;

	filename = passfile;
	if (strstr(filename, "rm:") == filename) {
		filename += strlen("rm:");
		remove = 1;
	} else if (strstr(filename, "read:") == filename) {
		filename += strlen("read:");
		read_mode = 1;
		if (stat(filename, &sbuf) == 0) {
			if (sbuf.st_mtime <= last_read) {
				return 0;
			}
			last_read = sbuf.st_mtime;
		}
	}

	if (stat(filename, &sbuf) == 0) {
		/* (poor...) upper bound to number of lines */
		max = (int) sbuf.st_size;
		last_read = sbuf.st_mtime;
	} else {
		max = 64;
	}

	/* create 1 more than max to have it be the ending NULL */
	passwd_list = (char **) malloc( (max+1) * (sizeof(char *)) );
	for (i=0; i<max+1; i++) {
		passwd_list[i] = NULL;
	}
	
	in = fopen(filename, "r");
	if (in == NULL) {
		rfbLog("cannot open passwdfile: %s\n", passfile);
		rfbLogPerror("fopen");
		if (remove) {
			unlink(filename);
		}
		clean_up_exit(1);
	}

	if (getenv("DEBUG_PASSWDFILE") != NULL) {
		db_passwd = 1;
	}

	while (fgets(line, 1024, in) != NULL) {
		char *p;
		int blank = 1;
		int len = strlen(line); 

		if (db_passwd) {
			fprintf(stderr, "read_passwds: raw line: %s\n", line);
		}

		if (len == 0) {
			continue;
		} else if (line[len-1] == '\n') {
			line[len-1] = '\0';
		}
		if (line[0] == '\0') {
			continue;
		}
		if (strstr(line, "__SKIP__") != NULL) {
			continue;
		}
		if (strstr(line, "__COMM__") == line) {
			continue;
		}
		if (!strcmp(line, "__BEGIN_VIEWONLY__")) {
			if (begin_vo < 0) {
				begin_vo = linecount;
			}
			continue;
		}
		if (line[0] == '#') {
			/* commented out, cannot have password beginning with # */
			continue;
		}
		p = line;
		while (*p != '\0') {
			if (! isspace(*p)) {
				blank = 0;
				break;
			}
			p++;
		}
		if (blank) {
			continue;
		}

		passwd_list[linecount++] = strdup(line);
		if (db_passwd) {
			fprintf(stderr, "read_passwds: keepline: %s\n", line);
			fprintf(stderr, "read_passwds: begin_vo: %d\n", begin_vo);
		}

		if (linecount >= max) {
			break;
		}
	}
	fclose(in);

	for (i=0; i<1024; i++) {
		line[i] = '\0';
	}

	if (remove) {
		unlink(filename);
	}

	if (! linecount) {
		rfbLog("cannot read a valid line from passwdfile: %s\n",
		    passfile);
		if (read_cnt == 0) {
			clean_up_exit(1);
		} else {
			return 0;
		}
	}
	read_cnt++;

	for (i=0; i<linecount; i++) {
		char *q, *p = passwd_list[i];
		if (!strcmp(p, "__EMPTY__")) {
			*p = '\0';
		} else if ((q = strstr(p, "__COMM__")) != NULL) {
			*q = '\0';
		}
		passwd_list[i] = strdup(p);
		if (db_passwd) {
			fprintf(stderr, "read_passwds: trimline: %s\n", p);
		}
		strzero(p);
	}

	begin_viewonly = begin_vo;
	if (read_mode && read_cnt > 1) {
		if (viewonly_passwd) {
			free(viewonly_passwd);
			viewonly_passwd = NULL;
		}
	}

	if (begin_viewonly < 0 && linecount == 2) {
		/* for compatibility with previous 2-line usage: */
		viewonly_passwd = strdup(passwd_list[1]);
		if (db_passwd) {
			fprintf(stderr, "read_passwds: linecount is 2.\n");
		}
		if (screen) {
			char **apd = (char **) screen->authPasswdData;
			if (apd) {
				if (apd[0] != NULL) {
					strzero(apd[0]);
				}
				apd[0] = strdup(passwd_list[0]);
			}
		}
		begin_viewonly = 1;
	}

	if (old_passwd_list != NULL) {
		char *p;
		i = 0;
		while (old_passwd_list[i] != NULL) {
			p = old_passwd_list[i];
			strzero(p);
			free(old_passwd_list[i]);
			i++;
		}
		free(old_passwd_list);
	}
	return 1;
}

void install_passwds(void) {
	if (viewonly_passwd) {
		/* append the view only passwd after the normal passwd */
		char **passwds_new = (char **) malloc(3*sizeof(char *));
		char **passwds_old = (char **) screen->authPasswdData;
		passwds_new[0] = passwds_old[0];
		passwds_new[1] = viewonly_passwd;
		passwds_new[2] = NULL;
		screen->authPasswdData = (void*) passwds_new;
	} else if (passwd_list) {
		int i = 0;
		while(passwd_list[i] != NULL) {
			i++;
		}
		if (begin_viewonly < 0) {
			begin_viewonly = i+1;
		}
		screen->authPasswdData = (void*) passwd_list;
		screen->authPasswdFirstViewOnly = begin_viewonly;
	}
}

void check_new_passwds(void) {
	static time_t last_check = 0;
	time_t now;

	if (! passwdfile) {
		return;
	}
	if (strstr(passwdfile, "read:") != passwdfile) {
		return;
	}
	now = time(0);
	if (now > last_check + 1) {
		if (read_passwds(passwdfile)) {
			install_passwds();
		}
		last_check = now;
	}
}



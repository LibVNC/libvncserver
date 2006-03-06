/* -- sslcmds.c -- */

#include "x11vnc.h"
#include "inet.h"
#include "cleanup.h"

#if LIBVNCSERVER_HAVE_FORK
#if LIBVNCSERVER_HAVE_SYS_WAIT_H
#if LIBVNCSERVER_HAVE_WAITPID
#define SSLCMDS
#endif
#endif
#endif


void check_stunnel(void);
int start_stunnel(int stunnel_port, int x11vnc_port);
void stop_stunnel(void);
void setup_stunnel(int rport, int *argc, char **argv);

static pid_t stunnel_pid = 0;

void check_stunnel(void) {
	static time_t last_check = 0;
	time_t now = time(0);

	if (last_check + 3 >= now) {
		return;
	}
	last_check = now;

	if (stunnel_pid > 0) {
		int status;
		waitpid(stunnel_pid, &status, WNOHANG); 
		if (kill(stunnel_pid, 0) != 0) {
			waitpid(stunnel_pid, &status, WNOHANG); 
			rfbLog("stunnel subprocess %d died.\n", stunnel_pid); 
			stunnel_pid = 0;
			clean_up_exit(1);
		}
	}
}

int start_stunnel(int stunnel_port, int x11vnc_port) {
#ifdef SSLCMDS
	char extra[] = ":/usr/sbin:/usr/local/sbin";
	char *path, *p, *exe;
	char *stunnel_path = NULL;
	int status;

	if (stunnel_pid) {
		stop_stunnel();
	}
	stunnel_pid = 0;

	path = getenv("PATH");
	if (! path) {
		path = strdup(extra);
	} else {
		path = (char *) malloc(strlen(path)+strlen(extra)+1);
		if (! path) {
			return 0;
		}
		strcpy(path, getenv("PATH"));
		strcat(path, extra);
	}

	exe = (char *) malloc(strlen(path) + strlen("stunnel") + 1);

	p = strtok(path, ":");

	exe[0] = '\0';

	while (p) {
		struct stat sbuf;

		sprintf(exe, "%s/%s", p, "stunnel");
		if (! stunnel_path && stat(exe, &sbuf) == 0) {
			if (! S_ISDIR(sbuf.st_mode)) {
				stunnel_path = exe;
				break;
			}
		}

		p = strtok(NULL, ":");
	}
	if (path) {
		free(path);
	}

	if (! stunnel_path) {
		return 0;
	}
	if (stunnel_path[0] == '\0') {
		free(stunnel_path);
		return 0;
	}

	if (! quiet) {
		rfbLog("\n");
		rfbLog("starting ssl tunnel: %s  %d -> %d\n", stunnel_path,
		    stunnel_port, x11vnc_port);
	}

	if (0) {
		fprintf(stderr, "foreground = yes\n");
		fprintf(stderr, "pid =\n");
		fprintf(stderr, ";debug = 7\n");
		fprintf(stderr, "[x11vnc_stunnel]\n");
		fprintf(stderr, "accept = %d\n", stunnel_port);
		fprintf(stderr, "connect = %d\n", x11vnc_port);
	}

	stunnel_pid = fork();

	if (stunnel_pid < 0) {
		stunnel_pid = 0;
		free(stunnel_path);
		return 0;
	}

	if (stunnel_pid == 0) {
		FILE *in;
		char fd[20];
		int i;

		for (i=3; i<256; i++) {
			close(i);
		}

		if (use_stunnel == 3) {
			char sp[20], xp[20];

			sprintf(sp, "%d", stunnel_port);
			sprintf(xp, "%d", x11vnc_port);
			
			if (stunnel_pem) {
				execlp(stunnel_path, stunnel_path, "-f", "-d",
				    sp, "-r", xp, "-P", "none", "-p",
				    stunnel_pem, (char *) NULL);
			} else {
				execlp(stunnel_path, stunnel_path, "-f", "-d",
				    sp, "-r", xp, "-P", "none", (char *) NULL);
			}
			exit(1);
		}

		in = tmpfile();
		if (! in) {
			exit(1);
		}
		fprintf(in, "foreground = yes\n");
		fprintf(in, "pid =\n");
		if (stunnel_pem) {
			fprintf(in, "cert = %s\n", stunnel_pem);
		}
		fprintf(in, ";debug = 7\n");
		fprintf(in, "[x11vnc_stunnel]\n");
		fprintf(in, "accept = %d\n", stunnel_port);
		fprintf(in, "connect = %d\n", x11vnc_port);

		fflush(in);
		rewind(in);
		
		sprintf(fd, "%d", fileno(in));
		execlp(stunnel_path, stunnel_path, "-fd", fd, (char *) NULL);
		exit(1);
	}
	free(stunnel_path);
	usleep(500 * 1000);

	waitpid(stunnel_pid, &status, WNOHANG); 
	if (kill(stunnel_pid, 0) != 0) {
		waitpid(stunnel_pid, &status, WNOHANG); 
		stunnel_pid = 0;
		return 0;
	}

	if (! quiet) {
		rfbLog("stunnel pid is: %d\n", (int) stunnel_pid);
	}

	return 1;
#else
	return 0;
#endif
}

void stop_stunnel(void) {
	int status;
	if (! stunnel_pid) {
		return;
	}
#ifdef SSLCMDS
	kill(stunnel_pid, SIGTERM);
	usleep (150 * 1000);
	kill(stunnel_pid, SIGKILL);
	usleep (50 * 1000);
	waitpid(stunnel_pid, &status, WNOHANG); 
#endif
	stunnel_pid = 0;
}

void setup_stunnel(int rport, int *argc, char **argv) {
	int i, xport = 0;
	if (! rport) {
		for (i=0; i< *argc; i++) {
			if (!strcmp(argv[i], "-rfbport")) {
				if (i < *argc - 1) {
					rport = atoi(argv[i+1]);
					break;
				}
			}
		}
	}

	if (! rport) {
		/* we do our own autoprobing then... */
		rport = find_free_port(5900, 5999);
		if (! rport) {
			goto stunnel_fail;
		}
	}
	xport = find_free_port(5950, 5999);
	if (! xport) {
		goto stunnel_fail; 
	}
	if (start_stunnel(rport, xport)) {
		int tweaked = 0;
		char tmp[10];
		sprintf(tmp, "%d", xport);
		if (argv) {
			for (i=0; i< *argc; i++) {
				if (!strcmp(argv[i], "-rfbport")) {
					if (i < *argc - 1) {
						argv[i+i] = strdup(tmp); 
						tweaked = 1;
						break;
					}
				}
			}
			if (! tweaked) {
				i = *argc;
				argv[i] = strdup("-rfbport");
				argv[i+1] = strdup(tmp);
				*argc += 2;
				got_rfbport = 1;
			}
		}
		stunnel_port = rport;
		return;
	}

	stunnel_fail:
	rfbLog("failed to start stunnel.\n");
	clean_up_exit(1);
}


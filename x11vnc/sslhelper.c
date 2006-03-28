/* -- sslhelper.c -- */

#include "x11vnc.h"
#include "inet.h"
#include "cleanup.h"
#include "screen.h"
#include "scan.h"
#include "connections.h"

#define OPENSSL_INETD 1
#define OPENSSL_VNC   2
#define OPENSSL_HTTPS 3

#if LIBVNCSERVER_HAVE_FORK
#if LIBVNCSERVER_HAVE_SYS_WAIT_H && LIBVNCSERVER_HAVE_WAITPID
#define FORK_OK
#endif
#endif

int openssl_sock = -1;
int openssl_port_num = 0;
int https_sock = -1;
pid_t openssl_last_helper_pid = 0;

#if !LIBVNCSERVER_HAVE_LIBSSL
int openssl_present(void) {return 0;}
void openssl_init(void) {
	rfbLog("not compiled with libssl support.\n");
	clean_up_exit(1);
}
void openssl_port(void) {}
void https_port(void) {}
void check_openssl(void) {}
void check_https(void) {}
void ssl_helper_pid(pid_t pid, int sock) {sock = pid;}
void accept_openssl(int mode) {mode = 0; clean_up_exit(1);}
#else

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

int openssl_present(void);
void openssl_init(void);
void openssl_port(void);
void check_openssl(void);
void check_https(void);
void ssl_helper_pid(pid_t pid, int sock);
void accept_openssl(int mode);

static SSL_CTX *ctx = NULL;
static RSA *rsa_512 = NULL;
static RSA *rsa_1024 = NULL;
static SSL *ssl = NULL;


static void init_prng(void);
static void sslerrexit(void);
static char *create_tmp_pem(void);
static int  ssl_init(int s_in, int s_out);
static void ssl_xfer(int csock, int s_in, int s_out, int is_https);

#ifndef FORK_OK
void openssl_init(void) {
	rfbLog("openssl_init: fork is not supported. cannot create"
	    " ssl helper process.\n");
	clean_up_exit(1);
}
int openssl_present(void) {return 0;}
#else
int openssl_present(void) {return 1;}

static void sslerrexit(void) {
	unsigned long err = ERR_get_error();
	char str[256];
	
	if (err) {
		ERR_error_string(err, str);
		fprintf(stderr, "ssl error: %s\n", str);
	}
	clean_up_exit(1);
}

/* uses /usr/bin/openssl to create a tmp cert */

static char *create_tmp_pem(void) {
	pid_t pid, pidw;
	FILE *in, *out;
	char cnf[] = "/tmp/x11vnc-cnf.XXXXXX";
	char pem[] = "/tmp/x11vnc-pem.XXXXXX";
	char str[4096], line[1024], *path, *p, *exe;
	int found_openssl = 0, cnf_fd, pem_fd, status, show_cert = 1;
	struct stat sbuf;
	char tmpl[] = 
"[ req ]\n"
"prompt = no\n"
"default_bits = 1024\n"
"encrypt_key = yes\n"
"distinguished_name = req_dn\n"
"x509_extensions = cert_type\n"
"\n"
"[ req_dn ]\n"
"countryName=AU\n"
"localityName=%s\n"
"organizationalUnitName=%s-%f\n"
"organizationName=x11vnc\n"
"commonName=x11vnc-SELF-SIGNED-TEMPORARY-CERT-%d\n"
"emailAddress=nobody@x11vnc.server\n"
"\n"
"[ cert_type ]\n"
"nsCertType = server\n"
;

	if (no_external_cmds) {
		rfbLog("create_tmp_pem: cannot run external commands.\n");	
		return NULL;
	}
	rfbLog("\n");	
	rfbLog("Creating a temporary, self-signed PEM certificate...\n");	
	rfbLog("\n");	
	rfbLog("This will NOT prevent man-in-the-middle attacks UNLESS you\n");	
	rfbLog("get the certificate information to the VNC viewers SSL\n");	
	rfbLog("tunnel configuration. However, it will prevent passive\n");	
	rfbLog("network sniffing.\n");	
	rfbLog("\n");	
	rfbLog("The cert inside -----BEGIN CERTIFICATE-----\n");	
	rfbLog("                           ....\n");	
	rfbLog("                -----END CERTIFICATE-----\n");	
	rfbLog("printed below may be used on the VNC viewer-side to\n");	
	rfbLog("authenticate this server for this session.  See the -ssl\n");
	rfbLog("help output and the FAQ for how to create a permanent\n");
	rfbLog("server certificate.\n");	
	rfbLog("\n");	

	if (! getenv("PATH")) {
		return NULL;
	}
	path = strdup(getenv("PATH"));

	/* find openssl binary: */
	exe = (char *) malloc(strlen(path) + strlen("/openssl") + 1);
	p = strtok(path, ":");

	while (p) {
		sprintf(exe, "%s/openssl", p);
		if (stat(exe, &sbuf) == 0) {
			if (! S_ISDIR(sbuf.st_mode)) {
				found_openssl = 1;
				break;
			}
		}
		p = strtok(NULL, ":");
	}
	free(path);

	if (! found_openssl) {
		return NULL;
	}

	cnf_fd = mkstemp(cnf);
	pem_fd = mkstemp(pem);

	if (cnf_fd < 0 || pem_fd < 0) {
		return NULL;
	}

	close(pem_fd);

	/* create template file with our made up stuff: */
	sprintf(str, tmpl, UT.sysname, UT.nodename, dnow(), (int) getpid());
	write(cnf_fd, str, strlen(str));
	close(cnf_fd);

	/* make RSA key */
	pid = fork();
	if (pid < 0) {
		return NULL;
	} else if (pid == 0) {
		int i;
		for (i=0; i<256; i++) {
			close(i);
		}
		execlp(exe, exe, "req", "-new", "-x509", "-nodes",
		    "-config", cnf, "-out", pem, "-keyout", pem, (char *)0);
		exit(1);
	}
	pidw = waitpid(pid, &status, 0); 
	if (pidw != pid) {
		return NULL;
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		;
	} else {
		return NULL;
	}

	/* make DH parameters */
	pid = fork();
	if (pid < 0) {
		return NULL;
	} else if (pid == 0) {
		int i;
		for (i=0; i<256; i++) {
			close(i);
		}
		execlp(exe, exe, "dhparam", "-out", cnf, "512", (char *)0);
		exit(1);
	}
	pidw = waitpid(pid, &status, 0); 
	if (pidw != pid) {
		return NULL;
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		;
	} else {
		return NULL;
	}

	/* append result: */
	in = fopen(cnf, "r");
	if (in == NULL) {
		return NULL;
	}
	out = fopen(pem, "a");
	if (out == NULL) {
		fclose(in);
		return NULL;
	}
	while (fgets(line, 1024, in) != NULL) {
		fprintf(out, "%s", line);
	}
	fclose(in);
	fclose(out);

	unlink(cnf);
	free(exe);

	if (show_cert) {
		char cmd[100];
		if (inetd) {
			sprintf(cmd, "openssl x509 -text -in %s 1>&2", pem);
		} else {
			sprintf(cmd, "openssl x509 -text -in %s", pem);
		}
		fprintf(stderr, "\n");
		system(cmd);
		fprintf(stderr, "\n");
	}

	return strdup(pem);
}

void openssl_init(void) {
	int db = 0, tmp_pem = 0, do_dh = 1;
	FILE *in;
	double ds;
	long mode;

	if (! quiet) {
		rfbLog("\n");
		rfbLog("Initializing SSL.\n");
	}
	if (db) fprintf(stderr, "\nSSL_load_error_strings()\n");

	SSL_load_error_strings();

	if (db) fprintf(stderr, "SSL_library_init()\n");

	SSL_library_init();

	if (db) fprintf(stderr, "init_prng()\n");

	init_prng();

	ctx = SSL_CTX_new( SSLv23_server_method() );

	if (ctx == NULL) {
		rfbLog("openssl_init: SSL_CTX_new failed.\n");	
		sslerrexit();
	}

	ds = dnow();
	rsa_512 = RSA_generate_key(512,RSA_F4,NULL,NULL);
	if (rsa_512 == NULL) {
		rfbLog("openssl_init: RSA_generate_key(512) failed.\n");	
		sslerrexit();
	}

	rfbLog("created  512 bit temporary RSA key: %.3fs\n", dnow() - ds);

	ds = dnow();
	rsa_1024 = RSA_generate_key(1024,RSA_F4,NULL,NULL);
	if (rsa_1024 == NULL) {
		rfbLog("openssl_init: RSA_generate_key(1024) failed.\n");	
		sslerrexit();
	}

	rfbLog("created 1024 bit temporary RSA key: %.3fs\n", dnow() - ds);

	if (db) fprintf(stderr, "SSL_CTX_set_tmp_rsa()\n");

	if (! SSL_CTX_set_tmp_rsa(ctx, rsa_1024)) {
		rfbLog("openssl_init: SSL_CTX_set_tmp_rsa(1024) failed.\n");	
		sslerrexit();
	}

	mode = 0;
	mode |= SSL_MODE_ENABLE_PARTIAL_WRITE;
	mode |= SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER;
	SSL_CTX_set_mode(ctx, mode);

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_BOTH);
	SSL_CTX_set_timeout(ctx, 300);

	ds = dnow();
	if (! openssl_pem) {
		openssl_pem = create_tmp_pem();
		if (! openssl_pem) {
			rfbLog("openssl_init: could not create temporary,"
			    " self-signed PEM.\n");	
			clean_up_exit(1);
		}
		tmp_pem = 1;
	}

	rfbLog("using PEM %s  %.3fs\n", openssl_pem, dnow() - ds);

	if (do_dh) {
		DH *dh;
		BIO *bio;

		ds = dnow();
		in = fopen(openssl_pem, "r");
		if (in == NULL) {
			rfbLogPerror("fopen");
			clean_up_exit(1);
		}
		bio = BIO_new_fp(in, BIO_CLOSE|BIO_FP_TEXT);
		if (! bio) {
			rfbLog("openssl_init: BIO_new_fp() failed.\n");	
			sslerrexit();
		}
		dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
		if (dh == NULL) {
			rfbLog("openssl_init: PEM_read_bio_DHparams() failed.\n");	
			BIO_free(bio);
			sslerrexit();
		}
		BIO_free(bio);
		SSL_CTX_set_tmp_dh(ctx, dh);
		rfbLog("loaded Diffie Hellman %d bits, %.3fs\n",
		    8*DH_size(dh), dnow()-ds);
		DH_free(dh);
	}

	if (! SSL_CTX_use_certificate_chain_file(ctx, openssl_pem)) {
		rfbLog("openssl_init: SSL_CTX_use_certificate_chain_file() failed.\n");	
		sslerrexit();
	}
	if(! SSL_CTX_use_RSAPrivateKey_file(ctx, openssl_pem,
	    SSL_FILETYPE_PEM)) {
		rfbLog("openssl_init: SSL_CTX_set_tmp_rsa(1024) failed.\n");	
		sslerrexit();
	}
	if (! SSL_CTX_check_private_key(ctx)) {
		rfbLog("openssl_init: SSL_CTX_set_tmp_rsa(1024) failed.\n");	
		sslerrexit();
	}

	if (tmp_pem && ! getenv("X11VNC_KEEP_TMP_PEM")) {
		if (getenv("X11VNC_SHOW_TMP_PEM")) {
			FILE *in = fopen(openssl_pem, "r");
			if (in != NULL) {
				char line[128];
				fprintf(stderr, "\n");
				while (fgets(line, 128, in) != NULL) {
					fprintf(stderr, "%s", line);
				}
				fprintf(stderr, "\n");
				fclose(in);
			}
			
		}
		unlink(openssl_pem);
		free(openssl_pem);
	}

	if (ssl_verify) {
		struct stat sbuf;
		int lvl;
		if (stat(ssl_verify, &sbuf) != 0) {
			rfbLog("openssl_init: -sslverify does not exists %s.\n",
			    ssl_verify);	
			rfbLogPerror("stat");
			clean_up_exit(1);
		}
		if (! S_ISDIR(sbuf.st_mode)) {
			if (! SSL_CTX_load_verify_locations(ctx, ssl_verify,
			    NULL)) {
				rfbLog("openssl_init: SSL_CTX_load_verify_"
				    "locations() failed.\n");	
				sslerrexit();
			}
		} else {
			if (! SSL_CTX_load_verify_locations(ctx, NULL,
			    ssl_verify)) {
				rfbLog("openssl_init: SSL_CTX_load_verify_"
				    "locations() failed.\n");	
				sslerrexit();
			}
		}
		lvl = SSL_VERIFY_FAIL_IF_NO_PEER_CERT|SSL_VERIFY_PEER;
		SSL_CTX_set_verify(ctx, lvl, NULL);
	} else {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	}

	rfbLog("\n");
}

void openssl_port(void) {
	int sock, shutdown = 0;
	static int port = 0;
	static in_addr_t iface = INADDR_ANY;
	int db = 0;

	if (! screen) {
		rfbLog("openssl_port: no screen!\n");
		clean_up_exit(1);
	}
	if (inetd) {
		ssl_initialized = 1;
		return;
	}

	if (screen->listenSock > -1 && screen->port > 0) {
		port = screen->port;
		shutdown = 1;
	}
	if (screen->listenInterface) {
		iface = screen->listenInterface;
	}

	if (shutdown) {
		if (db) fprintf(stderr, "shutting down %d/%d\n",
		    port, screen->listenSock);
		rfbShutdownSockets(screen);
	}

	sock = rfbListenOnTCPPort(port, iface);
	if (sock < 0) {
		rfbLog("openssl_port: could not reopen port %d\n", port);
		clean_up_exit(1);
	}
	if (db) fprintf(stderr, "listen on port/sock %d/%d\n", port, sock);
	openssl_sock = sock;
	openssl_port_num = port;

	ssl_initialized = 1;
}


void https_port(void) {
	int sock;
	static int port = 0;
	static in_addr_t iface = INADDR_ANY;
	int db = 0;

	/* as openssl_port above: open a listening socket for pure https: */
	if (https_port_num < 0) {
		return;
	}
	if (! screen) {
		rfbLog("https_port: no screen!\n");
		clean_up_exit(1);
	}
	if (screen->listenInterface) {
		iface = screen->listenInterface;
	}

	if (https_port_num == 0) {
		https_port_num = find_free_port(5801, 5851);
	}
	if (https_port_num <= 0) {
		rfbLog("https_port: could not find port %d\n", https_port_num);
		clean_up_exit(1);
	}
	port = https_port_num;

	sock = rfbListenOnTCPPort(port, iface);
	if (sock < 0) {
		rfbLog("https_port: could not open port %d\n", port);
		clean_up_exit(1);
	}
	if (db) fprintf(stderr, "https_port: listen on port/sock %d/%d\n", port, sock);
	https_sock = sock;
}

static void lose_ram(void) {
	/*
	 * for a forked child that will be around for a long time
	 * without doing exec().  we really should re-exec, but a pain
	 * to redo all SSL ctx.
	 */
	free_old_fb(main_fb, rfb_fb, cmap8to24_fb);
	main_fb = NULL;
	rfb_fb = NULL;
	cmap8to24_fb = NULL;

	if (snap_fb) {
		free(snap_fb);
		snap_fb = NULL;
	}
	if (raw_fb) {
		free(raw_fb);
		raw_fb = NULL;
	}
	free_tiles();
}

/* utility to keep track of existing helper processes: */

void ssl_helper_pid(pid_t pid, int sock) {
#	define HPSIZE 256
	static pid_t helpers[HPSIZE];
	static int   sockets[HPSIZE], first = 1;
	int i, empty, set, status, db = 0;

	if (first) {
		for (i=0; i < HPSIZE; i++)  {
			helpers[i] = 0;
			sockets[i] = 0;
		}
		first = 0;
	}

	if (pid == 0) {
		/* killall */
		for (i=0; i < HPSIZE; i++) {
			if (helpers[i] == 0) {
				sockets[i] = -1;
				continue;
			}
			if (kill(helpers[i], 0) == 0) {
				if (sock != -2) {
					if (sockets[i] >= 0) {
						close(sockets[i]);
					}
					kill(helpers[i], SIGTERM);
				}

#if LIBVNCSERVER_HAVE_SYS_WAIT_H && LIBVNCSERVER_HAVE_WAITPID 
if (db) fprintf(stderr, "waitpid(%d)\n", helpers[i]);
				waitpid(helpers[i], &status, WNOHANG); 
#endif
				if (sock == -2) {
					continue;
				}
			}
			helpers[i] = 0;
			sockets[i] = -1;
		}
		return;
	}

if (db) fprintf(stderr, "ssl_helper_pid(%d, %d)\n", pid, sock);

	/* add (or delete for sock == -1) */
	set = 0;
	empty = -1;
	for (i=0; i < HPSIZE; i++) {
		if (helpers[i] == pid) {
			if (sock == -1) {

#if LIBVNCSERVER_HAVE_SYS_WAIT_H && LIBVNCSERVER_HAVE_WAITPID 
if (db) fprintf(stderr, "waitpid(%d) 2\n", helpers[i]);
				waitpid(helpers[i], &status, WNOHANG); 
#endif
				helpers[i] = 0;
			}
			sockets[i] = sock;
			set = 1;
		} else if (empty == -1 && helpers[i] == 0) {
			empty = i;
		}
	}
	if (set || sock == -1) {
		return;	/* done */
	}

	/* now try to store */
	if (empty >= 0) {
		helpers[empty] = pid;
		sockets[empty] = sock;
		return;
	}
	for (i=0; i < HPSIZE; i++) {
		if (helpers[i] == 0) {
			continue;
		}
		/* clear out stale pids: */
		if (kill(helpers[i], 0) != 0) {
			helpers[i] = 0;
			sockets[i] = -1;
			
			if (empty == -1) {
				empty = i;
			}
		}
	}
	if (empty >= 0) {
		helpers[empty] = pid;
		sockets[empty] = sock;
	}
}

static int is_ssl_readable(int s_in, time_t last_https, char *last_get,
    int mode) {
	int nfd, db = 0;
	struct timeval tv;
	fd_set rd;
	/*
	 * we'll do a select() on s_in for reading.  this is not an
	 * absolute proof that SSL_read is ready (XXX use SSL utility).
	 */
	tv.tv_sec  = 2;
	tv.tv_usec = 0;

	if (mode == OPENSSL_INETD) {
		/*
		 * https via inetd is icky because x11vnc is restarted
		 * for each socket (and some clients send requests
		 * rapid fire).
		 */
		tv.tv_sec  = 6;
	}

	/*
	 * increase the timeout if we know HTTP traffic has occurred
	 * recently:
	 */
	if (time(0) < last_https + 30) {
		tv.tv_sec  = 8;
		if (strstr(last_get, "VncViewer")) {
			tv.tv_sec  = 4;
		}
	}

	FD_ZERO(&rd);
	FD_SET(s_in, &rd);

	do {
		nfd = select(s_in+1, &rd, NULL, NULL, &tv);
	} while (nfd < 0 && errno == EINTR);

	if (db) fprintf(stderr, "https nfd: %d\n", nfd);

	if (nfd <= 0 || ! FD_ISSET(s_in, &rd)) {
		return 0;
	}
	return 1;
}

#define BSIZE 16384
static int watch_for_http_traffic(char **buf_a, int *n_a) {
	int is_http, err, n, n2;
	char *buf = *buf_a;
	int db = 0;
	/*
	 * sniff the first couple bytes of the stream and try to see
	 * if it is http or not.  if we read them OK, we must read the
	 * rest of the available data otherwise we may deadlock.
	 * what has be read is returned in buf_a and n_a.
	 * *buf_a is BSIZE+1 long and zeroed.
	 */

	*n_a = 0;

	n = SSL_read(ssl, buf, 2);
	err = SSL_get_error(ssl, n);

	if (err != SSL_ERROR_NONE || n < 2) {
		if (n > 0) {
			*n_a = n;
		}
		return -1;
	}

	/* look for GET, HEAD, POST, CONNECT */
	is_http = 0;
	if (!strncmp("GE", buf, 2)) {
		is_http = 1;
	} else if (!strncmp("HE", buf, 2)) {
		is_http = 1;
	} else if (!strncmp("PO", buf, 2)) {
		is_http = 1;
	} else if (!strncmp("CO", buf, 2)) {
		is_http = 1;
	}
	if (db) fprintf(stderr, "read: '%s'\n", buf);

	/*
	 * better read all we can and fwd it along to avoid blocking
	 * in ssl_xfer().
	 */
	n2 = SSL_read(ssl, buf + n, BSIZE - n);
	if (n2 >= 0) {
		n += n2;
	}
	*n_a = n;
	return is_http;
}

static int csock_timeout_sock = -1;
static void csock_timeout (int sig) {
	rfbLog("sig: %d, csock_timeout.\n", sig);
	if (csock_timeout_sock >= 0) {
		close(csock_timeout_sock);
		csock_timeout_sock = -1;
	}
}

void accept_openssl(int mode) {
	int sock = -1, cport, csock, vsock;	
	int status, n, i, db = 0;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	char cookie[128], rcookie[128], *name;
	rfbClientPtr client;
	pid_t pid;
	char uniq[] = "__evilrats__";
	static time_t last_https = 0;
	static char last_get[128];
	static int first = 1;

	openssl_last_helper_pid = 0;

	/* zero buffers for use below. */
	for (i=0; i<128; i++) {
		if (first) {
			last_get[i] = '\0';
		}
		cookie[i] = '\0';
		rcookie[i] = '\0';
	}
	first = 0;

	/* do INETD, VNC, or HTTPS cases (result is client socket or pipe) */
	if (mode == OPENSSL_INETD) {
		ssl_initialized = 1;

	} else if (mode == OPENSSL_VNC && openssl_sock >= 0) {
		sock = accept(openssl_sock, (struct sockaddr *)&addr, &addrlen);
		if (sock < 0)  {
			rfbLog("SSL: accept_openssl: accept connection failed\n");
			rfbLogPerror("accept");
			return;
		}

	} else if (mode == OPENSSL_HTTPS && https_sock >= 0) {
		sock = accept(https_sock, (struct sockaddr *)&addr, &addrlen);
		if (sock < 0)  {
			rfbLog("SSL: accept_openssl: accept connection failed\n");
			rfbLogPerror("accept");
			return;
		}
	}
	if (db) fprintf(stderr, "SSL: accept_openssl: sock: %d\n", sock);


	/* now make a listening socket for child to connect back to us by: */

	cport = find_free_port(20000, 0);
	if (! cport) {
		rfbLog("SSL: accept_openssl: could not find open port.\n");
		close(sock);
		if (mode == OPENSSL_INETD) {
			clean_up_exit(1);
		}
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: cport: %d\n", cport);

	csock = rfbListenOnTCPPort(cport, htonl(INADDR_LOOPBACK));

	if (csock < 0) {
		rfbLog("SSL: accept_openssl: could not listen on port %d.\n",
		    cport);
		close(sock);
		if (mode == OPENSSL_INETD) {
			clean_up_exit(1);
		}
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: csock: %d\n", csock);

	fflush(stderr);

	/*
	 * make a simple cookie to id the child socket, not foolproof
	 * but hard to guess exactly (just worrying about local lusers
	 * here, since we use INADDR_LOOPBACK).
	 */
	sprintf(cookie, "%f/%f", dnow(), x11vnc_start);

	if (mode != OPENSSL_INETD) {
		name = get_remote_host(sock);
	} else {
		name = strdup("inetd-connection");
	}
	if (name) {
		rfbLog("SSL: spawning helper process to handle: %s\n", name);
		free(name);
	}

	/* now fork the child to handle the SSL: */
	pid = fork();

	if (pid < 0) {
		rfbLog("SSL: accept_openssl: could not fork.\n");
		rfbLogPerror("fork");
		close(sock);
		close(csock);
		if (mode == OPENSSL_INETD) {
			clean_up_exit(1);
		}
		return;

	} else if (pid == 0) {
		int s_in, s_out, httpsock = -1;
		int vncsock, sslsock = sock;
		int i, have_httpd = 0;
		int f_in  = fileno(stdin);
		int f_out = fileno(stdout);

		/* reset all handlers to default (no interrupted() calls) */
		signal(SIGHUP,  SIG_DFL);
		signal(SIGINT,  SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		/* close all non-essential fd's */
		for (i=0; i<256; i++) {
			if (i != sslsock && i != 2) {
				if (mode == OPENSSL_INETD) {
					if (i == f_in || i == f_out) {
						continue;
					}
				}
				close(i);
			}
		}

		/*
		 * sadly, we are a long lived child and so the large
		 * framebuffer memory areas will soon differ from parent.
		 * try to free as much as possible.
		 */
		lose_ram();

		/* now connect back to parent socket: */
		vncsock = rfbConnectToTcpAddr("127.0.0.1", cport);
		if (vncsock < 0) {
			close(vncsock);
			exit(1);
		}

		/* try to initialize SSL with the remote client */

		if (mode == OPENSSL_INETD) {
			s_in  = fileno(stdin);
			s_out = fileno(stdout);
		} else {
			s_in = s_out = sock;
		}
		if (! ssl_init(s_in, s_out)) {
			close(vncsock);
			exit(1);
		}

		/*
		 * things get messy below since we are trying to do
		 * *both* VNC and Java applet httpd through the same
		 * SSL socket.
		 */

		if (screen->httpListenSock >= 0 && screen->httpPort > 0) {
			have_httpd = 1;
		}
		if (mode == OPENSSL_HTTPS && ! have_httpd) {
			rfbLog("SSL: accept_openssl: no httpd socket for "
			    "-https mode\n");
			close(vncsock);
			exit(1);
		}

		if (have_httpd) {
			int n, is_http;
			int hport = screen->httpPort; 
			char *iface = NULL;
			static char *buf = NULL;

			if (buf == NULL) {
				buf = (char *) calloc(sizeof(BSIZE+1), 1);
			}

			if (mode == OPENSSL_HTTPS) {
				/*
				 * for this mode we know it is HTTP traffic
				 * so we skip trying to guess.
				 */
				is_http = 1;
				n = 0;
				goto connect_to_httpd;
			}

			/*
			 * Check if there is stuff to read from remote end
			 * if so it is likely a GET or HEAD.
			 */
			if (! is_ssl_readable(s_in, last_https, last_get,
			    mode)) {
				goto write_cookie;
			}
	
			/* 
			 * read first 2 bytes to try to guess.  sadly,
			 * the user is often pondering a "non-verified
			 * cert" dialog for a long time before the GET
			 * is ever sent.  So often we timeout here.
			 */

			is_http = watch_for_http_traffic(&buf, &n);

			if (is_http < 0 || is_http == 0) {
				/*
				 * error or http not detected, fall back
				 * to normal VNC socket.
				 */
				write(vncsock, cookie, strlen(cookie));
				if (n > 0) {
					write(vncsock, buf, n);
				}
				goto wrote_cookie;
			}

			connect_to_httpd:

			/*
			 * Here we go... no turning back.  we have to
			 * send failure to parent and close socket to have
			 * http processed at all in a timely fashion...
			 */

			/* send the failure tag: */
			write(vncsock, uniq, strlen(uniq));

			if (strstr(buf, "HTTP/") != NULL)  {
				/*
				 * Also send back the GET line for heuristics.
				 * (last_https, get file).
				 */
				char *q, *str = strdup(buf);
				q = strstr(str, "HTTP/");
				if (q != NULL) {
					*q = '\0';	
					write(vncsock, str, strlen(str));
				}
				free(str);
			}

			/*
			 * Also send the cookie to pad out the number of
			 * bytes to more than the parent wants to read.
			 * Since this is the failure case, it does not
			 * matter that we send more than strlen(cookie).
			 */
			write(vncsock, cookie, strlen(cookie));
			usleep(150*1000);
			close(vncsock);


			/* now, finally, connect to the libvncserver httpd: */
			if (screen->listenInterface == htonl(INADDR_ANY) ||
			    screen->listenInterface == htonl(INADDR_NONE)) {
				iface = "127.0.0.1";
			} else {
				struct in_addr in;
				in.s_addr = screen->listenInterface;
				iface = inet_ntoa(in);
			}
			if (iface == NULL || !strcmp(iface, "")) {
				iface = "127.0.0.1";
			}
			usleep(150*1000);

			httpsock = rfbConnectToTcpAddr(iface, hport);

			if (httpsock < 0) {
				/* UGH, after all of that! */
				rfbLog("Could not connect to httpd socket!\n");
				exit(1);
			}
			if (db) fprintf(stderr, "ssl_helper: httpsock: %d %d\n", httpsock, n);

			/*
			 * send what we read to httpd, and then connect
			 * the rest of the SSL session to it:
			 */
			if (n > 0) {
				write(httpsock, buf, n);
			}
			ssl_xfer(httpsock, s_in, s_out, is_http);
			exit(0);
		}

		/*
		 * ok, back from the above https mess, simply send the
		 * cookie back to the parent (who will attach us to
		 * libvncserver), and connect the rest of the SSL session
		 * to it.
		 */
		write_cookie:
		write(vncsock, cookie, strlen(cookie));

		wrote_cookie:
		ssl_xfer(vncsock, s_in, s_out, 0);

		exit(0);
	}

	if (mode != OPENSSL_INETD) {
		close(sock);
	}
	if (db) fprintf(stderr, "helper process is: %d\n", pid);

	/* accept connection from our child.  */
	signal(SIGALRM, csock_timeout);
	csock_timeout_sock = csock;
	alarm(20);

	vsock = accept(csock, (struct sockaddr *)&addr, &addrlen);

	alarm(0);
	signal(SIGALRM, SIG_DFL);
	close(csock);

	if (vsock < 0) {
		rfbLog("SSL: accept_openssl: connection from ssl_helper failed.\n");
		rfbLogPerror("accept");

		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		if (mode == OPENSSL_INETD) {
			clean_up_exit(1);
		}
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: vsock: %d\n", vsock);

	n = read(vsock, rcookie, strlen(cookie));
	if (n != (int) strlen(cookie) || strncmp(cookie, rcookie, n)) {
		rfbLog("SSL: accept_openssl: cookie from ssl_helper failed. %d\n", n);
		if (errno != 0) {
			rfbLogPerror("read");
		}
		if (db) fprintf(stderr, "'%s'\n'%s'\n", cookie, rcookie);
		close(vsock);

		if (strstr(rcookie, uniq) == rcookie) {
			int i;
			rfbLog("SSL: https for helper process succeeded.\n");
			if (mode != OPENSSL_HTTPS) {
				last_https = time(0);
				for (i=0; i<128; i++) {
					last_get[i] = '\0';
				}
				strncpy(last_get, rcookie, 100);
				if (db) fprintf(stderr, "last_get: '%s'\n", last_get);
			}
			ssl_helper_pid(pid, -2);

			if (mode == OPENSSL_INETD) {
				/* to expand $PORT correctly in index.vnc */
				if (screen->port == 0) {
					int fd = fileno(stdin);
					if (getenv("X11VNC_INETD_PORT")) {
						screen->port = atoi(getenv(
						    "X11VNC_INETD_PORT"));
					} else {
						int tport = get_local_port(fd);
						if (tport > 0) {
							screen->port = tport;
						}
					}
				}
				rfbLog("SSL: screen->port %d\n", screen->port);

				/* kludge for https fetch via inetd */
				double start = dnow();
				while (dnow() < start + 10.0) {
					rfbPE(10000);
					usleep(10000);
					waitpid(pid, &status, WNOHANG); 
					if (kill(pid, 0) != 0) {
						rfbPE(10000);
						rfbPE(10000);
						break;
					}
				}
				rfbLog("SSL: OPENSSL_INETD guessing "
				    "child https finished.\n");
				clean_up_exit(1);
			}
			return;
		}
		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		if (mode == OPENSSL_INETD) {
			clean_up_exit(1);
		}
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: cookie good: %s\n", cookie);

	rfbLog("SSL: handshake with helper process succeeded.\n");

	openssl_last_helper_pid = pid;
	ssl_helper_pid(pid, vsock);
	client = rfbNewClient(screen, vsock);
	openssl_last_helper_pid = 0;

	if (client) {
		if (db) fprintf(stderr, "accept_openssl: client %p\n", (void *) client);
		if (db) fprintf(stderr, "accept_openssl: new_client %p\n", (void *) screen->newClientHook);
		if (db) fprintf(stderr, "accept_openssl: new_client %p\n", (void *) new_client);
		if (mode == OPENSSL_INETD) {
			inetd_client = client;
			client->clientGoneHook = client_gone;
		}
	} else {
		rfbLog("SSL: accept_openssl: rfbNewClient failed.\n");
		close(vsock);

		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		if (mode == OPENSSL_INETD) {
			clean_up_exit(1);
		}
		return;
	}
}

static void ssl_timeout (int sig) {
	rfbLog("sig: %d, ssl_init timed out.\n", sig);
	exit(1);
}

static int ssl_init(int s_in, int s_out) {
	unsigned char *sid = (unsigned char *) "x11vnc SID";
	char *name;
	int db = 0, rc, err;
	int ssock = s_in;
	double start = dnow();
	int timeout = 20;

	if (db) fprintf(stderr, "ssl_init: %d/%d\n", s_in, s_out);
	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		fprintf(stderr, "SSL_new failed\n");
		return 0;
	}

	SSL_set_session_id_context(ssl, sid, strlen((char *)sid));

	if (s_in == s_out) {
		if (! SSL_set_fd(ssl, ssock)) {
			fprintf(stderr, "SSL_set_fd failed\n");
			return 0;
		}
	} else {
		if (! SSL_set_rfd(ssl, s_in)) {
			fprintf(stderr, "SSL_set_rfd failed\n");
			return 0;
		}
		if (! SSL_set_wfd(ssl, s_out)) {
			fprintf(stderr, "SSL_set_wfd failed\n");
			return 0;
		}
	}

	SSL_set_accept_state(ssl);

	name = get_remote_host(ssock);

	while (1) {
		if (db) fprintf(stderr, "calling SSL_accept...\n");

		signal(SIGALRM, ssl_timeout);
		alarm(timeout);

		rc = SSL_accept(ssl);
		err = SSL_get_error(ssl, rc);

		alarm(0);
		signal(SIGALRM, SIG_DFL);

		if (db) fprintf(stderr, "SSL_accept %d/%d\n", rc, err);
		if (err == SSL_ERROR_NONE) {
			break;
		} else if (err == SSL_ERROR_WANT_READ) {
			if (db) fprintf(stderr, "got SSL_ERROR_WANT_READ\n");
			rfbLog("SSL: ssl_helper: SSL_accept() failed for: %s\n",
			    name);
			return 0;
			
		} else if (err == SSL_ERROR_WANT_WRITE) {
			if (db) fprintf(stderr, "got SSL_ERROR_WANT_WRITE\n");
			rfbLog("SSL: ssl_helper: SSL_accept() failed for: %s\n",
			    name);
			return 0;
		} else if (err == SSL_ERROR_SYSCALL) {
			if (db) fprintf(stderr, "got SSL_ERROR_SYSCALL\n");
			rfbLog("SSL: ssl_helper: SSL_accept() failed for: %s\n",
			    name);
			return 0;
		} else if (err == SSL_ERROR_ZERO_RETURN) {
			if (db) fprintf(stderr, "got SSL_ERROR_ZERO_RETURN\n");
			rfbLog("SSL: ssl_helper: SSL_accept() failed for: %s\n",
			    name);
			return 0;
		} else if (rc < 0) {
			rfbLog("SSL: ssl_helper: SSL_accept() fatal: %d\n", rc);
			return 0;
		} else if (dnow() > start + 3.0) {
			rfbLog("SSL: ssl_helper: timeout looping SSL_accept() "
			    "fatal.\n");
			return 0;
		} else {
			BIO *bio = SSL_get_rbio(ssl);
			if (bio == NULL) {
				rfbLog("SSL: ssl_helper: ssl BIO is null. "
				    "fatal.\n");
				return 0;
			}
			if (BIO_eof(bio)) {
				rfbLog("SSL: ssl_helper: ssl BIO is EOF. "
				    "fatal.\n");
				return 0;
			}
		}
		usleep(10 * 1000);
	}

	rfbLog("SSL: ssl_helper: SSL_accept() succeeded for: %s\n", name);
	free(name);

	return 1;
}

static void ssl_xfer_debug(int csock, int s_in, int s_out) {
	char buf[2048];
	int sz = 2048, n, m, status;
	pid_t pid = fork();
	int db = 1;

	/* this is for testing, no SSL just socket redir */
	if (pid < 0) {
		exit(1);
	}
	if (pid) {
		if (db) fprintf(stderr, "ssl_xfer start: %d -> %d/%d\n", csock, s_in, s_out);

		while (1) {
			n = read(csock, buf, sz);
			if (n == 0 || (n < 0 && errno != EINTR) ) {
				break;
			} else if (n > 0) {
				m = write(s_out, buf, n);
				if (m != n) {
		if (db) fprintf(stderr, "ssl_xfer bad write:  %d -> %d | %d/%d\n", csock, s_out, m, n);
					break;
				}
			
			}
		}
		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		if (db) fprintf(stderr, "ssl_xfer done:  %d -> %d\n", csock, s_out);

	} else {
		if (db) fprintf(stderr, "ssl_xfer start: %d <- %d\n", csock, s_in);

		while (1) {
			n = read(s_in, buf, sz);
			if (n == 0 || (n < 0 && errno != EINTR) ) {
				break;
			} else if (n > 0) {
				m = write(csock, buf, n);
				if (m != n) {
		if (db) fprintf(stderr, "ssl_xfer bad write:  %d <- %d | %d/%d\n", csock, s_in, m, n);
					break;
				}
			}
		}
		if (db) fprintf(stderr, "ssl_xfer done:  %d <- %d\n", csock, s_in);

	}
	close(csock);
	close(s_in);
	close(s_out);
	exit(0);
}

static void ssl_xfer(int csock, int s_in, int s_out, int is_https) {
	int dbxfer = 0, db = 0, check_pending, fdmax, nfd, n, i, err;
	char cbuf[BSIZE], sbuf[BSIZE];
	int  cptr, sptr, c_rd, c_wr, s_rd, s_wr;
	fd_set rd, wr;
	struct timeval tv;
	int ssock;

	if (dbxfer) {
		ssl_xfer_debug(csock, s_in, s_out);
		return;
	}

	/*
	 * csock: clear text socket with libvncserver.    "C"
	 * ssock: ssl data socket with remote vnc viewer. "S"
	 *
	 * to cover inetd mode, we have s_in and s_out, but in non-ssl mode they
	 * both ssock.
	 *
	 * cbuf[] is data from csock that we have read but not passed on to ssl 
	 * sbuf[] is data from ssl that we have read but not passed on to csock 
	 */
	for (i=0; i<BSIZE; i++) {
		cbuf[i] = '\0';
		sbuf[i] = '\0';
	}
		
	if (s_out > s_in) {
		ssock = s_out;
	} else {
		ssock = s_in;
	}

	if (csock > ssock) {
		fdmax = csock; 
	} else {
		fdmax = ssock; 
	}

	c_rd = 1;	/* clear text (libvncserver) socket open for reading */
	c_wr = 1;	/* clear text (libvncserver) socket open for writing */
	s_rd = 1;	/* ssl data (remote client)  socket open for reading */
	s_wr = 1;	/* ssl data (remote client)  socket open for writing */

	cptr = 0;	/* offsets into BSIZE buffers */
	sptr = 0;

	while (1) {
		int c_to_s, s_to_c;

		if ( s_wr && (c_rd || cptr > 0) ) {
			/* 
			 * S is writable and 
			 * C is readable or some cbuf data remaining
			 */
			c_to_s = 1;
		} else {
			c_to_s = 0;
		}

		if ( c_wr && (s_rd || sptr > 0) ) {
			/* 
			 * C is writable and 
			 * S is readable or some sbuf data remaining
			 */
			s_to_c = 1;
		} else {
			s_to_c = 0;
		}

		if (! c_to_s && ! s_to_c) {
			/*
			 * nothing can be sent either direction.
			 * break out of the loop to finish all work.
			 */
			break;
		}

		/* set up the fd sets for the two sockets for read & write: */

		FD_ZERO(&rd);

		if (c_rd && cptr < BSIZE) {
			/* we could read more from C since cbuf is not full */
			FD_SET(csock, &rd);
		}
		if (s_rd) {
			/*
			 * we could read more from S since sbuf not full,
			 * OR ssl is waiting for more BIO to be able to
			 * read and we have some C data still buffered.
			 */
			if (sptr < BSIZE || (cptr > 0 && SSL_want_read(ssl))) {
				FD_SET(s_in, &rd);
			}
		}
		
		FD_ZERO(&wr);

		if (c_wr && sptr > 0) {
			/* we could write more to C since sbuf is not empty */
			FD_SET(csock, &wr);
		}
		if (s_wr) {
			/*
			 * we could write more to S since cbuf not empty,
			 * OR ssl is waiting for more BIO to be able
			 * write and we haven't filled up sbuf yet.
			 */
			if (cptr > 0 || (sptr < BSIZE && SSL_want_write(ssl))) {
				FD_SET(s_out, &wr);
			}
		}

		if (is_https) {
			tv.tv_sec  = 45;
		} else {
			tv.tv_sec  = 20;
		}
		tv.tv_usec = 0;

		/*  do the select, repeat if interrupted */
		do {
			nfd = select(fdmax+1, &rd, &wr, NULL, &tv);
		} while (nfd < 0 && errno == EINTR);

if (db) fprintf(stderr, "nfd: %d\n", nfd);

		if (nfd < 0) {
			rfbLog("SSL: ssl_xfer[%d]: select error: %d\n", getpid(), nfd);
			perror("select");
			/* connection finished */
			return;	
		}

		if (nfd == 0) {
			rfbLog("SSL: ssl_xfer[%d]: connection timedout.\n",
			    getpid());
			/* connection finished */
			return;
		}

		/* used to see if SSL_pending() should be checked: */
		check_pending = 0;

		if (c_wr && FD_ISSET(csock, &wr)) {

			/* try to write some of our sbuf to C: */
			n = write(csock, sbuf, sptr); 

			if (n < 0) {
				if (errno != EINTR) {
					/* connection finished */
					return;
				}
				/* proceed */
			} else if (n == 0) {
				/* connection finished XXX double check */
				return;
			} else {
				/* shift over the data in sbuf by n */
				memmove(sbuf, sbuf + n, sptr - n);
				if (sptr == BSIZE) {
					check_pending = 1;
				}
				sptr -= n;

				if (! s_rd && sptr == 0) {
					/* finished sending last of sbuf */
					shutdown(csock, SHUT_WR);
					c_wr = 0;
				}
			}
		}

		if (s_wr) {
			if ((cptr > 0 && FD_ISSET(s_out, &wr)) ||
			    (SSL_want_read(ssl) && FD_ISSET(s_in, &rd))) {

				/* try to write some of our cbuf to S: */

				n = SSL_write(ssl, cbuf, cptr);
				err = SSL_get_error(ssl, n);

				if (err == SSL_ERROR_NONE) {
					/* shift over the data in cbuf by n */
					memmove(cbuf, cbuf + n, cptr - n);
					cptr -= n;

					if (! c_rd && cptr == 0 && s_wr) {
						/* finished sending last cbuf */
						SSL_shutdown(ssl);
						s_wr = 0;
					}

				} else if (err == SSL_ERROR_WANT_WRITE
					|| err == SSL_ERROR_WANT_READ
					|| err == SSL_ERROR_WANT_X509_LOOKUP) {

						;	/* proceed */

				} else if (err == SSL_ERROR_SYSCALL) {
					if (n < 0 && errno != EINTR) {
						/* connection finished */
						return;
					}
					/* proceed */
				} else if (err == SSL_ERROR_ZERO_RETURN) {
					/* S finished */
					s_rd = 0;
					s_wr = 0;
				} else if (err == SSL_ERROR_SSL) {
					/* connection finished */
					return;
				}
			}
		}

		if (c_rd && FD_ISSET(csock, &rd)) {


			/* try to read some data from C into our cbuf */

			n = read(csock, cbuf + cptr, BSIZE - cptr);

			if (n < 0) {
				if (errno != EINTR) {
					/* connection finished */
					return;
				}
				/* proceed */
			} else if (n == 0) {
				/* C is EOF */
				c_rd = 0;
				if (cptr == 0 && s_wr) {
					/* and no more in cbuf to send */
					SSL_shutdown(ssl);
					s_wr = 0;
				}
			} else {
				/* good */

				cptr += n;
			}
		}

		if (s_rd) {
			if ((sptr < BSIZE && FD_ISSET(s_in, &rd)) ||
			    (SSL_want_write(ssl) && FD_ISSET(s_out, &wr)) ||
			    (check_pending && SSL_pending(ssl))) {

				/* try to read some data from S into our sbuf */

				n = SSL_read(ssl, sbuf + sptr, BSIZE - sptr);
				err = SSL_get_error(ssl, n);

				if (err == SSL_ERROR_NONE) {
					/* good */

					sptr += n;

				} else if (err == SSL_ERROR_WANT_WRITE
					|| err == SSL_ERROR_WANT_READ
					|| err == SSL_ERROR_WANT_X509_LOOKUP) {

						;	/* proceed */

				} else if (err == SSL_ERROR_SYSCALL) {
					if (n < 0) {
						if(errno != EINTR) {
							/* connection finished */
							return;
						}
						/* proceed */
					} else {
						/* S finished */
						s_rd = 0;
						s_wr = 0;
					}
				} else if (err == SSL_ERROR_ZERO_RETURN) {
					/* S is EOF */
					s_rd = 0;
					if (cptr == 0 && s_wr) {
						/* and no more in cbuf to send */
						SSL_shutdown(ssl);
						s_wr = 0;
					}
					if (sptr == 0 && c_wr) {
						/* and no more in sbuf to send */
						shutdown(csock, SHUT_WR);
						c_wr = 0;
					}
				} else if (err == SSL_ERROR_SSL) {
					/* connection finished */
					return;
				}
			}
		}
	}
}

void check_openssl(void) {
	fd_set fds;
	struct timeval tv;
	int nfds;
	static time_t last_waitall = 0;
	static double last_check = 0.0;
	double now;

	if (! use_openssl || openssl_sock < 0) {
		return;
	}

	now = dnow();
	if (now < last_check + 0.5) {
		return;
	}
	last_check = now;

	if (time(0) > last_waitall + 150) {
		last_waitall = time(0);
		ssl_helper_pid(0, -2);	/* waitall */
	}

	FD_ZERO(&fds);
	FD_SET(openssl_sock, &fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	nfds = select(openssl_sock+1, &fds, NULL, NULL, &tv);

	if (nfds <= 0) {
		return;
	}
	
	rfbLog("SSL: accept_openssl(OPENSSL_VNC)\n");
	accept_openssl(OPENSSL_VNC);
}

void check_https(void) {
	fd_set fds;
	struct timeval tv;
	int nfds;
	static double last_check = 0.0;
	double now;

	if (! use_openssl || https_sock < 0) {
		return;
	}

	now = dnow();
	if (now < last_check + 0.5) {
		return;
	}
	last_check = now;

	FD_ZERO(&fds);
	FD_SET(https_sock, &fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	nfds = select(https_sock+1, &fds, NULL, NULL, &tv);

	if (nfds <= 0) {
		return;
	}
	rfbLog("SSL: accept_openssl(OPENSSL_HTTPS)\n");
	accept_openssl(OPENSSL_HTTPS);
}

#define MSZ 4096
static void init_prng(void) {
	int db = 0, bytes;
	char file[MSZ];

	RAND_file_name(file, MSZ);

	rfbLog("RAND_file_name: %s\n", file);

	bytes = RAND_load_file(file, -1);
	if (db) fprintf(stderr, "bytes read: %d\n", bytes);
	
	bytes += RAND_load_file("/dev/urandom", 64);
	if (db) fprintf(stderr, "bytes read: %d\n", bytes);
	
	if (bytes > 0) {
		if (! quiet) {
			rfbLog("initialized PRNG with %d random bytes.\n",
			    bytes);
		}
		return;
	}

	bytes += RAND_load_file("/dev/random", 8);
	if (db) fprintf(stderr, "bytes read: %d\n", bytes);
	if (! quiet) {
		rfbLog("initialized PRNG with %d random bytes.\n", bytes);
	}
}
#endif	/* FORK_OK */
#endif	/* LIBVNCSERVER_HAVE_LIBSSL */


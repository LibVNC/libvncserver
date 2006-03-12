/* -- sslhelper.c -- */

#include "x11vnc.h"
#include "inet.h"
#include "cleanup.h"
#include "screen.h"
#include "scan.h"

#if LIBVNCSERVER_HAVE_FORK
#if LIBVNCSERVER_HAVE_SYS_WAIT_H && LIBVNCSERVER_HAVE_WAITPID
#define FORK_OK
#endif
#endif

int openssl_sock = -1;
pid_t openssl_last_helper_pid = 0;

#if !LIBVNCSERVER_HAVE_LIBSSL
int openssl_present(void) {return 0;}
void openssl_init(void) {
	rfbLog("not compiled with libssl support.\n");
	clean_up_exit(1);
}
void openssl_port(void) {}
void check_openssl(void) {}
void ssh_helper_pid(pid_t pid, int sock) {sock = pid;}
#else

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

int openssl_present(void);
void openssl_init(void);
void openssl_port(void);
void check_openssl(void);
void ssh_helper_pid(pid_t pid, int sock);

static SSL_CTX *ctx = NULL;
static RSA *rsa_512 = NULL;
static RSA *rsa_1024 = NULL;
static SSL *ssl = NULL;


static void init_prng(void);
static void sslerrexit(void);
static char *create_tmp_pem(void);
static int  ssl_init(int csock, int ssock);
static void ssl_xfer(int csock, int ssock);

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

static char *create_tmp_pem(void) {
	pid_t pid, pidw;
	FILE *in, *out;
	char cnf[] = "/tmp/x11vnc-cnf.XXXXXX";
	char pem[] = "/tmp/x11vnc-pem.XXXXXX";
	char str[4096], line[1024], *path, *p, *exe;
	int found_openssl = 0, cnf_fd, pem_fd, status, db = 1;
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
"commonName=x11vnc-%d\n"
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
	rfbLog("This will NOT prevent man-in-the-middle attacks unless you\n");	
	rfbLog("get the certificate information to the VNC viewers ssl\n");	
	rfbLog("tunnel configuration. But it will prevent passive sniffing.\n");	

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

	if (db) {
		char cmd[100];
		sprintf(cmd, "openssl x509 -text -in %s", pem);
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

	if (db) fprintf(stderr, "\n");
}

void openssl_port(void) {
	int sock, shutdown = 0;
	static int port = 0;
	static in_addr_t iface = 0;
	int db = 0;

	if (! screen) {
		rfbLog("openssl_port: no screen!\n");
		clean_up_exit(1);
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

	ssl_initialized = 1;
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

void ssh_helper_pid(pid_t pid, int sock) {
#	define HPSIZE 256
	static pid_t helpers[HPSIZE];
	static int   sockets[HPSIZE], first = 1;
	int i, empty, set;

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
				continue;
				sockets[i] = -1;
			}
			if (kill(helpers[i], 0) == 0) {
				int status;
				if (sockets[i] >= 0) {
					close(sockets[i]);
				}
				kill(helpers[i], SIGTERM);
#if LIBVNCSERVER_HAVE_SYS_WAIT_H && LIBVNCSERVER_HAVE_WAITPID 
				waitpid(helpers[i], &status, WNOHANG); 
#endif
			}
			helpers[i] = 0;
			sockets[i] = -1;
		}
		return;
	}
	/* add */
	set = 0;
	empty = -1;
	for (i=0; i < HPSIZE; i++) {
		if (helpers[i] == pid) {
			if (sock == -1) {
				helpers[i] = 0;
			}
			sockets[i] = sock;
			set = 1;
		} else if (empty == -1 && helpers[i] == 0) {
			empty = i;
		}
	}
	if (set || sock == -1) {
		return;
	}
	if (empty >= 0) {
		helpers[empty] = pid;
		sockets[empty] = sock;
		return;
	}
	for (i=0; i < HPSIZE; i++) {
		if (helpers[i] == 0) {
			continue;
		}
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

void accept_openssl(void) {
	int sock, cport, csock, vsock;	
	int status, n, db = 0;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	char cookie[128], rcookie[128], *name;
	rfbClientPtr client;
	pid_t pid;

	openssl_last_helper_pid = 0;

	sock = accept(openssl_sock, (struct sockaddr *)&addr, &addrlen);
	if (sock < 0)  {
		rfbLog("accept_openssl: accept connection failed\n");
		rfbLogPerror("accept");
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: sock: %d\n", sock);

	cport = find_free_port(20000, 0);
	if (! cport) {
		rfbLog("accept_openssl: could not find open port.\n");
		close(sock);
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: cport: %d\n", cport);

	csock = rfbListenOnTCPPort(cport, htonl(INADDR_LOOPBACK));
	if (csock < 0) {
		rfbLog("accept_openssl: could not listen on port %d.\n",
		    cport);
		close(sock);
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: csock: %d\n", csock);

	fflush(stderr);
	sprintf(cookie, "%f/%f", dnow(), x11vnc_start);

	name = get_remote_host(sock);
	if (name) {
		rfbLog("SSL: spawning helper process to handle: %s\n", name);
		free(name);
	}

	pid = fork();
	if (pid < 0) {
		rfbLog("accept_openssl: could not fork.\n");
		rfbLogPerror("fork");
		close(sock);
		close(csock);
		return;
	} else if (pid == 0) {
		int i, vncsock, sslsock = sock;

		signal(SIGHUP,  SIG_DFL);
		signal(SIGINT,  SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		for (i=0; i<256; i++) {
			if (i != sslsock && i != 2) {
				close(i);
			}
		}

		lose_ram();

		vncsock = rfbConnectToTcpAddr("127.0.0.1", cport);
		if (vncsock < 0) {
			close(vncsock);
			exit(1);
		}
		if (! ssl_init(vncsock, sslsock)) {
			close(vncsock);
			exit(1);
		}
		write(vncsock, cookie, strlen(cookie));
		ssl_xfer(vncsock, sslsock);
		exit(0);
	}
	close(sock);

	vsock = accept(csock, (struct sockaddr *)&addr, &addrlen);
	close(csock);
	if (vsock < 0) {
		rfbLog("accept_openssl: connection from ssl_helper failed.\n");
		rfbLogPerror("accept");

		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: vsock: %d\n", vsock);

	n = read(vsock, rcookie, strlen(cookie));
	if (n != (int) strlen(cookie) || strncmp(cookie, rcookie, n)) {
		rfbLog("accept_openssl: cookie from ssl_helper failed. %d\n", n);
		if (errno != 0) {
			rfbLogPerror("read");
		}
		if (db) fprintf(stderr, "'%s' '%s'\n", cookie, rcookie);
		close(vsock);

		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		return;
	}
	if (db) fprintf(stderr, "accept_openssl: cookie good: %s\n", cookie);

	rfbLog("SSL: handshake with helper process succeeded.\n");

	openssl_last_helper_pid = pid;
	ssh_helper_pid(pid, vsock);
	client = rfbNewClient(screen, vsock);
	openssl_last_helper_pid = 0;
	if (client) {
		if (db) fprintf(stderr, "accept_openssl: client %p\n", (void *) client);
	} else {
		rfbLog("accept_openssl: rfbNewClient failed.\n");
		close(vsock);

		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		return;
	}
}

static void ssl_timeout (int sig) {
	fprintf(stderr, "sig: %d, ssh_init timed out.\n", sig);
	exit(1);
}

static int ssl_init(int csock, int ssock) {
	unsigned char *sid = (unsigned char *) "x11vnc SID";
	char *name;
	int db = 0, rc, err;

	if (db) fprintf(stderr, "ssl_init: %d %d\n", csock, ssock);
	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		fprintf(stderr, "SSL_new failed\n");
		return 0;
	}

	SSL_set_session_id_context(ssl, sid, strlen((char *)sid));

	if (! SSL_set_fd(ssl, ssock)) {
		fprintf(stderr, "SSL_set_fd failed\n");
		return 0;
	}

	SSL_set_accept_state(ssl);

	name = get_remote_host(ssock);

	while (1) {
		if (db) fprintf(stderr, "calling SSL_accept...\n");

		signal(SIGALRM, ssl_timeout);
		alarm(20);

		rc = SSL_accept(ssl);
		err = SSL_get_error(ssl, rc);

		alarm(0);
		signal(SIGALRM, SIG_DFL);

		if (db) fprintf(stderr, "SSL_accept %d/%d\n", rc, err);
		if (err == SSL_ERROR_NONE) {
			break;
		} else if (err == SSL_ERROR_WANT_READ) {
			if (db) fprintf(stderr, "got SSL_ERROR_WANT_READ\n");
			rfbLog("SSL: ssh_helper: SSL_accept() failed for: %s\n",
			    name);
			return 0;
			
		} else if (err == SSL_ERROR_WANT_WRITE) {
			if (db) fprintf(stderr, "got SSL_ERROR_WANT_WRITE\n");
			rfbLog("SSL: ssh_helper: SSL_accept() failed for: %s\n",
			    name);
			return 0;
		} else if (err == SSL_ERROR_SYSCALL) {
			if (db) fprintf(stderr, "got SSL_ERROR_SYSCALL\n");
			rfbLog("SSL: ssh_helper: SSL_accept() failed for: %s\n",
			    name);
			return 0;
		} else if (rc < 0) {
			rfbLog("SSL: ssh_helper: SSL_accept() fatal: %d\n",
			    rc);
			return 0;
		}
	}

	rfbLog("SSL: ssh_helper: SSL_accept() succeeded for: %s\n", name);
	free(name);

	return 1;
}

static void ssl_xfer_debug(int csock, int ssock) {
	char buf[2048];
	int sz = 2048, n, m, status;
	pid_t pid = fork();
	int db = 1;

	/* this is for testing, no SSL just socket redir */
	if (pid < 0) {
		exit(1);
	}
	if (pid) {
		if (db) fprintf(stderr, "ssl_xfer start: %d -> %d\n", csock, ssock);

		while (1) {
			n = read(csock, buf, sz);
			if (n == 0 || (n < 0 && errno != EINTR) ) {
				break;
			} else if (n > 0) {
				m = write(ssock, buf, n);
				if (m != n) {
		if (db) fprintf(stderr, "ssl_xfer bad write:  %d -> %d | %d/%d\n", csock, ssock, m, n);
					break;
				}
			
			}
		}
		kill(pid, SIGTERM);
		waitpid(pid, &status, WNOHANG); 
		if (db) fprintf(stderr, "ssl_xfer done:  %d -> %d\n", csock, ssock);

	} else {
		if (db) fprintf(stderr, "ssl_xfer start: %d <- %d\n", csock, ssock);

		while (1) {
			n = read(ssock, buf, sz);
			if (n == 0 || (n < 0 && errno != EINTR) ) {
				break;
			} else if (n > 0) {
				m = write(csock, buf, n);
				if (m != n) {
		if (db) fprintf(stderr, "ssl_xfer bad write:  %d <- %d | %d/%d\n", csock, ssock, m, n);
					break;
				}
			}
		}
		if (db) fprintf(stderr, "ssl_xfer done:  %d <- %d\n", csock, ssock);

	}
	close(csock);
	close(ssock);
	exit(0);
}

#define BSIZE 16384
static void ssl_xfer(int csock, int ssock) {
	int db = 0, check_pending, fdmax, nfd, n, err;
	char cbuf[BSIZE], sbuf[BSIZE];
	int  cptr, sptr, c_rd, c_wr, s_rd, s_wr;
	fd_set rd, wr;
	struct timeval tv;

	if (db) {
		ssl_xfer_debug(csock, ssock);
		return;
	}

	/*
	 * csock: clear text socket with libvncserver.    "C"
	 * ssock: ssl data socket with remote vnc viewer. "S"
	 *
	 * cbuf[] is data from csock that we have read but not passed on to ssl 
	 * sbuf[] is data from ssl that we have read but not passed on to csock 
	 */

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
				FD_SET(ssock, &rd);
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
				FD_SET(ssock, &wr);
			}
		}

		tv.tv_sec  = 20;
		tv.tv_usec = 0;

		/*  do the select, repeat if interrupted */
		do {
			nfd = select(fdmax+1, &rd, &wr, NULL, &tv);
		} while (nfd < 0 && errno == EINTR);

		if (nfd < 0) {
			fprintf(stderr, "select error: %d\n", nfd);
			perror("select");
			/* connection finished */
			return;	
		}

		if (nfd == 0) {
			fprintf(stderr, "timeout\n");
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
			if ((cptr > 0 && FD_ISSET(ssock, &wr)) ||
			    (SSL_want_read(ssl) && FD_ISSET(ssock, &rd))) {

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
			if ((sptr < BSIZE && FD_ISSET(ssock, &rd)) ||
			    (SSL_want_write(ssl) && FD_ISSET(ssock, &wr)) ||
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

if (0) fprintf(stderr, "check_openssl()\n");

	if (! use_openssl || openssl_sock < 0) {
		return;
	}

	FD_ZERO(&fds);
	FD_SET(openssl_sock, &fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	nfds = select(openssl_sock+1, &fds, NULL, NULL, &tv);

	if (nfds <= 0) {
		return;
	}
	accept_openssl();
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


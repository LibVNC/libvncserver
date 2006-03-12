#ifndef _X11VNC_SSLHELPER_H
#define _X11VNC_SSLHELPER_H

/* -- sslhelper.h -- */


extern int openssl_sock;
extern pid_t openssl_last_helper_pid;

extern int openssl_present(void);
extern void openssl_init(void);
extern void openssl_port(void);
extern void check_openssl(void);
extern void ssh_helper_pid(pid_t pid, int sock);


#endif /* _X11VNC_SSLHELPER_H */

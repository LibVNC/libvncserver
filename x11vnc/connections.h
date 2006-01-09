#ifndef _X11VNC_CONNECTIONS_H
#define _X11VNC_CONNECTIONS_H

/* -- connections.h -- */

extern char vnc_connect_str[];
extern Atom vnc_connect_prop;

extern int all_clients_initialized(void);
extern char *list_clients(void);
extern int new_fb_size_clients(rfbScreenInfoPtr s);
extern void close_all_clients(void);
extern void close_clients(char *str);
extern void set_client_input(char *str);
extern void set_child_info(void);
extern void reverse_connect(char *str);
extern void set_vnc_connect_prop(char *str);
extern void read_vnc_connect_prop(void);
extern void check_connect_inputs(void);
extern void check_gui_inputs(void);
extern enum rfbNewClientAction new_client(rfbClientPtr client);
extern void start_client_info_sock(char *host_port_cookie);
extern void send_client_info(char *str);
extern void check_new_clients(void);

#endif /* _X11VNC_CONNECTIONS_H */

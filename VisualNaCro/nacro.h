#ifndef NACRO_H
#define NACRO_H

#ifdef SWIG
%module nacro

%{

/* types used */

/* 0=false, every other value=true */
typedef int bool_t;

/* a keysym: identical with ASCII for values between 0-127 */
typedef int keysym_t;

/* this can be negative, because of a new origin set via visual grep */
typedef int coordinate_t;

/* left button is 1<<0, middle button is 1<<1, right button is 1<<2 */
typedef unsigned char buttons_t;

/* this is sort of a "file descriptor" for the proxy */
typedef int resource_t;

/* the timeout, specified in microseconds, for process() and friends */
typedef double timeout_t;

/* the return values of process() and friends */
typedef int result_t;
/*
%constant int RESULT_TIMEOUT=1;
%constant int RESULT_KEY=2;
%constant int RESULT_MOUSE=4;
%constant int RESULT_TEXT_CLIENT=8;
%constant int RESULT_TEXT_CLIENT=16;
%constant int RESULT_SCREEN=32;
%constant int RESULT_FOUNDIMAGE=64;
%constant int RESULT_SHUTDOWN=128;
*/

%}

#endif // SWIG

typedef int bool_t;
typedef int keysym_t;
typedef int coordinate_t;
typedef unsigned char buttons_t;
typedef int resource_t;
typedef double timeout_t;
typedef int result_t;
#define RESULT_TIMEOUT 1
#define  RESULT_KEY 2
#define  RESULT_MOUSE 4
#define  RESULT_TEXT_CLIENT 8
#define  RESULT_TEXT_SERVER 16
#define  RESULT_SCREEN 32
#define  RESULT_FOUNDIMAGE 64
#define  RESULT_SHUTDOWN 128

/* init/shutdown */

resource_t initvnc(const char* server,int serverPort,int listenPort);
void closevnc(resource_t res);

/* run the event loop for a while: process() and friends:
 * process() returns only on timeout,
 * waitforanything returns on any event (input, output or timeout),
 * waitforupdate() returns only on timeout or screen update,
 * waitforinput() returns only on timeout or user input,
 * visualgrep() returns only on timeout or if the specified PNM was found
 * 	(in that case, x_origin and y_origin are set to the upper left
 * 	 corner of the matched image). */

result_t process(resource_t res,timeout_t seconds);
result_t waitforanything(resource_t res,timeout_t seconds);
result_t waitforupdate(resource_t res,timeout_t seconds);
result_t waitforinput(resource_t res,timeout_t seconds);
result_t visualgrep(resource_t res,const char* filename,timeout_t seconds);

/* inspect last events */

keysym_t getkeysym(resource_t res);
bool_t getkeydown(resource_t res);

coordinate_t getx(resource_t res);
coordinate_t gety(resource_t res);
buttons_t getbuttons(resource_t res);

const char *gettext_client(resource_t res);
const char *gettext_server(resource_t res);

/* send events to the server */

bool_t sendkey(resource_t res,keysym_t keysym,bool_t keydown);
bool_t sendascii(resource_t res,const char *string);
bool_t sendmouse(resource_t res,coordinate_t x,coordinate_t y,buttons_t buttons);
bool_t sendtext(resource_t res, const char *string);
bool_t sendtext_to_server(resource_t res, const char *string);

/* for visual grepping */

coordinate_t getxorigin(resource_t res);
coordinate_t getyorigin(resource_t res);

bool_t savepnm(resource_t res,const char* filename,coordinate_t x1, coordinate_t y1, coordinate_t x2, coordinate_t y2);

result_t displaypnm(resource_t res, const char *filename, coordinate_t x, coordinate_t y, bool_t border, timeout_t timeout);

/* this displays an overlay which is shown for a certain time */

result_t alert(resource_t res,const char* message,timeout_t timeout);

/* display a rectangular rubber band between (x0, y0) and the current
   mouse pointer, as long as a button us pressed. */

result_t rubberband(resource_t res, coordinate_t x0, coordinate_t y0);

#endif

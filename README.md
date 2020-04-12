[![Build Status](https://travis-ci.org/LibVNC/libvncserver.svg?branch=master)](https://travis-ci.org/LibVNC/libvncserver)
[![Build status](https://ci.appveyor.com/api/projects/status/fao6m1md3q4g2bwn/branch/master?svg=true)](https://ci.appveyor.com/project/bk138/libvncserver/branch/master)
[![Help making this possible](https://img.shields.io/badge/liberapay-donate-yellow.png)](https://liberapay.com/LibVNC/donate)

LibVNCServer: A library for easy implementation of a VNC server.
Copyright (C) 2001-2003 Johannes E. Schindelin

If you already used LibVNCServer, you probably want to read [NEWS](NEWS.md).

What is it?
===========

[VNC](https://en.wikipedia.org/wiki/Virtual_Network_Computing) is a set of programs
using the [RFB (Remote Frame Buffer)](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst)
protocol. They are designed to "export" a frame buffer via net (if you don't know VNC, I
suggest you read "Basics" below). It is already in wide use for
administration, but it is not that easy to program a server yourself.

This has been changed by LibVNCServer.

There are several examples included, both for [servers](./examples) and 
[clients](./client_examples).

These examples are not too well documented, but easy straight forward and a
good starting point.

Try 'example', a shared scribble sheet: it outputs on which port it listens (default: 5900), so it is
display 0. To view, call
	`vncviewer :0`
You should see a sheet with a gradient and "Hello World!" written on it. Try
to paint something. Note that every time you click, there is some bigger blot,
whereas when you drag the mouse while clicked you draw a line. The size of the
blot depends on the mouse button you click. Open a second vncviewer with
the same parameters and watch it as you paint in the other window. This also
works over internet. You just have to know either the name or the IP of your
machine. Then it is
	`vncviewer machine.where.example.runs.com:0`
or similar for the remote client. Now you are ready to type something. Be sure
that your mouse sits still, because every time the mouse moves, the cursor is
reset to the position of the pointer! If you are done with that demo, press
the down or up arrows. If your viewer supports it, then the dimensions of the
sheet change. Just press Escape in the viewer. Note that the server still
runs, even if you closed both windows. When you reconnect now, everything you
painted and wrote is still there. You can press "Page Up" for a blank page.

The demo 'pnmshow' is much simpler: you either provide a filename as argument
or pipe a file through stdin. Note that the file has to be a raw pnm/ppm file,
i.e. a truecolour graphics. Only the Escape key is implemented. This may be
the best starting point if you want to learn how to use LibVNCServer. You
are confronted with the fact that the bytes per pixel can only be 8, 16 or 32.

If you want to build a VNC client instead, please have a look at the [various
client examples](./client_examples).

Projects using it
=================

The [homepage has a tentative list](https://libvnc.github.io/#projects-using) of
all the projects using either LibVNCServer or LibVNCClient or both.

How to build
============

LibVNCServer uses CMake, so you can build via:

    mkdir build
    cd build
    cmake ..
    cmake --build .

Crypto support in LibVNCClient and LibVNCServer can use different backends:

 * OpenSSL   (`-DWITH_OPENSSL=ON -DWITH_GCRYPT=OFF`)
   * Supports all authentication methods in LibVNCClient and LibVNCServer.
   * Supports WebSockets in LibVNCServer.
 * Libgcrypt (`-DWITH_OPENSSL=OFF -DWITH_GCRYPT=ON`)
   * Supports all authentication methods in LibVNCClient and LibVNCServer.
   * Supports WebSockets in LibVNCServer.
 * Included  (`-DWITH_OPENSSL=OFF -DWITH_GCRYPT=OFF`)
   * Supports _only VNC authentication_ in LibVNCClient and LibVNCServer.
   * Supports WebSockets in LibVNCServer.

Transport Layer Security support in LibVNCClient and LibVNCServer can use:

 * OpenSSL (`-DWITH_OPENSSL=ON -DWITH_GNUTLS=OFF`)
 * GnuTLS  (`-DWITH_OPENSSL=OFF -DWITH_GNUTLS=ON`)

For some more comprehensive examples that include installation of dependencies, see
the [Unix CI](.travis.yml) and [Windows CI](.appveyor.yml) build setups.

Crosscompiling from Unix to Android
-----------------------------------

See https://developer.android.com/ndk/guides/cmake.html as a reference,
but basically it boils down to:

    mkdir build
    cd build
    cmake .. -DANDROID_NDK=<path> -DCMAKE_TOOLCHAIN_FILE=<path> -DANDROID_NATIVE_API_LEVEL=<API level you want> -DWITH_PNG=OFF # NDK not shipping png per default
    cmake --build .

Crosscompiling from Linux to Windows
------------------------------------

Tested with MinGW-w64 on Debian, which you should install via `sudo apt install mingw-w64`.
You can make use of the [provided toolchainfile](cmake/Toolchain-cross-mingw32-linux.cmake).
It sets CMake to expect (optional) win32 dependencies like libjpeg and friends
in the `deps` directory. Note that you need (probably self-built) development packages for
win32, the `-dev` packages coming with your distribution won't work.


	mkdir build
	cd build
	cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-cross-mingw32-linux.cmake ..
	cmake --build .

How to use
==========

To make a server, you just have to initialise a server structure using the
function rfbDefaultScreenInit, like

	rfbScreenInfoPtr rfbScreen = rfbGetScreen(argc,argv,width,height,8,3,bpp);

where byte per pixel should be 1, 2 or 4. If performance doesn't matter,
you may try bpp=3 (internally one cannot use native data types in this
case; if you want to use this, look at pnmshow24).


You then can set hooks and io functions (see below) or other
options (see below).

And you allocate the frame buffer like this:

	rfbScreen->frameBuffer = (char*)malloc(width*height*bpp);

After that, you initialize the server, like

	rfbInitServer(rfbScreen);

You can use a blocking event loop, a background (pthread based) event loop,
or implement your own using the rfbProcessEvents function.

Making it interactive
---------------------

Input is handled by IO functions (see below).

Whenever you change something in the frame buffer, call rfbMarkRectAsModified.

Utility functions
-----------------

Whenever you draw something, you have to call

	rfbMarkRectAsModified(screen,x1,y1,x2,y2).

This tells LibVNCServer to send updates to all connected clients.

Before you draw something, be sure to call

	rfbUndrawCursor(screen).

This tells LibVNCServer to hide the cursor.
Remark: There are vncviewers out there, which know a cursor encoding, so
that network traffic is low, and also the cursor doesn't need to be
drawn the cursor every time an update is sent. LibVNCServer handles
all the details. Just set the cursor and don't bother any more.

To set the mouse coordinates (or emulate mouse clicks), call

	rfbDefaultPtrAddEvent(buttonMask,x,y,cl);

IMPORTANT: do this at the end of your function, because this actually draws
the cursor if no cursor encoding is active.

What is the difference between rfbScreenInfoPtr and rfbClientPtr?
-----------------------------------------------------------------

The rfbScreenInfoPtr is a pointer to a rfbScreenInfo structure, which
holds information about the server, like pixel format, io functions,
frame buffer etc.

The rfbClientPtr is a pointer to an rfbClientRec structure, which holds
information about a client, like pixel format, socket of the
connection, etc.

A server can have several clients, but needn't have any. So, if you
have a server and three clients are connected, you have one instance
of a rfbScreenInfo and three instances of rfbClientRec's.

The rfbClientRec structure holds a member

	rfbScreenInfoPtr screen

which points to the server and a member

	rfbClientPtr next

to the next client.

The rfbScreenInfo structure holds a member

	rfbClientPtr rfbClientHead

which points to the first client.

So, to access the server from the client structure, you use client->screen.
To access all clients from a server, get screen->rfbClientHead and
iterate using client->next.

If you change client settings, be sure to use the provided iterator

	rfbGetClientIterator(rfbScreen)

with

	rfbClientIteratorNext(iterator)

and
	
	rfbReleaseClientIterator

to prevent thread clashes.

Other options
-------------

These options have to be set between rfbGetScreen and rfbInitServer.

If you already have a socket to talk to, just set rfbScreen->inetdSock
(originally this is for inetd handling, but why not use it for your purpose?).

To also start an HTTP server (running on port 5800+display_number), you have
to set rfbScreen->httpdDir to a directory containing vncviewer.jar and
index.vnc (like the included "webclients" directory).

Hooks and IO functions
----------------------

There exist the following IO functions as members of rfbScreen:
kbdAddEvent, kbdReleaseAllKeys, ptrAddEvent and setXCutText

	kbdAddEvent(rfbBool down,rfbKeySym key,rfbClientPtr cl)

is called when a key is pressed.

	kbdReleaseAllKeys(rfbClientPtr cl)

is not called at all (maybe in the future).

	ptrAddEvent(int buttonMask,int x,int y,rfbClientPtr cl)

is called when the mouse moves or a button is pressed.
 WARNING: if you want to have proper cursor handling, call

	rfbDefaultPtrAddEvent(buttonMask,x,y,cl)

in your own function. This sets the coordinates of the cursor.

	setXCutText(char* str,int len,rfbClientPtr cl)

is called when the selection changes.

There are only two hooks:

	newClientHook(rfbClientPtr cl)

is called when a new client has connected.

	displayHook

is called just before a frame buffer update is sent.

You can also override the following methods:

	getCursorPtr(rfbClientPtr cl)

This could be used to make an animated cursor (if you really want ...)

	setTranslateFunction(rfbClientPtr cl)

If you insist on colour maps or something more obscure, you have to
implement this. Default is a trueColour mapping.

Cursor handling
---------------

The screen holds a pointer

	rfbCursorPtr cursor

to the current cursor. Whenever you set it, remember that any dynamically
created cursor (like return value from rfbMakeXCursor) is not free'd!

The rfbCursor structure consists mainly of a mask and a source. The mask
describes, which pixels are drawn for the cursor (a cursor needn't be
rectangular). The source describes, which colour those pixels should have.

The standard is an XCursor: a cursor with a foreground and a background
colour (stored in `backRed`,`backGreen`,`backBlue` and the same for foreground
in a range from 0-0xffff). Therefore, the arrays `mask` and `source`
contain pixels as single bits stored in bytes in MSB order. The rows are
padded, such that each row begins with a new byte (i.e. a 10x4
cursor's mask has 2x4 bytes, because 2 bytes are needed to hold 10 bits).

It is however very easy to make a cursor like this:

	char* cur="    "
              " xx "
	          " x  "
	          "    ";
	char* mask="xxxx"
               "xxxx"
	           "xxxx"
	           "xxx ";
    rfbCursorPtr c=rfbMakeXCursor(4,4,cur,mask);

You can even set `mask` to NULL in this call and LibVNCServer will calculate
a mask for you (dynamically, so you have to free it yourself).

There is also an array named `richSource` for colourful cursors. They have
the same format as the frameBuffer (i.e. if the server is 32 bit,
a 10x4 cursor has 4x10x4 bytes).

Using Websockets
----------------

You can try out the built-in websockets support by starting the example server
from the [webclients](webclients) directory via `../examples/example`. It's
important to _not_ start from within the `examples` directory as otherwise the
server program won't find its HTTP index file. The server program will tell you
a URL to point your web browser to. There, you can click on the noVNC-Button to
connect using the noVNC viewer git submodule (installable via
`git submodule update --init`).

### Using Secure Websockets

If you don't already have an SSL cert that's trusted by your browser, the most
comfortable way to create one is using [minica](https://github.com/jsha/minica).
On Debian-based distros, you can install it via `sudo apt install minica`, on
MacOS via `brew install minica`.

Go to the webclients directory and create host and CA certs via:

	cd webclients
	minica -org "LibVNC" $(hostname)

Trust the cert in your browser by importing the created `cacert.crt`, e.g. for
Firefox go to Options->Privacy & Security->View Certificates->Authorities and
import the created `cacert.crt`, tick the checkbox to use it for trusting
websites. For other browsers, the process is similar.

Then, you can finally start the example server, giving it the created host
key and cert:

	../examples/example -sslkeyfile $(hostname).key -sslcertfile $(hostname).crt

The server program will tell you a URL to point your web browser to. There,
you can click on the noVNC-encrypted-connection-button to connect using the
bundled noVNC viewer using an encrypted Websockets connection.



Basics
======

VNC (Virtual network computing) works like this: You set up a server and can
connect to it via vncviewers. The communication uses a protocol named RFB
(Remote Frame Buffer). If the server supports WebSockets (which LibVNCServer does), 
you can also connect using an in-browser VNC viewer like [noVNC](https://novnc.com). 

There exist several encodings for VNC, which are used to compress the regions
which have changed before they are sent to the client. A client need not be
able to understand every encoding, but at least Raw encoding. Which encoding
it understands is negotiated by the RFB protocol.

If you want to know how RFB works, please take the time and read the [protocol
specification](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst),
it is very well written and contains a lot of prose that really explains how stuff
works.

There is the possibility to set a password, which is also negotiated by the
RFB protocol, but IT IS NOT SECURE. Anybody sniffing your net can get the
password. You really should tunnel through SSH.

Commercial Use
==============

At the beginning of this project Dscho, the original author, would have
liked to make it a BSD license. However, it is based on plenty of GPL'ed
code, so it has to be a GPL.

The people at AT&T worked really well to produce something as clean and lean
as VNC. The managers decided that for their fame, they would release the
program for free. But not only that! They realized that by releasing also
the code for free, VNC would become an evolving little child, conquering
new worlds, making its parents very proud. As well they can be! To protect
this innovation, they decided to make it GPL, not BSD. The principal
difference is: You can make closed source programs deriving from BSD, not
from GPL. You have to give proper credit with both.

Now, why not BSD? Well, imagine your child being some famous actor. Along
comes a manager who exploits your child exclusively, that is: nobody else
can profit from the child, it itself included. Got it?

What reason do you have now to use this library commercially?

Several: You don't have to give away your product. Then you have effectively
circumvented the GPL, because you have the benefits of other's work and you
don't give back anything. Not good.

Better: Use a concept like MySQL. This is free software, however, they make
money with it. If you want something implemented, you have the choice:
Ask them to do it (and pay a fair price), or do it yourself, normally giving
back your enhancements to the free world of computing.

License
-------

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.dfdf

Contact
=======

* To file an issue, go to https://github.com/LibVNC/libvncserver/issues
* For non-public contact mail dontmind at sdf dot org


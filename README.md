[![Build Status](https://travis-ci.org/LibVNC/libvncserver.svg?branch=master)](https://travis-ci.org/LibVNC/libvncserver)
[![Build status](https://ci.appveyor.com/api/projects/status/fao6m1md3q4g2bwn/branch/master?svg=true)](https://ci.appveyor.com/project/bk138/libvncserver/branch/master)
[![Help making this possible](https://img.shields.io/badge/liberapay-donate-yellow.png)](https://liberapay.com/LibVNC/donate)

LibVNCServer: A library for easy implementation of a VNC server.
Copyright (C) 2001-2003 Johannes E. Schindelin

If you already used LibVNCServer, you probably want to read NEWS.

What is it?
===========

VNC is a set of programs using the RFB (Remote Frame Buffer) protocol. They
are designed to "export" a frame buffer via net (if you don't know VNC, I
suggest you read "Basics" below). It is already in wide use for
administration, but it is not that easy to program a server yourself.

This has been changed by LibVNCServer.

There are two examples included:
 - example, a shared scribble sheet
 - pnmshow, a program to show PNMs (pictures) over the net.

The examples are not too well documented, but easy straight forward and a
good starting point.

Try example: it outputs on which port it listens (default: 5900), so it is
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

The demo pnmshow is much simpler: you either provide a filename as argument
or pipe a file through stdin. Note that the file has to be a raw pnm/ppm file,
i.e. a truecolour graphics. Only the Escape key is implemented. This may be
the best starting point if you want to learn how to use LibVNCServer. You
are confronted with the fact that the bytes per pixel can only be 8, 16 or 32.

If you want to build a VNC client instead, please have a look at the [various
client examples](./client_examples).

Projects using it
=================

VNC for KDE
http://www.tjansen.de/krfb

GemsVNC
http://www.elilabs.com/~rj/gemsvnc/

VNC for Netware
http://forge.novell.com/modules/xfmod/project/?vncnw

RDesktop
http://rdesktop.sourceforge.net

VNCpp
https://github.com/ocrespo/VNCpp

VirtualBox
https://www.virtualbox.org/

Veyon
https://veyon.io

Mail us if your application is missing!

How to build
============

LibVNCServer uses CMake, so you can build via:

    mkdir build
    cd build
    cmake ..
    cmake --build .

For some more comprehensive examples that include installation of dependencies, see
the [Unix CI](.travis.yml) and [Windows CI](.appveyor.yml) build setups.

Crosscompiling involves some more advanced command line switches but is easily possible
as well.

For instance, building for Android (see https://developer.android.com/ndk/guides/cmake.html as a reference):

    mkdir build
    cd build
    cmake .. -DANDROID_NDK=<path> -DCMAKE_TOOLCHAIN_FILE=<path> -DANDROID_NATIVE_API_LEVEL=<API level you want> -DWITH_PNG=OFF # NDK not shipping png per default
    cmake --build .


How to use
==========

To make a server, you just have to initialise a server structure using the
function rfbDefaultScreenInit, like
  rfbScreenInfoPtr rfbScreen =
    rfbGetScreen(argc,argv,width,height,8,3,bpp);
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
colour (stored in backRed,backGreen,backBlue and the same for foreground
in a range from 0-0xffff). Therefore, the arrays "mask" and "source"
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

You can even set "mask" to NULL in this call and LibVNCServer will calculate
a mask for you (dynamically, so you have to free it yourself).

There is also an array named "richSource" for colourful cursors. They have
the same format as the frameBuffer (i.e. if the server is 32 bit,
a 10x4 cursor has 4x10x4 bytes).

Using Websockets
----------------

You can try out the built-in websockets support by starting the example server
from the build directory via `examples/example`. It's important to _not_ start
from within the `examples` directory as otherwise the server program won't find
its HTTP index file. The server program will tell you a URL to point your web
browser to. There, you can click on the noVNC-Button to connect using the bundled
noVNC viewer.

History
=======

LibVNCServer is based on Tridia VNC and OSXvnc, which in turn are based on
the original code from ORL/AT&T.

When I began hacking with computers, my first interest was speed. So, when I
got around assembler, I programmed the floppy to do much of the work, because
its clock rate was higher than that of my C64. This was my first experience
with client/server techniques.

When I came around Xwindows (much later), I was at once intrigued by the
elegance of such connectedness between the different computers. I used it
a lot - not the least priority lay on games. However, when I tried it over
modem from home, it was no longer that much fun.

When I started working with ASP (Application Service Provider) programs, I
tumbled across Tarantella and Citrix. Being a security fanatic, the idea of
running a server on windows didn't appeal to me, so Citrix went down the
basket. However, Tarantella has its own problems (security as well as the
high price). But at the same time somebody told me about this "great little
administrator's tool" named VNC. Being used to windows programs' sizes, the
surprise was reciprocal inverse to the size of VNC!

At the same time, the program "rdesktop" (a native Linux client for the
Terminal Services of Windows servers) came to my attention. There where even
works under way to make a protocol converter "rdp2vnc" out of this. However,
my primary goal was a slow connection and rdp2vnc could only speak RRE
encoding, which is not that funny with just 5kB/s. Tim Edmonds, the original
author of rdp2vnc, suggested that I adapt it to Hextile Encoding, which is
better. I first tried that, but had no success at all (crunchy pictures).

Also, I liked the idea of an HTTP server included and possibly other
encodings like the Tight Encodings from Const Kaplinsky. So I started looking
for libraries implementing a VNC server where I could steal what I can't make.
I found some programs based on the demo server from AT&T, which was also the
basis for rdp2vnc (can only speak Raw and RRE encoding). There were some
rumors that GGI has a VNC backend, but I didn't find any code, so probably
there wasn't a working version anyway.

All of a sudden, everything changed: I read on freshmeat that "OSXvnc" was
released. I looked at the code and it was not much of a problem to work out
a simple server - using every functionality there is in Xvnc. It became clear
to me that I *had* to build a library out of it, so everybody can use it.
Every change, every new feature can propagate to every user of it.

It also makes everything easier:
 You don't care about the cursor, once set (or use the standard cursor).
You don't care about those sockets. You don't care about encodings.
You just change your frame buffer and inform the library about it. Every once
in a while you call rfbProcessEvents and that's it.

Basics
======

VNC (Virtual network computing) works like this: You set up a server and can
connect to it via vncviewers. The communication uses a protocol named RFB
(Remote Frame Buffer). If the server supports HTTP, you can also connect
using a java enabled browser. In this case, the server sends back a
vncviewer applet with the correct settings.

There exist several encodings for VNC, which are used to compress the regions
which have changed before they are sent to the client. A client need not be
able to understand every encoding, but at least Raw encoding. Which encoding
it understands is negotiated by the RFB protocol.

The following encodings are known to me:
Raw, RRE, CoRRE, Hextile, CopyRect from the original AT&T code and
Tight, ZLib, LastRect, XCursor, RichCursor from Const Kaplinsky et al.

If you are using a modem, you want to try the "new" encodings. Especially
with my 56k modem I like ZLib or Tight with Quality 0. In my tests, it even
beats Tarantella.

There is the possibility to set a password, which is also negotiated by the
RFB protocol, but IT IS NOT SECURE. Anybody sniffing your net can get the
password. You really should tunnel through SSH.

Windows or: why do you do that to me?
=====================================

If you love products from Redmod, you better skip this paragraph.
I am always amazed how people react whenever Microsoft(tm) puts in some
features into their products which were around for a long time. Especially
reporters seem to not know dick about what they are reporting about! But
what is every time annoying again, is that they don't do it right. Every
concept has its new name (remember what enumerators used to be until
Mickeysoft(tm) claimed that enumerators are what we thought were iterators.
Yeah right, enumerators are also containers. They are not separated. Muddy.)

There are three packages you want to get hold of: zlib, jpeg and pthreads.
The latter is not strictly necessary, but when you put something like this
into your source:

```
#define MUTEX(s)
	struct {
		int something;
		MUTEX(latex);
	}
```

Microsoft's C++ compiler doesn't do it. It complains that this is an error.
This, however, is how I implemented mutexes in case you don't need pthreads,
and so don't need the mutex.

You can find the packages at
http://www.gimp.org/win32/extralibs-dev-20001007.zip

Thanks go to all the GIMP team!

What are those other targets in the Makefile?
=============================================

OSXvnc-server is the original OSXvnc adapted to use the library, which was in
turn adapted from OSXvnc. As you easily can see, the OSX dependend part is
minimal.

storepasswd is the original program to save a vnc style password in a file.
Unfortunately, authentication as every vncviewer speaks it means the server
has to know the plain password. You really should tunnel via ssh or use
your own PasswordCheck to build a PIN/TAN system.

sratest is a test unit. Run it to assert correct behaviour of sraRegion. I
wrote this to test my iterator implementation.

blooptest is a test of pthreads. It is just the example, but with a background
loop to hunt down thread lockups.

pnmshow24 is like pnmshow, but it uses 3 bytes/pixel internally, which is not
as efficient as 4 bytes/pixel for translation, because there is no native data
type of that size, so you have to memcpy pixels and be real cautious with
endianness. Anyway, it works.

fontsel is a test for rfbSelectBox and rfbLoadConsoleFont. If you have Linux
console fonts, you can browse them via VNC. Directory browsing not implemented
yet :-(

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


[![Build Status](https://travis-ci.org/LibVNC/libvncserver.svg?branch=master)](https://travis-ci.org/LibVNC/libvncserver)
[![Build status](https://ci.appveyor.com/api/projects/status/fao6m1md3q4g2bwn/branch/master?svg=true)](https://ci.appveyor.com/project/bk138/libvncserver/branch/master)
[![Help making this possible](https://img.shields.io/badge/liberapay-donate-yellow.png)](https://liberapay.com/LibVNC/donate) [![Join the chat at https://gitter.im/LibVNC/libvncserver](https://badges.gitter.im/LibVNC/libvncserver.svg)](https://gitter.im/LibVNC/libvncserver?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

LibVNCServer: A library for easy implementation of a VNC server.
Copyright (C) 2001-2003 Johannes E. Schindelin

If you already used LibVNCServer, you probably want to read [NEWS](NEWS.md).

What is it?
===========

[VNC](https://en.wikipedia.org/wiki/Virtual_Network_Computing) is a set of programs
using the [RFB (Remote Frame Buffer)](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst)
protocol. They are designed to "export" a frame buffer via net: you set up a server and can
connect to it via VNC viewers. If the server supports WebSockets (which LibVNCServer does), 
you can also connect using an in-browser VNC viewer like [noVNC](https://novnc.com). 

It is already in wide use for administration, but it is not that easy to program a server yourself.

This has been changed by LibVNCServer.

Projects using it
=================

The [homepage has a tentative list](https://libvnc.github.io/#projects-using) of
all the projects using either LibVNCServer or LibVNCClient or both.

RFB Protocol Support Status
===========================

## [Security Types](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#712security)

|Name               |Number      | LibVNCServer | LibVNCClient |
|-------------------|------------|--------------|--------------|
|None               |          1 |            ✔ |            ✔ |
|VNC Authentication |          2 |            ✔ |            ✔ |
|SASL               |         20 |              |            ✔ |
|MSLogon            | 0xfffffffa |              |            ✔ |
|Apple ARD          |         30 |              |            ✔ |
|TLS                |         18 |              |            ✔ |
|VeNCrypt           |         19 |              |            ✔ |

## [Encodings](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#76encodings)

| Name     | Number | LibVNCServer | LibVNCClient |
|----------|--------|--------------|--------------|
| Raw      | 1      | ✔            | ✔            |
| CopyRect | 2      | ✔            | ✔            |
| RRE      | 3      | ✔            | ✔            |
| CoRRE    | 4      | ✔            | ✔            |
| Hextile  | 5      | ✔            | ✔            |
| Zlib     | 6      | ✔            | ✔            |
| Tight    | 7      | ✔            | ✔            |
| Zlibhex  | 8      | ✔            |              |
| Ultra    | 9      | ✔            | ✔            |
| TRLE     | 15     |              | ✔            |
| ZRLE     | 16     | ✔            | ✔            |
| ZYWRLE   | 17     | ✔            | ✔            |
| TightPNG | -260   | ✔            |              |


How to build
============

LibVNCServer uses CMake, which you can download [here](https://cmake.org/download/)
or, better yet, install using your platform's package manager (apt, yum, brew, macports,
chocolatey, etc.).

You can then build via:

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
    cmake .. -DANDROID_NDK=<path> -DCMAKE_TOOLCHAIN_FILE=<path> -DANDROID_NATIVE_API_LEVEL=<API level you want>
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

See the [LibVNCServer API intro documentation](https://libvnc.github.io/doc/html/libvncserver_doc.html)
for how to create a server instance, wire up input handlers and handle cursors.

In case you prefer to learn LibVNCServer by example, have a look at the servers in the
[examples](examples) directory. 

For LibVNCClient, examples can be found in [client_examples](client_examples).

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Contact
=======

* To file an issue, go to https://github.com/LibVNC/libvncserver/issues
* For non-public contact please see [SECURITY.md](SECURITY.md).


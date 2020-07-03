high-prio:
----------
- [Merge MulticastVNC](https://github.com/LibVNC/libvncserver/issues/394) 
- [style fixes: use Linux' coding guidelines & ANSIfy tightvnc-filetransfer](https://github.com/LibVNC/libvncserver/issues/395)
- [Implement encryption in libvncserver](https://github.com/LibVNC/libvncserver/issues/396)


maybe-later:
------------

- selectbox: scroll bars
- authentification schemes (secure vnc)
	- IO function ptr exists; now explain how to tunnel and implement a
	- client address restriction scheme.

- make SDLvncviewer more versatile
	- test for missing keys (especially "[]{}" with ./examples/mac),
	- map Apple/Linux/Windows keys onto each other,
	- handle selection
	- handle scroll wheel
- LibVNCClient cleanup: prefix with "rfbClient", and make sure it does
	not deliberately die() or exit() anywhere!
- make corre work again (libvncclient or libvncserver?)
- teach SDLvncviewer about CopyRect...
- implement "-record" in libvncclient
- implement QoS for Windows in libvncclient

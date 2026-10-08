#ifndef LIBVNCSERVER_SRC_LIBVNCCLIENT_ARDAUTH_H
#define LIBVNCSERVER_SRC_LIBVNCCLIENT_ARDAUTH_H

#include <rfb/rfbclient.h>

rfbBool rfbClientHandleARDAuth(rfbClient *client, uint32_t authScheme);

#endif

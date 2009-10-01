/*
 *  Copyright (C) 2009 Vic Lee.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <rfb/rfbclient.h>
#include <errno.h>
#include "tls.h"

#ifdef LIBVNCSERVER_WITH_CLIENT_TLS

static const int rfbCertTypePriority[] = { GNUTLS_CRT_X509, 0 };
static const int rfbProtoPriority[]= { GNUTLS_TLS1_1, GNUTLS_TLS1_0, GNUTLS_SSL3, 0 };
static const int rfbKXPriority[] = {GNUTLS_KX_DHE_DSS, GNUTLS_KX_RSA, GNUTLS_KX_DHE_RSA, GNUTLS_KX_SRP, 0};
static const int rfbKXAnon[] = {GNUTLS_KX_ANON_DH, 0};

#define DH_BITS 1024
static gnutls_dh_params_t rfbDHParams;

static rfbBool rfbTLSInitialized = FALSE;

static rfbBool
InitializeTLS(void)
{
  int ret;

  if (rfbTLSInitialized) return TRUE;
  if ((ret = gnutls_global_init()) < 0 ||
      (ret = gnutls_dh_params_init(&rfbDHParams)) < 0 ||
      (ret = gnutls_dh_params_generate2(rfbDHParams, DH_BITS)) < 0)
  {
    rfbClientLog("Failed to initialized GnuTLS: %s.\n", gnutls_strerror(ret));
    return FALSE;
  }
  rfbClientLog("GnuTLS initialized.\n");
  rfbTLSInitialized = TRUE;
  return TRUE;
}

static ssize_t
PushTLS(gnutls_transport_ptr_t transport, const void *data, size_t len)
{
  rfbClient *client = (rfbClient*)transport;
  int ret;

  while (1)
  {
    ret = write(client->sock, data, len);
    if (ret < 0)
    {
      if (errno == EINTR) continue;
      return -1;
    }
    return ret;
  }
}


static ssize_t
PullTLS(gnutls_transport_ptr_t transport, void *data, size_t len)
{
  rfbClient *client = (rfbClient*)transport;
  int ret;

  while (1)
  {
    ret = read(client->sock, data, len);
    if (ret < 0)
    {
      if (errno == EINTR) continue;
      return -1;
    }
    return ret;
  }
}

static rfbBool
InitializeTLSSession(rfbClient* client, rfbBool anonTLS)
{
  int ret;

  if (client->tlsSession) return TRUE;

  if ((ret = gnutls_init(&client->tlsSession, GNUTLS_CLIENT)) < 0)
  {
    rfbClientLog("Failed to initialized TLS session: %s.\n", gnutls_strerror(ret));
    return FALSE;
  }

  if ((ret = gnutls_set_default_priority(client->tlsSession)) < 0 ||
      (ret = gnutls_kx_set_priority(client->tlsSession, anonTLS ? rfbKXAnon : rfbKXPriority)) < 0 ||
      (ret = gnutls_certificate_type_set_priority(client->tlsSession, rfbCertTypePriority)) < 0 ||
      (ret = gnutls_protocol_set_priority(client->tlsSession, rfbProtoPriority)) < 0)
  {
    FreeTLS(client);
    rfbClientLog("Failed to set TLS priority: %s.\n", gnutls_strerror(ret));
    return FALSE;
  }

  gnutls_transport_set_ptr(client->tlsSession, (gnutls_transport_ptr_t)client);
  gnutls_transport_set_push_function(client->tlsSession, PushTLS);
  gnutls_transport_set_pull_function(client->tlsSession, PullTLS);

  rfbClientLog("TLS session initialized.\n");

  return TRUE;
}

static rfbBool
SetTLSAnonCredential(rfbClient* client)
{
  gnutls_anon_client_credentials anonCred;
  int ret;

  if ((ret = gnutls_anon_allocate_client_credentials(&anonCred)) < 0 ||
      (ret = gnutls_credentials_set(client->tlsSession, GNUTLS_CRD_ANON, anonCred)) < 0)
  {
    FreeTLS(client);
    rfbClientLog("Failed to create anonymous credentials: %s", gnutls_strerror(ret));
    return FALSE;
  }
  rfbClientLog("TLS anonymous credential created.\n");
  return TRUE;
}

static rfbBool
HandshakeTLS(rfbClient* client)
{
  int timeout = 15;
  int ret;

  while (timeout > 0 && (ret = gnutls_handshake(client->tlsSession)) < 0)
  {
    if (!gnutls_error_is_fatal(ret))
    {
      rfbClientLog("TLS handshake blocking.\n");
      sleep(1);
      timeout--;
      continue;
    }
    rfbClientLog("TLS handshake failed: %s.\n", gnutls_strerror(ret));
    FreeTLS(client);
    return FALSE;
  }

  if (timeout <= 0)
  {
    rfbClientLog("TLS handshake timeout.\n");
    FreeTLS(client);
    return FALSE;
  }

  rfbClientLog("TLS handshake done.\n");
  return TRUE;
}

#endif

rfbBool
HandleAnonTLSAuth(rfbClient* client)
{
#ifdef LIBVNCSERVER_WITH_CLIENT_TLS

  if (!InitializeTLS() || !InitializeTLSSession(client, TRUE)) return FALSE;

  if (!SetTLSAnonCredential(client)) return FALSE;

  if (!HandshakeTLS(client)) return FALSE;

  return TRUE;

#else
  rfbClientLog("TLS is not supported.\n");
  return FALSE;
#endif
}

rfbBool
HandleVeNCryptAuth(rfbClient* client)
{
#ifdef LIBVNCSERVER_WITH_CLIENT_TLS
  int ret;

  if (!InitializeTLS() || !InitializeTLSSession(client, FALSE)) return FALSE;

  /* TODO: read VeNCrypt version, etc */
  /* TODO: call GetCredential and set to TLS session */

  if (!HandshakeTLS(client)) return FALSE;

  /* TODO: validate certificate */

  return TRUE;

#else
  rfbClientLog("TLS is not supported.\n");
  return FALSE;
#endif
}

int
ReadFromTLS(rfbClient* client, char *out, unsigned int n)
{
#ifdef LIBVNCSERVER_WITH_CLIENT_TLS
  ssize_t ret;

  ret = gnutls_record_recv(client->tlsSession, out, n);
  if (ret >= 0) return ret;
  if (ret == GNUTLS_E_REHANDSHAKE || ret == GNUTLS_E_AGAIN)
  {
    errno = EAGAIN;
  } else
  {
    rfbClientLog("Error reading from TLS: %s.\n", gnutls_strerror(ret));
    errno = EINTR;
  }
  return -1;
#else
  rfbClientLog("TLS is not supported.\n");
  errno = EINTR;
  return -1;
#endif
}

int
WriteToTLS(rfbClient* client, char *buf, unsigned int n)
{
#ifdef LIBVNCSERVER_WITH_CLIENT_TLS
  unsigned int offset = 0;
  ssize_t ret;

  while (offset < n)
  {
    ret = gnutls_record_send(client->tlsSession, buf+offset, (size_t)(n-offset));
    if (ret == 0) continue;
    if (ret < 0)
    {
      if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED) continue;
      rfbClientLog("Error writing to TLS: %s.\n", gnutls_strerror(ret));
      return -1;
    }
    offset += (unsigned int)ret;
  }
  return offset;
#else
  rfbClientLog("TLS is not supported.\n");
  errno = EINTR;
  return -1;
#endif
}

void FreeTLS(rfbClient* client)
{
#ifdef LIBVNCSERVER_WITH_CLIENT_TLS
  if (client->tlsSession)
  {
    gnutls_deinit(client->tlsSession);
    client->tlsSession = NULL;
  }
#endif
}

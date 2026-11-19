/**
 * @example ardauthprobe.c
 *
 * Headless ARD auth probe client for testing one forced security type at a
 * time against a remote Screen Sharing / ARD server.
 *
 * Usage:
 *   VNC_USER=<username> VNC_PASS=<password> \
 *   ./ardauthprobe -auth <security-type> <host[:port]>
 *
 * Optional Kerberos overrides for auth type 35:
 *   VNC_ARD_REALM=<realm>
 *   VNC_ARD_CLIENT_PRINCIPAL=<client-principal>
 *   VNC_ARD_SERVICE_PRINCIPAL=<service-principal>
 *
 * Example:
 *   VNC_USER=alex VNC_PASS=secret \
 *   ./ardauthprobe -auth 33 Alexs-Mac-mini.local:5900
 *
 * ARD auth type samples:
 *   -auth 30   ARD DH
 *   -auth 33   ARD RSA-SRP
 *   -auth 35   ARD Kerberos GSS-API
 *   -auth 36   ARD Direct SRP
 *
 * Kerberos sample:
 *   VNC_USER=alex VNC_PASS=secret \
 *   VNC_ARD_REALM='LKDC:SHA1.XXXXXX' \
 *   VNC_ARD_SERVICE_PRINCIPAL='vnc/LKDC:SHA1.XXXXXX@LKDC:SHA1.XXXXXX' \
 *   ./ardauthprobe -auth 35 Alexs-Mac-mini.local:5900
 *
 * On success the probe prints one AUTH_OK line with the requested and selected
 * auth schemes plus the negotiated desktop name and size, then exits.
 */

#include <rfb/rfbclient.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void HandleRect(rfbClient *client, int x, int y, int w, int h) {
    (void)client;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

static rfbCredential *GetCredential(rfbClient *client, int credentialType) {
    const char *user = getenv("VNC_USER");
    const char *pass = getenv("VNC_PASS");
    rfbCredential *cred = NULL;

    (void)client;
    if (credentialType != rfbCredentialTypeUser)
        return NULL;
    if (!user || !pass) {
        rfbClientErr("Set VNC_USER and VNC_PASS in the environment.\n");
        return NULL;
    }

    cred = (rfbCredential *)calloc(1, sizeof(*cred));
    if (!cred)
        return NULL;
    cred->userCredential.username = strdup(user);
    cred->userCredential.password = strdup(pass);
    if (!cred->userCredential.username || !cred->userCredential.password) {
        free(cred->userCredential.username);
        free(cred->userCredential.password);
        free(cred);
        return NULL;
    }
    return cred;
}

/*
 * Kerberos-based ARD auth needs more than a username/password pair. These
 * environment variables let the probe supply the same realm/principal data we
 * discover externally from Keychain or prior captures without hardcoding it.
 */
static void ApplyOptionalKerberosConfig(rfbClient *client) {
    const char *realm = getenv("VNC_ARD_REALM");
    const char *client_principal = getenv("VNC_ARD_CLIENT_PRINCIPAL");
    const char *service_principal = getenv("VNC_ARD_SERVICE_PRINCIPAL");

    if (realm && *realm)
        rfbClientSetARDAuthRealm(client, realm);
    if (client_principal && *client_principal)
        rfbClientSetARDAuthClientPrincipal(client, client_principal);
    if (service_principal && *service_principal)
        rfbClientSetARDAuthServicePrincipal(client, service_principal);
}

static int ParseAuthType(const char *value, uint32_t *out) {
    char *end = NULL;
    unsigned long parsed = 0;

    if (!value || !out)
        return 0;
    parsed = strtoul(value, &end, 0);
    if (!end || *end != '\0' || parsed > 255)
        return 0;
    *out = (uint32_t)parsed;
    return 1;
}

int main(int argc, char **argv) {
    rfbClient *client = NULL;
    uint32_t auth_types[2] = {0, 0};
    int init_argc = 2;
    char *init_argv[3] = {NULL, NULL, NULL};

    if (argc < 4 || strcmp(argv[1], "-auth") != 0 ||
        !ParseAuthType(argv[2], &auth_types[0])) {
        fprintf(stderr, "usage: %s -auth <security-type> <host[:port]>\n",
                argv[0]);
        return 2;
    }

    client = rfbGetClient(8, 3, 4);
    if (!client) {
        fprintf(stderr, "failed to create VNC client\n");
        return 1;
    }

    client->GotFrameBufferUpdate = HandleRect;
    client->GetCredential = GetCredential;
    ApplyOptionalKerberosConfig(client);

    /* Restrict negotiation to the requested auth type so each probe run
     * answers one concrete question about server compatibility. */
    SetClientAuthSchemes(client, auth_types, -1);

    init_argv[0] = argv[0];
    init_argv[1] = argv[3];

    if (!rfbInitClient(client, &init_argc, init_argv)) {
        rfbClientCleanup(client);
        return 1;
    }

    fprintf(stderr,
            "AUTH_OK requested=%u selected=%u subauth=%u desktop=\"%s\" "
            "size=%dx%d\n",
            auth_types[0], client->authScheme, client->subAuthScheme,
            client->desktopName ? client->desktopName : "",
            client->width, client->height);

    rfbClientCleanup(client);
    return 0;
}

#include <rfb/rfbclient.h>

#include <stdlib.h>
#include <string.h>

#include "crypto.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <GSS/GSS.h>
#endif

#if defined(__APPLE__) && defined(LIBVNCSERVER_HAVE_LIBSSL) && defined(__has_include)
#if __has_include(<openssl/bn.h>)
#include <openssl/bn.h>
#define LIBVNCCLIENT_APPLE_HAS_OPENSSL_BN 1
#endif
#endif

#include "ardauth.h"

static void WriteBEU16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)((value >> 8) & 0xff);
    out[1] = (uint8_t)(value & 0xff);
}

static void WriteBEU32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)((value >> 24) & 0xff);
    out[1] = (uint8_t)((value >> 16) & 0xff);
    out[2] = (uint8_t)((value >> 8) & 0xff);
    out[3] = (uint8_t)(value & 0xff);
}

static uint16_t ReadBEU16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t ReadBEU32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t ReadBEU64(const uint8_t *p) {
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8) | (uint64_t)p[7];
}

static void FreeARDUserCredential(rfbCredential *cred) {
    if (!cred)
        return;
    free(cred->userCredential.username);
    free(cred->userCredential.password);
    free(cred);
}

static const char *GetARDSRPMethodName(uint32_t auth_type) {
    switch (auth_type) {
    case rfbARDAuthRSASRP:
        return "rsa-srp";
    case rfbARDAuthDirectSRP:
        return "direct-srp";
    default:
        return "srp";
    }
}

static int HashSHA512Parts(uint8_t out[SHA512_HASH_SIZE], const void **parts,
                           const size_t *part_lens, size_t part_count) {
    size_t i;
    size_t total_len = 0;
    uint8_t *buf = NULL;
    uint8_t *p = NULL;
    int ok;

    if (!out)
        return 0;
    for (i = 0; i < part_count; ++i) {
        if (parts[i] && part_lens[i] != 0)
            total_len += part_lens[i];
    }

    if (total_len == 0)
        return hash_sha512(out, "", 0);

    buf = (uint8_t *)malloc(total_len);
    if (!buf)
        return 0;
    p = buf;
    for (i = 0; i < part_count; ++i) {
        if (parts[i] && part_lens[i] != 0) {
            memcpy(p, parts[i], part_lens[i]);
            p += part_lens[i];
        }
    }
    ok = hash_sha512(out, buf, total_len);
    free(buf);
    return ok;
}

static int HashSHA512TwoParts(uint8_t out[SHA512_HASH_SIZE], const void *a,
                              size_t a_len, const void *b, size_t b_len) {
    const void *parts[2] = {a, b};
    const size_t lens[2] = {a_len, b_len};

    return HashSHA512Parts(out, parts, lens, 2);
}

static rfbBool ReadLengthPrefixedBlob(rfbClient *client, uint8_t **outbuf,
                                      uint32_t *outlen, const char *what) {
    uint8_t inhdr[4];
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (!outbuf || !outlen)
        return FALSE;
    *outbuf = NULL;
    *outlen = 0;
    if (!ReadFromRFBServer(client, (char *)inhdr, 4)) {
        rfbClientErr("ard auth: failed reading %s length\n", what);
        return FALSE;
    }
    n = ReadBEU32(inhdr);
    if (n == 0 || n > (1u << 20)) {
        rfbClientErr("ard auth: suspicious %s length=%u\n", what, (unsigned)n);
        return FALSE;
    }
    buf = (uint8_t *)malloc(n);
    if (!buf)
        return FALSE;
    if (!ReadFromRFBServer(client, (char *)buf, n)) {
        free(buf);
        return FALSE;
    }
    *outbuf = buf;
    *outlen = n;
    return TRUE;
}

static rfbBool WriteLengthPrefixedBlob(rfbClient *client, const uint8_t *buf,
                                       size_t len, const char *what);

#ifdef LIBVNCSERVER_HAVE_SASL
#define ARD_CLIENT_HAS_SASL_STATE(client) ((client)->saslconn != NULL)
#else
#define ARD_CLIENT_HAS_SASL_STATE(client) FALSE
#endif

static rfbBool ConsumeRSASRPServerFinalIfPresent(rfbClient *client) {
    uint8_t hdr[4];
    uint32_t n = 0;
    uint8_t *buf = NULL;
    int wm = WaitForMessage(client, 1500000);
    int r;

    if (wm < 0)
        return FALSE;
    if (wm == 0)
        return TRUE;
    if (client->buffered >= sizeof(hdr)) {
        memcpy(hdr, client->bufoutptr, sizeof(hdr));
    } else {
        if (client->buffered > 0 || client->tlsSession || ARD_CLIENT_HAS_SASL_STATE(client))
            return TRUE;
        r = recv(client->sock, (char *)hdr, sizeof(hdr), MSG_PEEK);
        if (r < (int)sizeof(hdr))
            return TRUE;
    }
    n = ReadBEU32(hdr);
    if (n < 16 || n > 4096)
        return TRUE;
    if (!ReadFromRFBServer(client, (char *)hdr, 4))
        return FALSE;
    buf = (uint8_t *)malloc(n);
    if (!buf)
        return FALSE;
    if (!ReadFromRFBServer(client, (char *)buf, n)) {
        free(buf);
        return FALSE;
    }
    free(buf);
    return TRUE;
}

static rfbBool HandleARDAuthDH(rfbClient *client) {
    uint8_t gen[2], len[2];
    size_t keylen;
    uint8_t *mod = NULL, *resp = NULL, *priv = NULL, *pub = NULL, *key = NULL,
            *shared = NULL;
    uint8_t userpass[128], ciphertext[128];
    int ciphertext_len;
    int passwordLen, usernameLen;
    rfbCredential *cred = NULL;
    rfbBool result = FALSE;

    if (!ReadFromRFBServer(client, (char *)gen, 2)) {
        rfbClientErr("HandleARDAuthDH: reading generator value failed\n");
        goto out;
    }
    if (!ReadFromRFBServer(client, (char *)len, 2)) {
        rfbClientErr("HandleARDAuthDH: reading key length failed\n");
        goto out;
    }
    keylen = 256 * len[0] + len[1];

    mod = (uint8_t *)malloc(keylen * 5);
    if (!mod)
        goto out;

    resp = mod + keylen;
    pub = resp + keylen;
    priv = pub + keylen;
    key = priv + keylen;

    if (!ReadFromRFBServer(client, (char *)mod, keylen)) {
        rfbClientErr("HandleARDAuthDH: reading prime modulus failed\n");
        goto out;
    }
    if (!ReadFromRFBServer(client, (char *)resp, keylen)) {
        rfbClientErr(
            "HandleARDAuthDH: reading peer's generated public key failed\n");
        goto out;
    }

    if (!dh_generate_keypair(priv, pub, gen, 2, mod, keylen)) {
        rfbClientErr("HandleARDAuthDH: generating keypair failed\n");
        goto out;
    }

    if (!dh_compute_shared_key(key, priv, resp, mod, keylen)) {
        rfbClientErr("HandleARDAuthDH: creating shared key failed\n");
        goto out;
    }

    shared = malloc(MD5_HASH_SIZE);
    if (!hash_md5(shared, key, keylen)) {
        rfbClientErr("HandleARDAuthDH: hashing shared key failed\n");
        goto out;
    }

    if (!client->GetCredential) {
        rfbClientErr("HandleARDAuthDH: GetCredential callback is not set\n");
        goto out;
    }
    cred = client->GetCredential(client, rfbCredentialTypeUser);
    if (!cred) {
        rfbClientErr("HandleARDAuthDH: reading credential failed\n");
        goto out;
    }
    passwordLen = strlen(cred->userCredential.password) + 1;
    usernameLen = strlen(cred->userCredential.username) + 1;
    if (passwordLen > sizeof(userpass) / 2)
        passwordLen = sizeof(userpass) / 2;
    if (usernameLen > sizeof(userpass) / 2)
        usernameLen = sizeof(userpass) / 2;
    random_bytes(userpass, sizeof(userpass));
    memcpy(userpass, cred->userCredential.username, usernameLen);
    memcpy(userpass + sizeof(userpass) / 2, cred->userCredential.password,
           passwordLen);

    if (!encrypt_aes128ecb(ciphertext, &ciphertext_len, shared, userpass,
                           sizeof(userpass))) {
        rfbClientErr("HandleARDAuthDH: encrypting credentials failed\n");
        goto out;
    }

    if (!WriteToRFBServer(client, (char *)ciphertext, sizeof(ciphertext)))
        goto out;
    if (!WriteToRFBServer(client, (char *)pub, keylen))
        goto out;

    result = TRUE;

out:
    if (cred)
        FreeARDUserCredential(cred);

    free(mod);
    free(shared);

    return result;
}

struct ardsrp_challenge_fields {
    uint8_t cflag;
    const uint8_t *N;
    size_t N_len;
    const uint8_t *g;
    size_t g_len;
    const uint8_t *salt;
    size_t salt_len;
    const uint8_t *B;
    size_t B_len;
    uint64_t iterations;
    const uint8_t *options;
    size_t options_len;
};

struct ardsrp_step2 {
    uint8_t *A;
    size_t A_len;
    uint8_t *m1;
    size_t m1_len;
    const uint8_t *options;
    size_t options_len;
};

static int ComputeSRPStep2(const char *password, uint32_t auth_type,
                           const struct ardsrp_challenge_fields *parsed,
                           struct ardsrp_step2 *step2);
static int BuildSRPStep2Response(const struct ardsrp_step2 *step2,
                                 uint8_t *outbuf, size_t outcap,
                                 size_t *outlen);

static int ParseARDSRPChallengeFieldsAt(const uint8_t *buf, size_t n,
                                        size_t off,
                                        struct ardsrp_challenge_fields *out) {
    uint16_t field_len;

    if (!buf || !out || n < 11)
        return 0;
    memset(out, 0, sizeof(*out));
    if (off + 1 > n)
        return 0;
    out->cflag = buf[off++];
    if (off + 2 > n)
        return 0;
    field_len = ReadBEU16(buf + off);
    off += 2;
    if (off + field_len > n)
        return 0;
    out->N = buf + off;
    out->N_len = field_len;
    off += field_len;
    if (off + 2 > n)
        return 0;
    field_len = ReadBEU16(buf + off);
    off += 2;
    if (off + field_len > n)
        return 0;
    out->g = buf + off;
    out->g_len = field_len;
    off += field_len;
    if (off + 1 > n)
        return 0;
    field_len = buf[off++];
    if (off + field_len > n)
        return 0;
    out->salt = buf + off;
    out->salt_len = field_len;
    off += field_len;
    if (off + 2 > n)
        return 0;
    field_len = ReadBEU16(buf + off);
    off += 2;
    if (off + field_len > n)
        return 0;
    out->B = buf + off;
    out->B_len = field_len;
    off += field_len;
    if (off + 8 > n)
        return 0;
    out->iterations = ReadBEU64(buf + off);
    off += 8;
    if (off + 2 > n)
        return 0;
    field_len = ReadBEU16(buf + off);
    off += 2;
    if (off + field_len > n)
        return 0;
    out->options = buf + off;
    out->options_len = field_len;
    off += field_len;
    return off == n;
}

static int ParseARDSRPChallengeFields(const uint8_t *buf, size_t n,
                                      uint32_t auth_type,
                                      struct ardsrp_challenge_fields *out) {
    size_t off = 0;

    if (!buf || !out)
        return 0;
    if (auth_type == rfbARDAuthRSASRP) {
        if (n >= 10 && memcmp(buf + 2, "RSA1", 4) == 0) {
            off = 10;
        } else if (n >= 6 && ReadBEU32(buf) == 2 &&
                   ReadBEU16(buf + 4) == n - 6) {
            off = 10;
        } else {
            rfbClientErr(
                "ard %s: RSA1 challenge header missing or short (len=%lu)\n",
                GetARDSRPMethodName(auth_type), (unsigned long)n);
            return 0;
        }
    } else if (auth_type == rfbARDAuthDirectSRP) {
        if (n < 4 || ReadBEU32(buf) != n - 4) {
            rfbClientErr("ard %s: direct SRP challenge inner length mismatch "
                         "(len=%lu, inner=%u)\n",
                         GetARDSRPMethodName(auth_type), (unsigned long)n,
                         n >= 4 ? ReadBEU32(buf) : 0);
            return 0;
        }
        off = 4;
    }
    if (!ParseARDSRPChallengeFieldsAt(buf, n, off, out)) {
        rfbClientErr(
            "ard %s: failed to parse SRP challenge fields (len=%lu, off=%lu)\n",
            GetARDSRPMethodName(auth_type), (unsigned long)n,
            (unsigned long)off);
        return 0;
    }
    return 1;
}

#if defined(__APPLE__)
static void ReleaseCFRef(CFTypeRef ref) {
    if (ref)
        CFRelease(ref);
}

static void LogGSSStatus1(const char *prefix, OM_uint32 code, int status_type) {
    OM_uint32 msg_ctx = 0;
    OM_uint32 minor = 0;
    gss_buffer_desc msg = GSS_C_EMPTY_BUFFER;

    do {
        if (gss_display_status(&minor, code, status_type, GSS_C_NO_OID,
                               &msg_ctx, &msg) != GSS_S_COMPLETE)
            break;
        rfbClientErr("%s%s\n", prefix,
                     msg.value ? (const char *)msg.value : "<empty>");
        gss_release_buffer(&minor, &msg);
    } while (msg_ctx != 0);
}

static void LogGSSError(const char *prefix, OM_uint32 major, OM_uint32 minor) {
    char line[256];

    snprintf(line, sizeof(line), "%smajor: ", prefix);
    LogGSSStatus1(line, major, GSS_C_GSS_CODE);
    snprintf(line, sizeof(line), "%sminor: ", prefix);
    LogGSSStatus1(line, minor, GSS_C_MECH_CODE);
}

static void LogCFError(const char *prefix, CFErrorRef error) {
    CFStringRef desc = NULL;
    char buf[256];

    if (!error)
        return;
    desc = CFErrorCopyDescription(error);
    if (!desc)
        return;
    if (CFStringGetCString(desc, buf, sizeof(buf), kCFStringEncodingUTF8))
        rfbClientErr("%s%s\n", prefix, buf);
    CFRelease(desc);
}
#endif

#if defined(__APPLE__) && defined(LIBVNCCLIENT_APPLE_HAS_OPENSSL_BN)
static int BNToPad(const BIGNUM *bn, uint8_t *out, size_t out_len) {
    if (!bn || !out || out_len == 0)
        return 0;
    return BN_bn2binpad(bn, out, (int)out_len) == (int)out_len;
}

static int RandomBigInt(BIGNUM *out, int bits) {
    int byte_len;
    uint8_t *buf;

    if (!out)
        return 0;
    if (bits < 64)
        bits = 64;
    byte_len = (bits + 7) / 8;
    buf = (uint8_t *)malloc((size_t)byte_len);
    if (!buf)
        return 0;
    random_bytes(buf, (size_t)byte_len);
    if (bits % 8)
        buf[0] &= (uint8_t)((1u << (bits % 8)) - 1u);
    if (BN_bin2bn(buf, byte_len, out) == NULL) {
        free(buf);
        return 0;
    }
    free(buf);
    return 1;
}
#endif

static int BuildRSASRPKeyRequestPacket(uint8_t *out, size_t out_len,
                                       uint16_t packet_version) {
    if (!out || out_len < 14)
        return 0;
    memset(out, 0, 14);
    WriteBEU32(out, 10);
    WriteBEU16(out + 4, packet_version);
    memcpy(out + 6, "RSA1", 4);
    return 1;
}

static int BuildRSASRPInitPacket(uint8_t *out, size_t out_len,
                                 uint16_t packet_version, uint16_t auth_type,
                                 uint16_t aux_type, const uint8_t *key_material,
                                 size_t key_material_len) {
    if (!out || !key_material || out_len < 654 || key_material_len != 256)
        return 0;
    memset(out, 0, 654);
    WriteBEU32(out, 650);
    WriteBEU16(out + 4, packet_version);
    memcpy(out + 6, "RSA1", 4);
    WriteBEU16(out + 10, auth_type);
    WriteBEU16(out + 12, aux_type);
    memcpy(out + 14, key_material, key_material_len);
    return 1;
}

static int BuildSRPInitPlaintext(const char *username, uint8_t *out,
                                 size_t out_len, size_t *plaintext_len) {
    size_t user_len;
    size_t total_plain_len;
    uint8_t *p;

    if (!username || !out || !plaintext_len)
        return 0;
    user_len = strlen(username);
    if (user_len > 0xffffu)
        return 0;
    total_plain_len = 11 + user_len;
    if (out_len < total_plain_len)
        return 0;
    memset(out, 0, out_len);
    WriteBEU32(out, (uint32_t)(total_plain_len - 4));
    WriteBEU16(out + 4, 0);
    WriteBEU16(out + 6, (uint16_t)user_len);
    memcpy(out + 8, username, user_len);
    p = out + 8 + user_len;
    WriteBEU16(p, 0);
    p += 2;
    *p = 0;
    *plaintext_len = total_plain_len;
    return 1;
}

static int BuildDirectSRPBranchEntryPacket(const char *username, uint8_t *out,
                                           size_t out_len, size_t *packet_len) {
    uint8_t plaintext[512];
    size_t plaintext_len = 0;

    if (!username || !out || !packet_len)
        return 0;
    if (!BuildSRPInitPlaintext(username, plaintext, sizeof(plaintext),
                               &plaintext_len))
        return 0;
    if (out_len < 1 + 4 + plaintext_len)
        return 0;

    out[0] = (uint8_t)rfbARDAuthDirectSRP;
    WriteBEU32(out + 1, (uint32_t)plaintext_len);
    memcpy(out + 5, plaintext, plaintext_len);
    *packet_len = 1 + 4 + plaintext_len;
    return 1;
}

static void FreeARDSRPStep2(struct ardsrp_step2 *step2) {
    if (!step2)
        return;
    free(step2->A);
    free(step2->m1);
    memset(step2, 0, sizeof(*step2));
}

#if defined(__APPLE__)
static rfbBool BuildRSASRPInitKeyMaterial(const char *username,
                                          const uint8_t *type0_reply,
                                          uint32_t type0_reply_len,
                                          uint8_t *outbuf, size_t outcap,
                                          size_t *outlen) {
    uint32_t der_len;
    uint8_t plaintext[512];
    size_t plaintext_len = 0;
    const uint8_t *der;

    if (!username || !type0_reply || type0_reply_len < 6 || !outbuf || !outlen)
        return FALSE;
    der_len = ReadBEU32(type0_reply + 2);
    if (6u + der_len > type0_reply_len)
        return FALSE;
    der = type0_reply + 6;
    if (!BuildSRPInitPlaintext(username, plaintext, sizeof(plaintext),
                               &plaintext_len))
        return FALSE;
    *outlen = outcap;
    if (!encrypt_rsa_pkcs1_spki_der(outbuf, outlen, der, der_len, plaintext,
                                    plaintext_len))
        return FALSE;
    return *outlen == 256;
}
#else
static rfbBool BuildRSASRPInitKeyMaterial(const char *username,
                                          const uint8_t *type0_reply,
                                          uint32_t type0_reply_len,
                                          uint8_t *outbuf, size_t outcap,
                                          size_t *outlen) {
    (void)username;
    (void)type0_reply;
    (void)type0_reply_len;
    (void)outbuf;
    (void)outcap;
    (void)outlen;
    return FALSE;
}
#endif

static int BuildSRPStep2Inner(rfbClient *client, uint32_t auth_type,
                              const char *password, const uint8_t *challenge,
                              uint32_t challenge_len, uint8_t *outbuf,
                              size_t outcap, size_t *outlen) {
    struct ardsrp_challenge_fields parsed;
    struct ardsrp_step2 step2;
    int ok = FALSE;

    if (!client || !password || !*password || !challenge || !challenge_len ||
        !outbuf || !outlen)
        return FALSE;
    if (!ParseARDSRPChallengeFields(challenge, challenge_len, auth_type,
                                    &parsed))
        return FALSE;
    if (!ComputeSRPStep2(password, auth_type, &parsed, &step2))
        return FALSE;
    ok = BuildSRPStep2Response(&step2, outbuf, outcap, outlen);
    if (!ok) {
        rfbClientErr("ard %s: output buffer too small for response\n",
                     GetARDSRPMethodName(auth_type));
    }
    FreeARDSRPStep2(&step2);
    return ok;
}

static int ComputeSRPStep2(const char *password, uint32_t auth_type,
                           const struct ardsrp_challenge_fields *parsed,
                           struct ardsrp_step2 *step2) {
#if !defined(__APPLE__) || !defined(LIBVNCCLIENT_APPLE_HAS_OPENSSL_BN)
    (void)password;
    (void)auth_type;
    (void)parsed;
    (void)step2;
    return FALSE;
#else
    static const uint8_t empty_user_hash[SHA512_HASH_SIZE] = {
        0xcf, 0x83, 0xe1, 0x35, 0x7e, 0xef, 0xb8, 0xbd, 0xf1, 0x54, 0x28,
        0x50, 0xd6, 0x6d, 0x80, 0x07, 0xd6, 0x20, 0xe4, 0x05, 0x0b, 0x57,
        0x15, 0xdc, 0x83, 0xf4, 0xa9, 0x21, 0xd3, 0x6c, 0xe9, 0xce, 0x47,
        0xd0, 0xd1, 0x3c, 0x5d, 0x85, 0xf2, 0xb0, 0xff, 0x83, 0x18, 0xd2,
        0x87, 0x7e, 0xec, 0x2f, 0x63, 0xb9, 0x31, 0xbd, 0x47, 0x41, 0x7a,
        0x81, 0xa5, 0x38, 0x32, 0x7a, 0xf9, 0x27, 0xda, 0x3e};
    BIGNUM *N = NULL;
    BIGNUM *g = NULL;
    BIGNUM *B = NULL;
    BIGNUM *a = NULL;
    BIGNUM *A = NULL;
    BIGNUM *x = NULL;
    BIGNUM *v = NULL;
    BIGNUM *k = NULL;
    BIGNUM *u = NULL;
    BIGNUM *ux = NULL;
    BIGNUM *exp = NULL;
    BIGNUM *tmp = NULL;
    BIGNUM *base = NULL;
    BIGNUM *S = NULL;
    BN_CTX *bn_ctx = NULL;
    uint8_t *Np = NULL;
    uint8_t *gp = NULL;
    uint8_t *Ap = NULL;
    uint8_t *Bp = NULL;
    uint8_t *K = NULL;
    uint8_t *pbkdf2_pass = NULL;
    uint8_t *x_bytes = NULL;
    uint8_t *Sp = NULL;
    uint8_t hN[SHA512_HASH_SIZE];
    uint8_t hg[SHA512_HASH_SIZE];
    uint8_t hu[SHA512_HASH_SIZE];
    uint8_t xor_ng[SHA512_HASH_SIZE];
    uint8_t digest[SHA512_HASH_SIZE];
    uint8_t digest2[SHA512_HASH_SIZE];
    size_t pad_len;
    size_t password_len;
    size_t x_len;
    size_t k_len;
    size_t i;
    int ok = FALSE;

    if (!password || !*password || !parsed || !step2)
        return FALSE;
    memset(step2, 0, sizeof(*step2));

    password_len = strlen(password);
    pad_len = parsed->N_len;
    N = BN_bin2bn(parsed->N, (int)parsed->N_len, NULL);
    g = BN_bin2bn(parsed->g, (int)parsed->g_len, NULL);
    B = BN_bin2bn(parsed->B, (int)parsed->B_len, NULL);
    a = BN_new();
    A = BN_new();
    x = BN_new();
    v = BN_new();
    k = BN_new();
    u = BN_new();
    ux = BN_new();
    exp = BN_new();
    tmp = BN_new();
    base = BN_new();
    S = BN_new();
    bn_ctx = BN_CTX_new();
    Np = (uint8_t *)malloc(pad_len);
    gp = (uint8_t *)malloc(pad_len);
    Ap = (uint8_t *)malloc(pad_len);
    Bp = (uint8_t *)malloc(pad_len);
    step2->m1 = (uint8_t *)malloc(SHA512_HASH_SIZE);
    if (!N || !g || !B || !a || !A || !x || !v || !k || !u || !ux || !exp ||
        !tmp || !base || !S || !bn_ctx || !Np || !gp || !Ap || !Bp ||
        !step2->m1) {
        rfbClientErr("ard %s: BN allocation failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }
    if (!RandomBigInt(a, 512) || !BN_mod_exp(A, g, a, N, bn_ctx) ||
        !BNToPad(N, Np, pad_len) || !BNToPad(g, gp, pad_len) ||
        !BNToPad(B, Bp, pad_len) || !BNToPad(A, Ap, pad_len)) {
        rfbClientErr("ard %s: failed generating or padding SRP public values\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }

    pbkdf2_pass = (uint8_t *)malloc(128);
    if (!pbkdf2_pass) {
        rfbClientErr("ard %s: PBKDF buffer allocation failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }
    if (!pbkdf2_hmac_sha512((const uint8_t *)password, password_len,
                            parsed->salt, parsed->salt_len,
                            (uint32_t)parsed->iterations, pbkdf2_pass, 128)) {
        rfbClientErr("ard %s: PBKDF2-SHA512 failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }
    {
        const void *parts[2] = {":", pbkdf2_pass};
        const size_t lens[2] = {1, 128};
        HashSHA512Parts(digest, parts, lens, 2);
    }
    {
        const void *parts[2] = {parsed->salt, digest};
        const size_t lens[2] = {parsed->salt_len, sizeof(digest)};
        HashSHA512Parts(digest2, parts, lens, 2);
    }
    x_len = sizeof(digest2);
    x_bytes = (uint8_t *)malloc(x_len);
    if (!x_bytes) {
        rfbClientErr("ard %s: x buffer allocation failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }
    memcpy(x_bytes, digest2, x_len);
    if (!BN_bin2bn(x_bytes, (int)x_len, x)) {
        rfbClientErr("ard %s: BN_bin2bn(x) failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }

    HashSHA512TwoParts(digest, Np, pad_len, gp, pad_len);
    if (!BN_bin2bn(digest, sizeof(digest), k)) {
        rfbClientErr("ard %s: BN_bin2bn(k) failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }

    HashSHA512TwoParts(digest, Ap, pad_len, Bp, pad_len);
    if (!BN_bin2bn(digest, sizeof(digest), u)) {
        rfbClientErr("ard %s: BN_bin2bn(u) failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }
    if (!BN_mod_exp(v, g, x, N, bn_ctx) || !BN_mod_mul(tmp, k, v, N, bn_ctx) ||
        !BN_mod_sub(base, B, tmp, N, bn_ctx) || !BN_mul(ux, u, x, bn_ctx) ||
        !BN_add(exp, a, ux) || !BN_mod_exp(S, base, exp, N, bn_ctx)) {
        rfbClientErr("ard %s: SRP shared secret computation failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }

    Sp = (uint8_t *)malloc(pad_len);
    if (!Sp || !BNToPad(S, Sp, pad_len)) {
        rfbClientErr("ard %s: failed serializing shared secret\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }
    HashSHA512TwoParts(digest, Sp, pad_len, NULL, 0);
    K = (uint8_t *)malloc(sizeof(digest));
    if (!K) {
        rfbClientErr("ard %s: session hash buffer allocation failed\n",
                     GetARDSRPMethodName(auth_type));
        goto done;
    }
    memcpy(K, digest, sizeof(digest));
    k_len = sizeof(digest);
    HashSHA512TwoParts(hN, Np, pad_len, NULL, 0);
    HashSHA512TwoParts(hg, gp, pad_len, NULL, 0);
    memcpy(hu, empty_user_hash, sizeof(hu));
    for (i = 0; i < sizeof(xor_ng); ++i)
        xor_ng[i] = hN[i] ^ hg[i];
    {
        const void *parts[6] = {xor_ng, hu, parsed->salt, Ap, Bp, K};
        const size_t lens[6] = {sizeof(xor_ng), sizeof(hu), parsed->salt_len,
                                pad_len,        pad_len,    k_len};
        HashSHA512Parts(step2->m1, parts, lens, 6);
    }

    step2->A = Ap;
    step2->A_len = pad_len;
    Ap = NULL;
    step2->m1_len = SHA512_HASH_SIZE;
    step2->options = parsed->options;
    step2->options_len = parsed->options_len;
    ok = TRUE;

done:
    if (!ok)
        FreeARDSRPStep2(step2);
    BN_free(N);
    BN_free(g);
    BN_free(B);
    BN_free(a);
    BN_free(A);
    BN_free(x);
    BN_free(v);
    BN_free(k);
    BN_free(u);
    BN_free(ux);
    BN_free(exp);
    BN_free(tmp);
    BN_free(base);
    BN_free(S);
    BN_CTX_free(bn_ctx);
    free(Np);
    free(gp);
    free(Ap);
    free(Bp);
    free(K);
    free(pbkdf2_pass);
    free(x_bytes);
    free(Sp);
    return ok;
#endif
}

static int BuildSRPStep2Response(const struct ardsrp_step2 *step2,
                                 uint8_t *outbuf, size_t outcap,
                                 size_t *outlen) {
    uint8_t nonce16[16];
    size_t inner_len;
    size_t off = 0;

    if (!step2 || !step2->A || !step2->m1 || !outbuf || !outlen)
        return FALSE;

    inner_len = 2 + step2->A_len + 1 + step2->m1_len + 2 + step2->options_len +
                1 + sizeof(nonce16);
    if (outcap < inner_len)
        return FALSE;

    random_bytes(nonce16, sizeof(nonce16));
    WriteBEU16(outbuf + off, (uint16_t)step2->A_len);
    off += 2;
    memcpy(outbuf + off, step2->A, step2->A_len);
    off += step2->A_len;
    outbuf[off++] = (uint8_t)step2->m1_len;
    memcpy(outbuf + off, step2->m1, step2->m1_len);
    off += step2->m1_len;
    WriteBEU16(outbuf + off, (uint16_t)step2->options_len);
    off += 2;
    memcpy(outbuf + off, step2->options, step2->options_len);
    off += step2->options_len;
    outbuf[off++] = (uint8_t)sizeof(nonce16);
    memcpy(outbuf + off, nonce16, sizeof(nonce16));
    off += sizeof(nonce16);
    *outlen = off;
    return TRUE;
}

static int BuildRSASRPPacket2Candidate(rfbClient *client, const char *password,
                                       const uint8_t *challenge,
                                       uint32_t challenge_len, uint8_t *outbuf,
                                       size_t outcap, size_t *outlen) {
#if !defined(__APPLE__) || !defined(LIBVNCCLIENT_APPLE_HAS_OPENSSL_BN)
    (void)client;
    (void)password;
    (void)challenge;
    (void)challenge_len;
    (void)outbuf;
    (void)outcap;
    (void)outlen;
    return FALSE;
#else
    uint8_t inner[4096];
    size_t inner_len = 0;
    size_t wrapped_body_len;
    size_t wrapped_len;

    if (!BuildSRPStep2Inner(client, rfbARDAuthRSASRP, password, challenge,
                            challenge_len, inner, sizeof(inner), &inner_len)) {
        return FALSE;
    }

    wrapped_body_len = 4 + inner_len;
    wrapped_len = wrapped_body_len + 384;
    if (!outbuf || !outlen || outcap < 14 + wrapped_len)
        return FALSE;
    memset(outbuf, 0, 14 + wrapped_len);
    WriteBEU16(outbuf + 14 + 0, 0);
    WriteBEU16(outbuf + 14 + 2, (uint16_t)inner_len);
    memcpy(outbuf + 14 + 4, inner, inner_len);
    WriteBEU32(outbuf, (uint32_t)(10 + wrapped_len));
    WriteBEU16(outbuf + 4, 0x0100);
    memcpy(outbuf + 6, "RSA1", 4);
    WriteBEU16(outbuf + 10, 0x0002);
    WriteBEU16(outbuf + 12, (uint16_t)wrapped_body_len);
    *outlen = 14 + wrapped_len;
    return TRUE;
#endif
}

static rfbBool HandleARDAuthRSASRP(rfbClient *client) {
    uint8_t outbuf[8192];
    uint8_t keyreq[14];
    uint8_t init_key_material[256];
    uint8_t *type0_reply = NULL;
    uint8_t *inbuf = NULL;
    size_t outlen = 654;
    uint32_t type0_reply_len = 0;
    uint32_t inlen = 0;
    uint16_t packet_version = 0x0100;
    uint16_t auth_type = 0x0002;
    uint16_t aux_type = 0x0100;
    rfbCredential *cred = NULL;
    rfbBool ok = FALSE;

    if (!client->GetCredential) {
        rfbClientErr("ard rsa-srp: GetCredential callback is not set\n");
        return FALSE;
    }
    cred = client->GetCredential(client, rfbCredentialTypeUser);
    if (!cred || !cred->userCredential.username ||
        !cred->userCredential.password) {
        rfbClientErr("ard rsa-srp: reading credential failed\n");
        FreeARDUserCredential(cred);
        return FALSE;
    }

    memset(outbuf, 0, sizeof(outbuf));

    if (!BuildRSASRPKeyRequestPacket(keyreq, sizeof(keyreq), packet_version))
        goto done;
    if (!WriteToRFBServer(client, (const char *)keyreq, sizeof(keyreq)))
        goto done;
    if (!ReadLengthPrefixedBlob(client, &type0_reply, &type0_reply_len,
                                "rsa-srp type0 reply"))
        goto done;

    if (!BuildRSASRPInitKeyMaterial(cred->userCredential.username, type0_reply,
                                    type0_reply_len, init_key_material,
                                    sizeof(init_key_material), &outlen)) {
        goto done;
    }
    free(type0_reply);
    type0_reply = NULL;

    outlen = 654;
    if (!BuildRSASRPInitPacket(outbuf, sizeof(outbuf), packet_version,
                               auth_type, aux_type, init_key_material,
                               sizeof(init_key_material))) {
        goto done;
    }
    if (!WriteToRFBServer(client, (const char *)outbuf, (unsigned int)outlen))
        goto done;

    if (!ReadLengthPrefixedBlob(client, &inbuf, &inlen, "rsa-srp challenge"))
        goto done;
    if (!BuildRSASRPPacket2Candidate(client, cred->userCredential.password,
                                     inbuf, inlen, outbuf, sizeof(outbuf),
                                     &outlen)) {
        goto done;
    }
    free(inbuf);
    inbuf = NULL;
    if (!WriteToRFBServer(client, (const char *)outbuf, (unsigned int)outlen))
        goto done;
    if (!ConsumeRSASRPServerFinalIfPresent(client))
        goto done;

    ok = TRUE;

done:
    FreeARDUserCredential(cred);
    free(type0_reply);
    free(inbuf);
    return ok;
}

static rfbBool HandleARDAuthDirectSRP(rfbClient *client) {
    uint8_t entry[1024];
    uint8_t response_inner[4096];
    uint8_t response[4100];
    uint8_t *challenge = NULL;
    uint8_t *final_token = NULL;
    uint32_t challenge_len = 0;
    uint32_t final_token_len = 0;
    size_t entry_len = 0;
    size_t response_inner_len = 0;
    size_t response_len = 0;
    rfbCredential *cred = NULL;
    rfbBool ok = FALSE;

    if (!client->GetCredential) {
        rfbClientErr("ard direct-srp: GetCredential callback is not set\n");
        return FALSE;
    }
    cred = client->GetCredential(client, rfbCredentialTypeUser);
    if (!cred || !cred->userCredential.username ||
        !cred->userCredential.password) {
        rfbClientErr("ard direct-srp: reading credential failed\n");
        FreeARDUserCredential(cred);
        return FALSE;
    }

    if (!BuildDirectSRPBranchEntryPacket(cred->userCredential.username, entry,
                                         sizeof(entry), &entry_len)) {
        goto done;
    }
    if (!WriteToRFBServer(client, (const char *)entry, (unsigned int)entry_len))
        goto done;

    if (!ReadLengthPrefixedBlob(client, &challenge, &challenge_len,
                                "direct-srp challenge"))
        goto done;
    if (!BuildSRPStep2Inner(client, rfbARDAuthDirectSRP,
                            cred->userCredential.password, challenge,
                            challenge_len, response_inner,
                            sizeof(response_inner), &response_inner_len)) {
        goto done;
    }
    if (response_inner_len > 0xffffffffu ||
        response_inner_len + 4 > sizeof(response))
        goto done;
    WriteBEU32(response, (uint32_t)response_inner_len);
    memcpy(response + 4, response_inner, response_inner_len);
    response_len = response_inner_len + 4;
    if (!WriteLengthPrefixedBlob(client, response, response_len,
                                 "direct-srp response"))
        goto done;
    if (!ReadLengthPrefixedBlob(client, &final_token, &final_token_len,
                                "direct-srp final token"))
        goto done;

    ok = TRUE;

done:
    FreeARDUserCredential(cred);
    free(challenge);
    free(final_token);
    return ok;
}

#if defined(__APPLE__)
static rfbBool ImportKerberosName(const char *value, gss_const_OID oid,
                                  gss_name_t *out_name, const char *what) {
    OM_uint32 major = 0;
    OM_uint32 minor = 0;
    gss_buffer_desc buf = GSS_C_EMPTY_BUFFER;

    if (!value || !out_name)
        return FALSE;
    *out_name = GSS_C_NO_NAME;
    buf.value = (void *)value;
    buf.length = strlen(value);
    major = gss_import_name(&minor, &buf, oid, out_name);
    if (major != GSS_S_COMPLETE) {
        char prefix[128];
        snprintf(prefix, sizeof(prefix),
                 "ard kerberos: gss_import_name(%s) failed: ", what);
        LogGSSError(prefix, major, minor);
        return FALSE;
    }
    return TRUE;
}
#endif

static rfbBool WriteLengthPrefixedBlob(rfbClient *client, const uint8_t *buf,
                                       size_t len, const char *what) {
    uint8_t hdr[4];

    if (!client || !buf || len == 0 || len > 0xffffffffu)
        return FALSE;
    WriteBEU32(hdr, (uint32_t)len);
    if (!WriteToRFBServer(client, (const char *)hdr, sizeof(hdr))) {
        rfbClientErr("ard kerberos: failed writing %s length\n", what);
        return FALSE;
    }
    if (!WriteToRFBServer(client, (const char *)buf, (unsigned int)len)) {
        rfbClientErr("ard kerberos: failed writing %s body\n", what);
        return FALSE;
    }
    return TRUE;
}

#if defined(__APPLE__)
static char *BuildKerberosClientPrincipal(rfbClient *client,
                                          const char *username) {
    const char *override = client ? client->ardAuthClientPrincipal : NULL;
    const char *realm = client ? client->ardAuthRealm : NULL;
    size_t need = 0;
    char *out = NULL;

    if (override)
        return strdup(override);
    if (!username || !*username)
        return NULL;
    if (strchr(username, '@'))
        return strdup(username);
    if (!realm || !*realm)
        return NULL;
    need = strlen(username) + 1 + strlen(realm) + 1;
    out = (char *)malloc(need);
    if (!out)
        return NULL;
    snprintf(out, need, "%s@%s", username, realm);
    return out;
}

static char *BuildKerberosServicePrincipal(rfbClient *client) {
    const char *override = client ? client->ardAuthServicePrincipal : NULL;
    const char *realm = client ? client->ardAuthRealm : NULL;
    const char *serverHost = client ? client->serverHost : NULL;
    size_t need = 0;
    char *out = NULL;

    if (override)
        return strdup(override);
    if (!serverHost || !*serverHost || !realm || !*realm)
        return NULL;
    need = 4 + strlen(serverHost) + 1 + strlen(realm) + 1;
    out = (char *)malloc(need);
    if (!out)
        return NULL;
    snprintf(out, need, "vnc/%s@%s", serverHost, realm);
    return out;
}

static rfbBool HandleARDAuthKerberosGSSAPI(rfbClient *client) {
    static const uint8_t preface[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t zero_word[4];
    uint8_t *aprep = NULL;
    uint8_t *wrap = NULL;
    uint32_t aprep_len = 0;
    uint32_t wrap_len = 0;
    rfbCredential *cred = NULL;
    gss_name_t user_name = GSS_C_NO_NAME;
    gss_name_t target_name = GSS_C_NO_NAME;
    gss_cred_id_t gss_cred = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t gss_ctx = GSS_C_NO_CONTEXT;
    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc output = GSS_C_EMPTY_BUFFER;
    OM_uint32 major = 0;
    OM_uint32 minor = 0;
    OM_uint32 ret_flags = 0;
    int conf_state = 0;
    CFStringRef password = NULL;
    CFStringRef lkdc_host = NULL;
    CFMutableDictionaryRef attrs = NULL;
    CFErrorRef cferr = NULL;
    char *client_principal = NULL;
    char *service_principal = NULL;
    rfbBool ok = FALSE;

    if (!client->GetCredential) {
        rfbClientErr("ard kerberos: GetCredential callback is not set\n");
        return FALSE;
    }
    cred = client->GetCredential(client, rfbCredentialTypeUser);
    if (!cred || !cred->userCredential.username ||
        !cred->userCredential.password || !client->serverHost ||
        !*client->serverHost) {
        rfbClientErr("ard kerberos: reading credential or hostname failed\n");
        goto done;
    }

    if (!WriteToRFBServer(client, (const char *)preface, sizeof(preface)))
        goto done;
    if (!ReadFromRFBServer(client, (char *)zero_word, sizeof(zero_word)))
        goto done;
    if (ReadBEU32(zero_word) != 0)
        rfbClientLog("ard kerberos: server preface word was 0x%08x\n",
                     ReadBEU32(zero_word));

    client_principal =
        BuildKerberosClientPrincipal(client, cred->userCredential.username);
    service_principal = BuildKerberosServicePrincipal(client);
    if (!client_principal || !service_principal) {
        rfbClientErr(
            "ard kerberos: missing Kerberos configuration. Set ARD auth "
            "realm/service overrides on the client or pass a "
            "fully-qualified Kerberos principal in VNC_USER.\n");
        goto done;
    }
    if (!ImportKerberosName(client_principal, GSS_KRB5_NT_PRINCIPAL_NAME,
                            &user_name, "client principal"))
        goto done;

    password = CFStringCreateWithCString(kCFAllocatorDefault,
                                         cred->userCredential.password,
                                         kCFStringEncodingUTF8);
    lkdc_host = CFStringCreateWithCString(
        kCFAllocatorDefault, client->serverHost, kCFStringEncodingUTF8);
    attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
    if (!password || !lkdc_host || !attrs)
        goto done;
    CFDictionarySetValue(attrs, kGSSICPassword, password);
    CFDictionarySetValue(attrs, kGSSCredentialUsage, kGSS_C_INITIATE);
    CFDictionarySetValue(attrs, kGSSICLKDCHostname, lkdc_host);

    major = gss_aapl_initial_cred(user_name, gss_mech_krb5, attrs, &gss_cred,
                                  &cferr);
    if (major != GSS_S_COMPLETE || gss_cred == GSS_C_NO_CREDENTIAL) {
        LogCFError("ard kerberos: gss_aapl_initial_cred failed: ", cferr);
        LogGSSError("ard kerberos: gss_aapl_initial_cred failed: ", major, 0);
        goto done;
    }

    if (!ImportKerberosName(service_principal, GSS_KRB5_NT_PRINCIPAL_NAME,
                            &target_name, "service principal"))
        goto done;

    major = gss_init_sec_context(
        &minor, gss_cred, &gss_ctx, target_name, (gss_OID)gss_mech_krb5,
        GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
        GSS_C_NO_BUFFER, NULL, &output, &ret_flags, NULL);
    if ((major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED) ||
        output.length == 0) {
        LogGSSError("ard kerberos: initial gss_init_sec_context failed: ",
                    major, minor);
        goto done;
    }
    if (!WriteLengthPrefixedBlob(client, (const uint8_t *)output.value,
                                 output.length, "AP-REQ")) {
        gss_release_buffer(&minor, &output);
        goto done;
    }
    gss_release_buffer(&minor, &output);

    if (!ReadLengthPrefixedBlob(client, &aprep, &aprep_len, "kerberos AP-REP"))
        goto done;
    input.value = aprep;
    input.length = aprep_len;
    major = gss_init_sec_context(
        &minor, gss_cred, &gss_ctx, target_name, (gss_OID)gss_mech_krb5,
        GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
        &input, NULL, &output, &ret_flags, NULL);
    if (major != GSS_S_COMPLETE) {
        LogGSSError("ard kerberos: AP-REP processing failed: ", major, minor);
        goto done;
    }
    if (output.length != 0) {
        rfbClientErr(
            "ard kerberos: unexpected AP-REP output token length=%lu\n",
            (unsigned long)output.length);
        gss_release_buffer(&minor, &output);
        goto done;
    }
    gss_release_buffer(&minor, &output);

    if (!ReadLengthPrefixedBlob(client, &wrap, &wrap_len,
                                "kerberos wrap token"))
        goto done;
    input.value = wrap;
    input.length = wrap_len;
    major = gss_unwrap(&minor, gss_ctx, &input, &output, &conf_state, NULL);
    if (major != GSS_S_COMPLETE) {
        LogGSSError("ard kerberos: gss_unwrap failed: ", major, minor);
        goto done;
    }
    if (!conf_state) {
        rfbClientErr(
            "ard kerberos: wrap token did not provide confidentiality\n");
        gss_release_buffer(&minor, &output);
        goto done;
    }
    gss_release_buffer(&minor, &output);
    ok = TRUE;

done:
    if (gss_ctx != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor, &gss_ctx, GSS_C_NO_BUFFER);
    if (gss_cred != GSS_C_NO_CREDENTIAL)
        gss_release_cred(&minor, &gss_cred);
    if (user_name != GSS_C_NO_NAME)
        gss_release_name(&minor, &user_name);
    if (target_name != GSS_C_NO_NAME)
        gss_release_name(&minor, &target_name);
    if (output.length != 0)
        gss_release_buffer(&minor, &output);
    ReleaseCFRef(cferr);
    ReleaseCFRef(attrs);
    ReleaseCFRef(password);
    ReleaseCFRef(lkdc_host);
    FreeARDUserCredential(cred);
    free(client_principal);
    free(service_principal);
    free(aprep);
    free(wrap);
    return ok;
}
#else
static rfbBool HandleARDAuthKerberosGSSAPI(rfbClient *client) {
    (void)client;
    rfbClientErr(
        "ARD Kerberos GSS-API auth requires macOS GSS/Kerberos support\n");
    return FALSE;
}
#endif

rfbBool rfbClientHandleARDAuth(rfbClient *client, uint32_t authScheme) {
    if (!client)
        return FALSE;

    switch (authScheme) {
    case rfbARDAuthDH:
        return HandleARDAuthDH(client);
    case rfbARDAuthRSASRP:
        return HandleARDAuthRSASRP(client);
    case rfbARDAuthKerberosGSSAPI:
        return HandleARDAuthKerberosGSSAPI(client);
    case rfbARDAuthDirectSRP:
        return HandleARDAuthDirectSRP(client);
    default:
        return FALSE;
    }
}

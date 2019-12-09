/*
 * rfbcrypto_included.c - Crypto wrapper (included version)
 */

/*
 *  Copyright (C) 2011 Gernot Tenchio
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

#include <string.h>
#include "md5.h"
#include "sha.h"
#include "rfbcrypto.h"


int hash_md5(void *out, const void *in, const size_t in_len)
{
    return 0;
}

int hash_sha1(void *out, const void *in, const size_t in_len)
{
    return 0;
}

void random_bytes(void *out, size_t len)
{

}

int encrypt_aes128ecb(void *out, int *out_len, const void *key, const void *in, const size_t in_len)
{
    return 0;
}

int dh_generate_keypair(uint8_t *priv_out, uint8_t *pub_out, const uint8_t *gen, const size_t gen_len, const uint8_t *prime, const size_t keylen)
{
    return 0;
}

int dh_compute_shared_key(uint8_t *shared_out, const uint8_t *priv, const uint8_t *pub, const uint8_t *prime, const size_t keylen)
{
    return 0;
}

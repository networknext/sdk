/*
    Network Next. Copyright © 2017 - 2024 Network Next, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following 
    conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
       and the following disclaimer in the documentation and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote 
       products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
    OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "next.h"

#ifndef NEXT_CRYPTO_H
#define NEXT_CRYPTO_H

#define NEXT_CRYPTO_GENERICHASH_KEYBYTES                    32

#define NEXT_CRYPTO_SECRETBOX_KEYBYTES                      32
#define NEXT_CRYPTO_SECRETBOX_MACBYTES                      16
#define NEXT_CRYPTO_SECRETBOX_NONCEBYTES                    24

#define NEXT_CRYPTO_KX_PUBLICKEYBYTES                       32
#define NEXT_CRYPTO_KX_SECRETKEYBYTES                       32
#define NEXT_CRYPTO_KX_SESSIONKEYBYTES                      32

#define NEXT_CRYPTO_BOX_MACBYTES                            16
#define NEXT_CRYPTO_BOX_NONCEBYTES                          24
#define NEXT_CRYPTO_BOX_PUBLICKEYBYTES                      32
#define NEXT_CRYPTO_BOX_SECRETKEYBYTES                      32

#define NEXT_CRYPTO_SIGN_BYTES                              64
#define NEXT_CRYPTO_SIGN_PUBLICKEYBYTES                     32
#define NEXT_CRYPTO_SIGN_SECRETKEYBYTES                     64

#define NEXT_CRYPTO_AEAD_CHACHA20POLY1305_ABYTES            16
#define NEXT_CRYPTO_AEAD_CHACHA20POLY1305_KEYBYTES          32
#define NEXT_CRYPTO_AEAD_CHACHA20POLY1305_NPUBBYTES          8

#define NEXT_CRYPTO_AEAD_CHACHA20POLY1305_IETF_ABYTES       16
#define NEXT_CRYPTO_AEAD_CHACHA20POLY1305_IETF_KEYBYTES     32
#define NEXT_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NPUBBYTES    12

int next_crypto_init();

void next_crypto_random_bytes( uint8_t * buffer, int bytes );

int next_crypto_generichash( unsigned char * out, size_t outlen, const unsigned char * in, unsigned long long inlen, const unsigned char * key, size_t keylen );

struct next_crypto_sign_state_t
{
    uint8_t dummy[1024];
};

int next_crypto_sign_keypair( unsigned char * pk, unsigned char * sk );

int next_crypto_sign_init( struct next_crypto_sign_state_t * state );

int next_crypto_sign_update( struct next_crypto_sign_state_t * state, const unsigned char * m, unsigned long long mlen );

int next_crypto_sign_final_create( struct next_crypto_sign_state_t * state, unsigned char * sig, unsigned long long * siglen_p, const unsigned char *sk );

int next_crypto_sign_final_verify( struct next_crypto_sign_state_t * state, const unsigned char * sig, const unsigned char * pk );

void next_crypto_secretbox_keygen( unsigned char * k );

int next_crypto_secretbox_easy( unsigned char * c, const unsigned char * m, unsigned long long mlen, const unsigned char * n, const unsigned char * k );

int next_crypto_secretbox_open_easy( unsigned char * m, const unsigned char * c, unsigned long long clen, const unsigned char * n, const unsigned char * k );

void next_crypto_aead_chacha20poly1305_keygen( unsigned char * k );

int next_crypto_aead_chacha20poly1305_encrypt( unsigned char * c, unsigned long long * clen_p, const unsigned char * m, unsigned long long mlen, const unsigned char * ad, unsigned long long adlen, const unsigned char * nsec, const unsigned char * npub, const unsigned char * k );

int next_crypto_aead_chacha20poly1305_decrypt( unsigned char * m, unsigned long long * mlen_p, unsigned char * nsec, const unsigned char * c, unsigned long long clen, const unsigned char * ad, unsigned long long adlen, const unsigned char * npub, const unsigned char * k );

void next_crypto_aead_chacha20poly1305_ietf_keygen( unsigned char * k );

int next_crypto_aead_chacha20poly1305_ietf_encrypt( unsigned char * c, unsigned long long * clen_p, const unsigned char * m, unsigned long long mlen, const unsigned char * ad, unsigned long long adlen, const unsigned char * nsec, const unsigned char * npub, const unsigned char * k );

int next_crypto_aead_chacha20poly1305_ietf_decrypt( unsigned char * m, unsigned long long * mlen_p, unsigned char * nsec, const unsigned char * c, unsigned long long clen, const unsigned char * ad, unsigned long long adlen, const unsigned char * npub, const unsigned char * k );

int next_crypto_aead_xchacha20poly1305_ietf_encrypt( unsigned char * c, unsigned long long * clen_p, const unsigned char * m, unsigned long long mlen, const unsigned char * ad, unsigned long long adlen, const unsigned char * nsec, const unsigned char * npub, const unsigned char * k );

int next_crypto_aead_xchacha20poly1305_ietf_decrypt( unsigned char * m, unsigned long long * mlen_p, unsigned char * nsec, const unsigned char * c, unsigned long long clen, const unsigned char * ad, unsigned long long adlen, const unsigned char * npub, const unsigned char * k );

int next_crypto_kx_keypair( unsigned char * pk, unsigned char * sk );

int next_crypto_kx_client_session_keys( unsigned char * rx, unsigned char * tx, const unsigned char * client_pk, const unsigned char * client_sk, const unsigned char * server_pk );

int next_crypto_kx_server_session_keys( unsigned char * rx, unsigned char * tx, const unsigned char * server_pk, const unsigned char * server_sk, const unsigned char * client_pk );

int next_crypto_box_keypair( unsigned char * pk, unsigned char * sk );

int next_crypto_box_easy( unsigned char * c, const unsigned char * m, unsigned long long mlen, const unsigned char * n, const unsigned char * pk, const unsigned char * sk );

int next_crypto_box_open_easy( unsigned char * m, const unsigned char * c, unsigned long long clen, const unsigned char * n, const unsigned char * pk, const unsigned char * sk );

int next_crypto_hash_sha256( unsigned char * hash, const unsigned char * data, size_t data_bytes );

#endif // #ifndef NEXT_CRYPTO_H

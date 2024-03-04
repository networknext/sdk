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

#ifndef NEXT_ROUTE_TOKEN_H
#define NEXT_ROUTE_TOKEN_H

#include "next.h"
#include "next_crypto.h"
#include "next_read_write.h"

struct next_route_token_t
{
    uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
    uint64_t expire_timestamp;
    uint64_t session_id;
    int kbps_up;
    int kbps_down;
    uint32_t next_address;              // big endian
    uint32_t prev_address;              // big endian
    uint16_t next_port;
    uint16_t prev_port;    
    uint8_t session_version;
    uint8_t next_internal;
    uint8_t prev_internal;
};

inline void next_read_route_token( next_route_token_t * token, const uint8_t * buffer )
{
    next_assert( token );
    next_assert( buffer );

    const uint8_t * p = buffer;

    next_read_bytes( &p, token->private_key, NEXT_SESSION_PRIVATE_KEY_BYTES );
    token->expire_timestamp = next_read_uint64( &p );
    token->session_id = next_read_uint64( &p );
    token->kbps_up = next_read_uint32( &p );
    token->kbps_down = next_read_uint32( &p );
    token->next_address = next_read_uint32( &p );
    token->prev_address = next_read_uint32( &p );
    token->next_port = next_read_uint16( &p );
    token->prev_port = next_read_uint16( &p );
    token->session_version = next_read_uint8( &p );
    token->next_internal = next_read_uint8( &p );
    token->prev_internal = next_read_uint8( &p );

    next_assert( p - buffer == NEXT_ROUTE_TOKEN_BYTES );
}

inline int next_decrypt_route_token( const uint8_t * key, const uint8_t * nonce, uint8_t * buffer, uint8_t * decrypted )
{
    next_assert( key );
    next_assert( buffer );
    unsigned long long decrypted_len;
    if ( next_crypto_aead_xchacha20poly1305_ietf_decrypt( decrypted, &decrypted_len, NULL, buffer, NEXT_ROUTE_TOKEN_BYTES + 16, NULL, 0, nonce, key ) != 0 )
    {
        return NEXT_ERROR;
    }
    return NEXT_OK;
}

inline int next_read_encrypted_route_token( uint8_t ** buffer, next_route_token_t * token, const uint8_t * key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( key );

    const uint8_t * nonce = *buffer;

    *buffer += 24;

    uint8_t decrypted[NEXT_ROUTE_TOKEN_BYTES];

    if ( next_decrypt_route_token( key, nonce, *buffer, decrypted ) != NEXT_OK )
    {
        return NEXT_ERROR;
    }

    next_read_route_token( token, decrypted );

    *buffer += NEXT_ROUTE_TOKEN_BYTES + 16;

    return NEXT_OK;
}

#endif // #ifndef NEXT_ROUTE_TOKEN_H

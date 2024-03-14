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

#ifndef NEXT_CONTINUE_TOKEN_H
#define NEXT_CONTINUE_TOKEN_H

#include "next.h"
#include "next_crypto.h"

struct next_continue_token_t
{
    uint64_t expire_timestamp;
    uint64_t session_id;
    uint8_t session_version;
};

inline void next_read_continue_token( next_continue_token_t * token, const uint8_t * buffer )
{
    next_assert( token );
    next_assert( buffer );

    const uint8_t * p = buffer;

    token->expire_timestamp = next_read_uint64( &p );
    token->session_id = next_read_uint64( &p );
    token->session_version = next_read_uint8( &p );

    next_assert( p - buffer == NEXT_CONTINUE_TOKEN_BYTES );
}

inline int next_decrypt_continue_token( const uint8_t * key, const uint8_t * nonce, uint8_t * buffer, uint8_t * decrypted )
{
    next_assert( key );
    next_assert( buffer );
    unsigned long long decrypted_len;
    if ( next_crypto_aead_xchacha20poly1305_ietf_decrypt( decrypted, &decrypted_len, NULL, buffer, NEXT_CONTINUE_TOKEN_BYTES + 16, NULL, 0, nonce, key ) != 0 )
    {
        return NEXT_ERROR;
    }
    return NEXT_OK;
}

inline int next_read_encrypted_continue_token( uint8_t ** buffer, next_continue_token_t * token, const uint8_t * key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( key );

    const uint8_t * nonce = *buffer;

    *buffer += 24;

    uint8_t decrypted[NEXT_CONTINUE_TOKEN_BYTES];

    if ( next_decrypt_continue_token( key, nonce, *buffer, decrypted ) != NEXT_OK )
    {
        return NEXT_ERROR;
    }

    next_read_continue_token( token, decrypted );

    *buffer += NEXT_CONTINUE_TOKEN_BYTES + 16;

    return NEXT_OK;
}

#endif // #ifndef NEXT_CONTINUE_TOKEN_H

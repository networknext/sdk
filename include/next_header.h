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

#ifndef NEXT_HEADER_H
#define NEXT_HEADER_H

#include "next.h"
#include "next_crypto.h"
#include "next_read_write.h"

inline int next_write_header( uint8_t type, uint64_t sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, uint8_t * buffer )
{
    next_assert( private_key );
    next_assert( buffer );

    uint8_t * start = buffer;

    (void) start;

    next_write_uint64( &buffer, sequence );

    uint8_t * additional = buffer;
    const int additional_length = 8 + 1;

    next_write_uint64( &buffer, session_id );
    next_write_uint8( &buffer, session_version );

    uint8_t nonce[12];
    {
        uint8_t * p = nonce;
        next_write_uint32( &p, type );
        next_write_uint64( &p, sequence );
    }

    unsigned long long encrypted_length = 0;

    int result = next_crypto_aead_chacha20poly1305_ietf_encrypt( buffer, &encrypted_length,
                                                                 buffer, 0,
                                                                 additional, (unsigned long long) additional_length,
                                                                 NULL, nonce, private_key );

    if ( result != 0 )
        return NEXT_ERROR;

    buffer += encrypted_length;

    next_assert( int( buffer - start ) == NEXT_HEADER_BYTES );

    return NEXT_OK;
}

inline void next_peek_header( uint64_t * sequence, uint64_t * session_id, uint8_t * session_version, const uint8_t * buffer, int buffer_length )
{
    uint64_t packet_sequence;

    next_assert( buffer );
    next_assert( buffer_length >= NEXT_HEADER_BYTES );

    packet_sequence = next_read_uint64( &buffer );

    *sequence = packet_sequence;
    *session_id = next_read_uint64( &buffer );
    *session_version = next_read_uint8( &buffer );

    (void) buffer_length;
}

inline int next_read_header( int packet_type, uint64_t * sequence, uint64_t * session_id, uint8_t * session_version, const uint8_t * private_key, uint8_t * buffer, int buffer_length )
{
    next_assert( private_key );
    next_assert( buffer );

    if ( buffer_length < NEXT_HEADER_BYTES )
    {
        return NEXT_ERROR;
    }

    const uint8_t * p = buffer;

    uint64_t packet_sequence = next_read_uint64( &p );

    const uint8_t * additional = p;

    const int additional_length = 8 + 1;

    uint64_t packet_session_id = next_read_uint64( &p );
    uint8_t packet_session_version = next_read_uint8( &p );

    uint8_t nonce[12];
    {
        uint8_t * q = nonce;
        next_write_uint32( &q, packet_type );
        next_write_uint64( &q, packet_sequence );
    }

    unsigned long long decrypted_length;

    int result = next_crypto_aead_chacha20poly1305_ietf_decrypt( buffer + 17, &decrypted_length, NULL,
                                                                 buffer + 17, (unsigned long long) NEXT_CRYPTO_AEAD_CHACHA20POLY1305_IETF_ABYTES,
                                                                 additional, (unsigned long long) additional_length,
                                                                 nonce, private_key );

    if ( result != 0 )
    {
        return NEXT_ERROR;
    }

    *sequence = packet_sequence;
    *session_id = packet_session_id;
    *session_version = packet_session_version;

    return NEXT_OK;
}

#endif // #ifndef NEXT_HEADER_H

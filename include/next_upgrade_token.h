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

#ifndef NEXT_UPGRADE_TOKEN_H
#define NEXT_UPGRADE_TOKEN_H

struct NextUpgradeToken
{
    uint64_t session_id;
    uint64_t expire_timestamp;
    next_address_t client_address;
    next_address_t server_address;

    NextUpgradeToken()
    {
        session_id = 0;
        expire_timestamp = 0;
        memset( &client_address, 0, sizeof(next_address_t) );
        memset( &server_address, 0, sizeof(next_address_t) );
    }

    int Write( uint8_t * buffer, const uint8_t * private_key )
    {
        next_assert( buffer );
        next_assert( private_key );

        memset( buffer, 0, NEXT_UPGRADE_TOKEN_BYTES );

        uint8_t * nonce = buffer;
        next_crypto_random_bytes( nonce, NEXT_CRYPTO_SECRETBOX_NONCEBYTES );
        buffer += NEXT_CRYPTO_SECRETBOX_NONCEBYTES;

        uint8_t * p = buffer;

        next_write_uint64( &p, session_id );
        next_write_uint64( &p, expire_timestamp );
        next_write_address( &p, &client_address );
        next_write_address( &p, &server_address );

        int bytes_written = p - buffer;

        next_crypto_secretbox_easy( buffer, buffer, NEXT_UPGRADE_TOKEN_BYTES - NEXT_CRYPTO_SECRETBOX_NONCEBYTES - NEXT_CRYPTO_SECRETBOX_MACBYTES, nonce, private_key );

        next_assert( NEXT_CRYPTO_SECRETBOX_NONCEBYTES + bytes_written + NEXT_CRYPTO_SECRETBOX_MACBYTES <= NEXT_UPGRADE_TOKEN_BYTES );

        return NEXT_CRYPTO_SECRETBOX_NONCEBYTES + bytes_written + NEXT_CRYPTO_SECRETBOX_MACBYTES;
    }

    bool Read( const uint8_t * buffer, const uint8_t * private_key )
    {
        next_assert( buffer );
        next_assert( private_key );

        const uint8_t * nonce = buffer;

        uint8_t decrypted[NEXT_UPGRADE_TOKEN_BYTES];
        memcpy( decrypted, buffer + NEXT_CRYPTO_SECRETBOX_NONCEBYTES, NEXT_UPGRADE_TOKEN_BYTES - NEXT_CRYPTO_SECRETBOX_NONCEBYTES );

        if ( next_crypto_secretbox_open_easy( decrypted, decrypted, NEXT_UPGRADE_TOKEN_BYTES - NEXT_CRYPTO_SECRETBOX_NONCEBYTES, nonce, private_key ) != 0 )
            return false;

        const uint8_t * p = decrypted;

        session_id = next_read_uint64( &p );
        expire_timestamp = next_read_uint64( &p );
        next_read_address( &p, &client_address );
        next_read_address( &p, &server_address );

        return true;
    }
};

#endif // #ifndef NEXT_UPGRADE_TOKEN_H

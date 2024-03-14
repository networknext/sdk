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

#include "next_packet_filter.h"
#include "next_address.h"
#include "next_hash.h"

#include <memory.h>

void next_generate_pittle( uint8_t * output, const uint8_t * from_address, const uint8_t * to_address, uint16_t packet_length )
{
    next_assert( output );
    next_assert( from_address );
    next_assert( to_address );
    next_assert( packet_length > 0 );
#if NEXT_BIG_ENDIAN
    next_bswap( packet_length );
#endif // #if NEXT_BIG_ENDIAN
    uint16_t sum = 0;
    for ( int i = 0; i < 4; ++i ) { sum += uint8_t(from_address[i]); }
    for ( int i = 0; i < 4; ++i ) { sum += uint8_t(to_address[i]); }
    const char * packet_length_data = (const char*) &packet_length;
    sum += uint8_t(packet_length_data[0]);
    sum += uint8_t(packet_length_data[1]);
#if NEXT_BIG_ENDIAN
    next_bswap( sum );
#endif // #if NEXT_BIG_ENDIAN
    const char * sum_data = (const char*) &sum;
    output[0] = 1 | ( uint8_t(sum_data[0]) ^ uint8_t(sum_data[1]) ^ 193 );
    output[1] = 1 | ( ( 255 - output[0] ) ^ 113 );
}

void next_generate_chonkle( uint8_t * output, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address, uint16_t packet_length )
{
    next_assert( output );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );
    next_assert( packet_length > 0 );
#if NEXT_BIG_ENDIAN
    next_bswap( packet_length );
#endif // #if NEXT_BIG_ENDIAN
    next_fnv_t fnv;
    next_fnv_init( &fnv );
    next_fnv_write( &fnv, magic, 8 );
    next_fnv_write( &fnv, from_address, 4 );
    next_fnv_write( &fnv, to_address, 4 );
    next_fnv_write( &fnv, (const uint8_t*) &packet_length, 2 );
    uint64_t hash = next_fnv_finalize( &fnv );
#if NEXT_BIG_ENDIAN
    next_bswap( hash );
#endif // #if NEXT_BIG_ENDIAN
    const char * data = (const char*) &hash;
    output[0] = ( ( data[6] & 0xC0 ) >> 6 ) + 42;
    output[1] = ( data[3] & 0x1F ) + 200;
    output[2] = ( ( data[2] & 0xFC ) >> 2 ) + 5;
    output[3] = data[0];
    output[4] = ( data[2] & 0x03 ) + 78;
    output[5] = ( data[4] & 0x7F ) + 96;
    output[6] = ( ( data[1] & 0xFC ) >> 2 ) + 100;
    if ( ( data[7] & 1 ) == 0 ) { output[7] = 79; } else { output[7] = 7; }
    if ( ( data[4] & 0x80 ) == 0 ) { output[8] = 37; } else { output[8] = 83; }
    output[9] = ( data[5] & 0x07 ) + 124;
    output[10] = ( ( data[1] & 0xE0 ) >> 5 ) + 175;
    output[11] = ( data[6] & 0x3F ) + 33;
    const int value = ( data[1] & 0x03 );
    if ( value == 0 ) { output[12] = 97; } else if ( value == 1 ) { output[12] = 5; } else if ( value == 2 ) { output[12] = 43; } else { output[12] = 13; }
    output[13] = ( ( data[5] & 0xF8 ) >> 3 ) + 210;
    output[14] = ( ( data[7] & 0xFE ) >> 1 ) + 17;
}

bool next_basic_packet_filter( const uint8_t * data, uint16_t packet_length )
{
    if ( packet_length == 0 )
        return false;

    if ( data[0] == 0 ) // IMPORTANT: passthrough packet type
        return true;

    if ( packet_length < 18 )
        return false;

    if ( data[2] != ( 1 | ( ( 255 - data[1] ) ^ 113 ) ) )
        return false;

    if ( data[3] < 0x2A || data[3] > 0x2D )
        return false;

    if ( data[4] < 0xC8 || data[4] > 0xE7 )
        return false;

    if ( data[5] < 0x05 || data[5] > 0x44 )
        return false;

    if ( data[7] < 0x4E || data[7] > 0x51 )
        return false;

    if ( data[8] < 0x60 || data[8] > 0xDF )
        return false;

    if ( data[9] < 0x64 || data[9] > 0xE3 )
        return false;

    if ( data[10] != 0x07 && data[10] != 0x4F )
        return false;

    if ( data[11] != 0x25 && data[11] != 0x53 )
        return false;

    if ( data[12] < 0x7C || data[12] > 0x83 )
        return false;

    if ( data[13] < 0xAF || data[13] > 0xB6 )
        return false;

    if ( data[14] < 0x21 || data[14] > 0x60 )
        return false;

    if ( data[15] != 0x61 && data[15] != 0x05 && data[15] != 0x2B && data[15] != 0x0D )
        return false;

    if ( data[16] < 0xD2 || data[16] > 0xF1 )
        return false;

    if ( data[17] < 0x11 || data[17] > 0x90 )
        return false;

    return true;
}

void next_address_data( const next_address_t * address, uint8_t * address_data )
{
    // IMPORTANT: Only IPv4 addresses are supported for the packet filter right now.
    // The socket code automatically converts IPv4 as IPv6 addresses to IPv4 before passing in for dual stack
    next_assert( address );
    address_data[0] = address->data.ipv4[0];
    address_data[1] = address->data.ipv4[1];
    address_data[2] = address->data.ipv4[2];
    address_data[3] = address->data.ipv4[3];
}

bool next_advanced_packet_filter( const uint8_t * data, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address, uint16_t packet_length )
{
    if ( data[0] == 0 ) // IMPORTANT: for passthrough packet type
        return true;

    if ( packet_length < 18 )
        return false;
    
    uint8_t a[2];
    uint8_t b[15];

    next_generate_pittle( a, from_address, to_address, packet_length );

    next_generate_chonkle( b, magic, from_address, to_address, packet_length );

    if ( memcmp( a, data + 1, 2 ) != 0 )
        return false;

    if ( memcmp( b, data + 3, 15 ) != 0 )
        return false;

    return true;
}

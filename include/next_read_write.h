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

#ifndef NEXT_READ_WRITE_H
#define NEXT_READ_WRITE_H

#include "next_config.h"
#include "next_address.h"

#include <memory.h>

// ----------------------------------------------------------------------

inline void next_write_uint8( uint8_t ** p, uint8_t value )
{
    **p = value;
    ++(*p);
}

inline void next_write_uint16( uint8_t ** p, uint16_t value )
{
    (*p)[0] = value & 0xFF;
    (*p)[1] = value >> 8;
    *p += 2;
}

inline void next_write_uint32( uint8_t ** p, uint32_t value )
{
    (*p)[0] = value & 0xFF;
    (*p)[1] = ( value >> 8  ) & 0xFF;
    (*p)[2] = ( value >> 16 ) & 0xFF;
    (*p)[3] = value >> 24;
    *p += 4;
}

inline void next_write_uint64( uint8_t ** p, uint64_t value )
{
    (*p)[0] = value & 0xFF;
    (*p)[1] = ( value >> 8  ) & 0xFF;
    (*p)[2] = ( value >> 16 ) & 0xFF;
    (*p)[3] = ( value >> 24 ) & 0xFF;
    (*p)[4] = ( value >> 32 ) & 0xFF;
    (*p)[5] = ( value >> 40 ) & 0xFF;
    (*p)[6] = ( value >> 48 ) & 0xFF;
    (*p)[7] = value >> 56;
    *p += 8;
}

inline void next_write_float32( uint8_t ** p, float value )
{
    uint32_t value_int = 0;
    char * p_value = (char*)(&value);
    char * p_value_int = (char*)(&value_int);
    memcpy(p_value_int, p_value, sizeof(uint32_t));
    next_write_uint32( p, value_int);
}

inline void next_write_float64( uint8_t ** p, double value )
{
    uint64_t value_int = 0;
    char * p_value = (char *)(&value);
    char * p_value_int = (char *)(&value_int);
    memcpy(p_value_int, p_value, sizeof(uint64_t));
    next_write_uint64( p, value_int);
}

inline void next_write_bytes( uint8_t ** p, const uint8_t * byte_array, int num_bytes )
{
    for ( int i = 0; i < num_bytes; ++i )
    {
        next_write_uint8( p, byte_array[i] );
    }
}

inline void next_write_address( uint8_t ** buffer, const next_address_t * address )
{
    next_assert( buffer );
    next_assert( *buffer );
    next_assert( address );

    if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        next_write_uint8( buffer, NEXT_ADDRESS_IPV4 );
        for ( int i = 0; i < 4; ++i )
        {
            next_write_uint8( buffer, address->data.ipv4[i] );
        }
        next_write_uint16( buffer, address->port );
    }
    else if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        next_write_uint8( buffer, NEXT_ADDRESS_IPV6 );
        for ( int i = 0; i < 8; ++i )
        {
            next_write_uint16( buffer, address->data.ipv6[i] );
        }
        next_write_uint16( buffer, address->port );
    }
    else
    {
        next_write_uint8( buffer, NEXT_ADDRESS_NONE );    
    }
}

// ----------------------------------------------------------------------

inline uint8_t next_read_uint8( const uint8_t ** p )
{
    uint8_t value = **p;
    ++(*p);
    return value;
}

inline uint16_t next_read_uint16( const uint8_t ** p )
{
    uint16_t value;
    value = (*p)[0];
    value |= ( ( (uint16_t)( (*p)[1] ) ) << 8 );
    *p += 2;
    return value;
}

inline uint32_t next_read_uint32( const uint8_t ** p )
{
    uint32_t value;
    value  = (*p)[0];
    value |= ( ( (uint32_t)( (*p)[1] ) ) << 8 );
    value |= ( ( (uint32_t)( (*p)[2] ) ) << 16 );
    value |= ( ( (uint32_t)( (*p)[3] ) ) << 24 );
    *p += 4;
    return value;
}

inline uint64_t next_read_uint64( const uint8_t ** p )
{
    uint64_t value;
    value  = (*p)[0];
    value |= ( ( (uint64_t)( (*p)[1] ) ) << 8  );
    value |= ( ( (uint64_t)( (*p)[2] ) ) << 16 );
    value |= ( ( (uint64_t)( (*p)[3] ) ) << 24 );
    value |= ( ( (uint64_t)( (*p)[4] ) ) << 32 );
    value |= ( ( (uint64_t)( (*p)[5] ) ) << 40 );
    value |= ( ( (uint64_t)( (*p)[6] ) ) << 48 );
    value |= ( ( (uint64_t)( (*p)[7] ) ) << 56 );
    *p += 8;
    return value;
}

inline float next_read_float32( const uint8_t ** p )
{
    uint32_t value_int = next_read_uint32( p );
    float value_float = 0.0f;
    uint8_t * pointer_int = (uint8_t *)( &value_int );
    uint8_t * pointer_float = (uint8_t *)( &value_float );
    memcpy( pointer_float, pointer_int, sizeof( value_int ) );
    return value_float;
}

inline double next_read_float64( const uint8_t ** p )
{
    uint64_t value_int = next_read_uint64( p );
    double value_float = 0.0;
    uint8_t * pointer_int = (uint8_t *)( &value_int );
    uint8_t * pointer_float = (uint8_t *)( &value_float );
    memcpy( pointer_float, pointer_int, sizeof( value_int ) );
    return value_float;
}

inline void next_read_bytes( const uint8_t ** p, uint8_t * byte_array, int num_bytes )
{
    for ( int i = 0; i < num_bytes; ++i )
    {
        byte_array[i] = next_read_uint8( p );
    }
}

inline void next_read_address( const uint8_t ** buffer, next_address_t * address )
{
    memset( address, 0, sizeof(next_address_t) );

    address->type = next_read_uint8( buffer );

    if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        for ( int j = 0; j < 4; ++j )
        {
            address->data.ipv4[j] = next_read_uint8( buffer );
        }
        address->port = next_read_uint16( buffer );
    }
    else if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        for ( int j = 0; j < 8; ++j )
        {
            address->data.ipv6[j] = next_read_uint16( buffer );
        }
        address->port = next_read_uint16( buffer );
    }
}

inline void next_read_address_variable( const uint8_t ** buffer, next_address_t * address )
{
    const uint8_t * start = *buffer;

    memset( address, 0, sizeof(next_address_t) );

    address->type = next_read_uint8( buffer );

    if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        for ( int j = 0; j < 4; ++j )
        {
            address->data.ipv4[j] = next_read_uint8( buffer );
        }
        address->port = next_read_uint16( buffer );
    }
    else if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        for ( int j = 0; j < 8; ++j )
        {
            address->data.ipv6[j] = next_read_uint16( buffer );
        }
        address->port = next_read_uint16( buffer );
    }

    (void) start;
}

// ----------------------------------------------------------------------

inline void next_write_address_ipv4( uint8_t ** buffer, const next_address_t * address )
{
    next_assert( buffer );
    next_assert( *buffer );
    next_assert( address );

    uint8_t * start = *buffer;

    (void) buffer;

    next_assert( address->type == NEXT_ADDRESS_IPV4 );

    for ( int i = 0; i < 4; ++i )
    {
        next_write_uint8( buffer, address->data.ipv4[i] );
    }
    next_write_uint16( buffer, address->port );

    (void) start;

    next_assert( *buffer - start == 6 );
}

inline void next_read_address_ipv4( const uint8_t ** buffer, next_address_t * address )
{
    const uint8_t * start = *buffer;

    memset( address, 0, sizeof(next_address_t) );

    address->type = NEXT_ADDRESS_IPV4;

    for ( int j = 0; j < 4; ++j )
    {
        address->data.ipv4[j] = next_read_uint8( buffer );
    }
    address->port = next_read_uint16( buffer );

    (void) start;

    next_assert( *buffer - start == 6 );
}

// ---------------------------------------------------------------------

#endif // #ifndef NEXT_READ_WRITE_H

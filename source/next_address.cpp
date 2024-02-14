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

#include "next_address.h"
#include "next_platform.h"

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

#define NEXT_ADDRESS_BUFFER_SAFETY 32

int next_address_parse( next_address_t * address, const char * address_string_in )
{
    next_assert( address );
    next_assert( address_string_in );

    if ( !address )
        return NEXT_ERROR;

    if ( !address_string_in )
        return NEXT_ERROR;

    memset( address, 0, sizeof( next_address_t ) );

    // first try to parse the string as an IPv6 address:
    // 1. if the first character is '[' then it's probably an ipv6 in form "[addr6]:portnum"
    // 2. otherwise try to parse as an IPv6 address using inet_pton

    char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH + NEXT_ADDRESS_BUFFER_SAFETY*2];

    char * address_string = buffer + NEXT_ADDRESS_BUFFER_SAFETY;
    next_copy_string( address_string, address_string_in, NEXT_MAX_ADDRESS_STRING_LENGTH );

    int address_string_length = (int) strlen( address_string );

    if ( address_string[0] == '[' )
    {
        const int base_index = address_string_length - 1;

        // note: no need to search past 6 characters as ":65535" is longest possible port value
        for ( int i = 0; i < 6; ++i )
        {
            const int index = base_index - i;
            if ( index < 0 )
            {
                return NEXT_ERROR;
            }
            if ( address_string[index] == ':' )
            {
                address->port = (uint16_t) ( atoi( &address_string[index + 1] ) );
                address_string[index-1] = '\0';
                break;
            }
            else if ( address_string[index] == ']' )
            {
                // no port number
                address->port = 0;
                address_string[index] = '\0';
                break;
            }
        }
        address_string += 1;
    }

    uint16_t addr6[8];
    if ( next_platform_inet_pton6( address_string, addr6 ) == NEXT_OK )
    {
        address->type = NEXT_ADDRESS_IPV6;
        for ( int i = 0; i < 8; ++i )
        {
            address->data.ipv6[i] = next_platform_ntohs( addr6[i] );
        }
        return NEXT_OK;
    }

    // otherwise it's probably an IPv4 address:
    // 1. look for ":portnum", if found save the portnum and strip it out
    // 2. parse remaining ipv4 address via inet_pton

    address_string_length = (int) strlen( address_string );
    const int base_index = address_string_length - 1;
    for ( int i = 0; i < 6; ++i )
    {
        const int index = base_index - i;
        if ( index < 0 )
            break;
        if ( address_string[index] == ':' )
        {
            address->port = (uint16_t)( atoi( &address_string[index + 1] ) );
            address_string[index] = '\0';
        }
    }

    uint32_t addr4;
    if ( next_platform_inet_pton4( address_string, &addr4 ) == NEXT_OK )
    {
        address->type = NEXT_ADDRESS_IPV4;
        address->data.ipv4[3] = (uint8_t) ( ( addr4 & 0xFF000000 ) >> 24 );
        address->data.ipv4[2] = (uint8_t) ( ( addr4 & 0x00FF0000 ) >> 16 );
        address->data.ipv4[1] = (uint8_t) ( ( addr4 & 0x0000FF00 ) >> 8  );
        address->data.ipv4[0] = (uint8_t) ( ( addr4 & 0x000000FF )     );
        return NEXT_OK;
    }

    return NEXT_ERROR;
}

const char * next_address_to_string( const next_address_t * address, char * buffer )
{
    next_assert( buffer );

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
#if defined(WINVER) && WINVER <= 0x0502
        // ipv6 not supported
        buffer[0] = '\0';
        return buffer;
#else
        uint16_t ipv6_network_order[8];
        for ( int i = 0; i < 8; ++i )
            ipv6_network_order[i] = next_platform_htons( address->data.ipv6[i] );
        char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_platform_inet_ntop6( ipv6_network_order, address_string, sizeof( address_string ) );
        if ( address->port == 0 )
        {
            next_copy_string( buffer, address_string, NEXT_MAX_ADDRESS_STRING_LENGTH );
            return buffer;
        }
        else
        {
            if ( snprintf( buffer, NEXT_MAX_ADDRESS_STRING_LENGTH, "[%s]:%hu", address_string, address->port ) < 0 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "address string truncated: [%s]:%hu", address_string, address->port );
            }
            return buffer;
        }
#endif
    }
    else if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        if ( address->port != 0 )
        {
            snprintf( buffer,
                      NEXT_MAX_ADDRESS_STRING_LENGTH,
                      "%d.%d.%d.%d:%d",
                      address->data.ipv4[0],
                      address->data.ipv4[1],
                      address->data.ipv4[2],
                      address->data.ipv4[3],
                      address->port );
        }
        else
        {
            snprintf( buffer,
                      NEXT_MAX_ADDRESS_STRING_LENGTH,
                      "%d.%d.%d.%d",
                      address->data.ipv4[0],
                      address->data.ipv4[1],
                      address->data.ipv4[2],
                      address->data.ipv4[3] );
        }
        return buffer;
    }
    else
    {
        snprintf( buffer, NEXT_MAX_ADDRESS_STRING_LENGTH, "%s", "NONE" );
        return buffer;
    }
}

const char * next_address_to_string_without_port( const next_address_t * address, char * buffer )
{
    next_assert( buffer );

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
#if defined(WINVER) && WINVER <= 0x0502
        // ipv6 not supported
        buffer[0] = '\0';
        return buffer;
#else
        uint16_t ipv6_network_order[8];
        for ( int i = 0; i < 8; ++i )
            ipv6_network_order[i] = next_platform_htons( address->data.ipv6[i] );
        char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_platform_inet_ntop6( ipv6_network_order, address_string, sizeof( address_string ) );
        next_copy_string( buffer, address_string, NEXT_MAX_ADDRESS_STRING_LENGTH );
        return buffer;
#endif
    }
    else if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        snprintf( buffer,
                  NEXT_MAX_ADDRESS_STRING_LENGTH,
                  "%d.%d.%d.%d",
                  address->data.ipv4[0],
                  address->data.ipv4[1],
                  address->data.ipv4[2],
                  address->data.ipv4[3] );
        return buffer;
    }
    else
    {
        snprintf( buffer, NEXT_MAX_ADDRESS_STRING_LENGTH, "%s", "NONE" );
        return buffer;
    }
}

bool next_address_equal( const next_address_t * a, const next_address_t * b )
{
    next_assert( a );
    next_assert( b );

    if ( a->type != b->type )
        return false;

    if ( a->type == NEXT_ADDRESS_IPV4 )
    {
        if ( a->port != b->port )
            return false;

        for ( int i = 0; i < 4; ++i )
        {
            if ( a->data.ipv4[i] != b->data.ipv4[i] )
                return false;
        }
    }
    else if ( a->type == NEXT_ADDRESS_IPV6 )
    {
        if ( a->port != b->port )
            return false;

        for ( int i = 0; i < 8; ++i )
        {
            if ( a->data.ipv6[i] != b->data.ipv6[i] )
                return false;
        }
    }

    return true;
}

void next_address_anonymize( next_address_t * address )
{
    next_assert( address );
    if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        address->data.ipv4[3] = 0;
    }
    else
    {
        if ( next_address_is_ipv4_in_ipv6( address ) )
        {
            address->data.ipv6[7] &= 0xFF00;
        }
        else
        {
            address->data.ipv6[4] = 0;
            address->data.ipv6[5] = 0;
            address->data.ipv6[6] = 0;
            address->data.ipv6[7] = 0;
        }
    }
    address->port = 0;
}

bool next_address_is_ipv4_in_ipv6( struct next_address_t * address )
{
    next_assert( address );
    if ( address->type != NEXT_ADDRESS_IPV6 )
        return false;
    if ( address->data.ipv6[0] != 0x0000 )
        return false;
    if ( address->data.ipv6[1] != 0x0000 )
        return false;
    if ( address->data.ipv6[2] != 0x0000 )
        return false;
    if ( address->data.ipv6[3] != 0x0000 )
        return false;
    if ( address->data.ipv6[4] != 0x0000 )
        return false;
    if ( address->data.ipv6[5] != 0xFFFF )
        return false;
    return true;
}

void next_address_convert_ipv4_to_ipv6( struct next_address_t * address )
{
    next_assert( address );
    const uint8_t a = uint8_t( address->data.ipv4[0] );
    const uint8_t b = uint8_t( address->data.ipv4[1] );
    const uint8_t c = uint8_t( address->data.ipv4[2] );
    const uint8_t d = uint8_t( address->data.ipv4[3] );
    address->type = NEXT_ADDRESS_IPV6;
    address->data.ipv6[0] = 0x0000;
    address->data.ipv6[1] = 0x0000;
    address->data.ipv6[2] = 0x0000;
    address->data.ipv6[3] = 0x0000;
    address->data.ipv6[4] = 0x0000;
    address->data.ipv6[5] = 0xFFFF;
    address->data.ipv6[6] = ( uint16_t(a) << 8 ) | uint16_t(b);
    address->data.ipv6[7] = ( uint16_t(c) << 8 ) | uint16_t(d);
}

void next_address_convert_ipv6_to_ipv4( struct next_address_t * address )
{
    // IMPORTANT: this function is *only* for converting ipv4 mapped addresses in ipv6 back to native ipv4
    next_assert( address );
    next_assert( next_address_is_ipv4_in_ipv6( address ) );
    const uint8_t a = ( address->data.ipv6[6] & 0xFF00 ) >> 8;
    const uint8_t b = ( address->data.ipv6[6] & 0x00FF );
    const uint8_t c = ( address->data.ipv6[7] & 0xFF00 ) >> 8;
    const uint8_t d = ( address->data.ipv6[7] & 0x00FF );
    address->type = NEXT_ADDRESS_IPV4;
    address->data.ipv4[0] = a;
    address->data.ipv4[1] = b;
    address->data.ipv4[2] = c;
    address->data.ipv4[3] = d;
}

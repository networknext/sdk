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

#ifndef NEXT_ADDRESS_H
#define NEXT_ADDRESS_H

#include "next.h"

#include <stdint.h>

#ifndef NEXT_ADDRESS_ALREADY_DEFINED
#define NEXT_ADDRESS_ALREADY_DEFINED
struct next_address_t
{
    union { uint32_t ip; uint8_t ipv4[4]; uint16_t ipv6[8]; } data;
    uint16_t port;
    uint8_t type;
};
#endif 

NEXT_EXPORT_FUNC int next_address_parse( struct next_address_t * address, const char * address_string );

NEXT_EXPORT_FUNC const char * next_address_to_string( const struct next_address_t * address, char * buffer );

NEXT_EXPORT_FUNC const char * next_address_to_string_without_port( const struct next_address_t * address, char * buffer );

NEXT_EXPORT_FUNC bool next_address_equal( const struct next_address_t * a, const struct next_address_t * b );

NEXT_EXPORT_FUNC void next_address_anonymize( struct next_address_t * address );

NEXT_EXPORT_FUNC bool next_address_is_ipv4_in_ipv6( struct next_address_t * address );

NEXT_EXPORT_FUNC void next_address_convert_ipv6_to_ipv4( struct next_address_t * address );

NEXT_EXPORT_FUNC void next_address_convert_ipv4_to_ipv6( struct next_address_t * address );

#endif // #ifndef NEXT_ADDRESS_H

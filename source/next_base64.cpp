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

#include "next_base64.h"

#include <string.h>

static const unsigned char base64_table_encode[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int next_base64_encode_data( const uint8_t * input, size_t input_length, char * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    char * pos;
    const uint8_t * end;
    const uint8_t * in;

    size_t output_length = 4 * ( ( input_length + 2 ) / 3 ); // 3-byte blocks to 4-byte

    if ( output_length < input_length )
    {
        return -1; // integer overflow
    }

    if ( output_length >= output_size )
    {
        return -1; // not enough room in output buffer
    }

    end = input + input_length;
    in = input;
    pos = output;
    while ( end - in >= 3 )
    {
        *pos++ = base64_table_encode[in[0] >> 2];
        *pos++ = base64_table_encode[( ( in[0] & 0x03 ) << 4 ) | ( in[1] >> 4 )];
        *pos++ = base64_table_encode[( ( in[1] & 0x0f ) << 2 ) | ( in[2] >> 6 )];
        *pos++ = base64_table_encode[in[2] & 0x3f];
        in += 3;
    }

    if ( end - in )
    {
        *pos++ = base64_table_encode[in[0] >> 2];
        if (end - in == 1)
        {
            *pos++ = base64_table_encode[(in[0] & 0x03) << 4];
            *pos++ = '=';
        }
        else
        {
            *pos++ = base64_table_encode[((in[0] & 0x03) << 4) | (in[1] >> 4)];
            *pos++ = base64_table_encode[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    output[output_length] = '\0';

    return int( output_length );
}

static const int base64_table_decode[256] =
{
    0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,
    0,  0,  0,  63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
};

int next_base64_decode_data( const char * input, uint8_t * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    size_t input_length = strlen( input );
    int pad = input_length > 0 && ( input_length % 4 || input[input_length - 1] == '=' );
    size_t L = ( ( input_length + 3 ) / 4 - pad ) * 4;
    size_t output_length = L / 4 * 3 + pad;

    if ( output_length > output_size )
    {
        return 0;
    }

    for ( size_t i = 0, j = 0; i < L; i += 4 )
    {
        int n = base64_table_decode[int( input[i] )] << 18 | base64_table_decode[int( input[i + 1] )] << 12 | base64_table_decode[int( input[i + 2] )] << 6 | base64_table_decode[int( input[i + 3] )];
        output[j++] = uint8_t( n >> 16 );
        output[j++] = uint8_t( n >> 8 & 0xFF );
        output[j++] = uint8_t( n & 0xFF );
    }

    if ( pad )
    {
        int n = base64_table_decode[int( input[L] )] << 18 | base64_table_decode[int( input[L + 1] )] << 12;
        output[output_length - 1] = uint8_t( n >> 16 );

        if (input_length > L + 2 && input[L + 2] != '=')
        {
            n |= base64_table_decode[int( input[L + 2] )] << 6;
            output_length += 1;
            if ( output_length > output_size )
            {
                return 0;
            }
            output[output_length - 1] = uint8_t( n >> 8 & 0xFF );
        }
    }

    return int( output_length );
}

int next_base64_encode_string( const char * input, char * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    return next_base64_encode_data( (const uint8_t *)( input ), strlen( input ), output, output_size );
}

int next_base64_decode_string( const char * input, char * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    int output_length = next_base64_decode_data( input, (uint8_t *)( output ), output_size - 1 );
    if ( output_length < 0 )
    {
        return 0;
    }

    output[output_length] = '\0';

    return output_length;
}

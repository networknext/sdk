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

#include "next_tests.h"
#include "next.h"

#if NEXT_DEVELOPMENT

#include "next_crypto.h"
#include "next_platform.h"
#include "next_address.h"
#include "next_read_write.h"
#include "next_bitpacker.h"
#include "next_stream.h"
#include "next_serialize.h"
#include "next_base64.h"
#include "next_queue.h"
#include "next_hash.h"
#include "next_replay_protection.h"
#include "next_ping_history.h"
#include "next_upgrade_token.h"
#include "next_route_token.h"
#include "next_continue_token.h"
#include "next_header.h"
#include "next_packet_filter.h"
#include "next_bandwidth_limiter.h"
#include "next_packet_loss_tracker.h"
#include "next_out_of_order_tracker.h"
#include "next_jitter_tracker.h"
#include "next_pending_session_manager.h"
#include "next_proxy_session_manager.h"
#include "next_session_manager.h"
#include "next_relay_manager.h"
#include "next_internal_config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void next_check_handler( const char * condition,
                                const char * function,
                                const char * file,
                                int line )
{
    printf( "check failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
    fflush( stdout );
#ifndef NDEBUG
    #if defined( __GNUC__ )
        __builtin_trap();
    #elif defined( _MSC_VER )
        __debugbreak();
    #endif
#endif
    exit( 1 );
}

#define next_check( condition )                                                                                 \
do                                                                                                              \
{                                                                                                               \
    if ( !(condition) )                                                                                         \
    {                                                                                                           \
        next_check_handler( #condition, (const char*) __FUNCTION__, (const char*) __FILE__, __LINE__ );         \
    }                                                                                                           \
} while(0)

void test_time()
{
    double start = next_platform_time();
    next_platform_sleep( 0.1 );
    double finish = next_platform_time();
    next_check( finish > start );
}

void test_endian()
{
    uint32_t value = 0x11223344;
    char bytes[4];
    memcpy( bytes, &value, 4 );

#if NEXT_LITTLE_ENDIAN

    next_check( bytes[0] == 0x44 );
    next_check( bytes[1] == 0x33 );
    next_check( bytes[2] == 0x22 );
    next_check( bytes[3] == 0x11 );

#else // #if NEXT_LITTLE_ENDIAN

    next_check( bytes[3] == 0x44 );
    next_check( bytes[2] == 0x33 );
    next_check( bytes[1] == 0x22 );
    next_check( bytes[0] == 0x11 );

#endif // #if NEXT_LITTLE_ENDIAN
}

void test_base64()
{
    const char * input = "a test string. let's see if it works properly";
    char encoded[1024];
    char decoded[1024];
    next_check( next_base64_encode_string( input, encoded, sizeof(encoded) ) > 0 );
    next_check( next_base64_decode_string( encoded, decoded, sizeof(decoded) ) > 0 );
    next_check( strcmp( decoded, input ) == 0 );
    next_check( next_base64_decode_string( encoded, decoded, 10 ) == 0 );
}

void test_hash()
{
    uint64_t hash = next_datacenter_id( "local" );
    next_check( hash == 0x249f1fb6f3a680e8ULL );
}

void test_queue()
{
    const int QueueSize = 64;
    const int EntrySize = 1024;

    next_queue_t * queue = next_queue_create( NULL, QueueSize );

    next_check( queue->num_entries == 0 );
    next_check( queue->start_index == 0 );

    // attempting to pop a packet off an empty queue should return NULL

    next_check( next_queue_pop( queue ) == NULL );

    // add some entries to the queue and make sure they pop off in the correct order
    {
        const int NumEntries = 50;

        void * entries[NumEntries];

        int i;
        for ( i = 0; i < NumEntries; ++i )
        {
            entries[i] = next_malloc( NULL, EntrySize );
            memset( entries[i], 0, EntrySize );
            next_check( next_queue_push( queue, entries[i] ) == NEXT_OK );
        }

        next_check( queue->num_entries == NumEntries );

        for ( i = 0; i < NumEntries; ++i )
        {
            void * entry = next_queue_pop( queue );
            next_check( entry == entries[i] );
            next_free( NULL, entry );
        }
    }

    // after all entries are popped off, the queue is empty, so calls to pop should return NULL

    next_check( queue->num_entries == 0 );

    next_check( next_queue_pop( queue ) == NULL );

    // test that the queue can be filled to max capacity

    void * entries[QueueSize];

    int i;
    for ( i = 0; i < QueueSize; ++i )
    {
        entries[i] = next_malloc( NULL, EntrySize );
        next_check( next_queue_push( queue, entries[i] ) == NEXT_OK );
    }

    next_check( queue->num_entries == QueueSize );

    // when the queue is full, attempting to push an entry should fail

    next_check( next_queue_push( queue, next_malloc( NULL, 100 ) ) == NEXT_ERROR );

    // make sure all packets pop off in the correct order

    for ( i = 0; i < QueueSize; ++i )
    {
        void * entry = next_queue_pop( queue );
        next_check( entry == entries[i] );
        next_free( NULL, entry );
    }

    // add some entries again

    for ( i = 0; i < QueueSize; ++i )
    {
        entries[i] = next_malloc( NULL, EntrySize );
        next_check( next_queue_push( queue, entries[i] ) == NEXT_OK );
    }

    // clear the queue and make sure that all entries are freed

    next_queue_clear( queue );

    next_check( queue->start_index == 0 );
    next_check( queue->num_entries == 0 );
    for ( i = 0; i < QueueSize; ++i )
        next_check( queue->entries[i] == NULL );

    // destroy the queue

    next_queue_destroy( queue );
}

using namespace next;

void test_bitpacker()
{
    const int BufferSize = 256;

    uint8_t buffer[BufferSize];

    BitWriter writer( buffer, BufferSize );

    next_check( writer.GetData() == buffer );
    next_check( writer.GetBitsWritten() == 0 );
    next_check( writer.GetBytesWritten() == 0 );
    next_check( writer.GetBitsAvailable() == BufferSize * 8 );

    writer.WriteBits( 0, 1 );
    writer.WriteBits( 1, 1 );
    writer.WriteBits( 10, 8 );
    writer.WriteBits( 255, 8 );
    writer.WriteBits( 1000, 10 );
    writer.WriteBits( 50000, 16 );
    writer.WriteBits( 9999999, 32 );
    writer.FlushBits();

    const int bitsWritten = 1 + 1 + 8 + 8 + 10 + 16 + 32;

    next_check( writer.GetBytesWritten() == 10 );
    next_check( writer.GetBitsWritten() == bitsWritten );
    next_check( writer.GetBitsAvailable() == BufferSize * 8 - bitsWritten );

    const int bytesWritten = writer.GetBytesWritten();

    next_check( bytesWritten == 10 );

    memset( buffer + bytesWritten, 0, size_t(BufferSize) - bytesWritten );

    BitReader reader( buffer, bytesWritten );

    next_check( reader.GetBitsRead() == 0 );
    next_check( reader.GetBitsRemaining() == bytesWritten * 8 );

    uint32_t a = reader.ReadBits( 1 );
    uint32_t b = reader.ReadBits( 1 );
    uint32_t c = reader.ReadBits( 8 );
    uint32_t d = reader.ReadBits( 8 );
    uint32_t e = reader.ReadBits( 10 );
    uint32_t f = reader.ReadBits( 16 );
    uint32_t g = reader.ReadBits( 32 );

    next_check( a == 0 );
    next_check( b == 1 );
    next_check( c == 10 );
    next_check( d == 255 );
    next_check( e == 1000 );
    next_check( f == 50000 );
    next_check( g == 9999999 );

    next_check( reader.GetBitsRead() == bitsWritten );
    next_check( reader.GetBitsRemaining() == bytesWritten * 8 - bitsWritten );
}

const int MaxItems = 11;

struct TestData
{
    TestData()
    {
        memset( this, 0, sizeof( TestData ) );
    }

    int a,b,c;
    uint32_t d : 8;
    uint32_t e : 8;
    uint32_t f : 8;
    bool g;
    int numItems;
    int items[MaxItems];
    float float_value;
    double double_value;
    uint64_t uint64_value;
    uint8_t bytes[17];
    char string[256];
    next_address_t address_a, address_b, address_c;
};

struct TestContext
{
    int min;
    int max;
};

struct TestObject
{
    TestData data;

    void Init()
    {
        data.a = 1;
        data.b = -2;
        data.c = 150;
        data.d = 55;
        data.e = 255;
        data.f = 127;
        data.g = true;

        data.numItems = MaxItems / 2;
        for ( int i = 0; i < data.numItems; ++i )
            data.items[i] = i + 10;

        data.float_value = 3.1415926f;
        data.double_value = 1 / 3.0;
        data.uint64_value = 0x1234567898765432L;

        for ( int i = 0; i < (int) sizeof( data.bytes ); ++i )
            data.bytes[i] = ( i * 37 ) % 255;

        strcpy( data.string, "hello world!" );

        memset( &data.address_a, 0, sizeof(next_address_t) );

        next_address_parse( &data.address_b, "127.0.0.1:50000" );

        next_address_parse( &data.address_c, "[::1]:50000" );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        const TestContext & context = *(const TestContext*) stream.GetContext();

        serialize_int( stream, data.a, context.min, context.max );
        serialize_int( stream, data.b, context.min, context.max );

        serialize_int( stream, data.c, -100, 10000 );

        serialize_bits( stream, data.d, 6 );
        serialize_bits( stream, data.e, 8 );
        serialize_bits( stream, data.f, 7 );

        serialize_align( stream );

        serialize_bool( stream, data.g );

        serialize_int( stream, data.numItems, 0, MaxItems - 1 );
        for ( int i = 0; i < data.numItems; ++i )
            serialize_bits( stream, data.items[i], 8 );

        serialize_float( stream, data.float_value );

        serialize_double( stream, data.double_value );

        serialize_uint64( stream, data.uint64_value );

        serialize_bytes( stream, data.bytes, sizeof( data.bytes ) );

        serialize_string( stream, data.string, sizeof( data.string ) );

        serialize_address( stream, data.address_a );
        serialize_address( stream, data.address_b );
        serialize_address( stream, data.address_c );

        return true;
    }

    bool operator == ( const TestObject & other ) const
    {
        return memcmp( &data, &other.data, sizeof( TestData ) ) == 0;
    }

    bool operator != ( const TestObject & other ) const
    {
        return ! ( *this == other );
    }
};

void test_stream()
{
    const int BufferSize = 1024;

    uint8_t buffer[BufferSize];

    TestContext context;
    context.min = -10;
    context.max = +10;

    WriteStream writeStream( buffer, BufferSize );

    TestObject writeObject;
    writeObject.Init();
    writeStream.SetContext( &context );
    writeObject.Serialize( writeStream );
    writeStream.Flush();

    const int bytesWritten = writeStream.GetBytesProcessed();

    memset( buffer + bytesWritten, 0, size_t(BufferSize) - bytesWritten );

    TestObject readObject;

    ReadStream readStream( buffer, bytesWritten );
    readStream.SetContext( &context );
    readObject.Serialize( readStream );

    next_check( readObject == writeObject );
}

void test_bits_required()
{
    next_check( bits_required( 0, 0 ) == 0 );
    next_check( bits_required( 0, 1 ) == 1 );
    next_check( bits_required( 0, 2 ) == 2 );
    next_check( bits_required( 0, 3 ) == 2 );
    next_check( bits_required( 0, 4 ) == 3 );
    next_check( bits_required( 0, 5 ) == 3 );
    next_check( bits_required( 0, 6 ) == 3 );
    next_check( bits_required( 0, 7 ) == 3 );
    next_check( bits_required( 0, 8 ) == 4 );
    next_check( bits_required( 0, 255 ) == 8 );
    next_check( bits_required( 0, 65535 ) == 16 );
    next_check( bits_required( 0, 4294967295U ) == 32 );
}

void test_address()
{
    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "[" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "[]" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "[]:" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, ":" ) == NEXT_ERROR );
#if !defined(WINVER) || WINVER > 0x502 // windows xp sucks
        next_check( next_address_parse( &address, "1" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "12" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "123" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "1234" ) == NEXT_ERROR );
#endif
        next_check( next_address_parse( &address, "1234.0.12313.0000" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "1234.0.12313.0000.0.0.0.0.0" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "1312313:123131:1312313:123131:1312313:123131:1312313:123131:1312313:123131:1312313:123131" ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "." ) == NEXT_ERROR );
        next_check( next_address_parse( &address, ".." ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "..." ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "...." ) == NEXT_ERROR );
        next_check( next_address_parse( &address, "....." ) == NEXT_ERROR );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "107.77.207.77" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV4 );
        next_check( address.port == 0 );
        next_check( address.data.ipv4[0] == 107 );
        next_check( address.data.ipv4[1] == 77 );
        next_check( address.data.ipv4[2] == 207 );
        next_check( address.data.ipv4[3] == 77 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "127.0.0.1" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV4 );
        next_check( address.port == 0 );
        next_check( address.data.ipv4[0] == 127 );
        next_check( address.data.ipv4[1] == 0 );
        next_check( address.data.ipv4[2] == 0 );
        next_check( address.data.ipv4[3] == 1 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "107.77.207.77:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV4 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv4[0] == 107 );
        next_check( address.data.ipv4[1] == 77 );
        next_check( address.data.ipv4[2] == 207 );
        next_check( address.data.ipv4[3] == 77 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "127.0.0.1:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV4 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv4[0] == 127 );
        next_check( address.data.ipv4[1] == 0 );
        next_check( address.data.ipv4[2] == 0 );
        next_check( address.data.ipv4[3] == 1 );
    }

#if NEXT_PLATFORM_HAS_IPV6
    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "fe80::202:b3ff:fe1e:8329" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 0 );
        next_check( address.data.ipv6[0] == 0xfe80 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0202 );
        next_check( address.data.ipv6[5] == 0xb3ff );
        next_check( address.data.ipv6[6] == 0xfe1e );
        next_check( address.data.ipv6[7] == 0x8329 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "::" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 0 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0x0000 );
        next_check( address.data.ipv6[6] == 0x0000 );
        next_check( address.data.ipv6[7] == 0x0000 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "::1" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 0 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0x0000 );
        next_check( address.data.ipv6[6] == 0x0000 );
        next_check( address.data.ipv6[7] == 0x0001 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "[fe80::202:b3ff:fe1e:8329]:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv6[0] == 0xfe80 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0202 );
        next_check( address.data.ipv6[5] == 0xb3ff );
        next_check( address.data.ipv6[6] == 0xfe1e );
        next_check( address.data.ipv6[7] == 0x8329 );
        next_check( !next_address_is_ipv4_in_ipv6( &address ) );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "[::]:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0x0000 );
        next_check( address.data.ipv6[6] == 0x0000 );
        next_check( address.data.ipv6[7] == 0x0000 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "[::1]:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0x0000 );
        next_check( address.data.ipv6[6] == 0x0000 );
        next_check( address.data.ipv6[7] == 0x0001 );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "[::ffff:127.0.0.1]:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0xFFFF );
        next_check( address.data.ipv6[6] == 0x7F00 );
        next_check( address.data.ipv6[7] == 0x0001 );
        next_check( next_address_is_ipv4_in_ipv6( &address ) );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "[::ffff:0.0.0.0]:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0xFFFF );
        next_check( address.data.ipv6[6] == 0x0000 );
        next_check( address.data.ipv6[7] == 0x0000 );
        next_check( next_address_is_ipv4_in_ipv6( &address ) );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "[::ffff:1.2.3.4]:40000" ) == NEXT_OK );
        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0xFFFF );
        next_check( address.data.ipv6[6] == 0x0102 );
        next_check( address.data.ipv6[7] == 0x0304 );
        next_check( next_address_is_ipv4_in_ipv6( &address ) );
    }

    {
        struct next_address_t address;
        next_check( next_address_parse( &address, "[::ffff:1.2.3.4]:40000" ) == NEXT_OK );
        next_check( next_address_is_ipv4_in_ipv6( &address ) );

        next_address_convert_ipv6_to_ipv4( &address );
        next_check( address.type == NEXT_ADDRESS_IPV4 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv4[0] == 1 );
        next_check( address.data.ipv4[1] == 2 );
        next_check( address.data.ipv4[2] == 3 );
        next_check( address.data.ipv4[3] == 4 );

        next_address_convert_ipv4_to_ipv6( &address );

        next_check( address.type == NEXT_ADDRESS_IPV6 );
        next_check( address.port == 40000 );
        next_check( address.data.ipv6[0] == 0x0000 );
        next_check( address.data.ipv6[1] == 0x0000 );
        next_check( address.data.ipv6[2] == 0x0000 );
        next_check( address.data.ipv6[3] == 0x0000 );
        next_check( address.data.ipv6[4] == 0x0000 );
        next_check( address.data.ipv6[5] == 0xFFFF );
        next_check( address.data.ipv6[6] == 0x0102 );
        next_check( address.data.ipv6[7] == 0x0304 );
        next_check( next_address_is_ipv4_in_ipv6( &address ) );
    }
#endif // #if NEXT_PLATFORM_HAS_IPV6
}

void test_replay_protection()
{
    next_replay_protection_t replay_protection;

    int i;
    for ( i = 0; i < 2; ++i )
    {
        next_replay_protection_reset( &replay_protection );

        next_check( replay_protection.most_recent_sequence == 0 );

        // the first time we receive packets, they should not be already received

        #define MAX_SEQUENCE ( NEXT_REPLAY_PROTECTION_BUFFER_SIZE * 4 )

        uint64_t sequence;
        for ( sequence = 0; sequence < MAX_SEQUENCE; ++sequence )
        {
            next_check( next_replay_protection_already_received( &replay_protection, sequence ) == 0 );
            next_replay_protection_advance_sequence( &replay_protection, sequence );
        }

        // old packets outside buffer should be considered already received

        next_check( next_replay_protection_already_received( &replay_protection, 0 ) == 1 );

        // packets received a second time should be detected as already received

        for ( sequence = MAX_SEQUENCE - 10; sequence < MAX_SEQUENCE; ++sequence )
        {
            next_check( next_replay_protection_already_received( &replay_protection, sequence ) == 1 );
        }

        // jumping ahead to a much higher sequence should be considered not already received

        next_check( next_replay_protection_already_received( &replay_protection, MAX_SEQUENCE + NEXT_REPLAY_PROTECTION_BUFFER_SIZE ) == 0 );

        // old packets should be considered already received

        for ( sequence = 0; sequence < MAX_SEQUENCE; ++sequence )
        {
            next_check( next_replay_protection_already_received( &replay_protection, sequence ) == 1 );
        }
    }
}

static bool equal_within_tolerance( float a, float b, float tolerance = 0.001f )
{
    return fabs(double(a)-double(b)) <= tolerance;
}

void test_ping_stats()
{
    // default ping history is 100% packet loss
    {
        static next_ping_history_t history;
        next_ping_history_clear( &history );

        next_route_stats_t route_stats;
        next_route_stats_from_ping_history( &history, 10.0, 100.0, &route_stats );

        next_check( route_stats.rtt == 0.0f );
        next_check( route_stats.jitter == 0.0f );
        next_check( route_stats.packet_loss == 100.0f );
    }

    // add some pings without pong response, packet loss should be 100%
    {
        static next_ping_history_t history;
        next_ping_history_clear( &history );

        for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; ++i )
        {
            next_ping_history_ping_sent( &history, 10 + i * 0.01 );
        }

        next_route_stats_t route_stats;
        next_route_stats_from_ping_history( &history, 10.0, 100.0, &route_stats );

        next_check( route_stats.rtt == 0.0f );
        next_check( route_stats.jitter == 0.0f );
        next_check( route_stats.packet_loss == 100.0f );
    }

    // add some pings and set them to have a pong response, packet loss should be 0%
    {
        static next_ping_history_t history;
        next_ping_history_clear( &history );

        const double expected_rtt = 0.1;

        for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; ++i )
        {
            uint64_t sequence = next_ping_history_ping_sent( &history, 10 + i * 0.1 );
            next_ping_history_pong_received( &history, sequence, 10 + i * 0.1 + expected_rtt );
        }

        next_route_stats_t route_stats;
        next_route_stats_from_ping_history( &history, 1.0, 100.0, &route_stats );

        next_check( equal_within_tolerance( route_stats.rtt, expected_rtt * 1000.0 ) );
        next_check( equal_within_tolerance( route_stats.jitter, 0.0 ) );
        next_check( route_stats.packet_loss == 0.0 );
    }

    // add some pings and set them to have a pong response, but leave the last second of pings without response. 
    // packet loss should be zero because ping safety stops us considering packets 1 second young from having PL
    {
        static next_ping_history_t history;
        next_ping_history_clear( &history );

        const double expected_rtt = 0.1;

        const double delta_time = 10.0 / NEXT_PING_HISTORY_ENTRY_COUNT;

        for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; ++i )
        {
            const double ping_send_time = 10 + i * delta_time;
            const double pong_recv_time = ping_send_time + expected_rtt;

            if ( ping_send_time > 9.0 )
            {
                uint64_t sequence = next_ping_history_ping_sent( &history, ping_send_time );
                next_ping_history_pong_received( &history, sequence, pong_recv_time );
            }
        }

        next_route_stats_t route_stats;
        next_route_stats_from_ping_history( &history, 1.0, 100.0, &route_stats );

        next_check( equal_within_tolerance( route_stats.rtt, expected_rtt * 1000.0 ) );
        next_check( equal_within_tolerance( route_stats.jitter, 0.0 ) );
        next_check( route_stats.packet_loss == 0.0 );
    }

    // drop 1 in every 2 packets. packet loss should be 50%
    {
        static next_ping_history_t history;
        next_ping_history_clear( &history );

        const double expected_rtt = 0.1;

        for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; ++i )
        {
            uint64_t sequence = next_ping_history_ping_sent( &history, 10 + i * 0.1 );
            if ( i & 1 )
                next_ping_history_pong_received( &history, sequence, 10 + i * 0.1 + expected_rtt );
        }

        next_route_stats_t route_stats;
        next_route_stats_from_ping_history( &history, 1.0, 100.0, &route_stats );

        next_check( equal_within_tolerance( route_stats.rtt, expected_rtt * 1000.0 ) );
        next_check( equal_within_tolerance( route_stats.jitter, 0.0 ) );
        next_check( equal_within_tolerance( route_stats.packet_loss, 50.0, 2.0 ) );
    }

    // drop 1 in every 10 packets. packet loss should be ~10%
    {
        static next_ping_history_t history;
        next_ping_history_clear( &history );

        const double expected_rtt = 0.1;

        for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; ++i )
        {
            uint64_t sequence = next_ping_history_ping_sent( &history, 10 + i * 0.1 );
            if ( ( i % 10 ) )
                next_ping_history_pong_received( &history, sequence, 10 + i * 0.1 + expected_rtt );
        }

        next_route_stats_t route_stats;
        next_route_stats_from_ping_history( &history, 1.0, 100.0, &route_stats );

        next_check( equal_within_tolerance( route_stats.rtt, expected_rtt * 1000.0f ) );
        next_check( equal_within_tolerance( route_stats.jitter, 0.0 ) );
        next_check( equal_within_tolerance( route_stats.packet_loss, 10.0f, 2.0f ) );
    }

    // drop 9 in every 10 packets. packet loss should be ~90%
    {
        static next_ping_history_t history;
        next_ping_history_clear( &history );

        const double expected_rtt = 0.1;

        for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; ++i )
        {
            uint64_t sequence = next_ping_history_ping_sent( &history, 10 + i * 0.1 );
            if ( ( i % 10 ) == 0 )
                next_ping_history_pong_received( &history, sequence, 10 + i * 0.1 + expected_rtt );
        }

        next_route_stats_t route_stats;
        next_route_stats_from_ping_history( &history, 1.0, 100.0, &route_stats );

        next_check( equal_within_tolerance( route_stats.rtt, expected_rtt * 1000.0f ) );
        next_check( equal_within_tolerance( route_stats.jitter, 0.0f ) );
        next_check( equal_within_tolerance( route_stats.packet_loss, 90.0f, 2.0f ) );
    }
}

void test_random_bytes()
{
    const int BufferSize = 999;
    uint8_t buffer[BufferSize];
    next_crypto_random_bytes( buffer, BufferSize );
    for ( int i = 0; i < 100; ++i )
    {
        uint8_t next_buffer[BufferSize];
        next_crypto_random_bytes( next_buffer, BufferSize );
        next_check( memcmp( buffer, next_buffer, BufferSize ) != 0 );
        memcpy( buffer, next_buffer, BufferSize );
    }
}

void test_random_float()
{
    for ( int i = 0; i < 1000; ++i )
    {
        float value = next_random_float();
        next_check( value >= 0.0f );
        next_check( value <= 1.0f );
    }
}

void test_crypto_box()
{
    #define CRYPTO_BOX_MESSAGE (const unsigned char *) "test"
    #define CRYPTO_BOX_MESSAGE_LEN 4
    #define CRYPTO_BOX_CIPHERTEXT_LEN ( NEXT_CRYPTO_BOX_MACBYTES + CRYPTO_BOX_MESSAGE_LEN )

    unsigned char sender_publickey[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];
    unsigned char sender_secretkey[NEXT_CRYPTO_BOX_SECRETKEYBYTES];
    next_crypto_box_keypair( sender_publickey, sender_secretkey );

    unsigned char receiver_publickey[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];
    unsigned char receiver_secretkey[NEXT_CRYPTO_BOX_SECRETKEYBYTES];
    next_crypto_box_keypair( receiver_publickey, receiver_secretkey );

    unsigned char nonce[NEXT_CRYPTO_BOX_NONCEBYTES];
    unsigned char ciphertext[CRYPTO_BOX_CIPHERTEXT_LEN];
    next_crypto_random_bytes( nonce, sizeof nonce );
    next_check( next_crypto_box_easy( ciphertext, CRYPTO_BOX_MESSAGE, CRYPTO_BOX_MESSAGE_LEN, nonce, receiver_publickey, sender_secretkey ) == 0 );

    unsigned char decrypted[CRYPTO_BOX_MESSAGE_LEN];
    next_check( next_crypto_box_open_easy( decrypted, ciphertext, CRYPTO_BOX_CIPHERTEXT_LEN, nonce, sender_publickey, receiver_secretkey ) == 0 );

    next_check( memcmp( decrypted, CRYPTO_BOX_MESSAGE, CRYPTO_BOX_MESSAGE_LEN ) == 0 );
}

void test_crypto_secret_box()
{
    #define CRYPTO_SECRET_BOX_MESSAGE ((const unsigned char *) "test")
    #define CRYPTO_SECRET_BOX_MESSAGE_LEN 4
    #define CRYPTO_SECRET_BOX_CIPHERTEXT_LEN (NEXT_CRYPTO_SECRETBOX_MACBYTES + CRYPTO_SECRET_BOX_MESSAGE_LEN)

    unsigned char key[NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    unsigned char nonce[NEXT_CRYPTO_SECRETBOX_NONCEBYTES];
    unsigned char ciphertext[CRYPTO_SECRET_BOX_CIPHERTEXT_LEN];

    next_crypto_secretbox_keygen( key );
    next_crypto_random_bytes( nonce, NEXT_CRYPTO_SECRETBOX_NONCEBYTES );
    next_crypto_secretbox_easy( ciphertext, CRYPTO_SECRET_BOX_MESSAGE, CRYPTO_SECRET_BOX_MESSAGE_LEN, nonce, key );

    unsigned char decrypted[CRYPTO_SECRET_BOX_MESSAGE_LEN];
    next_check( next_crypto_secretbox_open_easy( decrypted, ciphertext, CRYPTO_SECRET_BOX_CIPHERTEXT_LEN, nonce, key ) == 0 );
}

void test_crypto_aead()
{
    #define CRYPTO_AEAD_MESSAGE (const unsigned char *) "test"
    #define CRYPTO_AEAD_MESSAGE_LEN 4
    #define CRYPTO_AEAD_ADDITIONAL_DATA (const unsigned char *) "123456"
    #define CRYPTO_AEAD_ADDITIONAL_DATA_LEN 6

    unsigned char nonce[NEXT_CRYPTO_AEAD_CHACHA20POLY1305_NPUBBYTES];
    unsigned char key[NEXT_CRYPTO_AEAD_CHACHA20POLY1305_KEYBYTES];
    unsigned char ciphertext[CRYPTO_AEAD_MESSAGE_LEN + NEXT_CRYPTO_AEAD_CHACHA20POLY1305_ABYTES];
    unsigned long long ciphertext_len;

    next_crypto_aead_chacha20poly1305_keygen( key );
    next_crypto_random_bytes( nonce, sizeof(nonce) );

    next_crypto_aead_chacha20poly1305_encrypt( ciphertext, &ciphertext_len,
                                               CRYPTO_AEAD_MESSAGE, CRYPTO_AEAD_MESSAGE_LEN,
                                               CRYPTO_AEAD_ADDITIONAL_DATA, CRYPTO_AEAD_ADDITIONAL_DATA_LEN,
                                               NULL, nonce, key );

    unsigned char decrypted[CRYPTO_AEAD_MESSAGE_LEN];
    unsigned long long decrypted_len;
    next_check( next_crypto_aead_chacha20poly1305_decrypt( decrypted, &decrypted_len,
                                                      NULL,
                                                      ciphertext, ciphertext_len,
                                                      CRYPTO_AEAD_ADDITIONAL_DATA,
                                                      CRYPTO_AEAD_ADDITIONAL_DATA_LEN,
                                                      nonce, key) == 0 );
}

void test_crypto_aead_ietf()
{
    #define CRYPTO_AEAD_IETF_MESSAGE (const unsigned char *) "test"
    #define CRYPTO_AEAD_IETF_MESSAGE_LEN 4
    #define CRYPTO_AEAD_IETF_ADDITIONAL_DATA (const unsigned char *) "123456"
    #define CRYPTO_AEAD_IETF_ADDITIONAL_DATA_LEN 6

    unsigned char nonce[NEXT_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NPUBBYTES];
    unsigned char key[NEXT_CRYPTO_AEAD_CHACHA20POLY1305_IETF_KEYBYTES];
    unsigned char ciphertext[CRYPTO_AEAD_IETF_MESSAGE_LEN + NEXT_CRYPTO_AEAD_CHACHA20POLY1305_IETF_ABYTES];
    unsigned long long ciphertext_len;

    next_crypto_aead_chacha20poly1305_ietf_keygen( key );
    next_crypto_random_bytes( nonce, sizeof(nonce) );

    next_crypto_aead_chacha20poly1305_ietf_encrypt( ciphertext, &ciphertext_len, CRYPTO_AEAD_IETF_MESSAGE, CRYPTO_AEAD_IETF_MESSAGE_LEN, CRYPTO_AEAD_IETF_ADDITIONAL_DATA, CRYPTO_AEAD_IETF_ADDITIONAL_DATA_LEN, NULL, nonce, key);

    unsigned char decrypted[CRYPTO_AEAD_IETF_MESSAGE_LEN];
    unsigned long long decrypted_len;
    next_check( next_crypto_aead_chacha20poly1305_ietf_decrypt( decrypted, &decrypted_len, NULL, ciphertext, ciphertext_len, CRYPTO_AEAD_IETF_ADDITIONAL_DATA, CRYPTO_AEAD_IETF_ADDITIONAL_DATA_LEN, nonce, key ) == 0 );
}

void test_crypto_sign_detached()
{
    #define MESSAGE_PART1 ((const unsigned char *) "Arbitrary data to hash")
    #define MESSAGE_PART1_LEN 22

    #define MESSAGE_PART2 ((const unsigned char *) "is longer than expected")
    #define MESSAGE_PART2_LEN 23

    unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
    unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
    next_crypto_sign_keypair( public_key, private_key );

    next_crypto_sign_state_t state;

    unsigned char signature[NEXT_CRYPTO_SIGN_BYTES];

    next_crypto_sign_init( &state );
    next_crypto_sign_update( &state, MESSAGE_PART1, MESSAGE_PART1_LEN );
    next_crypto_sign_update( &state, MESSAGE_PART2, MESSAGE_PART2_LEN );
    next_crypto_sign_final_create( &state, signature, NULL, private_key );

    next_crypto_sign_init( &state );
    next_crypto_sign_update( &state, MESSAGE_PART1, MESSAGE_PART1_LEN );
    next_crypto_sign_update( &state, MESSAGE_PART2, MESSAGE_PART2_LEN );
    next_check( next_crypto_sign_final_verify( &state, signature, public_key ) == 0 );
}

void test_crypto_key_exchange()
{
    uint8_t client_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];
    uint8_t client_private_key[NEXT_CRYPTO_KX_SECRETKEYBYTES];
    next_crypto_kx_keypair( client_public_key, client_private_key );

    uint8_t server_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];
    uint8_t server_private_key[NEXT_CRYPTO_KX_SECRETKEYBYTES];
    next_crypto_kx_keypair( server_public_key, server_private_key );

    uint8_t client_send_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    uint8_t client_receive_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    next_check( next_crypto_kx_client_session_keys( client_receive_key, client_send_key, client_public_key, client_private_key, server_public_key ) == 0 );

    uint8_t server_send_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    uint8_t server_receive_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    next_check( next_crypto_kx_server_session_keys( server_receive_key, server_send_key, server_public_key, server_private_key, client_public_key ) == 0 );

    next_check( memcmp( client_send_key, server_receive_key, NEXT_CRYPTO_KX_SESSIONKEYBYTES ) == 0 );
    next_check( memcmp( server_send_key, client_receive_key, NEXT_CRYPTO_KX_SESSIONKEYBYTES ) == 0 );
}

void test_basic_read_and_write()
{
    uint8_t buffer[1024];

    uint8_t * p = buffer;
    next_write_uint8( &p, 105 );
    next_write_uint16( &p, 10512 );
    next_write_uint32( &p, 105120000 );
    next_write_uint64( &p, 105120000000000000LL );
    next_write_float32( &p, 100.0f );
    next_write_float64( &p, 100000000000000.0 );
    next_write_bytes( &p, (uint8_t*)"hello", 6 );

    const uint8_t * q = buffer;

    uint8_t a = next_read_uint8( &q );
    uint16_t b = next_read_uint16( &q );
    uint32_t c = next_read_uint32( &q );
    uint64_t d = next_read_uint64( &q );
    float e = next_read_float32( &q );
    double f = next_read_float64( &q );
    uint8_t g[6];
    next_read_bytes( &q, g, 6 );

    next_check( a == 105 );
    next_check( b == 10512 );
    next_check( c == 105120000 );
    next_check( d == 105120000000000000LL );
    next_check( e == 100.0f );
    next_check( f == 100000000000000.0 );
    next_check( memcmp( g, "hello", 6 ) == 0 );
}

void test_address_read_and_write()
{
    struct next_address_t a, b, c;

    memset( &a, 0, sizeof(a) );
    memset( &b, 0, sizeof(b) );
    memset( &c, 0, sizeof(c) );

    next_address_parse( &b, "127.0.0.1:50000" );

    next_address_parse( &c, "[::1]:50000" );

    uint8_t buffer[1024];

    uint8_t * p = buffer;

    next_write_address( &p, &a );
    next_write_address( &p, &b );
    next_write_address( &p, &c );

    struct next_address_t read_a, read_b, read_c;

    const uint8_t * q = buffer;

    next_read_address( &q, &read_a );
    next_read_address( &q, &read_b );
    next_read_address( &q, &read_c );

    next_check( next_address_equal( &a, &read_a ) );
    next_check( next_address_equal( &b, &read_b ) );
    next_check( next_address_equal( &c, &read_c ) );
}

void test_address_ipv4_read_and_write()
{
    struct next_address_t address;

    next_address_parse( &address, "127.0.0.1:50000" );

    uint8_t buffer[1024];

    uint8_t * p = buffer;

    next_write_address_ipv4( &p, &address );

    struct next_address_t read;

    const uint8_t * q = buffer;

    next_read_address_ipv4( &q, &read );

    next_check( next_address_equal( &address, &read ) );
}

void test_platform_socket()
{
    // non-blocking socket (ipv4)
    {
        next_address_t bind_address;
        next_address_t local_address;
        next_address_parse( &bind_address, "0.0.0.0" );
        next_address_parse( &local_address, "127.0.0.1" );
        next_platform_socket_t * socket = next_platform_socket_create( NULL, &bind_address, NEXT_PLATFORM_SOCKET_NON_BLOCKING, 0, 64*1024, 64*1024 );
        local_address.port = bind_address.port;
        next_check( socket );
        uint8_t packet[256];
        memset( packet, 0, sizeof(packet) );
        next_platform_socket_send_packet( socket, &local_address, packet, sizeof(packet) );
        next_address_t from;
        while ( next_platform_socket_receive_packet( socket, &from, packet, sizeof(packet) ) )
        {
            next_check( next_address_equal( &from, &local_address ) );
        }
        next_platform_socket_destroy( socket );
    }

    // blocking socket with timeout (ipv4)
    {
        next_address_t bind_address;
        next_address_t local_address;
        next_address_parse( &bind_address, "0.0.0.0" );
        next_address_parse( &local_address, "127.0.0.1" );
        next_platform_socket_t * socket = next_platform_socket_create( NULL, &bind_address, NEXT_PLATFORM_SOCKET_BLOCKING, 0.01f, 64*1024, 64*1024 );
        local_address.port = bind_address.port;
        next_check( socket );
        uint8_t packet[256];
        memset( packet, 0, sizeof(packet) );
        next_platform_socket_send_packet( socket, &local_address, packet, sizeof(packet) );
        next_address_t from;
        while ( next_platform_socket_receive_packet( socket, &from, packet, sizeof(packet) ) )
        {
            next_check( next_address_equal( &from, &local_address ) );
        }
        next_platform_socket_destroy( socket );
    }

    // blocking socket with no timeout (ipv4)
    {
        next_address_t bind_address;
        next_address_t local_address;
        next_address_parse( &bind_address, "0.0.0.0" );
        next_address_parse( &local_address, "127.0.0.1" );
        next_platform_socket_t * socket = next_platform_socket_create( NULL, &bind_address, NEXT_PLATFORM_SOCKET_BLOCKING, -1.0f, 64*1024, 64*1024 );
        local_address.port = bind_address.port;
        next_check( socket );
        uint8_t packet[256];
        memset( packet, 0, sizeof(packet) );
        next_platform_socket_send_packet( socket, &local_address, packet, sizeof(packet) );
        next_address_t from;
        next_platform_socket_receive_packet( socket, &from, packet, sizeof(packet) );
        next_check( next_address_equal( &from, &local_address ) );
        next_platform_socket_destroy( socket );
    }

#if NEXT_PLATFORM_HAS_IPV6

    // non-blocking socket (ipv6)
    {
        next_address_t bind_address;
        next_address_t local_address;
        next_address_parse( &bind_address, "[::]" );
        next_address_parse( &local_address, "[::1]" );
        next_platform_socket_t * socket = next_platform_socket_create( NULL, &bind_address, NEXT_PLATFORM_SOCKET_NON_BLOCKING, 0, 64*1024, 64*1024 );
        local_address.port = bind_address.port;
        next_check( socket );
        uint8_t packet[256];
        memset( packet, 0, sizeof(packet) );
        next_platform_socket_send_packet( socket, &local_address, packet, sizeof(packet) );
        next_address_t from;
        while ( next_platform_socket_receive_packet( socket, &from, packet, sizeof(packet) ) )
        {
            next_check( next_address_equal( &from, &local_address ) );
        }
        next_platform_socket_destroy( socket );
    }

    // blocking socket with timeout (ipv6)
    {
        next_address_t bind_address;
        next_address_t local_address;
        next_address_parse( &bind_address, "[::]" );
        next_address_parse( &local_address, "[::1]" );
        next_platform_socket_t * socket = next_platform_socket_create( NULL, &bind_address, NEXT_PLATFORM_SOCKET_BLOCKING, 0.01f, 64*1024, 64*1024 );
        local_address.port = bind_address.port;
        next_check( socket );
        uint8_t packet[256];
        memset( packet, 0, sizeof(packet) );
        next_platform_socket_send_packet( socket, &local_address, packet, sizeof(packet) );
        next_address_t from;
        while ( next_platform_socket_receive_packet( socket, &from, packet, sizeof(packet) ) )
        {
            next_check( next_address_equal( &from, &local_address ) );
        }
        next_platform_socket_destroy( socket );
    }

    // blocking socket with no timeout (ipv6)
    {
        next_address_t bind_address;
        next_address_t local_address;
        next_address_parse( &bind_address, "[::]" );
        next_address_parse( &local_address, "[::1]" );
        next_platform_socket_t * socket = next_platform_socket_create( NULL, &bind_address, NEXT_PLATFORM_SOCKET_BLOCKING, -1.0f, 64*1024, 64*1024 );
        local_address.port = bind_address.port;
        next_check( socket );
        uint8_t packet[256];
        memset( packet, 0, sizeof(packet) );
        next_platform_socket_send_packet( socket, &local_address, packet, sizeof(packet) );
        next_address_t from;
        next_platform_socket_receive_packet( socket, &from, packet, sizeof(packet) );
        next_check( next_address_equal( &from, &local_address ) );
        next_platform_socket_destroy( socket );
    }

#endif // #if NEXT_PLATFORM_HAS_IPV6
}

static bool threads_work = false;

static void test_thread_function(void*)
{
    threads_work = true;
}

void test_platform_thread()
{
    next_platform_thread_t * thread = next_platform_thread_create( NULL, test_thread_function, NULL );
    next_check( thread );
    next_platform_thread_join( thread );
    next_platform_thread_destroy( thread );
    next_check( threads_work );
}

void test_platform_mutex()
{
    next_platform_mutex_t mutex;
    int result = next_platform_mutex_create( &mutex );
    next_check( result == NEXT_OK );
    next_platform_mutex_acquire( &mutex );
    next_platform_mutex_release( &mutex );
    {
        next_platform_mutex_guard( &mutex );
        // ...
    }
    next_platform_mutex_destroy( &mutex );
}

static int num_client_packets_received = 0;

static void test_client_packet_received_callback( next_client_t * client, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    (void) client;
    (void) context;
    (void) from;
    (void) packet_data;
    (void) packet_bytes;
    num_client_packets_received++;
}

void test_client_ipv4()
{
    next_client_t * client = next_client_create( NULL, "0.0.0.0:0", test_client_packet_received_callback );
    next_check( client );
    next_check( next_client_port( client ) != 0 );
    next_client_open_session( client, "127.0.0.1:12345" );
    uint8_t packet[256];
    memset( packet, 0, sizeof(packet) );
    next_client_send_packet( client, packet, sizeof(packet) );
    next_client_update( client );
    next_client_close_session( client );
    next_client_destroy( client );
}

#if NEXT_PLATFORM_CAN_RUN_SERVER

static int num_server_packets_received = 0;

static void test_server_packet_received_callback(next_server_t* server, void* context, const next_address_t* from, const uint8_t* packet_data, int packet_bytes)
{
    (void)server; (void)context;
    next_server_send_packet(server, from, packet_data, packet_bytes);
    num_server_packets_received++;
}

void test_server_ipv4()
{
    next_server_t * server = next_server_create( NULL, "127.0.0.1:0", "0.0.0.0:0", "local", test_server_packet_received_callback );
    next_check( server );
    next_check( next_server_port( server ) != 0 );
    next_address_t address;
    next_address_parse( &address, "127.0.0.1" );
    address.port = next_server_port( server );;
    uint8_t packet[256];
    memset( packet, 0, sizeof(packet) );
    next_server_send_packet( server, &address, packet, sizeof(packet) );
    next_server_update( server );
    next_server_flush( server );
    next_server_destroy( server );
}

#endif // #if NEXT_PLATFORM_CAN_RUN_SERVER

void test_upgrade_token()
{
    NextUpgradeToken in, out;

    next_crypto_random_bytes( (uint8_t*) &in.session_id, 8 );
    next_crypto_random_bytes( (uint8_t*) &in.expire_timestamp, 8 );
    next_address_parse( &in.client_address, "127.0.0.1:40000" );
    next_address_parse( &in.server_address, "127.0.0.1:50000" );

    unsigned char private_key[NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    next_crypto_secretbox_keygen( private_key );

    uint8_t buffer[NEXT_UPGRADE_TOKEN_BYTES];

    in.Write( buffer, private_key );

    next_check( out.Read( buffer, private_key ) == true );

    next_check( memcmp( &in, &out, sizeof(NextUpgradeToken) ) == 0 );
}

void test_header()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint64_t send_sequence = i + 1000;
        uint64_t session_id = 0x12345LL;
        uint8_t session_version = uint8_t(i%256);
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        next_check( next_write_header( NEXT_CLIENT_TO_SERVER_PACKET, send_sequence, session_id, session_version, private_key, packet_data ) == NEXT_OK );

        uint64_t read_packet_sequence = 0;
        uint64_t read_packet_session_id = 0;
        uint8_t read_packet_session_version = 0;

        next_check( next_read_header( NEXT_CLIENT_TO_SERVER_PACKET, &read_packet_sequence, &read_packet_session_id, &read_packet_session_version, private_key, packet_data, NEXT_HEADER_BYTES ) == NEXT_OK );

        next_check( read_packet_sequence == send_sequence );
        next_check( read_packet_session_id == session_id );
        next_check( read_packet_session_version == session_version );
    }
}

void test_abi()
{
    uint8_t output[256];
    memset( output, 0, sizeof(output) );

    uint8_t magic[8];
    magic[0] = 1;
    magic[1] = 2;
    magic[2] = 3;
    magic[3] = 4;
    magic[4] = 5;
    magic[5] = 6;
    magic[6] = 7;
    magic[7] = 8;

    uint8_t from_address[4];
    from_address[0] = 1;
    from_address[1] = 2;
    from_address[2] = 3;
    from_address[3] = 4;

    uint8_t to_address[4];
    to_address[0] = 4;
    to_address[1] = 3;
    to_address[2] = 2;
    to_address[3] = 1;

    int packet_length = 1000;

    next_generate_pittle( output, from_address, to_address, packet_length );

    next_check( output[0] == 0x3f );
    next_check( output[1] == 0xb1 );

    next_generate_chonkle( output, magic, from_address, to_address, packet_length );

    next_check( output[0] == 0x2a );
    next_check( output[1] == 0xd0 );
    next_check( output[2] == 0x1e );
    next_check( output[3] == 0x4c );
    next_check( output[4] == 0x4e );
    next_check( output[5] == 0xdc );
    next_check( output[6] == 0x9f );
    next_check( output[7] == 0x07 );
}

void test_packet_filter()
{
    uint8_t output[NEXT_MAX_PACKET_BYTES];
    memset( output, 0, sizeof(output) );
    output[0] = 1;

    for ( int i = 0; i < 10000; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];

        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        int packet_length = 18 + ( i % ( sizeof(output) - 18 ) );
        
        next_generate_pittle( output + 1, from_address, to_address, packet_length );

        next_generate_chonkle( output + 3, magic, from_address, to_address, packet_length );

        next_check( next_basic_packet_filter( output, packet_length ) );

        next_check( next_advanced_packet_filter( output, magic, from_address, to_address, packet_length ) );
    }
}

void test_basic_packet_filter()
{
    uint8_t output[256];
    memset( output, 0, sizeof(output) );
    uint64_t pass = 0;
    uint64_t iterations = 100;
    srand( 100 );
    for ( int i = 0; i < int(iterations); ++i )
    {
        for ( int j = 0; j < int(sizeof(output)); ++j )
        {
            output[j] = uint8_t( rand() % 256 );
        }
        if ( next_basic_packet_filter( output, rand() % sizeof(output) ) )
        {
            pass++;
        }
    }
    next_check( pass == 0 );
}

void test_advanced_packet_filter()
{
    uint8_t output[256];
    memset( output, 0, sizeof(output) );
    uint64_t pass = 0;
    uint64_t iterations = 100;
    srand( 100 );
    for ( int i = 0; i < int(iterations); ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );
        int packet_length = 18 + ( i % ( sizeof(output) - 18 ) );
        for ( int j = 0; j < int(sizeof(output)); ++j )
        {
            output[j] = uint8_t( rand() % 256 );
        }
        if ( next_advanced_packet_filter( output, magic, from_address, to_address, packet_length ) )
        {
            pass++;
        }
    }
    next_check( pass == 0 );
}

void test_passthrough()
{
    uint8_t output[256];
    memset( output, 0, sizeof(output) );
    uint8_t magic[8];
    uint8_t from_address[4];
    uint8_t to_address[4];
    next_crypto_random_bytes( magic, 8 );
    next_crypto_random_bytes( from_address, 4 );
    next_crypto_random_bytes( to_address, 4 );
    int packet_length = sizeof(output);
    next_check( next_basic_packet_filter( output, packet_length ) );
    next_check( next_advanced_packet_filter( output, magic, from_address, to_address, packet_length ) );
}

void test_address_data_ipv4()
{
    next_address_t address;
    next_address_parse( &address, "127.0.0.1:50000" );
    next_check( address.type == NEXT_ADDRESS_IPV4 );
    uint8_t address_data[4];
    next_address_data( &address, address_data );
    next_check( address_data[0] == 127 );
    next_check( address_data[1] == 0 );
    next_check( address_data[2] == 0 );
    next_check( address_data[3] == 1 );
}

void test_anonymize_address_ipv4()
{
    next_address_t address;
    next_address_parse( &address, "1.2.3.4:5" );

    next_check( address.type == NEXT_ADDRESS_IPV4 );
    next_check( address.data.ipv4[0] == 1 );
    next_check( address.data.ipv4[1] == 2 );
    next_check( address.data.ipv4[2] == 3 );
    next_check( address.data.ipv4[3] == 4 );
    next_check( address.port == 5 );

    next_address_anonymize( &address );

    next_check( address.type == NEXT_ADDRESS_IPV4 );
    next_check( address.data.ipv4[0] == 1 );
    next_check( address.data.ipv4[1] == 2 );
    next_check( address.data.ipv4[2] == 3 );
    next_check( address.data.ipv4[3] == 0 );
    next_check( address.port == 0 );
}

#if NEXT_PLATFORM_HAS_IPV6

void test_anonymize_address_ipv6()
{
    next_address_t address;
    next_address_parse( &address, "[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:40000" );

    next_check( address.type == NEXT_ADDRESS_IPV6 );
    next_check( address.data.ipv6[0] == 0x2001 );
    next_check( address.data.ipv6[1] == 0x0db8 );
    next_check( address.data.ipv6[2] == 0x85a3 );
    next_check( address.data.ipv6[3] == 0x0000 );
    next_check( address.data.ipv6[4] == 0x0000 );
    next_check( address.data.ipv6[5] == 0x8a2e );
    next_check( address.data.ipv6[6] == 0x0370 );
    next_check( address.data.ipv6[7] == 0x7334 );
    next_check( address.port == 40000 );

    next_address_anonymize( &address );

    next_check( address.type == NEXT_ADDRESS_IPV6 );
    next_check( address.data.ipv6[0] == 0x2001 );
    next_check( address.data.ipv6[1] == 0x0db8 );
    next_check( address.data.ipv6[2] == 0x85a3 );
    next_check( address.data.ipv6[3] == 0x0000 );
    next_check( address.data.ipv6[4] == 0x0000 );
    next_check( address.data.ipv6[5] == 0x0000 );
    next_check( address.data.ipv6[6] == 0x0000 );
    next_check( address.data.ipv6[7] == 0x0000 );
    next_check( address.port == 0 );
}

#endif // #if NEXT_PLATFORM_HAS_IPV6

void test_bandwidth_limiter()
{
    next_bandwidth_limiter_t bandwidth_limiter;

    next_bandwidth_limiter_reset( &bandwidth_limiter );

    next_check( next_bandwidth_limiter_usage_kbps( &bandwidth_limiter ) == 0.0 );

    // come in way under
    {
        const int kbps_allowed = 1000;
        const int packet_bits = 50;

        for ( int i = 0; i < 10; ++i )
        {
            next_check( !next_bandwidth_limiter_add_packet( &bandwidth_limiter, i * ( NEXT_BANDWIDTH_LIMITER_INTERVAL / 10.0 ), kbps_allowed, packet_bits ) );
        }
    }

    // get really close
    {
        next_bandwidth_limiter_reset( &bandwidth_limiter );

        const int kbps_allowed = 1000;
        const int packet_bits = kbps_allowed / 10 * 1000;

        for ( int i = 0; i < 10; ++i )
        {
            next_check( !next_bandwidth_limiter_add_packet( &bandwidth_limiter, i * ( NEXT_BANDWIDTH_LIMITER_INTERVAL / 10.0 ), kbps_allowed, packet_bits ) );
        }
    }

    // really close for several intervals
    {
        next_bandwidth_limiter_reset( &bandwidth_limiter );

        const int kbps_allowed = 1000;
        const int packet_bits = kbps_allowed / 10 * 1000;

        for ( int i = 0; i < 30; ++i )
        {
            next_check( !next_bandwidth_limiter_add_packet( &bandwidth_limiter, i * ( NEXT_BANDWIDTH_LIMITER_INTERVAL / 10.0 ), kbps_allowed, packet_bits ) );
        }
    }

    // go over budget
    {
        next_bandwidth_limiter_reset( &bandwidth_limiter );

        const int kbps_allowed = 1000;
        const int packet_bits = kbps_allowed / 10 * 1000 * 1.01f;

        bool over_budget = false;

        for ( int i = 0; i < 30; ++i )
        {
            over_budget |= next_bandwidth_limiter_add_packet( &bandwidth_limiter, i * ( NEXT_BANDWIDTH_LIMITER_INTERVAL / 10.0 ), kbps_allowed, packet_bits );
        }

        next_check( over_budget );
    }
}

void test_packet_loss_tracker()
{
    next_packet_loss_tracker_t tracker;
    next_packet_loss_tracker_reset( &tracker );

    next_check( next_packet_loss_tracker_update( &tracker ) == 0 );

    uint64_t sequence = 0;

    for ( int i = 0; i < NEXT_PACKET_LOSS_TRACKER_SAFETY; ++i )
    {
        next_packet_loss_tracker_packet_received( &tracker, sequence );
        sequence++;
    }

    next_check( next_packet_loss_tracker_update( &tracker ) == 0 );

    for ( int i = 0; i < 200; ++i )
    {
        next_packet_loss_tracker_packet_received( &tracker, sequence );
        sequence++;
    }

    next_check( next_packet_loss_tracker_update( &tracker ) == 0 );

    for ( int i = 0; i < 200; ++i )
    {
        if ( sequence & 1 )
        {
            next_packet_loss_tracker_packet_received( &tracker, sequence );
        }
        sequence++;
    }

    next_check( next_packet_loss_tracker_update( &tracker ) == ( 200 - NEXT_PACKET_LOSS_TRACKER_SAFETY ) / 2 );

    next_check( next_packet_loss_tracker_update( &tracker ) == 0 );

    next_packet_loss_tracker_reset( &tracker );

    sequence = 0;

    next_packet_loss_tracker_packet_received( &tracker, 200 + NEXT_PACKET_LOSS_TRACKER_SAFETY - 1 );

    next_check( next_packet_loss_tracker_update( &tracker ) == 200 );

    next_packet_loss_tracker_packet_received( &tracker, 1000 );

    next_check( next_packet_loss_tracker_update( &tracker ) > 500 );

    next_packet_loss_tracker_packet_received( &tracker, 0xFFFFFFFFFFFFFFFULL );

    next_check( next_packet_loss_tracker_update( &tracker ) == 0 );
}

void test_out_of_order_tracker()
{
    next_out_of_order_tracker_t tracker;
    next_out_of_order_tracker_reset( &tracker );

    next_check( tracker.num_out_of_order_packets == 0 );

    uint64_t sequence = 0;

    for ( int i = 0; i < 1000; ++i )
    {
        next_out_of_order_tracker_packet_received( &tracker, sequence );
        sequence++;
    }

    next_check( tracker.num_out_of_order_packets == 0 );

    sequence = 500;

    for ( int i = 0; i < 500; ++i )
    {
        next_out_of_order_tracker_packet_received( &tracker, sequence );
        sequence++;
    }

    next_check( tracker.num_out_of_order_packets == 499 );

    next_out_of_order_tracker_reset( &tracker );

    next_check( tracker.last_packet_processed == 0 );
    next_check( tracker.num_out_of_order_packets == 0 );

    for ( int i = 0; i < 1000; ++i )
    {
        uint64_t mod_sequence = ( sequence / 2 ) * 2;
        if ( sequence % 2 )
            mod_sequence -= 1;
        next_out_of_order_tracker_packet_received( &tracker, mod_sequence );
        sequence++;
    }

    next_check( tracker.num_out_of_order_packets == 500 );
}

void test_jitter_tracker()
{
    next_jitter_tracker_t tracker;
    next_jitter_tracker_reset( &tracker );

    next_check( tracker.jitter == 0.0 );

    uint64_t sequence = 0;

    double t = 0.0;
    double dt = 1.0 / 60.0;

    for ( int i = 0; i < 1000; ++i )
    {
        next_jitter_tracker_packet_received( &tracker, sequence, t );
        sequence++;
        t += dt;
    }

    next_check( tracker.jitter < 0.000001 );

    for ( int i = 0; i < 1000; ++i )
    {
        t = i * dt;
        if ( (i%3) == 0 )
        {
            t += 2;
        }
        if ( (i%5) == 0 )
        {
            t += 5;
        }
        if ( (i%6) == 0 )
        {
            t -= 10;
        }
        next_jitter_tracker_packet_received( &tracker, sequence, t );
        sequence++;
    }

    next_check( tracker.jitter > 1.0 );

    next_jitter_tracker_reset( &tracker );

    next_check( tracker.jitter == 0.0 );

    for ( int i = 0; i < 1000; ++i )
    {
        t = i * dt;
        if ( (i%3) == 0 )
        {
            t += 0.01f;
        }
        if ( (i%5) == 0 )
        {
            t += 0.05;
        }
        if ( (i%6) == 0 )
        {
            t -= 0.1f;
        }
        next_jitter_tracker_packet_received( &tracker, sequence, t );
        sequence++;
    }

    next_check( tracker.jitter > 0.05 );
    next_check( tracker.jitter < 0.1 );

    for ( int i = 0; i < 10000; ++i )
    {
        t = i * dt;
        next_jitter_tracker_packet_received( &tracker, sequence, t );
        sequence++;
    }

    next_check( tracker.jitter >= 0.0 );
    next_check( tracker.jitter <= 0.000001 );
}

extern void * next_default_malloc_function( void * context, size_t bytes );

extern void next_default_free_function( void * context, void * p );

static void context_check_free( void * context, void * p )
{
    (void) p;
    next_check( context );
    next_check( *((int *)context) == 23 );
    next_default_free_function( context, p );;
}

void test_free_retains_context()
{
    void * (*current_malloc)( void * context, size_t bytes ) = next_default_malloc_function;
    void (*current_free)( void * context, void * p ) = next_default_free_function;

    next_allocator( next_default_malloc_function, context_check_free );

    int canary = 23;
    void * context = (void *)&canary;
    next_queue_t *q = next_queue_create( context, 1 );
    next_queue_destroy( q );

    next_check( context );
    next_check( *((int *)context) == 23 );
    next_check( canary == 23 );

    next_allocator( current_malloc, current_free );
}

void test_pending_session_manager()
{
    const int InitialSize = 32;

    next_pending_session_manager_t * pending_session_manager = next_pending_session_manager_create( NULL, InitialSize );

    next_check( pending_session_manager );

    next_address_t address;
    next_address_parse( &address, "127.0.0.1:12345" );

    double time = 10.0;

    // test private keys

    uint8_t private_keys[InitialSize*3*NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    next_crypto_random_bytes( private_keys, sizeof(private_keys) );

    // test upgrade tokens

    uint8_t upgrade_tokens[InitialSize*3*NEXT_UPGRADE_TOKEN_BYTES];
    next_crypto_random_bytes( upgrade_tokens, sizeof(upgrade_tokens) );

    // add enough entries to make sure we have to expand

    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_pending_session_entry_t * entry = next_pending_session_manager_add( pending_session_manager, &address, uint64_t(i)+1000, &private_keys[i*NEXT_CRYPTO_SECRETBOX_KEYBYTES], &upgrade_tokens[i*NEXT_UPGRADE_TOKEN_BYTES], time );
        next_check( entry );
        next_check( entry->session_id == uint64_t(i) + 1000 );
        next_check( entry->upgrade_time == time );
        next_check( entry->last_packet_send_time < 0.0 );
        next_check( next_address_equal( &address, &entry->address ) == 1 );
        next_check( memcmp( entry->private_key, &private_keys[i*NEXT_CRYPTO_SECRETBOX_KEYBYTES], NEXT_CRYPTO_SECRETBOX_KEYBYTES ) == 0 );
        next_check( memcmp( entry->upgrade_token, &upgrade_tokens[i*NEXT_UPGRADE_TOKEN_BYTES], NEXT_UPGRADE_TOKEN_BYTES ) == 0 );
        address.port++;
    }

    // verify that all entries are there

    address.port = 12345;
    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_pending_session_entry_t * entry = next_pending_session_manager_find( pending_session_manager, &address );
        next_check( entry );
        next_check( entry->session_id == uint64_t(i) + 1000 );
        next_check( entry->upgrade_time == time );
        next_check( entry->last_packet_send_time < 0.0 );
        next_check( next_address_equal( &address, &entry->address ) == 1 );
        address.port++;
    }

    next_check( next_pending_session_manager_num_entries( pending_session_manager ) == InitialSize*3 );

    // remove every second entry

    for ( int i = 0; i < InitialSize*3; ++i )
    {
        if ( (i%2) == 0 )
        {
            next_pending_session_manager_remove_by_address( pending_session_manager, &pending_session_manager->addresses[i] );
        }
    }

    // verify only the entries that remain can be found

    address.port = 12345;
    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_pending_session_entry_t * entry = next_pending_session_manager_find( pending_session_manager, &address );
        if ( (i%2) != 0 )
        {
            next_check( entry );
            next_check( entry->session_id == uint64_t(i) + 1000 );
            next_check( entry->upgrade_time == time );
            next_check( entry->last_packet_send_time < 0.0 );
            next_check( next_address_equal( &address, &entry->address ) == 1 );
        }
        else
        {
            next_check( entry == NULL );
        }
        address.port++;
    }

    // expand, and verify that all entries get collapsed

    next_pending_session_manager_expand( pending_session_manager );

    address.port = 12346;
    for ( int i = 0; i < pending_session_manager->size; ++i )
    {
        if ( pending_session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            next_check( next_address_equal( &address, &pending_session_manager->addresses[i] ) == 1 );
            next_pending_session_entry_t * entry = &pending_session_manager->entries[i];
            next_check( entry->session_id == uint64_t(i)*2+1001 );
            next_check( entry->upgrade_time == time );
            next_check( entry->last_packet_send_time < 0.0 );
            next_check( next_address_equal( &address, &entry->address ) == 1 );
        }
        address.port += 2;
    }

    // remove all remaining entries manually

    for ( int i = 0; i < pending_session_manager->size; ++i )
    {
        if ( pending_session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            next_pending_session_manager_remove_by_address( pending_session_manager, &pending_session_manager->addresses[i] );
        }
    }

    next_check( pending_session_manager->max_entry_index == 0 );

    next_check( next_pending_session_manager_num_entries( pending_session_manager ) == 0 );

    next_pending_session_manager_destroy( pending_session_manager );
}

void test_proxy_session_manager()
{
    const int InitialSize = 32;

    next_proxy_session_manager_t * proxy_session_manager = next_proxy_session_manager_create( NULL, InitialSize );

    next_check( proxy_session_manager );

    next_address_t address;
    next_address_parse( &address, "127.0.0.1:12345" );

    // test private keys

    uint8_t private_keys[InitialSize*3*NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    next_crypto_random_bytes( private_keys, sizeof(private_keys) );

    // test upgrade tokens

    uint8_t upgrade_tokens[InitialSize*3*NEXT_UPGRADE_TOKEN_BYTES];
    next_crypto_random_bytes( upgrade_tokens, sizeof(upgrade_tokens) );

    // add enough entries to make sure we have to expand

    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_proxy_session_entry_t * entry = next_proxy_session_manager_add( proxy_session_manager, &address, uint64_t(i)+1000 );
        next_check( entry );
        next_check( entry->session_id == uint64_t(i) + 1000 );
        next_check( next_address_equal( &address, &entry->address ) == 1 );
        address.port++;
    }

    // verify that all entries are there

    address.port = 12345;
    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_proxy_session_entry_t * entry = next_proxy_session_manager_find( proxy_session_manager, &address );
        next_check( entry );
        next_check( entry->session_id == uint64_t(i) + 1000 );
        next_check( next_address_equal( &address, &entry->address ) == 1 );
        address.port++;
    }

    next_check( next_proxy_session_manager_num_entries( proxy_session_manager ) == InitialSize*3 );

    // remove every second entry

    for ( int i = 0; i < InitialSize*3; ++i )
    {
        if ( (i%2) == 0 )
        {
            next_proxy_session_manager_remove_by_address( proxy_session_manager, &proxy_session_manager->addresses[i] );
        }
    }

    // verify only the entries that remain can be found

    address.port = 12345;
    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_proxy_session_entry_t * entry = next_proxy_session_manager_find( proxy_session_manager, &address );
        if ( (i%2) != 0 )
        {
            next_check( entry );
            next_check( entry->session_id == uint64_t(i) + 1000 );
            next_check( next_address_equal( &address, &entry->address ) == 1 );
        }
        else
        {
            next_check( entry == NULL );
        }
        address.port++;
    }

    // expand, and verify that all entries get collapsed

    next_proxy_session_manager_expand( proxy_session_manager );

    address.port = 12346;
    for ( int i = 0; i < proxy_session_manager->size; ++i )
    {
        if ( proxy_session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            next_check( next_address_equal( &address, &proxy_session_manager->addresses[i] ) == 1 );
            next_proxy_session_entry_t * entry = &proxy_session_manager->entries[i];
            next_check( entry->session_id == uint64_t(i)*2+1001 );
            next_check( next_address_equal( &address, &entry->address ) == 1 );
        }
        address.port += 2;
    }

    // remove all remaining entries manually

    for ( int i = 0; i < proxy_session_manager->size; ++i )
    {
        if ( proxy_session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            next_proxy_session_manager_remove_by_address( proxy_session_manager, &proxy_session_manager->addresses[i] );
        }
    }

    next_check( proxy_session_manager->max_entry_index == 0 );

    next_check( next_proxy_session_manager_num_entries( proxy_session_manager ) == 0 );

    next_proxy_session_manager_destroy( proxy_session_manager );
}

void test_session_manager()
{
    const int InitialSize = 1;

    next_session_manager_t * session_manager = next_session_manager_create( NULL, InitialSize );

    next_check( session_manager );

    next_address_t address;
    next_address_parse( &address, "127.0.0.1:12345" );

    // test private keys

    uint8_t private_keys[InitialSize*3*NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    next_crypto_random_bytes( private_keys, sizeof(private_keys) );

    // test upgrade tokens

    uint8_t upgrade_tokens[InitialSize*3*NEXT_UPGRADE_TOKEN_BYTES];
    next_crypto_random_bytes( upgrade_tokens, sizeof(upgrade_tokens) );

    // add enough entries to make sure we have to expand

    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_session_entry_t * entry = next_session_manager_add( session_manager, &address, uint64_t(i)+1000, &private_keys[i*NEXT_CRYPTO_SECRETBOX_KEYBYTES], &upgrade_tokens[i*NEXT_UPGRADE_TOKEN_BYTES] );
        next_check( entry );
        next_check( entry->session_id == uint64_t(i) + 1000 );
        next_check( next_address_equal( &address, &entry->address ) == 1 );
        next_check( memcmp( entry->ephemeral_private_key, &private_keys[i*NEXT_CRYPTO_SECRETBOX_KEYBYTES], NEXT_CRYPTO_SECRETBOX_KEYBYTES ) == 0 );
        next_check( memcmp( entry->upgrade_token, &upgrade_tokens[i*NEXT_UPGRADE_TOKEN_BYTES], NEXT_UPGRADE_TOKEN_BYTES ) == 0 );
        address.port++;
    }

    // verify that all entries are there

    address.port = 12345;
    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_session_entry_t * entry = next_session_manager_find_by_address( session_manager, &address );
        next_check( entry );
        next_check( entry->session_id == uint64_t(i)+1000 );
        next_check( next_address_equal( &address, &entry->address ) == 1 );
        address.port++;
    }

    next_check( next_session_manager_num_entries( session_manager ) == InitialSize*3 );

    // remove every second entry

    for ( int i = 0; i < InitialSize*3; ++i )
    {
        if ( (i%2) == 0 )
        {
            next_session_manager_remove_by_address( session_manager, &session_manager->addresses[i] );
        }
    }

    // verify only the entries that remain can be found

    address.port = 12345;
    for ( int i = 0; i < InitialSize*3; ++i )
    {
        next_session_entry_t * entry = next_session_manager_find_by_address( session_manager, &address );
        if ( (i%2) != 0 )
        {
            next_check( entry );
            next_check( entry->session_id == uint64_t(i)+1000 );
            next_check( next_address_equal( &address, &entry->address ) == 1 );
        }
        else
        {
            next_check( entry == NULL );
        }
        address.port++;
    }

    // expand, and verify that all entries get collapsed

    next_session_manager_expand( session_manager );

    address.port = 12346;
    for ( int i = 0; i < session_manager->size; ++i )
    {
        if ( session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            next_check( next_address_equal( &address, &session_manager->addresses[i] ) == 1 );
            next_session_entry_t * entry = &session_manager->entries[i];
            next_check( entry->session_id == uint64_t(i)*2+1001 );
            next_check( next_address_equal( &address, &entry->address ) == 1 );
        }
        address.port += 2;
    }

    // remove all remaining entries manually

    for ( int i = 0; i < session_manager->size; ++i )
    {
        if ( session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            next_session_manager_remove_by_address( session_manager, &session_manager->addresses[i] );
        }
    }

    next_check( session_manager->max_entry_index == 0 );

    next_check( next_session_manager_num_entries( session_manager ) == 0 );

    next_session_manager_destroy( session_manager );
}

void test_relay_manager()
{
    uint64_t relay_ids[NEXT_MAX_CLIENT_RELAYS];
    next_address_t relay_addresses[NEXT_MAX_CLIENT_RELAYS];
    uint8_t relay_ping_tokens[NEXT_MAX_CLIENT_RELAYS * NEXT_PING_TOKEN_BYTES];
    uint64_t relay_ping_expire_timestamp = 0x129387193871987LL;

    for ( int i = 0; i < NEXT_MAX_CLIENT_RELAYS; ++i )
    {
        relay_ids[i] = i;
        char address_string[256];
        snprintf( address_string, sizeof(address_string), "127.0.0.1:%d", 40000 + i );
        next_address_parse( &relay_addresses[i], address_string );
    }

    next_relay_manager_t * manager = next_relay_manager_create( NULL, 10 );

    // should be no relays when manager is first created
    {
        next_relay_stats_t stats;
        next_relay_manager_get_stats( manager, &stats );
        next_check( stats.num_relays == 0 );
    }

    // add max relays

    next_relay_manager_update( manager, NEXT_MAX_CLIENT_RELAYS, relay_ids, relay_addresses, relay_ping_tokens, relay_ping_expire_timestamp );
    {
        next_relay_stats_t stats;
        next_relay_manager_get_stats( manager, &stats );
        next_check( stats.num_relays == NEXT_MAX_CLIENT_RELAYS );
        for ( int i = 0; i < NEXT_MAX_CLIENT_RELAYS; ++i )
        {
            next_check( relay_ids[i] == stats.relay_ids[i] );
            next_check( stats.relay_rtt[i] == 0 );
            next_check( stats.relay_jitter[i] == 0 );
            next_check( stats.relay_packet_loss[i] == 100.0 );
        }
    }

    // remove all relays

    next_relay_manager_update( manager, 0, relay_ids, relay_addresses, NULL, 0 );
    {
        next_relay_stats_t stats;
        next_relay_manager_get_stats( manager, &stats );
        next_check( stats.num_relays == 0 );
    }

    // add same relay set repeatedly

    for ( int j = 0; j < 2; ++j )
    {
        next_relay_manager_update( manager, NEXT_MAX_CLIENT_RELAYS, relay_ids, relay_addresses, relay_ping_tokens, relay_ping_expire_timestamp );
        {
            next_relay_stats_t stats;
            next_relay_manager_get_stats( manager, &stats );
            next_check( stats.num_relays == NEXT_MAX_CLIENT_RELAYS );
            for ( int i = 0; i < NEXT_MAX_CLIENT_RELAYS; ++i )
            {
                next_check( relay_ids[i] == stats.relay_ids[i] );
            }
        }
    }

    // now add a few new relays, while some relays remain the same

    next_relay_manager_update( manager, NEXT_MAX_CLIENT_RELAYS, relay_ids + 4, relay_addresses + 4, relay_ping_tokens, relay_ping_expire_timestamp );
    {
        next_relay_stats_t stats;
        next_relay_manager_get_stats( manager, &stats );
        next_check( stats.num_relays == NEXT_MAX_CLIENT_RELAYS );
        for ( int i = 0; i < NEXT_MAX_CLIENT_RELAYS - 4; ++i )
        {
            next_check( relay_ids[i+4] == stats.relay_ids[i] );
        }
    }

    // remove all relays

    next_relay_manager_update( manager, 0, relay_ids, relay_addresses, NULL, 0 );
    {
        next_relay_stats_t stats;
        next_relay_manager_get_stats( manager, &stats );
        next_check( stats.num_relays == 0 );
    }

    next_relay_manager_destroy( manager );
}

void test_direct_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint8_t open_session_sequence = uint8_t( i + 10 );
        uint64_t send_sequence = i;

        uint8_t game_packet_data[NEXT_MTU];
        int game_packet_bytes = rand() % NEXT_MTU;
        for ( int j = 0; j < game_packet_bytes; j++ ) 
        { 
            game_packet_data[j] = uint8_t( rand() % 256 ); 
        }

        int packet_bytes = next_write_direct_packet( packet_data, open_session_sequence, send_sequence, game_packet_data, game_packet_bytes, magic, from_address, to_address );

        next_check( packet_bytes >= 0 );
        next_check( packet_bytes <= NEXT_MTU + 27 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_DIRECT_PACKET );
        next_check( memcmp( packet_data + 18 + 1 + 8, game_packet_data, game_packet_bytes ) == 0 );
    }
}

// ---------------------------------------------------------------

extern next_internal_config_t next_global_config;

extern int next_signed_packets[256];

extern int next_encrypted_packets[256];

// ---------------------------------------------------------------

void test_direct_ping_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint64_t in_sequence = i;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );

        NextDirectPingPacket in;
        in.ping_sequence = i + 1000;
        int packet_bytes = 0;
        int result = next_write_packet( NEXT_DIRECT_PING_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address );

        next_check( result == NEXT_OK );
        next_check( packet_bytes == 18 + 8 + 8 + 16 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        NextDirectPingPacket out;
        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        int packet_type = next_read_packet( NEXT_DIRECT_PING_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection );

        next_check( packet_type == NEXT_DIRECT_PING_PACKET );

        next_check( in.ping_sequence == out.ping_sequence );

        next_check( in_sequence == out_sequence + 1 );
    }
}

void test_direct_pong_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint64_t in_sequence = i;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );

        NextDirectPongPacket in;
        in.ping_sequence = i + 1000;
        int packet_bytes = 0;
        int result = next_write_packet( NEXT_DIRECT_PONG_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address );

        next_check( result == NEXT_OK );
        next_check( packet_bytes == 18 + 8 + 8 + 16 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        NextDirectPongPacket out;
        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        int packet_type = next_read_packet( NEXT_DIRECT_PONG_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection );

        next_check( packet_type == NEXT_DIRECT_PONG_PACKET );

        next_check( in.ping_sequence == out.ping_sequence );

        next_check( in_sequence == out_sequence + 1 );
    }
}

void test_upgrade_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

    uint64_t iterations = 100;
    
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        static NextUpgradeRequestPacket in, out;
        in.protocol_version = next_protocol_version();
        in.session_id = 1231234127431LL;
        next_address_parse( &in.client_address, "127.0.0.1:50000" );
        next_address_parse( &in.server_address, "127.0.0.1:12345" );
        next_crypto_random_bytes( in.server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        next_crypto_random_bytes( in.upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
        next_crypto_random_bytes( in.upcoming_magic, 8 );
        next_crypto_random_bytes( in.current_magic, 8 );
        next_crypto_random_bytes( in.previous_magic, 8 );

        int packet_bytes = 0;
        int result = next_write_packet( NEXT_UPGRADE_REQUEST_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, NULL, NULL, private_key, NULL, magic, from_address, to_address );

        next_check( result == NEXT_OK );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        int packet_type = next_read_packet( NEXT_UPGRADE_REQUEST_PACKET, packet_data, begin, end, &out, next_signed_packets, NULL, NULL, public_key, NULL, NULL );

        next_check( packet_type == NEXT_UPGRADE_REQUEST_PACKET );

        next_check( in.protocol_version == out.protocol_version );
        next_check( in.session_id == out.session_id );
        next_check( next_address_equal( &in.client_address, &out.client_address ) );
        next_check( next_address_equal( &in.server_address, &out.server_address ) );
        next_check( memcmp( in.server_kx_public_key, out.server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES ) == 0 );
        next_check( memcmp( in.upgrade_token, out.upgrade_token, NEXT_UPGRADE_TOKEN_BYTES ) == 0 );
        next_check( memcmp( in.upcoming_magic, out.upcoming_magic, 8 ) == 0 );
        next_check( memcmp( in.current_magic, out.current_magic, 8 ) == 0 );
        next_check( memcmp( in.previous_magic, out.previous_magic, 8 ) == 0 );
    }
}

void test_upgrade_response_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextUpgradeResponsePacket in, out;
        next_crypto_random_bytes( in.client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        next_crypto_random_bytes( in.client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
        next_crypto_random_bytes( in.upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
        in.platform_id = NEXT_PLATFORM_WINDOWS;
        in.connection_type = NEXT_CONNECTION_TYPE_CELLULAR;

        int packet_bytes = 0;
        int result = next_write_packet( NEXT_UPGRADE_RESPONSE_PACKET, &in, packet_data, &packet_bytes, NULL, NULL, NULL, NULL, NULL, magic, from_address, to_address );

        next_check( result == NEXT_OK );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        int packet_type = next_read_packet( NEXT_UPGRADE_RESPONSE_PACKET, packet_data, begin, end, &out, NULL, NULL, NULL, NULL, NULL, NULL );

        next_check( packet_type == NEXT_UPGRADE_RESPONSE_PACKET );

        next_check( memcmp( in.client_kx_public_key, out.client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES ) == 0 );
        next_check( memcmp( in.client_route_public_key, out.client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES ) == 0 );
        next_check( memcmp( in.upgrade_token, out.upgrade_token, NEXT_UPGRADE_TOKEN_BYTES ) == 0 );
        next_check( in.platform_id == out.platform_id );
        next_check( in.connection_type == out.connection_type );
    }
}

void test_upgrade_confirm_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        static NextUpgradeConfirmPacket in, out;
        in.upgrade_sequence = 1000;
        in.session_id = 1231234127431LL;
        next_address_parse( &in.server_address, "127.0.0.1:12345" );
        next_crypto_random_bytes( in.client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        next_crypto_random_bytes( in.server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );

        int packet_bytes = 0;
        int result = next_write_packet( NEXT_UPGRADE_CONFIRM_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, NULL, NULL, private_key, NULL, magic, from_address, to_address );

        next_check( result == NEXT_OK );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        int packet_type = next_read_packet( NEXT_UPGRADE_CONFIRM_PACKET, packet_data, begin, end, &out, next_signed_packets, NULL, NULL, public_key, NULL, NULL );

        next_check( packet_type == NEXT_UPGRADE_CONFIRM_PACKET );

        next_check( in.upgrade_sequence == out.upgrade_sequence );
        next_check( in.session_id == out.session_id );
        next_check( next_address_equal( &in.server_address, &out.server_address ) );
        next_check( memcmp( in.client_kx_public_key, out.client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES ) == 0 );
        next_check( memcmp( in.server_kx_public_key, out.server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES ) == 0 );
    }
}

void test_route_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint8_t token_data[1024];
        int token_bytes = rand() % sizeof(token_data);
        for ( int j = 0; j < token_bytes; j++ ) { token_data[j] = uint8_t( rand() % 256 ); }

        int packet_bytes = next_write_route_request_packet( packet_data, token_data, token_bytes, magic, from_address, to_address );

        next_check( packet_bytes > 0 );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_ROUTE_REQUEST_PACKET );
        next_check( memcmp( packet_data + 18, token_data, token_bytes ) == 0 );
    }
}

void test_route_response_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t send_sequence = i + 1000;
        uint64_t session_id = 0x12314141LL;
        uint8_t session_version = uint8_t(i%256);
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        int packet_bytes = next_write_route_response_packet( packet_data, send_sequence, session_id, session_version, private_key, magic, from_address, to_address );

        next_check( packet_bytes > 0 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_ROUTE_RESPONSE_PACKET );

        uint64_t read_packet_sequence = 0;
        uint64_t read_packet_session_id = 0;
        uint8_t read_packet_session_version = 0;

        uint8_t * read_packet_data = packet_data + 18;
        int read_packet_bytes = packet_bytes - 18;

        next_check( next_read_header( NEXT_ROUTE_RESPONSE_PACKET, &read_packet_sequence, &read_packet_session_id, &read_packet_session_version, private_key, read_packet_data, read_packet_bytes ) == NEXT_OK );

        next_check( read_packet_sequence == send_sequence );
        next_check( read_packet_session_id == session_id );
        next_check( read_packet_session_version == session_version );
    }
}

void test_client_to_server_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t send_sequence = i + 1000;
        uint64_t session_id = 0x12314141LL;
        uint8_t session_version = uint8_t(i%256);
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t game_packet_data[NEXT_MTU];
        int game_packet_bytes = rand() % NEXT_MTU;
        for ( int j = 0; j < game_packet_bytes; j++ ) { game_packet_data[j] = uint8_t( rand() % 256 ); }

        int packet_bytes = next_write_client_to_server_packet( packet_data, send_sequence, session_id, session_version, private_key, game_packet_data, game_packet_bytes, magic, from_address, to_address );
        next_check( packet_bytes > 0 );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_CLIENT_TO_SERVER_PACKET );

        next_check( memcmp( packet_data + 18 + NEXT_HEADER_BYTES, game_packet_data, game_packet_bytes ) == 0 );

        uint64_t read_packet_sequence = 0;
        uint64_t read_packet_session_id = 0;
        uint8_t read_packet_session_version = 0;

        uint8_t * read_packet_data = packet_data + 18;
        int read_packet_bytes = packet_bytes - 18;

        next_check( next_read_header( NEXT_CLIENT_TO_SERVER_PACKET, &read_packet_sequence, &read_packet_session_id, &read_packet_session_version, private_key, read_packet_data, read_packet_bytes ) == NEXT_OK );

        next_check( read_packet_sequence == send_sequence );
        next_check( read_packet_session_id == session_id );
        next_check( read_packet_session_version == session_version );
    }
}

void test_server_to_client_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t send_sequence = i + 1000;
        uint64_t session_id = 0x12314141LL;
        uint8_t session_version = uint8_t(i%256);
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t game_packet_data[NEXT_MTU];
        int game_packet_bytes = rand() % NEXT_MTU;
        for ( int j = 0; j < game_packet_bytes; j++ ) { game_packet_data[j] = uint8_t( rand() % 256 ); }

        int packet_bytes = next_write_server_to_client_packet( packet_data, send_sequence, session_id, session_version, private_key, game_packet_data, game_packet_bytes, magic, from_address, to_address );

        next_check( packet_bytes > 0 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_SERVER_TO_CLIENT_PACKET );

        next_check( memcmp( packet_data + 18 + NEXT_HEADER_BYTES, game_packet_data, game_packet_bytes ) == 0 );

        uint64_t read_packet_sequence = 0;
        uint64_t read_packet_session_id = 0;
        uint8_t read_packet_session_version = 0;

        uint8_t * read_packet_data = packet_data + 18;
        int read_packet_bytes = packet_bytes - 18;

        next_check( next_read_header( NEXT_SERVER_TO_CLIENT_PACKET, &read_packet_sequence, &read_packet_session_id, &read_packet_session_version, private_key, read_packet_data, read_packet_bytes ) == NEXT_OK );

        next_check( read_packet_sequence == send_sequence );
        next_check( read_packet_session_id == session_id );
        next_check( read_packet_session_version == session_version );
    }
}

void test_session_ping_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t send_sequence = i + 1000;
        uint64_t session_id = 0x12314141LL;
        uint8_t session_version = uint8_t(i%256);
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint64_t ping_sequence = i;
        int packet_bytes = next_write_session_ping_packet( packet_data, send_sequence, session_id, session_version, private_key, ping_sequence, magic, from_address, to_address );

        next_check( packet_bytes > 0 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_SESSION_PING_PACKET );

        uint64_t read_packet_sequence = 0;
        uint64_t read_packet_session_id = 0;
        uint8_t read_packet_session_version = 0;

        uint8_t * read_packet_data = packet_data + 18;
        int read_packet_bytes = packet_bytes - 18;

        next_check( next_read_header( NEXT_SESSION_PING_PACKET, &read_packet_sequence, &read_packet_session_id, &read_packet_session_version, private_key, read_packet_data, read_packet_bytes ) == NEXT_OK );

        next_check( read_packet_sequence == send_sequence );
        next_check( read_packet_session_id == session_id );
        next_check( read_packet_session_version == session_version );
    }
}

void test_session_pong_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t send_sequence = i + 1000;
        uint64_t session_id = 0x12314141LL;
        uint8_t session_version = uint8_t(i%256);
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];

        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint64_t ping_sequence = i;
        int packet_bytes = next_write_session_pong_packet( packet_data, send_sequence, session_id, session_version, private_key, ping_sequence, magic, from_address, to_address );

        next_check( packet_bytes > 0 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_SESSION_PONG_PACKET );

        uint64_t read_packet_sequence = 0;
        uint64_t read_packet_session_id = 0;
        uint8_t read_packet_session_version = 0;

        uint8_t * read_packet_data = packet_data + 18;
        int read_packet_bytes = packet_bytes - 18;

        next_check( next_read_header( NEXT_SESSION_PONG_PACKET, &read_packet_sequence, &read_packet_session_id, &read_packet_session_version, private_key, read_packet_data, read_packet_bytes ) == NEXT_OK );

        next_check( read_packet_sequence == send_sequence );
        next_check( read_packet_session_id == session_id );
        next_check( read_packet_session_version == session_version );
    }
}

void test_continue_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );
        uint8_t token_data[256];
        int token_bytes = rand() % sizeof(token_data);
        for ( int j = 0; j < token_bytes; j++ ) { token_data[j] = uint8_t( rand() % 256 ); }
        int packet_bytes = next_write_continue_request_packet( packet_data, token_data, token_bytes, magic, from_address, to_address );
        next_check( packet_bytes >= 0 );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );
        next_check( packet_data[0] == NEXT_CONTINUE_REQUEST_PACKET );
        next_check( memcmp( packet_data + 18, token_data, token_bytes ) == 0 );
    }
}

void test_continue_response_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t send_sequence = i + 1000;
        uint64_t session_id = 0x12314141LL;
        uint8_t session_version = uint8_t(i%256);
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        int packet_bytes = next_write_continue_response_packet( packet_data, send_sequence, session_id, session_version, private_key, magic, from_address, to_address );

        next_check( packet_bytes > 0 );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_CONTINUE_RESPONSE_PACKET );

        uint64_t read_packet_sequence = 0;
        uint64_t read_packet_session_id = 0;
        uint8_t read_packet_session_version = 0;

        uint8_t * read_packet_data = packet_data + 18;
        int read_packet_bytes = packet_bytes - 18;

        next_check( next_read_header( NEXT_CONTINUE_RESPONSE_PACKET, &read_packet_sequence, &read_packet_session_id, &read_packet_session_version, private_key, read_packet_data, read_packet_bytes ) == NEXT_OK );

        next_check( read_packet_sequence == send_sequence );
        next_check( read_packet_session_id == session_id );
        next_check( read_packet_session_version == session_version );
    }
}

void test_client_stats_packet_with_client_relays()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextClientStatsPacket in, out;
        in.reported = true;
        in.fallback_to_direct = true;
        in.platform_id = NEXT_PLATFORM_WINDOWS;
        in.connection_type = NEXT_CONNECTION_TYPE_CELLULAR;
        in.direct_rtt = 50.0f;
        in.direct_jitter = 10.0f;
        in.direct_packet_loss = 0.1f;
        in.direct_max_packet_loss_seen = 0.25f;
        in.next = true;
        in.next_rtt = 50.0f;
        in.next_jitter = 5.0f;
        in.next_packet_loss = 0.01f;
        in.num_client_relays = NEXT_MAX_CLIENT_RELAYS;
        for ( int j = 0; j < NEXT_MAX_CLIENT_RELAYS; ++j )
        {
            in.client_relay_ids[j] = uint64_t(10000000) + j;
            in.client_relay_rtt[j] = 5 * j;
            in.client_relay_jitter[j] = 0.01f * j;
            in.client_relay_packet_loss[j] = j;
        }
        in.packets_lost_server_to_client = 1000;
        in.client_relay_request_id = 0x12345124761ULL;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );
        uint64_t in_sequence = 1000;

        int packet_bytes = 0;
        next_check( next_write_packet( NEXT_CLIENT_STATS_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_CLIENT_STATS_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_CLIENT_STATS_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_CLIENT_STATS_PACKET );

        next_check( in_sequence == out_sequence + 1 );
        next_check( in.reported == out.reported );
        next_check( in.fallback_to_direct == out.fallback_to_direct );
        next_check( in.platform_id == out.platform_id );
        next_check( in.connection_type == out.connection_type );
        next_check( in.direct_rtt == out.direct_rtt );
        next_check( in.direct_jitter == out.direct_jitter );
        next_check( in.direct_packet_loss == out.direct_packet_loss );
        next_check( in.direct_max_packet_loss_seen == out.direct_max_packet_loss_seen );
        next_check( in.next == out.next );
        next_check( in.next_rtt == out.next_rtt );
        next_check( in.next_jitter == out.next_jitter );
        next_check( in.next_packet_loss == out.next_packet_loss );
        next_check( in.num_client_relays == out.num_client_relays );
        for ( int j = 0; j < NEXT_MAX_CLIENT_RELAYS; ++j )
        {
            next_check( in.client_relay_ids[j] == out.client_relay_ids[j] );
            next_check( in.client_relay_rtt[j] == out.client_relay_rtt[j] );
            next_check( in.client_relay_jitter[j] == out.client_relay_jitter[j] );
            next_check( in.client_relay_packet_loss[j] == out.client_relay_packet_loss[j] );
        }
        next_check( in.packets_sent_client_to_server == out.packets_sent_client_to_server );
        next_check( in.packets_lost_server_to_client == out.packets_lost_server_to_client );
        next_check( in.client_relay_request_id == out.client_relay_request_id );
    }
}

void test_client_stats_packet_without_client_relays()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextClientStatsPacket in, out;
        in.reported = true;
        in.fallback_to_direct = true;
        in.platform_id = NEXT_PLATFORM_WINDOWS;
        in.connection_type = NEXT_CONNECTION_TYPE_CELLULAR;
        in.direct_rtt = 50.0f;
        in.direct_jitter = 10.0f;
        in.direct_packet_loss = 0.1f;
        in.direct_max_packet_loss_seen = 0.25f;
        in.next = true;
        in.next_rtt = 50.0f;
        in.next_jitter = 5.0f;
        in.next_packet_loss = 0.01f;
        in.num_client_relays = 0;
        in.packets_lost_server_to_client = 1000;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );
        uint64_t in_sequence = 1000;

        int packet_bytes = 0;
        next_check( next_write_packet( NEXT_CLIENT_STATS_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_CLIENT_STATS_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_CLIENT_STATS_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_CLIENT_STATS_PACKET );

        next_check( in_sequence == out_sequence + 1 );
        next_check( in.reported == out.reported );
        next_check( in.fallback_to_direct == out.fallback_to_direct );
        next_check( in.platform_id == out.platform_id );
        next_check( in.connection_type == out.connection_type );
        next_check( in.direct_rtt == out.direct_rtt );
        next_check( in.direct_jitter == out.direct_jitter );
        next_check( in.direct_packet_loss == out.direct_packet_loss );
        next_check( in.direct_max_packet_loss_seen == out.direct_max_packet_loss_seen );
        next_check( in.next == out.next );
        next_check( in.next_rtt == out.next_rtt );
        next_check( in.next_jitter == out.next_jitter );
        next_check( in.next_packet_loss == out.next_packet_loss );
        next_check( in.num_client_relays == out.num_client_relays );
        next_check( in.packets_sent_client_to_server == out.packets_sent_client_to_server );
        next_check( in.packets_lost_server_to_client == out.packets_lost_server_to_client );
    }
}

void test_route_update_packet_direct()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextRouteUpdatePacket in, out;

        in.sequence = 100000;

        in.update_type = NEXT_UPDATE_TYPE_DIRECT;
        in.packets_sent_server_to_client = 11000;
        in.packets_lost_client_to_server = 10000;
        in.packets_out_of_order_client_to_server = 9000;
        next_crypto_random_bytes( in.upcoming_magic, 8 );
        next_crypto_random_bytes( in.current_magic, 8 );
        next_crypto_random_bytes( in.previous_magic, 8 );
        in.jitter_client_to_server = 0.1f;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );
        uint64_t in_sequence = 1000;

        int packet_bytes = 0;
        next_check( next_write_packet( NEXT_ROUTE_UPDATE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_ROUTE_UPDATE_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_ROUTE_UPDATE_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_ROUTE_UPDATE_PACKET );

        next_check( in_sequence == out_sequence + 1 );
        next_check( in.sequence == out.sequence );

        next_check( in.update_type == out.update_type );
        next_check( in.packets_sent_server_to_client == out.packets_sent_server_to_client );
        next_check( in.packets_lost_client_to_server == out.packets_lost_client_to_server );
        next_check( in.packets_out_of_order_client_to_server == out.packets_out_of_order_client_to_server );
        next_check( memcmp( in.upcoming_magic, out.upcoming_magic, 8 ) == 0 );
        next_check( memcmp( in.current_magic, out.current_magic, 8 ) == 0 );
        next_check( memcmp( in.previous_magic, out.previous_magic, 8 ) == 0 );
        next_check( in.jitter_client_to_server == out.jitter_client_to_server );
    }
}

void test_route_update_packet_new_route()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextRouteUpdatePacket in, out;
        in.sequence = 100000;

        in.update_type = NEXT_UPDATE_TYPE_ROUTE;
        in.multipath = true;
        in.num_tokens = NEXT_MAX_TOKENS;
        next_crypto_random_bytes( in.tokens, NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES * NEXT_MAX_TOKENS );
        in.packets_sent_server_to_client = 11000;
        in.packets_lost_client_to_server = 10000;
        in.packets_out_of_order_client_to_server = 9000;
        next_crypto_random_bytes( in.upcoming_magic, 8 );
        next_crypto_random_bytes( in.current_magic, 8 );
        next_crypto_random_bytes( in.previous_magic, 8 );
        in.jitter_client_to_server = 0.25f;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );

        int packet_bytes = 0;
        uint64_t in_sequence = 1000;
        next_check( next_write_packet( NEXT_ROUTE_UPDATE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_ROUTE_UPDATE_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_ROUTE_UPDATE_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_ROUTE_UPDATE_PACKET );

        next_check( in_sequence == out_sequence + 1 );
        next_check( in.sequence == out.sequence );

        next_check( in.update_type == out.update_type );
        next_check( in.multipath == out.multipath );
        next_check( in.num_tokens == out.num_tokens );
        next_check( memcmp( in.tokens, out.tokens, NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES * NEXT_MAX_TOKENS ) == 0 );
        next_check( in.packets_sent_server_to_client == out.packets_sent_server_to_client );
        next_check( in.packets_lost_client_to_server == out.packets_lost_client_to_server );
        next_check( in.packets_out_of_order_client_to_server == out.packets_out_of_order_client_to_server );
        next_check( memcmp( in.upcoming_magic, out.upcoming_magic, 8 ) == 0 );
        next_check( memcmp( in.current_magic, out.current_magic, 8 ) == 0 );
        next_check( memcmp( in.previous_magic, out.previous_magic, 8 ) == 0 );
        next_check( in.jitter_client_to_server == out.jitter_client_to_server );
    }
}

void test_route_update_packet_continue_route()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );
        
        static NextRouteUpdatePacket in, out;
        in.sequence = 100000;

        in.update_type = NEXT_UPDATE_TYPE_CONTINUE;
        in.multipath = true;
        in.num_tokens = NEXT_MAX_TOKENS;
        next_crypto_random_bytes( in.tokens, NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES * NEXT_MAX_TOKENS );
        in.packets_lost_client_to_server = 10000;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );

        int packet_bytes = 0;
        uint64_t in_sequence = 1000;
        next_check( next_write_packet( NEXT_ROUTE_UPDATE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_ROUTE_UPDATE_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_ROUTE_UPDATE_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_ROUTE_UPDATE_PACKET );

        next_check( in_sequence == out_sequence + 1 );
        next_check( in.sequence == out.sequence );

        next_check( in.update_type == out.update_type );
        next_check( in.multipath == out.multipath );
        next_check( in.num_tokens == out.num_tokens );
        next_check( memcmp( in.tokens, out.tokens, NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES * NEXT_MAX_TOKENS ) == 0 );
        next_check( in.packets_lost_client_to_server == out.packets_lost_client_to_server );
        next_check( memcmp( in.upcoming_magic, out.upcoming_magic, 8 ) == 0 );
        next_check( memcmp( in.current_magic, out.current_magic, 8 ) == 0 );
        next_check( memcmp( in.previous_magic, out.previous_magic, 8 ) == 0 );
    }
}

void test_route_ack_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextRouteAckPacket in, out;
        in.sequence = 100000;

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );

        int packet_bytes = 0;
        uint64_t in_sequence = 1000;
        next_check( next_write_packet( NEXT_ROUTE_ACK_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_ROUTE_ACK_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_ROUTE_ACK_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_ROUTE_ACK_PACKET );

        next_check( in_sequence == out_sequence + 1 );
        next_check( in.sequence == out.sequence );
    }
}

void test_client_relay_update_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t j = 0; j < iterations; ++j )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );
        
        static NextClientRelayUpdatePacket in, out;

        in.request_id = next_random_uint64();
        in.num_client_relays = rand() % ( NEXT_MAX_CLIENT_RELAYS + 1 );
        for ( int i = 0; i < in.num_client_relays; i++ )
        {
            in.client_relay_ids[i] = next_random_uint64();
            next_address_parse( &in.client_relay_addresses[i], "127.0.0.1:50000" );
            next_crypto_random_bytes( in.client_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES );
        }
        in.expire_timestamp = next_random_uint64();

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );

        int packet_bytes = 0;
        uint64_t in_sequence = 1000;
        next_check( next_write_packet( NEXT_CLIENT_RELAY_UPDATE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_CLIENT_RELAY_UPDATE_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_CLIENT_RELAY_UPDATE_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_CLIENT_RELAY_UPDATE_PACKET );

        next_check( in_sequence == out_sequence + 1 );

        next_check( in.request_id == out.request_id );
        next_check( in.num_client_relays == out.num_client_relays );
        for ( int i = 0; i < in.num_client_relays; i++ )
        {
            next_check( in.client_relay_ids[i] == out.client_relay_ids[i] );
            next_check( next_address_equal( &in.client_relay_addresses[i], &out.client_relay_addresses[i] ) );
            next_check( memcmp( in.client_relay_ping_tokens[i], out.client_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES ) == 0 );
        }
        next_check( in.expire_timestamp == out.expire_timestamp );
    }
}

void test_client_relay_ack_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t private_key[NEXT_SESSION_PRIVATE_KEY_BYTES];
        next_crypto_random_bytes( private_key, sizeof(private_key) );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );
        
        static NextClientRelayAckPacket in, out;

        in.request_id = next_random_uint64();

        static next_replay_protection_t replay_protection;
        next_replay_protection_reset( &replay_protection );

        int packet_bytes = 0;
        uint64_t in_sequence = 1000;
        next_check( next_write_packet( NEXT_CLIENT_RELAY_ACK_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, next_encrypted_packets, &in_sequence, NULL, private_key, magic, from_address, to_address ) == NEXT_OK );

        next_check( packet_data[0] == NEXT_CLIENT_RELAY_ACK_PACKET );
        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        uint64_t out_sequence = 0;
        const int begin = 18;
        const int end = packet_bytes;
        next_check( next_read_packet( NEXT_CLIENT_RELAY_ACK_PACKET, packet_data, begin, end, &out, next_signed_packets, next_encrypted_packets, &out_sequence, NULL, private_key, &replay_protection ) == NEXT_CLIENT_RELAY_ACK_PACKET );

        next_check( in_sequence == out_sequence + 1 );

        next_check( in.request_id == out.request_id );
    }
}

void test_client_ping_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint8_t ping_token[NEXT_PING_TOKEN_BYTES];
        next_crypto_random_bytes( ping_token, NEXT_PING_TOKEN_BYTES );

        uint64_t ping_sequence = i;
        uint64_t ping_session_id = 0x12345;
        uint64_t ping_expire_timestamp = 0x123415817414;

        int packet_bytes = next_write_client_ping_packet( packet_data, ping_token, ping_sequence, ping_session_id, ping_expire_timestamp, magic, from_address, to_address );

        next_check( packet_bytes >= 0 );
        next_check( packet_bytes <= NEXT_MAX_PACKET_BYTES );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_CLIENT_PING_PACKET );

        const uint8_t * p = packet_data + 18;
        uint64_t read_ping_sequence = next_read_uint64( &p );
        uint64_t read_ping_session_id = next_read_uint64( &p );
        uint64_t read_ping_expire_timestamp = next_read_uint64( &p );

        next_check( read_ping_sequence == ping_sequence );
        next_check( read_ping_session_id == ping_session_id );
        next_check( read_ping_expire_timestamp == ping_expire_timestamp );

        next_check( memcmp( packet_data + 18 + 8 + 8 + 8, ping_token, NEXT_PING_TOKEN_BYTES ) == 0 );
    }
}

void test_client_pong_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t pong_sequence = i;
        uint64_t pong_session_id = 0x123456;

        int packet_bytes = next_write_client_pong_packet( packet_data, pong_sequence, pong_session_id, magic, from_address, to_address );

        next_check( packet_bytes >= 0 );
        next_check( packet_bytes <= NEXT_MAX_PACKET_BYTES );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_CLIENT_PONG_PACKET );

        const uint8_t * p = packet_data + 18;
        uint64_t read_pong_sequence = next_read_uint64( &p );
        uint64_t read_pong_session_id = next_read_uint64( &p );

        next_check( read_pong_sequence == pong_sequence );
        next_check( read_pong_session_id == pong_session_id );
    }
}

void test_server_ping_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint8_t ping_token[NEXT_PING_TOKEN_BYTES];
        next_crypto_random_bytes( ping_token, NEXT_PING_TOKEN_BYTES );

        uint64_t ping_sequence = i;
        uint64_t ping_expire_timestamp = 0x123415817414;

        int packet_bytes = next_write_server_ping_packet( packet_data, ping_token, ping_sequence, ping_expire_timestamp, magic, from_address, to_address );

        next_check( packet_bytes >= 0 );
        next_check( packet_bytes <= NEXT_MAX_PACKET_BYTES );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_SERVER_PING_PACKET );

        const uint8_t * p = packet_data + 18;
        uint64_t read_ping_sequence = next_read_uint64( &p );
        uint64_t read_ping_expire_timestamp = next_read_uint64( &p );

        next_check( read_ping_sequence == ping_sequence );
        next_check( read_ping_expire_timestamp == ping_expire_timestamp );

        next_check( memcmp( packet_data + 18 + 8 + 8, ping_token, NEXT_PING_TOKEN_BYTES ) == 0 );
    }
}

void test_server_pong_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        uint64_t pong_sequence = i;

        int packet_bytes = next_write_server_pong_packet( packet_data, pong_sequence, magic, from_address, to_address );

        next_check( packet_bytes >= 0 );
        next_check( packet_bytes <= NEXT_MAX_PACKET_BYTES );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        next_check( packet_data[0] == NEXT_SERVER_PONG_PACKET );

        const uint8_t * p = packet_data + 18;
        uint64_t read_pong_sequence = next_read_uint64( &p );

        next_check( read_pong_sequence == pong_sequence );
    }
}

void test_server_init_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendServerInitRequestPacket in, out;
        in.request_id = next_random_uint64();
        in.buyer_id = 1231234127431LL;
        in.datacenter_id = next_datacenter_id( "local" );
        strcpy( in.datacenter_name, "local" );

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET );

        next_check( in.request_id == out.request_id );
        next_check( in.version_major == out.version_major );
        next_check( in.version_minor == out.version_minor );
        next_check( in.version_patch == out.version_patch );
        next_check( in.buyer_id == out.buyer_id );
        next_check( in.datacenter_id == out.datacenter_id );
        next_check( strcmp( in.datacenter_name, out.datacenter_name ) == 0 );
    }
}

void test_server_init_response_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendServerInitResponsePacket in, out;
        in.request_id = next_random_uint64();
        in.response = NEXT_SERVER_INIT_RESPONSE_OK;
        next_crypto_random_bytes( in.upcoming_magic, 8 );
        next_crypto_random_bytes( in.current_magic, 8 );
        next_crypto_random_bytes( in.previous_magic, 8 );

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET );

        next_check( in.request_id == out.request_id );
        next_check( in.response == out.response );
        next_check( memcmp( in.upcoming_magic, out.upcoming_magic, 8 ) == 0 );
        next_check( memcmp( in.current_magic, out.current_magic, 8 ) == 0 );
        next_check( memcmp( in.previous_magic, out.previous_magic, 8 ) == 0 );
    }
}

void test_server_update_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendServerUpdateRequestPacket in, out;
        in.request_id = next_random_uint64();
        in.buyer_id = next_random_uint64();
        in.datacenter_id = next_random_uint64();
        in.num_sessions = 1000;
        next_address_parse( &in.server_address, "127.0.0.1:40000" );
        in.uptime = 0x12345;

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET );

        next_check( in.version_major == out.version_major );
        next_check( in.version_minor == out.version_minor );
        next_check( in.version_patch == out.version_patch );
        next_check( in.request_id == out.request_id );
        next_check( in.buyer_id == out.buyer_id );
        next_check( in.datacenter_id == out.datacenter_id );
        next_check( in.num_sessions == out.num_sessions );
        next_check( next_address_equal( &in.server_address, &out.server_address ) );
        next_check( in.uptime == out.uptime );
    }
}

void test_server_update_response_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendServerUpdateResponsePacket in, out;
        in.request_id = next_random_uint64();
        next_crypto_random_bytes( in.upcoming_magic, 8 );
        next_crypto_random_bytes( in.current_magic, 8 );
        next_crypto_random_bytes( in.previous_magic, 8 );

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET );

        next_check( in.request_id == out.request_id );
        next_check( memcmp( in.upcoming_magic, out.upcoming_magic, 8 ) == 0 );
        next_check( memcmp( in.current_magic, out.current_magic, 8 ) == 0 );
        next_check( memcmp( in.previous_magic, out.previous_magic, 8 ) == 0 );
    }
}

void test_session_update_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendSessionUpdateRequestPacket in, out;
        in.slice_number = 0;
        in.buyer_id = 1231234127431LL;
        in.datacenter_id = 111222454443LL;
        in.session_id = 1234342431431LL;
        in.user_hash = 11111111;
        in.platform_id = 3;
        in.session_events = next_random_uint64();
        in.internal_events = next_random_uint64();
        in.reported = true;
        in.connection_type = NEXT_CONNECTION_TYPE_WIRED;
        in.direct_rtt = 10.1f;
        in.direct_jitter = 5.2f;
        in.direct_packet_loss = 0.1f;
        in.direct_max_packet_loss_seen = 0.25f;
        in.next = true;
        in.has_client_relay_pings = true;
        in.has_server_relay_pings = true;
        in.client_relay_pings_have_changed = true;
        in.server_relay_pings_have_changed = true;
        in.next_rtt = 5.0f;
        in.next_jitter = 1.5f;
        in.next_packet_loss = 0.0f;
        in.num_client_relays = NEXT_MAX_CLIENT_RELAYS;
        for ( int j = 0; j < NEXT_MAX_CLIENT_RELAYS; ++j )
        {
            in.client_relay_ids[j] = j;
            in.client_relay_rtt[j] = j + 10.0f;
            in.client_relay_jitter[j] = j + 11.0f;
            in.client_relay_packet_loss[j] = j + 12.0f;
        }
        in.num_server_relays = NEXT_MAX_SERVER_RELAYS;
        for ( int j = 0; j < NEXT_MAX_SERVER_RELAYS; ++j )
        {
            in.server_relay_ids[j] = j;
            in.server_relay_rtt[j] = j + 10.0f;
            in.server_relay_jitter[j] = j + 11.0f;
            in.server_relay_packet_loss[j] = j + 12.0f;
        }
        next_address_parse( &in.client_address, "127.0.0.1:40000" );
        next_address_parse( &in.server_address, "127.0.0.1:12345" );
        next_crypto_random_bytes( in.client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
        next_crypto_random_bytes( in.server_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
        in.direct_kbps_up = 50.0f;
        in.direct_kbps_down = 75.0f;
        in.next_kbps_up = 100.0f;
        in.next_kbps_down = 200.0f;
        in.packets_lost_client_to_server = 100;
        in.packets_lost_server_to_client = 200;
        in.session_data_bytes = NEXT_MAX_SESSION_DATA_BYTES;
        for ( int j = 0; j < NEXT_MAX_SESSION_DATA_BYTES; ++j )
        {
            in.session_data[j] = uint8_t(j);
        }
        for ( int j = 0; j < NEXT_CRYPTO_SIGN_BYTES; ++j )
        {
            in.session_data_signature[j] = uint8_t(j);
        }

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET );

        next_check( in.slice_number == out.slice_number );
        next_check( in.buyer_id == out.buyer_id );
        next_check( in.datacenter_id == out.datacenter_id );
        next_check( in.session_id == out.session_id );
        next_check( in.user_hash == out.user_hash );
        next_check( in.platform_id == out.platform_id );
        next_check( in.session_events == out.session_events );
        next_check( in.internal_events == out.internal_events );
        next_check( in.reported == out.reported );
        next_check( in.connection_type == out.connection_type );
        next_check( in.direct_rtt == out.direct_rtt );
        next_check( in.direct_jitter == out.direct_jitter );
        next_check( in.direct_packet_loss == out.direct_packet_loss );
        next_check( in.direct_max_packet_loss_seen == out.direct_max_packet_loss_seen );
        next_check( in.next == out.next );
        next_check( in.next_rtt == out.next_rtt );
        next_check( in.next_jitter == out.next_jitter );
        next_check( in.next_packet_loss == out.next_packet_loss );
        next_check( in.has_client_relay_pings == out.has_client_relay_pings );
        next_check( in.has_server_relay_pings == out.has_server_relay_pings );
        next_check( in.client_relay_pings_have_changed == out.client_relay_pings_have_changed );
        next_check( in.server_relay_pings_have_changed == out.server_relay_pings_have_changed );
        next_check( in.num_client_relays == out.num_client_relays );
        for ( int j = 0; j < NEXT_MAX_CLIENT_RELAYS; ++j )
        {
            next_check( in.client_relay_ids[j] == out.client_relay_ids[j] );
            next_check( in.client_relay_rtt[j] == out.client_relay_rtt[j] );
            next_check( in.client_relay_jitter[j] == out.client_relay_jitter[j] );
            next_check( in.client_relay_packet_loss[j] == out.client_relay_packet_loss[j] );
        }
        next_check( in.num_server_relays == out.num_server_relays );
        for ( int j = 0; j < NEXT_MAX_SERVER_RELAYS; ++j )
        {
            next_check( in.server_relay_ids[j] == out.server_relay_ids[j] );
            next_check( in.server_relay_rtt[j] == out.server_relay_rtt[j] );
            next_check( in.server_relay_jitter[j] == out.server_relay_jitter[j] );
            next_check( in.server_relay_packet_loss[j] == out.server_relay_packet_loss[j] );
        }
        next_check( next_address_equal( &in.client_address, &out.client_address ) );
        next_check( next_address_equal( &in.server_address, &out.server_address ) );
        next_check( memcmp( in.client_route_public_key, out.client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES ) == 0 );
        next_check( memcmp( in.server_route_public_key, out.server_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES ) == 0 );
        next_check( in.direct_kbps_up == out.direct_kbps_up );
        next_check( in.direct_kbps_down == out.direct_kbps_down );
        next_check( in.next_kbps_up == out.next_kbps_up );
        next_check( in.next_kbps_down == out.next_kbps_down );
        next_check( in.packets_lost_client_to_server == out.packets_lost_client_to_server );
        next_check( in.packets_lost_server_to_client == out.packets_lost_server_to_client );
        next_check( in.session_data_bytes == out.session_data_bytes );
        for ( int j = 0; j < NEXT_MAX_SESSION_DATA_BYTES; ++j )
        {
            next_check( in.session_data[j] == out.session_data[j] );
        }
        for ( int j = 0; j < NEXT_CRYPTO_SIGN_BYTES; ++j )
        {
            next_check( in.session_data_signature[j] == out.session_data_signature[j] );
        }
    }
}

void test_session_update_response_packet_direct()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendSessionUpdateResponsePacket in, out;
        in.slice_number = 10000;
        in.session_id = 1234342431431LL;

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET );

        next_check( in.slice_number == out.slice_number );
        next_check( in.session_id == out.session_id );

        next_check( in.response_type == out.response_type );
        next_check( in.multipath == out.multipath );
    }
}

void test_session_update_response_packet_route()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendSessionUpdateResponsePacket in, out;
        in.slice_number = 10000;
        in.session_id = 1234342431431LL;

        in.response_type = NEXT_UPDATE_TYPE_ROUTE;
        in.multipath = true;
        in.num_tokens = NEXT_MAX_TOKENS;
        next_crypto_random_bytes( in.tokens, NEXT_MAX_TOKENS * NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES );
        in.session_data_bytes = NEXT_MAX_SESSION_DATA_BYTES;
        for ( int j = 0; j < NEXT_MAX_SESSION_DATA_BYTES; ++j )
        {
            in.session_data[j] = uint8_t(j);
        }

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET );

        next_check( in.slice_number == out.slice_number );
        next_check( in.session_id == out.session_id );
        next_check( in.multipath == out.multipath );

        next_check( in.response_type == out.response_type );
        next_check( in.num_tokens == out.num_tokens );
        next_check( memcmp( in.tokens, out.tokens, NEXT_MAX_TOKENS * NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES ) == 0 );
    }
}

void test_session_update_response_packet_continue()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendSessionUpdateResponsePacket in, out;
        in.slice_number = 10000;
        in.session_id = 1234342431431LL;

        in.response_type = NEXT_UPDATE_TYPE_CONTINUE;
        in.multipath = true;
        in.num_tokens = NEXT_MAX_TOKENS;
        next_crypto_random_bytes( in.tokens, NEXT_MAX_TOKENS * NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES );
        in.session_data_bytes = NEXT_MAX_SESSION_DATA_BYTES;
        for ( int j = 0; j < NEXT_MAX_SESSION_DATA_BYTES; ++j )
        {
            in.session_data[j] = uint8_t(j);
        }
        for ( int j = 0; j < NEXT_CRYPTO_SIGN_BYTES; ++j )
        {
            in.session_data_signature[j] = uint8_t(j);
        }

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET );

        next_check( in.slice_number == out.slice_number );
        next_check( in.session_id == out.session_id );
        next_check( in.multipath == out.multipath );

        next_check( in.response_type == out.response_type );
        next_check( in.num_tokens == out.num_tokens );
        next_check( memcmp( in.tokens, out.tokens, NEXT_MAX_TOKENS * NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES ) == 0 );
        next_check( in.session_data_bytes == out.session_data_bytes );
        for ( int j = 0; j < NEXT_MAX_SESSION_DATA_BYTES; ++j )
        {
            next_check( out.session_data[j] == uint8_t(j) );
        }
        for ( int j = 0; j < NEXT_CRYPTO_SIGN_BYTES; ++j )
        {
            next_check( out.session_data_signature[j] == uint8_t(j) );
        }
    }
}

void test_client_relay_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendClientRelayRequestPacket in, out;
        in.buyer_id = next_random_uint64();
        in.datacenter_id = next_random_uint64();
        in.request_id = next_random_uint64();
        next_address_parse( &in.client_address, "127.0.0.1:40000" );

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_CLIENT_RELAY_REQUEST_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_CLIENT_RELAY_REQUEST_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_CLIENT_RELAY_REQUEST_PACKET );

        next_check( in.version_major == out.version_major );
        next_check( in.version_minor == out.version_minor );
        next_check( in.version_patch == out.version_patch );
        next_check( in.buyer_id == out.buyer_id );
        next_check( in.datacenter_id == out.datacenter_id );
        next_check( in.request_id == out.request_id );
        next_check( next_address_equal( &in.client_address, &out.client_address ) );
    }
}

void test_client_relay_response_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t j = 0; j < iterations; ++j )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendClientRelayResponsePacket in, out;
        next_address_parse( &in.client_address, "127.0.0.1:40000" );
        in.request_id = next_random_uint64();
        in.latitude = next_random_float();
        in.longitude = next_random_float();
        in.num_client_relays = rand() % ( NEXT_MAX_CLIENT_RELAYS + 1 );
        for ( int i = 0; i < in.num_client_relays; i++ )
        {
            in.client_relay_ids[i] = next_random_uint64();
            next_address_parse( &in.client_relay_addresses[i], "127.0.0.1:50000" );
            next_crypto_random_bytes( in.client_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES );
        }
        in.expire_timestamp = next_random_uint64();

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_CLIENT_RELAY_RESPONSE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_CLIENT_RELAY_RESPONSE_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_CLIENT_RELAY_RESPONSE_PACKET );

        next_check( next_address_equal( &in.client_address, &out.client_address ) );
        next_check( in.request_id == out.request_id );
        next_check( in.latitude == out.latitude );
        next_check( in.longitude == out.longitude );
        next_check( in.num_client_relays == out.num_client_relays );
        for ( int i = 0; i < in.num_client_relays; i++ )
        {
            next_check( in.client_relay_ids[i] == out.client_relay_ids[i] );
            next_check( next_address_equal( &in.client_relay_addresses[i], &out.client_relay_addresses[i] ) );
            next_check( memcmp( in.client_relay_ping_tokens[i], out.client_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES ) == 0 );
        }
        next_check( in.expire_timestamp == out.expire_timestamp );
    }
}

void test_server_relay_request_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t i = 0; i < iterations; ++i )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendServerRelayRequestPacket in, out;
        in.buyer_id = next_random_uint64();
        in.datacenter_id = next_random_uint64();
        in.request_id = next_random_uint64();

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SERVER_RELAY_REQUEST_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SERVER_RELAY_REQUEST_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SERVER_RELAY_REQUEST_PACKET );

        next_check( in.version_major == out.version_major );
        next_check( in.version_minor == out.version_minor );
        next_check( in.version_patch == out.version_patch );
        next_check( in.buyer_id == out.buyer_id );
        next_check( in.datacenter_id == out.datacenter_id );
        next_check( in.request_id == out.request_id );
    }
}

void test_server_relay_response_packet()
{
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
    uint64_t iterations = 100;
    for ( uint64_t j = 0; j < iterations; ++j )
    {
        unsigned char public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        next_crypto_sign_keypair( public_key, private_key );

        uint8_t magic[8];
        uint8_t from_address[4];
        uint8_t to_address[4];
        next_crypto_random_bytes( magic, 8 );
        next_crypto_random_bytes( from_address, 4 );
        next_crypto_random_bytes( to_address, 4 );

        static NextBackendServerRelayResponsePacket in, out;
        in.request_id = next_random_uint64();
        in.num_server_relays = rand() % ( NEXT_MAX_SERVER_RELAYS + 1 );
        for ( int i = 0; i < in.num_server_relays; i++ )
        {
            in.server_relay_ids[i] = next_random_uint64();
            next_address_parse( &in.server_relay_addresses[i], "127.0.0.1:50000" );
            next_crypto_random_bytes( in.server_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES );
        }
        in.expire_timestamp = next_random_uint64();

        int packet_bytes = 0;
        next_check( next_write_backend_packet( NEXT_BACKEND_SERVER_RELAY_RESPONSE_PACKET, &in, packet_data, &packet_bytes, next_signed_packets, private_key, magic, from_address, to_address ) == NEXT_OK );

        const uint8_t packet_id = packet_data[0];
        next_check( packet_id == NEXT_BACKEND_SERVER_RELAY_RESPONSE_PACKET );

        next_check( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_check( next_advanced_packet_filter( packet_data, magic, from_address, to_address, packet_bytes ) );

        const int begin = 18;
        const int end = packet_bytes;

        next_check( next_read_backend_packet( packet_id, packet_data, begin, end, &out, next_signed_packets, public_key ) == NEXT_BACKEND_SERVER_RELAY_RESPONSE_PACKET );

        next_check( in.request_id == out.request_id );
        next_check( in.num_server_relays == out.num_server_relays );
        for ( int i = 0; i < in.num_server_relays; i++ )
        {
            next_check( in.server_relay_ids[i] == out.server_relay_ids[i] );
            next_check( next_address_equal( &in.server_relay_addresses[i], &out.server_relay_addresses[i] ) );
            next_check( memcmp( in.server_relay_ping_tokens[i], out.server_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES ) == 0 );
        }
        next_check( in.expire_timestamp == out.expire_timestamp );
    }
}

static uint64_t test_passthrough_packets_client_packets_received;

void test_passthrough_packets_client_packet_received_callback( next_client_t * client, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    (void) client;
    (void) context;
    (void) from;
    for ( int i = 0; i < packet_bytes; i++ )
    {
        if ( packet_data[i] != uint8_t( packet_bytes + i ) )
            return;
    }
    test_passthrough_packets_client_packets_received++;
}

#if NEXT_PLATFORM_CAN_RUN_SERVER

static uint64_t test_passthrough_packets_server_packets_received;

void test_passthrough_packets_server_packet_received_callback( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    (void) context;
    next_server_send_packet( server, from, packet_data, packet_bytes );
    for ( int i = 0; i < packet_bytes; i++ )
    {
        if ( packet_data[i] != uint8_t( packet_bytes + i ) )
            return;
    }
    test_passthrough_packets_server_packets_received++;
}

void test_passthrough_packets()
{
    next_server_t * server = next_server_create( NULL, "127.0.0.1", "0.0.0.0:12345", "local", test_passthrough_packets_server_packet_received_callback );

    next_check( server );

    next_client_t * client = next_client_create( NULL, "0.0.0.0:0", test_passthrough_packets_client_packet_received_callback );

    next_check( client );

    next_check( next_client_port( client ) != 0 );

    next_client_open_session( client, "127.0.0.1:12345" );

    uint8_t packet_data[NEXT_MTU];
    memset( packet_data, 0, sizeof(packet_data) );

    for ( int i = 0; i < 10000; ++i )
    {
        int packet_bytes = 1 + rand() % NEXT_MTU;
        for ( int j = 0; j < packet_bytes; j++ )
        {
            packet_data[j] = uint8_t( packet_bytes + j );
        }

        next_client_send_packet( client, packet_data, packet_bytes );

        next_client_update( client );

        next_server_update( server );

        if ( test_passthrough_packets_client_packets_received > 10 && test_passthrough_packets_server_packets_received > 10 )
            break;
    }

    next_assert( test_passthrough_packets_client_packets_received > 10 );
    next_assert( test_passthrough_packets_server_packets_received > 10 );

    next_client_close_session( client );

    next_client_destroy( client );

    next_server_flush( server );

    next_server_destroy( server );
}

#endif // #if NEXT_PLATFORM_CAN_RUN_SERVER

void test_packet_tagging()
{
    if ( next_packet_tagging_can_be_enabled() )
    {
        next_enable_packet_tagging();

#if NEXT_PLATFORM_CAN_RUN_SERVER
        next_server_t * server = next_server_create( NULL, "127.0.0.1", "0.0.0.0:12345", "local", test_passthrough_packets_server_packet_received_callback );
        next_check( server );
#endif // #if NEXT_PLATFORM_CAN_RUN_SERVER

        next_client_t * client = next_client_create( NULL, "0.0.0.0:0", test_passthrough_packets_client_packet_received_callback );
        next_check( client );

        next_disable_packet_tagging();
    }
}

#define RUN_TEST( test_function )                                           \
    do                                                                      \
    {                                                                       \
        next_printf( NEXT_LOG_LEVEL_NONE, "    " #test_function );          \
        fflush( stdout );                                                   \
        test_function();                                                    \
    }                                                                       \
    while (0)

void next_run_tests()
{
    // while ( true )
    {
        RUN_TEST( test_time );
        RUN_TEST( test_endian );
        RUN_TEST( test_base64 );
        RUN_TEST( test_hash );
        RUN_TEST( test_queue );
        RUN_TEST( test_bitpacker );
        RUN_TEST( test_bits_required );
        RUN_TEST( test_stream );
        RUN_TEST( test_address );
        RUN_TEST( test_replay_protection );
        RUN_TEST( test_ping_stats );
        RUN_TEST( test_random_bytes );
        RUN_TEST( test_random_float );
        RUN_TEST( test_crypto_box );
        RUN_TEST( test_crypto_secret_box );
        RUN_TEST( test_crypto_aead );
        RUN_TEST( test_crypto_aead_ietf );
        RUN_TEST( test_crypto_sign_detached );
        RUN_TEST( test_crypto_key_exchange );
        RUN_TEST( test_basic_read_and_write );
        RUN_TEST( test_address_read_and_write );
        RUN_TEST( test_address_ipv4_read_and_write );
        RUN_TEST( test_platform_socket );
        RUN_TEST( test_platform_thread );
        RUN_TEST( test_platform_mutex );
        RUN_TEST( test_client_ipv4 );
#if NEXT_PLATFORM_CAN_RUN_SERVER
        RUN_TEST( test_server_ipv4 );
#endif // #if NEXT_PLATFORM_CAN_RUN_SERVER
        RUN_TEST( test_upgrade_token );
        RUN_TEST( test_header );
        RUN_TEST( test_abi );
        RUN_TEST( test_packet_filter );
        RUN_TEST( test_basic_packet_filter );
        RUN_TEST( test_advanced_packet_filter );
        RUN_TEST( test_passthrough );
        RUN_TEST( test_address_data_ipv4 );
        RUN_TEST( test_anonymize_address_ipv4 );
#if NEXT_PLATFORM_HAS_IPV6
        RUN_TEST( test_anonymize_address_ipv6 );
#endif // #if NEXT_PLATFORM_HAS_IPV6
        RUN_TEST( test_bandwidth_limiter );
        RUN_TEST( test_packet_loss_tracker );
        RUN_TEST( test_out_of_order_tracker );
        RUN_TEST( test_jitter_tracker );
        RUN_TEST( test_free_retains_context );
        RUN_TEST( test_pending_session_manager );
        RUN_TEST( test_proxy_session_manager );
        RUN_TEST( test_session_manager );
        RUN_TEST( test_relay_manager );
        RUN_TEST( test_direct_packet );
        RUN_TEST( test_direct_ping_packet );
        RUN_TEST( test_direct_pong_packet );
        RUN_TEST( test_upgrade_request_packet );
        RUN_TEST( test_upgrade_response_packet );
        RUN_TEST( test_upgrade_confirm_packet );
        RUN_TEST( test_route_request_packet );
        RUN_TEST( test_route_response_packet );
        RUN_TEST( test_client_to_server_packet );
        RUN_TEST( test_server_to_client_packet );
        RUN_TEST( test_session_ping_packet );
        RUN_TEST( test_session_pong_packet );
        RUN_TEST( test_continue_request_packet );
        RUN_TEST( test_continue_response_packet );
        RUN_TEST( test_client_stats_packet_with_client_relays );
        RUN_TEST( test_client_stats_packet_without_client_relays );
        RUN_TEST( test_route_update_packet_direct );
        RUN_TEST( test_route_update_packet_new_route );
        RUN_TEST( test_route_update_packet_continue_route );
        RUN_TEST( test_route_ack_packet );
        RUN_TEST( test_client_relay_update_packet );
        RUN_TEST( test_client_relay_ack_packet );
        RUN_TEST( test_client_ping_packet );
        RUN_TEST( test_client_pong_packet );
        RUN_TEST( test_server_ping_packet );
        RUN_TEST( test_server_pong_packet );
        RUN_TEST( test_server_init_request_packet );
        RUN_TEST( test_server_init_response_packet );
        RUN_TEST( test_server_update_request_packet );
        RUN_TEST( test_server_update_response_packet );
        RUN_TEST( test_session_update_request_packet );
        RUN_TEST( test_session_update_response_packet_direct );
        RUN_TEST( test_session_update_response_packet_route );
        RUN_TEST( test_session_update_response_packet_continue );
        RUN_TEST( test_client_relay_request_packet );
        RUN_TEST( test_client_relay_response_packet );
        RUN_TEST( test_server_relay_request_packet );
        RUN_TEST( test_server_relay_response_packet );
#if NEXT_PLATFORM_CAN_RUN_SERVER
        RUN_TEST( test_passthrough_packets );
#endif // #if NEXT_PLATFORM_CAN_RUN_SERVER
        RUN_TEST( test_packet_tagging );
    }
}

#else // #if NEXT_DEVELOPMENT

#include <stdio.h>

void next_run_tests()
{
    printf( "\n[tests are not included in this build]\n\n" );
}

#endif // #if NEXT_DEVELOPMENT

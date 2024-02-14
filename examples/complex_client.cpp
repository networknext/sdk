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

#include "next.h"
#include "next_platform.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <map>

const char * bind_address = "0.0.0.0:0";
const char * server_address = "127.0.0.1:50000";
const char * buyer_public_key = "zkaPRGcAuThG6poXMJ8di/yKzgZEbqyQ6Ky951reRq4sgCm83lV24g==";

// -------------------------------------------------------------

struct AllocatorEntry
{
    // ...
};

class Allocator
{
    int64_t num_allocations;
    next_platform_mutex_t mutex;
    std::map<void*, AllocatorEntry*> entries;

public:

    Allocator()
    {
        int result = next_platform_mutex_create( &mutex );
        (void) result;
        next_assert( result == NEXT_OK );
        num_allocations = 0;
    }

    ~Allocator()
    {
        next_platform_mutex_destroy( &mutex );
        next_assert( num_allocations == 0 );
        next_assert( entries.size() == 0 );
    }

    void * Alloc( size_t size )
    {
        next_platform_mutex_guard( &mutex );
        void * pointer = malloc( size );
        next_assert( pointer );
        next_assert( entries[pointer] == NULL );
        AllocatorEntry * entry = new AllocatorEntry();
        entries[pointer] = entry;
        num_allocations++;
        return pointer;
    }

    void Free( void * pointer )
    {
        next_platform_mutex_guard( &mutex );
        next_assert( pointer );
        next_assert( num_allocations > 0 );
        std::map<void*, AllocatorEntry*>::iterator itor = entries.find( pointer );
        next_assert( itor != entries.end() );
        entries.erase( itor );
        num_allocations--;
        free( pointer );
    }
};

Allocator global_allocator;

struct Context
{
    Allocator * allocator;
};

struct ClientContext
{
    Allocator * allocator;
    uint32_t client_data;
    double last_packet_receive_time;
};

void * malloc_function( void * _context, size_t bytes )
{
    Context * context = (Context*) _context;
    next_assert( context );
    next_assert( context->allocator );
    return context->allocator->Alloc( bytes );
}

void free_function( void * _context, void * p )
{
    Context * context = (Context*) _context;
    next_assert( context );
    next_assert( context->allocator );
    return context->allocator->Free( p );
}

// -------------------------------------------------------------

extern const char * log_level_string( int level )
{
    if ( level == NEXT_LOG_LEVEL_DEBUG )
        return "debug";
    else if ( level == NEXT_LOG_LEVEL_INFO )
        return "info";
    else if ( level == NEXT_LOG_LEVEL_ERROR )
        return "error";
    else if ( level == NEXT_LOG_LEVEL_WARN )
        return "warning";
    else
        return "???";
}

void log_function( int level, const char * format, ... ) 
{
    va_list args;
    va_start( args, format );
    char buffer[1024];
    vsnprintf( buffer, sizeof( buffer ), format, args );
    if ( level != NEXT_LOG_LEVEL_NONE )
    {
        const char * level_string = log_level_string( level );
        printf( "%.2f: %s: %s\n", next_platform_time(), level_string, buffer );
    }
    else
    {
        printf( "%s\n", buffer );
    }
    va_end( args );
    fflush( stdout );
}

void assert_function( const char * condition, const char * function, const char * file, int line )
{
    next_printf( NEXT_LOG_LEVEL_NONE, "assert failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
    fflush( stdout );
    #if defined(_MSC_VER)
        __debugbreak();
    #elif defined(__ORBIS__)
        __builtin_trap();
    #elif defined(__PROSPERO__)
        __builtin_trap();
    #elif defined(__clang__)
        __builtin_debugtrap();
    #elif defined(__GNUC__)
        __builtin_trap();
    #elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
        raise(SIGTRAP);
    #else
        #error "asserts not supported on this platform!"
    #endif
}

// -------------------------------------------------------------

static volatile int quit = 0;

void interrupt_handler( int signal )
{
    (void) signal; quit = 1;
}

void generate_packet( uint8_t * packet_data, int & packet_bytes )
{
    packet_bytes = 1 + ( rand() % NEXT_MTU );
    const int start = packet_bytes % 256;
    for ( int i = 0; i < packet_bytes; ++i )
        packet_data[i] = (uint8_t) ( start + i ) % 256;
}

bool verify_packet( const uint8_t * packet_data, int packet_bytes )
{
    const int start = packet_bytes % 256;
    for ( int i = 0; i < packet_bytes; ++i )
    {
        if ( packet_data[i] != (uint8_t) ( ( start + i ) % 256 ) )
            return false;
    }
    return true;
}

void client_packet_received( next_client_t * client, void * _context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    (void) client;
    (void) from;

    ClientContext * context = (ClientContext*) _context;

    next_assert( context );
    next_assert( context->allocator != NULL );
    next_assert( context->client_data == 0x12345 );

    if ( verify_packet( packet_data, packet_bytes ) )
    {
        context->last_packet_receive_time = next_platform_time();

        next_printf( NEXT_LOG_LEVEL_INFO, "client received packet from server (%d bytes)", packet_bytes );
    }
}

#if NEXT_PLATFORM != NEXT_PLATFORM_WINDOWS
#define strncpy_s strncpy
#endif // #if NEXT_PLATFORM != NEXT_PLATFORM_WINDOWS

void print_client_stats( next_client_t * client )
{
    bool show_detailed_stats = true;

    if ( !show_detailed_stats )
        return;

    printf( "================================================================\n" );
    
    const next_client_stats_t * stats = next_client_stats( client );

    const char * platform = "unknown";

    switch ( stats->platform_id )
    {
        case NEXT_PLATFORM_WINDOWS:
            platform = "windows";
            break;

        case NEXT_PLATFORM_MAC:
            platform = "mac";
            break;

        case NEXT_PLATFORM_LINUX:
            platform = "linux";
            break;

        case NEXT_PLATFORM_SWITCH:
            platform = "nintendo switch";
            break;

        case NEXT_PLATFORM_PS4:
            platform = "ps4";
            break;

        case NEXT_PLATFORM_PS5:
            platform = "ps5";
            break;

        case NEXT_PLATFORM_IOS:
            platform = "ios";
            break;

        case NEXT_PLATFORM_XBOX_ONE:
            platform = "xbox one";
            break;

        case NEXT_PLATFORM_XBOX_SERIES_X:
            platform = "xbox series x";
            break;

        default:
            break;
    }

    const char * state_string = "???";

    const int state = next_client_state( client );
    
    switch ( state )
    {
        case NEXT_CLIENT_STATE_CLOSED:
            state_string = "closed";
            break;

        case NEXT_CLIENT_STATE_OPEN:
            state_string = "open";
            break;

        case NEXT_CLIENT_STATE_ERROR:
            state_string = "error";
            break;

        default:
            break;
    }

    printf( "state = %s (%d)\n", state_string, state );

    printf( "session id = %" PRIx64 "\n", next_client_session_id( client ) );

    printf( "platform id = %s (%d)\n", platform, (int) stats->platform_id );

    const char * connection = "unknown";
    
    switch ( stats->connection_type )
    {
        case NEXT_CONNECTION_TYPE_WIRED:
            connection = "wired";
            break;

        case NEXT_CONNECTION_TYPE_WIFI:
            connection = "wifi";
            break;

        case NEXT_CONNECTION_TYPE_CELLULAR:
            connection = "cellular";
            break;

        default:
            break;
    }

    printf( "connection type = %s (%d)\n", connection, stats->connection_type );

    if ( !stats->fallback_to_direct )
    {
        printf( "upgraded = %s\n", stats->upgraded ? "true" : "false" );
        printf( "multipath = %s\n", stats->multipath ? "true" : "false" );
        printf( "reported = %s\n", stats->reported ? "true" : "false" );
    }

    printf( "fallback to direct = %s\n", stats->fallback_to_direct ? "true" : "false" );

    printf( "direct rtt = %.2fms\n", stats->direct_rtt );
    printf( "direct jitter = %.2fms\n", stats->direct_jitter );
    printf( "direct packet loss = %.1f%%\n", stats->direct_packet_loss );

    if ( stats->next )
    {
        printf( "next rtt = %.2fms\n", stats->next_rtt );
        printf( "next jitter = %.2fms\n", stats->next_jitter );
        printf( "next packet loss = %.1f%%\n", stats->next_packet_loss );
        printf( "next bandwidth up = %.1fkbps\n", stats->next_kbps_up );
        printf( "next bandwidth down = %.1fkbps\n", stats->next_kbps_down );
    }

    if ( stats->upgraded && !stats->fallback_to_direct )
    {
        printf( "packets sent client to server = %" PRId64 "\n", stats->packets_sent_client_to_server );
        printf( "packets sent server to client = %" PRId64 "\n", stats->packets_sent_server_to_client );
        printf( "packets lost client to server = %" PRId64 "\n", stats->packets_lost_client_to_server );
        printf( "packets lost server to client = %" PRId64 "\n", stats->packets_lost_server_to_client );
        printf( "packets out of order client to server = %" PRId64 "\n", stats->packets_out_of_order_client_to_server );
        printf( "packets out of order server to client = %" PRId64 "\n", stats->packets_out_of_order_server_to_client );
        printf( "jitter client to server = %f\n", stats->jitter_client_to_server );
        printf( "jitter server to client = %f\n", stats->jitter_server_to_client );
    }

    printf( "================================================================\n" );
}

void update_client_timeout( ClientContext * context )
{
    next_assert( context );
    if ( context->last_packet_receive_time + 5.0 < next_platform_time() )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "client connection timed out" );
        quit = true;
    }
}

int main()
{
    signal( SIGINT, interrupt_handler ); signal( SIGTERM, interrupt_handler );
    
    next_log_level( NEXT_LOG_LEVEL_INFO );

    next_log_function( log_function );

    next_assert_function( assert_function );

    next_allocator( malloc_function, free_function );

    Context global_context;
    global_context.allocator = &global_allocator;

    next_config_t config;
    next_default_config( &config );
    strncpy_s( config.buyer_public_key, buyer_public_key, sizeof(config.buyer_public_key) - 1 );

    if ( next_init( &global_context, &config ) != NEXT_OK )
    {
        printf( "error: could not initialize network next\n" );
        return 1;
    }

    Allocator client_allocator;
    
    ClientContext client_context;
    client_context.allocator = &client_allocator;
    client_context.client_data = 0x12345;
    client_context.last_packet_receive_time = next_platform_time();

    next_client_t * client = next_client_create( &client_context, bind_address, client_packet_received );
    if ( client == NULL )
    {
        printf( "error: failed to create client\n" );
        return 1;
    }

    uint16_t client_port = next_client_port( client );

    next_printf( NEXT_LOG_LEVEL_INFO, "client port is %d", client_port );

    next_client_open_session( client, server_address );

    double accumulator = 0.0;

    bool reported = false;

    const double delta_time = 0.25;

    while ( !quit )
    {
        next_client_update( client );

        const int client_state = next_client_state( client );

        if ( client_state == NEXT_CLIENT_STATE_ERROR )
        {
            printf( "error: client is in an error state\n" );
            break;
        }

        if ( next_client_ready( client ) ) 
        {
            int packet_bytes = 0;
            uint8_t packet_data[NEXT_MTU];
            generate_packet( packet_data, packet_bytes );
            next_client_send_packet( client, packet_data, packet_bytes );
        }
        
        if ( next_platform_time() > 60.0 && !reported )
        {
            next_client_report_session( client );
            reported = true;
        }

        accumulator += delta_time;

        if ( accumulator > 10.0 )
        {
            print_client_stats( client );
            accumulator = 0.0;
        }

        update_client_timeout( &client_context );

        next_platform_sleep( delta_time );
    }

    next_client_destroy( client );
    
    next_term();

    printf( "\n" );

    return 0;
}

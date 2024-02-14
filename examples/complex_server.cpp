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
#include "next_address.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <map>

const char * bind_address = "0.0.0.0:50000";
const char * server_address = "127.0.0.1:50000";
const char * server_datacenter = "local";
const char * server_backend_hostname = "server-dev.virtualgo.net";

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

struct ClientData
{
    uint64_t session_id;
    next_address_t address;
    double last_packet_receive_time;
};

typedef std::map<next_address_t, ClientData> ClientMap;

struct ServerContext
{
    Allocator * allocator;
    uint32_t server_data;
    ClientMap client_map;
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

const char * log_level_string( int level )
{
    if ( level == NEXT_LOG_LEVEL_SPAM )
        return "spam";
    else if ( level == NEXT_LOG_LEVEL_DEBUG )
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

bool operator < ( const next_address_t & a, const next_address_t & b) 
{
    next_assert( a.type == NEXT_ADDRESS_IPV4 );
    next_assert( b.type == NEXT_ADDRESS_IPV4 );
    uint64_t scalar_a = 0;
    scalar_a |= uint64_t(a.data.ipv4[0]);
    scalar_a |= uint64_t(a.data.ipv4[1]) << 8;
    scalar_a |= uint64_t(a.data.ipv4[2]) << 16;
    scalar_a |= uint64_t(a.data.ipv4[3]) << 24;
    scalar_a <<= 32;
    scalar_a |= uint64_t(a.port);
    uint64_t scalar_b = 0;
    scalar_b |= uint64_t(b.data.ipv4[0]);
    scalar_b |= uint64_t(b.data.ipv4[1]) << 8;
    scalar_b |= uint64_t(b.data.ipv4[2]) << 16;
    scalar_b |= uint64_t(b.data.ipv4[3]) << 24;
    scalar_b <<= 32;
    scalar_b |= uint64_t(b.port);
    return scalar_a < scalar_b;
}

void server_packet_received( next_server_t * server, void * _context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    ServerContext * context = (ServerContext*) _context;

    next_assert( context );
    next_assert( context->allocator != NULL );
    next_assert( context->server_data == 0x12345678 );

    if ( !next_server_ready( server ) )
        return;

    next_printf( NEXT_LOG_LEVEL_INFO, "server received packet from client (%d bytes)", packet_bytes );
    
    next_server_send_packet( server, from, packet_data, packet_bytes );
    
    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];

    ClientMap::iterator itor = context->client_map.find(*from);

    ClientData client_data;

    if ( itor == context->client_map.end() )
    {
        const char * user_id = "user id can be any id that is unique across all users. we hash it before sending up to our backend";

        uint64_t session_id = next_server_upgrade_session( server, from, user_id );

        next_printf( NEXT_LOG_LEVEL_INFO, "client connected: %s [%" PRIx64 "]", next_address_to_string( from, address_buffer ), session_id );

        client_data.address = *from;
        client_data.session_id = session_id;
        client_data.last_packet_receive_time = next_platform_time();

        context->client_map[*from] = client_data;
    }
    else
    {
        itor->second.last_packet_receive_time = next_platform_time();
    }
}

#if NEXT_PLATFORM != NEXT_PLATFORM_WINDOWS
#define strncpy_s strncpy
#endif // #if NEXT_PLATFORM != NEXT_PLATFORM_WINDOWS

void update_client_timeouts( ServerContext * context )
{
    next_assert( context );
    ClientMap::iterator itor = context->client_map.begin();
    const double current_time = next_platform_time();
    while ( itor != context->client_map.end() ) 
    {
        if ( itor->second.last_packet_receive_time + 5.0 < current_time )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_INFO, "client disconnected: %s [%" PRIx64 "]", next_address_to_string( &itor->second.address, address_buffer ), itor->second.session_id );
            itor = context->client_map.erase( itor );
        } 
        else 
        {
            itor++;
        }
    }
}

void print_server_stats( next_server_t * server, ServerContext * context )
{
    next_assert( server );
    next_assert( context );

    next_printf( NEXT_LOG_LEVEL_INFO, "%d connected clients", (int) context->client_map.size() );

    bool show_detailed_stats = true;

    if ( !show_detailed_stats )
        return;

    for ( ClientMap::iterator itor = context->client_map.begin(); itor != context->client_map.end(); ++itor ) 
    {
        next_server_stats_t stats;
        if ( !next_server_stats( server, &itor->second.address, &stats ) )
            continue;

        printf( "================================================================\n" );
        
        char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
        printf( "address = %s\n", next_address_to_string( &itor->second.address, address_buffer ) );

        const char * platform = "unknown";

        switch ( stats.platform_id )
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

        printf( "session_id = %" PRIx64 "\n", stats.session_id );

        printf( "platform_id = %s (%d)\n", platform, (int) stats.platform_id );

        const char * connection = "unknown";
        
        switch ( stats.connection_type )
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

        printf( "connection_type = %s (%d)\n", connection, stats.connection_type );

        if ( !stats.fallback_to_direct )
        {
            printf( "multipath = %s\n", stats.multipath ? "true" : "false" );
            printf( "reported = %s\n", stats.reported ? "true" : "false" );
        }

        printf( "fallback_to_direct = %s\n", stats.fallback_to_direct ? "true" : "false" );

        printf( "direct_rtt = %.2fms\n", stats.direct_rtt );
        printf( "direct_jitter = %.2fms\n", stats.direct_jitter );
        printf( "direct_packet_loss = %.1f%%\n", stats.direct_packet_loss );

        if ( stats.next )
        {
            printf( "next_rtt = %.2fms\n", stats.next_rtt );
            printf( "next_jitter = %.2fms\n", stats.next_jitter );
            printf( "next_packet_loss = %.1f%%\n", stats.next_packet_loss );
            printf( "next_bandwidth_up = %.1fkbps\n", stats.next_kbps_up );
            printf( "next_bandwidth_down = %.1fkbps\n", stats.next_kbps_down );
        }

        if ( !stats.fallback_to_direct )
        {
            printf( "packets_sent_client_to_server = %" PRId64 "\n", stats.packets_sent_client_to_server );
            printf( "packets_sent_server_to_client = %" PRId64 "\n", stats.packets_sent_server_to_client );
            printf( "packets_lost_client_to_server = %" PRId64 "\n", stats.packets_lost_client_to_server );
            printf( "packets_lost_server_to_client = %" PRId64 "\n", stats.packets_lost_server_to_client );
            printf( "packets_out_of_order_client_to_server = %" PRId64 "\n", stats.packets_out_of_order_client_to_server );
            printf( "packets_out_of_order_server_to_client = %" PRId64 "\n", stats.packets_out_of_order_server_to_client );
            printf( "jitter_client_to_server = %f\n", stats.jitter_client_to_server );
            printf( "jitter_server_to_client = %f\n", stats.jitter_server_to_client );
        }

        printf( "================================================================\n" );
    }
}

int main()
{
    signal( SIGINT, interrupt_handler ); signal( SIGTERM, interrupt_handler );

    Context global_context;
    global_context.allocator = &global_allocator;

    next_log_level( NEXT_LOG_LEVEL_INFO );

    next_log_function( log_function );

    next_assert_function( assert_function );

    next_allocator( malloc_function, free_function );

    next_config_t config;
    next_default_config( &config );
    strncpy_s( config.server_backend_hostname, server_backend_hostname, sizeof(config.server_backend_hostname) - 1 );

    if ( next_init( &global_context, &config ) != NEXT_OK )
    {
        printf( "error: could not initialize network next\n" );
        return 1;
    }

    Allocator server_allocator;

    ServerContext server_context;
    server_context.allocator = &server_allocator;
    server_context.server_data = 0x12345678;

    next_server_t * server = next_server_create( &server_context, server_address, bind_address, server_datacenter, server_packet_received );
    if ( server == NULL )
    {
        printf( "error: failed to create server\n" );
        return 1;
    }
    
    uint16_t server_port = next_server_port( server );

    next_printf( NEXT_LOG_LEVEL_INFO, "server port is %d", server_port );

    double accumulator = 0.0;

    const double delta_time = 0.25;

    while ( !quit )
    {
        next_server_update( server );

        update_client_timeouts( &server_context );

        accumulator += delta_time;

        if ( accumulator > 10.0 )
        {
            print_server_stats( server, &server_context );
            accumulator = 0.0;
        }

        next_platform_sleep( delta_time );
    }

    next_server_flush( server );
    
    next_server_destroy( server );
    
    next_term();

    printf( "\n" );

    return 0;
}

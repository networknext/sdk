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

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "next.h"
#include "next_platform.h"
#include "next_address.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <map>

#define NEXT_PLATFORM_SOCKET_NON_BLOCKING                               0
#define NEXT_PLATFORM_SOCKET_BLOCKING                                   1

const int MaxServers = 10;
const int MaxClients = 1000;

const char * server_datacenter = "local";
const char * server_backend_hostname = "127.0.0.1";
const char * buyer_public_key = "5Vr+VZdUXckPZsd89NGTmXASmmlHRuWiyVs7orAxRV6hDkvTc3VMtCBDAd09F+1z/whRYMvtl+28E7MT/5mmn48iNJTQrGbC";
const char * buyer_private_key = "5Vr+VZdUXckPZsd89NGTmXASmmlHRuWiyVs7orAxRV6hDkvTc3VMtCBDAd09F+1z/whRYMvtl+28E7MT/5mmn48iNJTQrGbC";

#define FUZZ_TEST 1

struct next_platform_socket_t;

extern next_platform_socket_t * next_platform_socket_create( void * context, next_address_t * address, int socket_type, float timeout_seconds, int send_buffer_size, int receive_buffer_size );

extern void next_platform_socket_destroy( next_platform_socket_t * socket );

extern void next_platform_socket_send_packet( next_platform_socket_t * socket, const next_address_t * to, const void * packet_data, int packet_bytes );

// -------------------------------------------

struct AllocatorEntry
{
    uint64_t bytes;
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
        entry->bytes = size;
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
        /*
        printf( "free %d bytes\n", (int)itor->second->bytes );
        */
        entries.erase( itor );
        num_allocations--;
        free( pointer );
    }
};

void * malloc_function( void * context, size_t bytes )
{
    next_assert( context );
    Allocator * allocator = (Allocator*) context;
    /*
    printf( "allocated %d bytes\n", (int)bytes ); fflush( stdout );
    */
    return allocator->Alloc( bytes );
}

void free_function( void * context, void * p )
{
    next_assert( context );

    Allocator * allocator = (Allocator*) context;
    return allocator->Free( p );
}

// ----------------------------------------------------------

static next_server_t * servers[MaxServers];
static next_client_t * clients[MaxClients];

static Allocator * server_allocator[MaxServers];
static Allocator * client_allocator[MaxClients];

static Allocator global_allocator;

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

void verify_packet( const uint8_t * packet_data, int packet_bytes )
{
#if !FUZZ_TEST
    const int start = packet_bytes % 256;
    for ( int i = 0; i < packet_bytes; ++i )
    {
        if ( packet_data[i] != (uint8_t) ( ( start + i ) % 256 ) )
        {
            printf( "%d: %d != %d (%d)\n", i, packet_data[i], ( start + i ) % 256, packet_bytes );
        }
        next_assert( packet_data[i] == (uint8_t) ( ( start + i ) % 256 ) );
    }
#else
    (void) packet_data;
    (void) packet_bytes;
#endif
}

void client_packet_received( next_client_t * client, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    (void) client; (void) context; (void) from;
    verify_packet( packet_data, packet_bytes );
}

void server_packet_received( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    (void) context;
    verify_packet( packet_data, packet_bytes );
    next_server_send_packet( server, from, packet_data, packet_bytes );
    if ( !next_server_session_upgraded( server, from ) )
    {
        next_server_upgrade_session( server, from, NULL );
    }
}

// ----------------------------------------------------------

int main( int argc, char ** argv )
{
    srand( (unsigned int) time( NULL ) );

    signal( SIGINT, interrupt_handler ); signal( SIGTERM, interrupt_handler );
    
    next_config_t config;
    next_default_config( &config );
    strncpy( config.server_backend_hostname, server_backend_hostname, sizeof(config.server_backend_hostname) - 1 );
    strncpy( config.buyer_public_key, buyer_public_key, sizeof(config.buyer_public_key) - 1 );
    strncpy( config.buyer_private_key, buyer_private_key, sizeof(config.buyer_private_key) - 1 );

    next_allocator( malloc_function, free_function );

    next_init( &global_allocator, &config );
    
    int duration_seconds = 0;
    if ( argc == 2 ) 
    {
        duration_seconds = atoi( argv[1] );
    }

#if FUZZ_TEST
    Allocator fuzz_allocator;
    next_address_t fuzz_address;
    memset( &fuzz_address, 0, sizeof(fuzz_address) );
    fuzz_address.type = NEXT_ADDRESS_IPV4;
    next_platform_socket_t * fuzz_socket = next_platform_socket_create( &fuzz_allocator, &fuzz_address, NEXT_PLATFORM_SOCKET_BLOCKING, -1.0f, 1024*1024, 1024*1024 );
    if ( !fuzz_socket )
    {
        printf( "error: could not create fuzz socket\n" );
        exit(1);
    }
#endif // FUZZ_TEST

    while ( !quit )
    {
        // randomly create clients

        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( clients[i] == NULL && ( rand() % 1000 ) == 0 )
            {
                next_assert( client_allocator[i] == NULL );
                client_allocator[i] = new Allocator();
                clients[i] = next_client_create( client_allocator[i], "0.0.0.0:0", client_packet_received );
                if ( clients[i] )
                {
                    next_printf( NEXT_LOG_LEVEL_INFO, "created client %d", i );
                }
                else
                {
                    next_printf( NEXT_LOG_LEVEL_INFO, "could not create client %d", i );
                    delete client_allocator[i];
                    client_allocator[i] = NULL;
                }
            }
        }

        // randomly destroy clients

        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( clients[i] && ( rand() % 15000 ) == 0 )
            {
                next_assert( client_allocator[i] != NULL );
                
                next_client_destroy( clients[i] );

                delete client_allocator[i];
                client_allocator[i] = NULL;
                clients[i] = NULL;
                
                next_printf( NEXT_LOG_LEVEL_INFO, "destroyed client %d", i );
            }
        }

        // randomly open client sessions

        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !clients[i] )
                continue;

            int client_state = next_client_state( clients[i] );

            if ( ( client_state == NEXT_CLIENT_STATE_CLOSED || client_state == NEXT_CLIENT_STATE_ERROR ) && ( rand() % 100 ) == 0 )
            {
                int j = rand() % MaxServers;
                char server_address_string[256]; 
                snprintf( server_address_string, sizeof(server_address_string), "127.0.0.1:%d", 20000 + j );
                next_client_open_session( clients[i], server_address_string );
            }
        }

        // randomly close client sessions

        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( clients[i] && ( rand() % 5000 ) == 0 )
            {
                next_client_close_session( clients[i] );
            }
        }

        // randomly create servers

        for ( int i = 0; i < MaxServers; ++i )
        {
            if ( servers[i] == NULL && ( rand() % 100 ) == 0 )
            {
                next_assert( server_allocator[i] == NULL );
                server_allocator[i] = new Allocator();
                next_assert( server_allocator[i] );
                char server_address_string[256]; 
                char bind_address_string[256];
                snprintf( server_address_string, sizeof(server_address_string), "127.0.0.1:%d", 20000 + i );
                snprintf( bind_address_string, sizeof(server_address_string), "0.0.0.0:%d", 20000 + i );
                servers[i] = next_server_create( server_allocator[i], server_address_string, bind_address_string, "local", server_packet_received );
                if ( servers[i] )
                {
                    next_printf( NEXT_LOG_LEVEL_INFO, "created server %d", i );
                }
                else
                {
                    next_printf( NEXT_LOG_LEVEL_INFO, "could not create server %d", i );
                    delete server_allocator[i];
                    server_allocator[i] = NULL;
                }
            }
        }

        // randomly destroy servers

        for ( int i = 0; i < MaxServers; ++i )
        {
            if ( servers[i] && ( rand() % 10000 ) == 0 )
            {
                next_assert( server_allocator[i] != NULL );

                next_server_flush( servers[i] );

                next_server_destroy( servers[i] );

                delete server_allocator[i];
                server_allocator[i] = NULL;
                servers[i] = NULL;

                next_printf( NEXT_LOG_LEVEL_INFO, "destroyed server %d", i );
            }
        }

#if FUZZ_TEST

        // fuzz clients

        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( clients[i] )
            {
                for ( int j = 0; j < 100; ++j )
                {
                    const int max_packet_bytes = NEXT_MAX_PACKET_BYTES * 2;
                    uint8_t packet_data[max_packet_bytes];
                    int packet_bytes = 1 + ( rand() % max_packet_bytes );
                    for ( int k = 0; k < packet_bytes; ++k )
                        packet_data[k] = rand() % 256;
                    next_address_t client_address;
                    client_address.type = NEXT_ADDRESS_IPV4;
                    client_address.data.ipv4[0] = 127;
                    client_address.data.ipv4[1] = 0;
                    client_address.data.ipv4[2] = 0;
                    client_address.data.ipv4[3] = 1;
                    client_address.port = next_client_port( clients[i] );
                    next_platform_socket_send_packet( fuzz_socket, &client_address, packet_data, packet_bytes );
                }
            }
        }

        // fuzz servers

        for ( int i = 0; i < MaxServers; ++i )
        {
            if ( servers[i] )
            {
                for ( int j = 0; j < 100; ++j )
                {
                    const int max_packet_bytes = NEXT_MAX_PACKET_BYTES * 2;
                    uint8_t packet_data[max_packet_bytes];
                    int packet_bytes = 1 + ( rand() % max_packet_bytes );
                    for ( int k = 0; k < packet_bytes; ++k )
                        packet_data[k] = rand() % 256;
                    const next_address_t * server_address = next_server_address( servers[i] );
                    next_platform_socket_send_packet( fuzz_socket, server_address, packet_data, packet_bytes );
                }
            }
        }

        // fuzz client -> servers

        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !clients[i] )
                continue;

            for ( int j = 0; j < MaxServers; ++j )
            {
                if ( !servers[j] )
                    continue;

                const int max_packet_bytes = NEXT_MAX_PACKET_BYTES * 2;
                uint8_t packet_data[max_packet_bytes];
                int packet_bytes = 1 + ( rand() % max_packet_bytes );
                for ( int k = 0; k < packet_bytes; ++k )
                    packet_data[k] = rand() % 256;

                const next_address_t * server_address = next_server_address( servers[j] );

                next_client_send_packet_raw( clients[i], server_address, packet_data, packet_bytes );
            }

        }

        // fuzz server -> clients

        for ( int i = 0; i < MaxServers; ++i )
        {
            if ( !servers[i] )
                continue;

            for ( int j = 0; j < MaxClients; ++j )
            {
                if ( !clients[j] )
                    continue;

                const int max_packet_bytes = NEXT_MAX_PACKET_BYTES * 2;
                uint8_t packet_data[max_packet_bytes];
                int packet_bytes = 1 + ( rand() % max_packet_bytes );
                for ( int k = 0; k < packet_bytes; ++k )
                    packet_data[k] = rand() % 256;

                next_address_t client_address;
                client_address.type = NEXT_ADDRESS_IPV4;
                client_address.data.ipv4[0] = 127;
                client_address.data.ipv4[1] = 0;
                client_address.data.ipv4[2] = 0;
                client_address.data.ipv4[3] = 1;
                client_address.port = next_client_port( clients[j] );

                next_server_send_packet_raw( servers[i], &client_address, packet_data, packet_bytes );
            }
        }

#endif // #if FUZZ_TEST

        // update clients

        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( clients[i] )
            {
                next_client_update( clients[i] );

                int packet_bytes = 0;
                uint8_t packet_data[NEXT_MTU];
                generate_packet( packet_data, packet_bytes );
                next_client_send_packet( clients[i], packet_data, packet_bytes );
            }
        }

        // update servers

        for ( int i = 0; i < MaxServers; ++i )
        {
            if ( servers[i] )
            {
                next_server_update( servers[i] );
            }
        }

        // optionally quit after a number of seconds

        if ( duration_seconds > 0 )
        {
            if ( (int) next_platform_time() > duration_seconds )
                quit = true;
        }

        next_platform_sleep( 0.01 );
    }

    // destroy clients

    for ( int i = 0; i < MaxClients; ++i )
    {
        if ( clients[i] )
        {
            next_client_destroy( clients[i] );
            next_assert( client_allocator[i] );
            delete client_allocator[i];
            next_printf( NEXT_LOG_LEVEL_INFO, "destroyed client %d", i );
        }
    }

    // destroy servers

    for ( int i = 0; i < MaxServers; ++i )
    {
        if ( servers[i] )
        {
            next_server_flush( servers[i] );
            next_server_destroy( servers[i] );
            next_assert( server_allocator[i] );
            delete server_allocator[i];
            next_printf( NEXT_LOG_LEVEL_INFO, "destroyed server %d", i );
        }
    }

#if FUZZ_TEST
    next_platform_socket_destroy( fuzz_socket );
#endif // #if FUZZ_TEST

    next_printf( NEXT_LOG_LEVEL_INFO, "done." );
    
    next_term();

    return 0;
}

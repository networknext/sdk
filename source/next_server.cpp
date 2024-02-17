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

#include "next_server.h"
#include "next_queue.h"
#include "next_hash.h"
#include "next_pending_session_manager.h"
#include "next_proxy_session_manager.h"
#include "next_packet_filter.h"
#include "next_header.h"
#include "next_upgrade_token.h"
#include "next_route_token.h"
#include "next_continue_token.h"
#include "next_autodetect.h"
#include "next_internal_config.h"
#include "next_platform.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ---------------------------------------------------------------

#define NEXT_SERVER_COMMAND_UPGRADE_SESSION                         0
#define NEXT_SERVER_COMMAND_SESSION_EVENT                           1
#define NEXT_SERVER_COMMAND_FLUSH                                   2
#define NEXT_SERVER_COMMAND_SET_PACKET_RECEIVE_CALLBACK             3
#define NEXT_SERVER_COMMAND_SET_SEND_PACKET_TO_ADDRESS_CALLBACK     4
#define NEXT_SERVER_COMMAND_SET_PAYLOAD_RECEIVE_CALLBACK            5

struct next_server_command_t
{
    int type;
};

struct next_server_command_upgrade_session_t : public next_server_command_t
{
    next_address_t address;
    uint64_t session_id;
    uint64_t user_hash;
};

struct next_server_command_session_event_t : public next_server_command_t
{
    next_address_t address;
    uint64_t session_events;
};

struct next_server_command_flush_t : public next_server_command_t
{
    // ...
};

struct next_server_command_set_packet_receive_callback_t : public next_server_command_t
{
    void (*callback) ( void * data, next_address_t * from, uint8_t * packet_data, int * begin, int * end );
    void * callback_data;
};

struct next_server_command_set_send_packet_to_address_callback_t : public next_server_command_t
{
    int (*callback) ( void * data, const next_address_t * address, const uint8_t * packet_data, int packet_bytes );
    void * callback_data;
};

struct next_server_command_set_payload_receive_callback_t : public next_server_command_t
{
    int (*callback) ( void * data, const next_address_t * client_address, const uint8_t * payload_data, int payload_bytes );
    void * callback_data;
};

// ---------------------------------------------------------------

#define NEXT_SERVER_NOTIFY_PACKET_RECEIVED                      0
#define NEXT_SERVER_NOTIFY_PENDING_SESSION_TIMED_OUT            1
#define NEXT_SERVER_NOTIFY_SESSION_UPGRADED                     2
#define NEXT_SERVER_NOTIFY_SESSION_TIMED_OUT                    3
#define NEXT_SERVER_NOTIFY_INIT_TIMED_OUT                       4
#define NEXT_SERVER_NOTIFY_READY                                5
#define NEXT_SERVER_NOTIFY_FLUSH_FINISHED                       6
#define NEXT_SERVER_NOTIFY_MAGIC_UPDATED                        7
#define NEXT_SERVER_NOTIFY_DIRECT_ONLY                          8

struct next_server_notify_t
{
    int type;
};

struct next_server_notify_packet_received_t : public next_server_notify_t
{
    next_address_t from;
    int packet_bytes;
    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];
};

struct next_server_notify_pending_session_cancelled_t : public next_server_notify_t
{
    next_address_t address;
    uint64_t session_id;
};

struct next_server_notify_pending_session_timed_out_t : public next_server_notify_t
{
    next_address_t address;
    uint64_t session_id;
};

struct next_server_notify_session_upgraded_t : public next_server_notify_t
{
    next_address_t address;
    uint64_t session_id;
};

struct next_server_notify_session_timed_out_t : public next_server_notify_t
{
    next_address_t address;
    uint64_t session_id;
};

struct next_server_notify_init_timed_out_t : public next_server_notify_t
{
    // ...
};

struct next_server_notify_ready_t : public next_server_notify_t
{
    char datacenter_name[NEXT_MAX_DATACENTER_NAME_LENGTH];
};

struct next_server_notify_flush_finished_t : public next_server_notify_t
{
    // ...
};

struct next_server_notify_magic_updated_t : public next_server_notify_t
{
    uint8_t current_magic[8];
};

struct next_server_notify_direct_only_t : public next_server_notify_t
{
    // ...
};

// ---------------------------------------------------------------

struct next_server_internal_t;

void next_server_internal_initialize_sentinels( next_server_internal_t * server );

void next_server_internal_verify_sentinels( next_server_internal_t * server );

void next_server_internal_resolve_hostname( next_server_internal_t * server );

void next_server_internal_autodetect( next_server_internal_t * server );

void next_server_internal_initialize( next_server_internal_t * server );

void next_server_internal_destroy( next_server_internal_t * server );

next_server_internal_t * next_server_internal_create( void * context, const char * server_address_string, const char * bind_address_string, const char * datacenter_string );

void next_server_internal_destroy( next_server_internal_t * server );

void next_server_internal_quit( next_server_internal_t * server );

void next_server_internal_send_packet_to_address( next_server_internal_t * server, const next_address_t * address, const uint8_t * packet_data, int packet_bytes );

void next_server_internal_send_packet_to_backend( next_server_internal_t * server, const uint8_t * packet_data, int packet_bytes );

int next_server_internal_send_packet( next_server_internal_t * server, const next_address_t * to_address, uint8_t packet_id, void * packet_object );

next_session_entry_t * next_server_internal_process_client_to_server_packet( next_server_internal_t * server, uint8_t packet_type, uint8_t * packet_data, int packet_bytes );

void next_server_internal_update_route( next_server_internal_t * server );

void next_server_internal_update_pending_upgrades( next_server_internal_t * server );

void next_server_internal_update_sessions( next_server_internal_t * server );

void next_server_internal_update_flush( next_server_internal_t * server );

void next_server_internal_process_network_next_packet( next_server_internal_t * server, const next_address_t * from, uint8_t * packet_data, int begin, int end );

void next_server_internal_process_passthrough_packet( next_server_internal_t * server, const next_address_t * from, uint8_t * packet_data, int packet_bytes );

void next_server_internal_block_and_receive_packet( next_server_internal_t * server );

void next_server_internal_upgrade_session( next_server_internal_t * server, const next_address_t * address, uint64_t session_id, uint64_t user_hash );

void next_server_internal_session_events( next_server_internal_t * server, const next_address_t * address, uint64_t session_events );

void next_server_internal_flush_session_update( next_server_internal_t * server );

void next_server_internal_flush( next_server_internal_t * server );

void next_server_internal_pump_commands( next_server_internal_t * server );

void next_server_internal_update_init( next_server_internal_t * server );

void next_server_internal_backend_update( next_server_internal_t * server );

// ---------------------------------------------------------------

extern next_internal_config_t next_global_config;

extern int next_signed_packets[256];

extern int next_encrypted_packets[256];

// ---------------------------------------------------------------

struct next_server_internal_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    int state;
    uint64_t buyer_id;
    uint64_t datacenter_id;
    uint64_t start_time;
    char datacenter_name[NEXT_MAX_DATACENTER_NAME_LENGTH];
    char autodetect_input[NEXT_MAX_DATACENTER_NAME_LENGTH];
    char autodetect_datacenter[NEXT_MAX_DATACENTER_NAME_LENGTH];
    bool autodetected_datacenter;

    NEXT_DECLARE_SENTINEL(1)

    uint8_t buyer_private_key[NEXT_CRYPTO_SIGN_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(2)

    bool valid_buyer_private_key;
    bool no_datacenter_specified;
    uint64_t upgrade_sequence;
    double next_resolve_hostname_time;
    next_address_t backend_address;
    next_address_t server_address;
    next_address_t bind_address;
    next_queue_t * command_queue;
    next_queue_t * notify_queue;
    next_platform_mutex_t session_mutex;
    next_platform_mutex_t command_mutex;
    next_platform_mutex_t notify_mutex;
    next_platform_socket_t * socket;
    next_pending_session_manager_t * pending_session_manager;
    next_session_manager_t * session_manager;

    NEXT_DECLARE_SENTINEL(3)

    bool resolving_hostname;
    bool resolve_hostname_finished;
    double resolve_hostname_start_time;
    next_address_t resolve_hostname_result;
    next_platform_mutex_t resolve_hostname_mutex;
    next_platform_thread_t * resolve_hostname_thread;

    NEXT_DECLARE_SENTINEL(4)

    bool autodetecting;
    bool autodetect_finished;
    bool autodetect_actually_did_something;
    bool autodetect_succeeded;
    double autodetect_start_time;
    char autodetect_result[NEXT_MAX_DATACENTER_NAME_LENGTH];
    next_platform_mutex_t autodetect_mutex;
    next_platform_thread_t * autodetect_thread;

    NEXT_DECLARE_SENTINEL(5)

    uint8_t server_kx_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];
    uint8_t server_kx_private_key[NEXT_CRYPTO_KX_SECRETKEYBYTES];
    uint8_t server_route_public_key[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];
    uint8_t server_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(6)

    uint8_t upcoming_magic[8];
    uint8_t current_magic[8];
    uint8_t previous_magic[8];

    NEXT_DECLARE_SENTINEL(7)

    uint64_t server_init_request_id;
    double server_init_resend_time;
    double server_init_timeout_time;
    bool received_init_response;

    NEXT_DECLARE_SENTINEL(8)

    uint64_t server_update_request_id;
    double server_update_last_time;
    double server_update_resend_time;
    int server_update_num_sessions;
    bool server_update_first;

    NEXT_DECLARE_SENTINEL(9)

    std::atomic<uint64_t> quit;

    NEXT_DECLARE_SENTINEL(10)

    bool flushing;
    bool flushed;
    uint64_t num_session_updates_to_flush;
    uint64_t num_flushed_session_updates;

    NEXT_DECLARE_SENTINEL(11)

    void (*packet_receive_callback) ( void * data, next_address_t * from, uint8_t * packet_data, int * begin, int * end );
    void * packet_receive_callback_data;

    int (*send_packet_to_address_callback)( void * data, const next_address_t * address, const uint8_t * packet_data, int packet_bytes );
    void * send_packet_to_address_callback_data;

    int (*payload_receive_callback)( void * data, const next_address_t * client_address, const uint8_t * payload_data, int payload_bytes );
    void * payload_receive_callback_data;

    NEXT_DECLARE_SENTINEL(12)
};

void next_server_internal_initialize_sentinels( next_server_internal_t * server )
{
    (void) server;
    next_assert( server );
    NEXT_INITIALIZE_SENTINEL( server, 0 )
    NEXT_INITIALIZE_SENTINEL( server, 1 )
    NEXT_INITIALIZE_SENTINEL( server, 2 )
    NEXT_INITIALIZE_SENTINEL( server, 3 )
    NEXT_INITIALIZE_SENTINEL( server, 4 )
    NEXT_INITIALIZE_SENTINEL( server, 5 )
    NEXT_INITIALIZE_SENTINEL( server, 6 )
    NEXT_INITIALIZE_SENTINEL( server, 7 )
    NEXT_INITIALIZE_SENTINEL( server, 8 )
    NEXT_INITIALIZE_SENTINEL( server, 9 )
    NEXT_INITIALIZE_SENTINEL( server, 10 )
    NEXT_INITIALIZE_SENTINEL( server, 11 )
    NEXT_INITIALIZE_SENTINEL( server, 12 )
}

void next_server_internal_verify_sentinels( next_server_internal_t * server )
{
    (void) server;
    next_assert( server );
    NEXT_VERIFY_SENTINEL( server, 0 )
    NEXT_VERIFY_SENTINEL( server, 1 )
    NEXT_VERIFY_SENTINEL( server, 2 )
    NEXT_VERIFY_SENTINEL( server, 3 )
    NEXT_VERIFY_SENTINEL( server, 4 )
    NEXT_VERIFY_SENTINEL( server, 5 )
    NEXT_VERIFY_SENTINEL( server, 6 )
    NEXT_VERIFY_SENTINEL( server, 7 )
    NEXT_VERIFY_SENTINEL( server, 8 )
    NEXT_VERIFY_SENTINEL( server, 9 )
    NEXT_VERIFY_SENTINEL( server, 10 )
    NEXT_VERIFY_SENTINEL( server, 11 )
    NEXT_VERIFY_SENTINEL( server, 12 )
    if ( server->session_manager )
        next_session_manager_verify_sentinels( server->session_manager );
    if ( server->pending_session_manager )
        next_pending_session_manager_verify_sentinels( server->pending_session_manager );
}

static void next_server_internal_resolve_hostname_thread_function( void * context );

static void next_server_internal_autodetect_thread_function( void * context );

void next_server_internal_resolve_hostname( next_server_internal_t * server )
{
    if ( server->resolving_hostname )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server is already resolving hostname" );
        return;
    }

    server->resolve_hostname_start_time = next_platform_time();
    server->resolving_hostname = true;
    server->resolve_hostname_finished = false;

    server->resolve_hostname_thread = next_platform_thread_create( server->context, next_server_internal_resolve_hostname_thread_function, server );
    if ( !server->resolve_hostname_thread )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create resolve hostname thread" );
        return;
    }
}

void next_server_internal_autodetect( next_server_internal_t * server )
{
    if ( server->autodetecting )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server is already autodetecting" );
        return;
    }

    server->autodetect_start_time = next_platform_time();
    server->autodetecting = true;

    server->autodetect_thread = next_platform_thread_create( server->context, next_server_internal_autodetect_thread_function, server );
    if ( !server->autodetect_thread )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create autodetect thread" );
        return;
    }
}

void next_server_internal_initialize( next_server_internal_t * server )
{
    if ( server->state != NEXT_SERVER_STATE_INITIALIZED )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server initializing with backend" );

        server->state = NEXT_SERVER_STATE_INITIALIZING;
        server->server_init_timeout_time = next_platform_time() + NEXT_SERVER_INIT_TIMEOUT;
    }
    
    next_server_internal_resolve_hostname( server );

    next_server_internal_autodetect( server );
}

static void next_server_internal_thread_function( void * context );

next_server_internal_t * next_server_internal_create( void * context, const char * server_address_string, const char * bind_address_string, const char * datacenter_string )
{
#if !NEXT_DEVELOPMENT
    next_printf( NEXT_LOG_LEVEL_INFO, "server sdk version is %s", NEXT_VERSION_FULL );
#endif // #if !NEXT_DEVELOPMENT

    next_assert( server_address_string );
    next_assert( bind_address_string );
    next_assert( datacenter_string );

    const char * server_address_override = next_platform_getenv( "NEXT_SERVER_ADDRESS" );
    if ( server_address_override )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server address override: '%s'", server_address_override );
        server_address_string = server_address_override;
    }

    next_address_t server_address;
    if ( next_address_parse( &server_address, server_address_string ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to parse server address: '%s'", server_address_string );
        return NULL;
    }

    const char * bind_address_override = next_platform_getenv( "NEXT_BIND_ADDRESS" );
    if ( bind_address_override )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server bind address override: '%s'", bind_address_override );
        bind_address_string = bind_address_override;
    }

    next_address_t bind_address;
    if ( next_address_parse( &bind_address, bind_address_string ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to parse bind address: '%s'", bind_address_string );
        return NULL;
    }

    next_server_internal_t * server = (next_server_internal_t*) next_malloc( context, sizeof(next_server_internal_t) );
    if ( !server )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create internal server" );
        return NULL;
    }

    char * just_clear_it_and_dont_complain = (char*) server;
    memset( just_clear_it_and_dont_complain, 0, sizeof(next_server_internal_t) );

    next_server_internal_initialize_sentinels( server );

    next_server_internal_verify_sentinels( server );

    server->context = context;
    server->start_time = time( NULL );
    server->buyer_id = next_global_config.server_buyer_id;
    memcpy( server->buyer_private_key, next_global_config.buyer_private_key, NEXT_CRYPTO_SIGN_SECRETKEYBYTES );
    server->valid_buyer_private_key = next_global_config.valid_buyer_private_key;

    const char * datacenter = datacenter_string;

    const char * datacenter_env = next_platform_getenv( "NEXT_DATACENTER" );

    if ( datacenter_env )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server datacenter override '%s'", datacenter_env );
        datacenter = datacenter_env;
    }

    next_assert( datacenter );

    next_copy_string( server->autodetect_input, datacenter, NEXT_MAX_DATACENTER_NAME_LENGTH );

    const bool datacenter_is_empty_string = datacenter[0] == '\0';

    if ( !datacenter_is_empty_string )
    {
        server->datacenter_id = next_datacenter_id( datacenter );
        next_copy_string( server->datacenter_name, datacenter, NEXT_MAX_DATACENTER_NAME_LENGTH );
        next_printf( NEXT_LOG_LEVEL_INFO, "server input datacenter is '%s' [%" PRIx64 "]", server->datacenter_name, server->datacenter_id );
    }
    else
    {
        server->no_datacenter_specified = true;
    }

    server->command_queue = next_queue_create( context, NEXT_COMMAND_QUEUE_LENGTH );
    if ( !server->command_queue )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create command queue" );
        next_server_internal_destroy( server );
        return NULL;
    }

    server->notify_queue = next_queue_create( context, NEXT_NOTIFY_QUEUE_LENGTH );
    if ( !server->notify_queue )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create notify queue" );
        next_server_internal_destroy( server );
        return NULL;
    }

    server->socket = next_platform_socket_create( server->context, &bind_address, NEXT_PLATFORM_SOCKET_BLOCKING, 0.1f, next_global_config.socket_send_buffer_size, next_global_config.socket_receive_buffer_size, true );
    if ( server->socket == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create server socket" );
        next_server_internal_destroy( server );
        return NULL;
    }

    if ( server_address.port == 0 )
    {
        server_address.port = bind_address.port;
    }

    char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_printf( NEXT_LOG_LEVEL_INFO, "server bound to %s", next_address_to_string( &bind_address, address_string ) );

    server->bind_address = bind_address;
    server->server_address = server_address;

    int result = next_platform_mutex_create( &server->session_mutex );
    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create session mutex" );
        next_server_internal_destroy( server );
        return NULL;
    }

    result = next_platform_mutex_create( &server->command_mutex );

    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create command mutex" );
        next_server_internal_destroy( server );
        return NULL;
    }

    result = next_platform_mutex_create( &server->notify_mutex );

    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create notify mutex" );
        next_server_internal_destroy( server );
        return NULL;
    }

    result = next_platform_mutex_create( &server->resolve_hostname_mutex );

    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create resolve hostname mutex" );
        next_server_internal_destroy( server );
        return NULL;
    }

    result = next_platform_mutex_create( &server->autodetect_mutex );
    
    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create autodetect mutex" );
        next_server_internal_destroy( server );
        return NULL;
    }

    server->pending_session_manager = next_pending_session_manager_create( context, NEXT_INITIAL_PENDING_SESSION_SIZE );
    if ( server->pending_session_manager == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create pending session manager" );
        next_server_internal_destroy( server );
        return NULL;
    }

    server->session_manager = next_session_manager_create( context, NEXT_INITIAL_SESSION_SIZE );
    if ( server->session_manager == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create session manager" );
        next_server_internal_destroy( server );
        return NULL;
    }

    const bool datacenter_is_local = datacenter[0] == 'l' &&
                                     datacenter[1] == 'o' &&
                                     datacenter[2] == 'c' &&
                                     datacenter[3] == 'a' &&
                                     datacenter[4] == 'l' &&
                                     datacenter[5] == '\0';

    const char * hostname = next_global_config.server_backend_hostname;

    const bool backend_is_local = hostname[0] == '1' && 
                                  hostname[1] == '2' &&
                                  hostname[2] == '7' &&
                                  hostname[3] == '.' &&
                                  hostname[4] == '0' &&
                                  hostname[5] == '.' &&
                                  hostname[6] == '0' &&
                                  hostname[7] == '.' &&
                                  hostname[8] == '1';

    if ( next_global_config.disable_network_next )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "network next is disabled" );
    }

    if ( !server->valid_buyer_private_key && !datacenter_is_local )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "we don't have a valid buyer private key :(" );
    }

    if ( datacenter_is_local && backend_is_local )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "special local backend codepath" );
    }

    const bool should_initialize = !next_global_config.disable_network_next     && 
                                        server->valid_buyer_private_key         && 
                                   ( (datacenter_is_local && backend_is_local) || !datacenter_is_local );

    if ( should_initialize )
    {
        next_server_internal_initialize( server );
    }

    next_printf( NEXT_LOG_LEVEL_INFO, "server started on %s", next_address_to_string( &server_address, address_string ) );

    next_crypto_kx_keypair( server->server_kx_public_key, server->server_kx_private_key );

    next_crypto_box_keypair( server->server_route_public_key, server->server_route_private_key );

    server->server_update_last_time = next_platform_time() - NEXT_SECONDS_BETWEEN_SERVER_UPDATES * next_random_float();

    server->server_update_first = true;

    return server;
}

void next_server_internal_destroy( next_server_internal_t * server )
{
    next_assert( server );

    next_server_internal_verify_sentinels( server );

    if ( server->socket )
    {
        next_platform_socket_destroy( server->socket );
    }
    if ( server->resolve_hostname_thread )
    {
        next_platform_thread_destroy( server->resolve_hostname_thread );
    }
    if ( server->autodetect_thread )
    {
        next_platform_thread_destroy( server->autodetect_thread );
    }
    if ( server->command_queue )
    {
        next_queue_destroy( server->command_queue );
    }
    if ( server->notify_queue )
    {
        next_queue_destroy( server->notify_queue );
    }
    if ( server->session_manager )
    {
        next_session_manager_destroy( server->session_manager );
        server->session_manager = NULL;
    }
    if ( server->pending_session_manager )
    {
        next_pending_session_manager_destroy( server->pending_session_manager );
        server->pending_session_manager = NULL;
    }

    next_platform_mutex_destroy( &server->session_mutex );
    next_platform_mutex_destroy( &server->command_mutex );
    next_platform_mutex_destroy( &server->notify_mutex );
    next_platform_mutex_destroy( &server->resolve_hostname_mutex );
    next_platform_mutex_destroy( &server->autodetect_mutex );

    next_server_internal_verify_sentinels( server );

    next_clear_and_free( server->context, server, sizeof(next_server_internal_t) );
}

void next_server_internal_quit( next_server_internal_t * server )
{
    next_assert( server );
    server->quit = 1;
}

void next_server_internal_send_packet_to_address( next_server_internal_t * server, const next_address_t * address, const uint8_t * packet_data, int packet_bytes )
{
    next_server_internal_verify_sentinels( server );

    next_assert( address );
    next_assert( address->type != NEXT_ADDRESS_NONE );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    if ( server->send_packet_to_address_callback )
    {
        void * callback_data = server->send_packet_to_address_callback_data;
        if ( server->send_packet_to_address_callback( callback_data, address, packet_data, packet_bytes ) != 0 )
            return;
    }

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( server->socket, address, packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING
}

void next_server_internal_send_packet_to_backend( next_server_internal_t * server, const uint8_t * packet_data, int packet_bytes )
{
    next_server_internal_verify_sentinels( server );

    if ( server->backend_address.type == NEXT_ADDRESS_NONE )
        return;

    next_assert( server->backend_address.type != NEXT_ADDRESS_NONE );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( server->socket, &server->backend_address, packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING
}

int next_server_internal_send_packet( next_server_internal_t * server, const next_address_t * to_address, uint8_t packet_id, void * packet_object )
{
    next_assert( server );
    next_assert( server->socket );
    next_assert( packet_object );

    next_server_internal_verify_sentinels( server );

    int packet_bytes = 0;

    uint8_t buffer[NEXT_MAX_PACKET_BYTES];

    uint64_t * sequence = NULL;
    uint8_t * send_key = NULL;

    uint8_t magic[8];
    if ( packet_id != NEXT_UPGRADE_REQUEST_PACKET )
    {
        memcpy( magic, server->current_magic, 8 );
    }
    else
    {
        memset( magic, 0, sizeof(magic) );
    }

    if ( next_encrypted_packets[packet_id] )
    {
        next_session_entry_t * session = next_session_manager_find_by_address( server->session_manager, to_address );

        if ( !session )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "server can't send encrypted packet to address. no session found" );
            return NEXT_ERROR;
        }

        sequence = &session->internal_send_sequence;
        send_key = session->send_key;
    }

    uint8_t from_address_data[32];
    uint8_t to_address_data[32];
    int from_address_bytes = 0;
    int to_address_bytes = 0;

    next_address_data( &server->server_address, from_address_data, &from_address_bytes );

    // IMPORTANT: when the upgrade request packet is sent, the client doesn't know it's external address yet
    // so we must encode with a to address of zero bytes for the upgrade request packet
    if ( packet_id != NEXT_UPGRADE_REQUEST_PACKET )
    {
        next_address_data( to_address, to_address_data, &to_address_bytes );
    }

    if ( next_write_packet( packet_id, packet_object, buffer, &packet_bytes, next_signed_packets, next_encrypted_packets, sequence, server->buyer_private_key, send_key, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to write internal packet with id %d", packet_id );
        return NEXT_ERROR;
    }

    next_assert( packet_bytes > 0 );
    next_assert( next_basic_packet_filter( buffer, packet_bytes ) );
    next_assert( next_advanced_packet_filter( buffer, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

    next_server_internal_send_packet_to_address( server, to_address, buffer, packet_bytes );

    return NEXT_OK;
}

inline int next_sequence_greater_than( uint8_t s1, uint8_t s2 )
{
    return ( ( s1 > s2 ) && ( s1 - s2 <= 128 ) ) ||
           ( ( s1 < s2 ) && ( s2 - s1  > 128 ) );
}

next_session_entry_t * next_server_internal_process_client_to_server_packet( next_server_internal_t * server, uint8_t packet_type, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );

    next_server_internal_verify_sentinels( server );

    if ( packet_bytes <= NEXT_HEADER_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored client to server packet. packet is too small to be valid" );
        return NULL;
    }

    uint64_t packet_sequence = 0;
    uint64_t packet_session_id = 0;
    uint8_t packet_session_version = 0;

    next_peek_header( &packet_sequence, &packet_session_id, &packet_session_version, packet_data, packet_bytes );

    next_session_entry_t * entry = next_session_manager_find_by_session_id( server->session_manager, packet_session_id );
    if ( !entry )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored client to server packet. could not find session" );
        return NULL;
    }

    if ( !entry->has_pending_route && !entry->has_current_route && !entry->has_previous_route )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored client to server packet. session has no route" );
        return NULL;
    }

    next_assert( packet_type == NEXT_CLIENT_TO_SERVER_PACKET || packet_type == NEXT_PING_PACKET );

    next_replay_protection_t * replay_protection = ( packet_type == NEXT_CLIENT_TO_SERVER_PACKET ) ? &entry->payload_replay_protection : &entry->special_replay_protection;

    if ( next_replay_protection_already_received( replay_protection, packet_sequence ) )
        return NULL;

    if ( entry->has_pending_route && next_read_header( packet_type, &packet_sequence, &packet_session_id, &packet_session_version, entry->pending_route_private_key, packet_data, packet_bytes ) == NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server promoted pending route for session %" PRIx64, entry->session_id );

        if ( entry->has_current_route )
        {
            entry->has_previous_route = true;
            entry->previous_route_send_address = entry->current_route_send_address;
            memcpy( entry->previous_route_private_key, entry->current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
        }

        entry->has_pending_route = false;
        entry->has_current_route = true;
        entry->current_route_session_version = entry->pending_route_session_version;
        entry->current_route_expire_timestamp = entry->pending_route_expire_timestamp;
        entry->current_route_expire_time = entry->pending_route_expire_time;
        entry->current_route_kbps_up = entry->pending_route_kbps_up;
        entry->current_route_kbps_down = entry->pending_route_kbps_down;
        entry->current_route_send_address = entry->pending_route_send_address;
        memcpy( entry->current_route_private_key, entry->pending_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );

        {
            next_platform_mutex_guard( &server->session_mutex );
            entry->mutex_envelope_kbps_up = entry->current_route_kbps_up;
            entry->mutex_envelope_kbps_down = entry->current_route_kbps_down;
            entry->mutex_send_over_network_next = true;
            entry->mutex_session_id = entry->session_id;
            entry->mutex_session_version = entry->current_route_session_version;
            entry->mutex_send_address = entry->current_route_send_address;
            memcpy( entry->mutex_private_key, entry->current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
        }
    }
    else
    {
        bool current_route_ok = false;
        bool previous_route_ok = false;

        if ( entry->has_current_route )
            current_route_ok = next_read_header( packet_type, &packet_sequence, &packet_session_id, &packet_session_version, entry->current_route_private_key, packet_data, packet_bytes ) == NEXT_OK;

        if ( entry->has_previous_route )
            previous_route_ok = next_read_header( packet_type, &packet_sequence, &packet_session_id, &packet_session_version, entry->previous_route_private_key, packet_data, packet_bytes ) == NEXT_OK;

        if ( !current_route_ok && !previous_route_ok )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored client to server packet. did not verify" );
            return NULL;
        }
    }

    next_replay_protection_advance_sequence( replay_protection, packet_sequence );

    if ( packet_type == NEXT_CLIENT_TO_SERVER_PACKET )
    {
        next_packet_loss_tracker_packet_received( &entry->packet_loss_tracker, packet_sequence );
        next_out_of_order_tracker_packet_received( &entry->out_of_order_tracker, packet_sequence );
        next_jitter_tracker_packet_received( &entry->jitter_tracker, packet_sequence, next_platform_time() );
    }

    return entry;
}

void next_server_internal_update_route( next_server_internal_t * server )
{
    next_assert( server );

    next_server_internal_verify_sentinels( server );

    next_assert( !next_global_config.disable_network_next );

    if ( server->flushing )
        return;

    const double current_time = next_platform_time();

    const int max_index = server->session_manager->max_entry_index;

    for ( int i = 0; i <= max_index; ++i )
    {
        if ( server->session_manager->session_ids[i] == 0 )
            continue;

        next_session_entry_t * entry = &server->session_manager->entries[i];

        if ( entry->update_dirty && !entry->client_ping_timed_out && !entry->stats_fallback_to_direct && entry->update_last_send_time + NEXT_UPDATE_SEND_TIME <= current_time )
        {
            NextRouteUpdatePacket packet;
            memcpy( packet.upcoming_magic, server->upcoming_magic, 8 );
            memcpy( packet.current_magic, server->current_magic, 8 );
            memcpy( packet.previous_magic, server->previous_magic, 8 );
            packet.sequence = entry->update_sequence;
            packet.has_near_relays = entry->update_has_near_relays;
            if ( packet.has_near_relays )
            {
                packet.num_near_relays = entry->update_num_near_relays;
                memcpy( packet.near_relay_ids, entry->update_near_relay_ids, size_t(8) * entry->update_num_near_relays );
                memcpy( packet.near_relay_addresses, entry->update_near_relay_addresses, sizeof(next_address_t) * entry->update_num_near_relays );
                memcpy( packet.near_relay_ping_tokens, entry->update_near_relay_ping_tokens, NEXT_MAX_NEAR_RELAYS * NEXT_PING_TOKEN_BYTES );
                packet.near_relay_expire_timestamp = entry->update_near_relay_expire_timestamp;
            }
            packet.update_type = entry->update_type;
            packet.multipath = entry->multipath;
            packet.num_tokens = entry->update_num_tokens;
            if ( entry->update_type == NEXT_UPDATE_TYPE_ROUTE )
            {
                memcpy( packet.tokens, entry->update_tokens, NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES * size_t(entry->update_num_tokens) );
            }
            else if ( entry->update_type == NEXT_UPDATE_TYPE_CONTINUE )
            {
                memcpy( packet.tokens, entry->update_tokens, NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES * size_t(entry->update_num_tokens) );
            }
            packet.packets_lost_client_to_server = entry->stats_packets_lost_client_to_server;
            packet.packets_out_of_order_client_to_server = entry->stats_packets_out_of_order_client_to_server;
            packet.jitter_client_to_server = float( entry->stats_jitter_client_to_server );

            {
                next_platform_mutex_guard( &server->session_mutex );
                packet.packets_sent_server_to_client = entry->stats_packets_sent_server_to_client;
            }

            packet.has_debug = entry->has_debug;
            memcpy( packet.debug, entry->debug, NEXT_MAX_SESSION_DEBUG );

            next_server_internal_send_packet( server, &entry->address, NEXT_ROUTE_UPDATE_PACKET, &packet );

            entry->update_last_send_time = current_time;

            next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent route update packet to session %" PRIx64, entry->session_id );
        }
    }
}

void next_server_internal_update_pending_upgrades( next_server_internal_t * server )
{
    next_assert( server );

    next_server_internal_verify_sentinels( server );

    next_assert( !next_global_config.disable_network_next );

    if ( server->flushing )
        return;

    if ( server->state == NEXT_SERVER_STATE_DIRECT_ONLY )
        return;

    const double current_time = next_platform_time();

    const double packet_resend_time = 0.25;

    const int max_index = server->pending_session_manager->max_entry_index;

    for ( int i = 0; i <= max_index; ++i )
    {
        if ( server->pending_session_manager->addresses[i].type == NEXT_ADDRESS_NONE )
            continue;

        next_pending_session_entry_t * entry = &server->pending_session_manager->entries[i];

        if ( entry->upgrade_time + NEXT_UPGRADE_TIMEOUT <= current_time )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server upgrade request timed out for client %s", next_address_to_string( &entry->address, address_buffer ) );
            next_pending_session_manager_remove_at_index( server->pending_session_manager, i );
            next_server_notify_pending_session_timed_out_t * notify = (next_server_notify_pending_session_timed_out_t*) next_malloc( server->context, sizeof( next_server_notify_pending_session_timed_out_t ) );
            notify->type = NEXT_SERVER_NOTIFY_PENDING_SESSION_TIMED_OUT;
            notify->address = entry->address;
            notify->session_id = entry->session_id;
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_PENDING_SESSION_TIMED_OUT at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING                
                next_platform_mutex_guard( &server->notify_mutex );
                next_queue_push( server->notify_queue, notify );
            }
            continue;
        }

        if ( entry->last_packet_send_time + packet_resend_time <= current_time )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent upgrade request packet to client %s", next_address_to_string( &entry->address, address_buffer ) );

            entry->last_packet_send_time = current_time;

            NextUpgradeRequestPacket packet;
            packet.protocol_version = next_protocol_version();
            packet.session_id = entry->session_id;
            packet.client_address = entry->address;
            packet.server_address = server->server_address;
            memcpy( packet.server_kx_public_key, server->server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
            memcpy( packet.upgrade_token, entry->upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
            memcpy( packet.upcoming_magic, server->upcoming_magic, 8 );
            memcpy( packet.current_magic, server->current_magic, 8 );
            memcpy( packet.previous_magic, server->previous_magic, 8 );

            next_server_internal_send_packet( server, &entry->address, NEXT_UPGRADE_REQUEST_PACKET, &packet );
        }
    }
}

void next_server_internal_update_sessions( next_server_internal_t * server )
{
    next_assert( server );

    next_server_internal_verify_sentinels( server );

    next_assert( !next_global_config.disable_network_next );

    if ( server->state == NEXT_SERVER_STATE_DIRECT_ONLY )
        return;

    const double current_time = next_platform_time();

    int index = 0;

    while ( index <= server->session_manager->max_entry_index )
    {
        if ( server->session_manager->session_ids[index] == 0 )
        {
            ++index;
            continue;
        }

        next_session_entry_t * entry = &server->session_manager->entries[index];

        // detect client ping timeout. this is not an error condition, it's just the client ending the session

        if ( !entry->client_ping_timed_out &&
             entry->last_client_direct_ping + NEXT_SERVER_PING_TIMEOUT <= current_time &&
             entry->last_client_next_ping + NEXT_SERVER_PING_TIMEOUT <= current_time )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server client ping timed out for session %" PRIx64, entry->session_id );
            entry->client_ping_timed_out = true;
        }

        // IMPORTANT: Don't time out sessions during server flush. Otherwise the server flush might wait longer than necessary.
        if ( !server->flushing && entry->last_client_stats_update + NEXT_SERVER_SESSION_TIMEOUT <= current_time )
        {
            next_server_notify_session_timed_out_t * notify = (next_server_notify_session_timed_out_t*) next_malloc( server->context, sizeof( next_server_notify_session_timed_out_t ) );
            notify->type = NEXT_SERVER_NOTIFY_SESSION_TIMED_OUT;
            notify->address = entry->address;
            notify->session_id = entry->session_id;
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_SESSION_TIMED_OUT at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING                
                next_platform_mutex_guard( &server->notify_mutex );
                next_queue_push( server->notify_queue, notify );
            }

            {
                next_platform_mutex_guard( &server->session_mutex );
                next_session_manager_remove_at_index( server->session_manager, index );
            }
    
            continue;
        }

        if ( entry->has_current_route && entry->current_route_expire_time <= current_time )
        {
            // IMPORTANT: Only print this out as an error if it occurs *before* the client ping times out
            // otherwise we get red herring errors on regular client disconnect from server that make it
            // look like something is wrong when everything is fine...
            if ( !entry->client_ping_timed_out )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "server network next route expired for session %" PRIx64, entry->session_id );
            }

            entry->has_current_route = false;
            entry->has_previous_route = false;
            entry->update_dirty = false;
            entry->waiting_for_update_response = false;

            {
                next_platform_mutex_guard( &server->session_mutex );
                entry->mutex_send_over_network_next = false;
            }
        }

        index++;
    }
}

void next_server_internal_update_flush( next_server_internal_t * server )
{
    next_assert( !next_global_config.disable_network_next );

    if ( !server->flushing )
        return;

    if ( server->flushed )
        return;

    if ( next_global_config.disable_network_next || server->state != NEXT_SERVER_STATE_INITIALIZED || 
         ( server->num_flushed_session_updates == server->num_session_updates_to_flush ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server internal flush completed" );
        
        server->flushed = true;

        next_server_notify_flush_finished_t * notify = (next_server_notify_flush_finished_t*) next_malloc( server->context, sizeof( next_server_notify_flush_finished_t ) );
        notify->type = NEXT_SERVER_NOTIFY_FLUSH_FINISHED;
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_FLUSH_FINISHED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING                
            next_platform_mutex_guard( &server->notify_mutex );
            next_queue_push( server->notify_queue, notify );
        }
    }
}

void next_server_internal_process_network_next_packet( next_server_internal_t * server, const next_address_t * from, uint8_t * packet_data, int begin, int end )
{
    next_assert( server );
    next_assert( from );
    next_assert( packet_data );
    next_assert( begin >= 0 );
    next_assert( end <= NEXT_MAX_PACKET_BYTES );

    next_server_internal_verify_sentinels( server );

    if ( next_global_config.disable_network_next )
        return;

    const int packet_id = packet_data[begin];

#if NEXT_ASSERTS
    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_printf( NEXT_LOG_LEVEL_SPAM, "server received packet type %d from %s (%d bytes)", packet_id, next_address_to_string( from, address_buffer ), packet_bytes );
#endif // #if NEXT_ASSERTS

    // run packet filters
    {
        if ( !next_basic_packet_filter( packet_data + begin, end - begin ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server basic packet filter dropped packet" );
            return;
        }

        uint8_t from_address_data[32];
        uint8_t to_address_data[32];
        int from_address_bytes;
        int to_address_bytes;

        next_address_data( from, from_address_data, &from_address_bytes );
        next_address_data( &server->server_address, to_address_data, &to_address_bytes );

        if ( packet_id != NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET &&
             packet_id != NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET &&
             packet_id != NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET &&
             packet_id != NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET &&
             packet_id != NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET )
        {
            if ( !next_advanced_packet_filter( packet_data + begin, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, end - begin ) )
            {
                if ( !next_advanced_packet_filter( packet_data + begin, server->upcoming_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, end - begin ) )
                {
                    if ( !next_advanced_packet_filter( packet_data + begin, server->previous_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, end - begin ) )
                    {
                        next_printf( NEXT_LOG_LEVEL_DEBUG, "server advanced packet filter dropped packet" );
                        return;
                    }
                }
            }
        }
        else
        {
            uint8_t magic[8];
            memset( magic, 0, sizeof(magic) );
            if ( !next_advanced_packet_filter( packet_data + begin, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, end - begin ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server advanced packet filter dropped packet (backend)" );
                return;
            }
        }
    }

    begin += 16;
    end -= 2;

    if ( server->state == NEXT_SERVER_STATE_INITIALIZING )
    {
        // server init response

        if ( packet_id == NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET )
        {
            if ( server->state != NEXT_SERVER_STATE_INITIALIZING )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored init response packet from backend. server is not initializing" );
                return;
            }

            NextBackendServerInitResponsePacket packet;

            if ( next_read_backend_packet( packet_id, packet_data, begin, end, &packet, next_signed_packets, next_server_backend_public_key ) != packet_id )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored server init response packet from backend. packet failed to read" );
                return;
            }

            if ( packet.request_id != server->server_init_request_id )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored server init response packet from backend. request id mismatch (got %" PRIx64 ", expected %" PRIx64 ")", packet.request_id, server->server_init_request_id );
                return;
            }

            next_printf( NEXT_LOG_LEVEL_INFO, "server received init response from backend" );

            if ( packet.response != NEXT_SERVER_INIT_RESPONSE_OK )
            {
                switch ( packet.response )
                {
                    case NEXT_SERVER_INIT_RESPONSE_UNKNOWN_BUYER:
                        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to initialize with backend. unknown buyer" );
                        return;

                    case NEXT_SERVER_INIT_RESPONSE_UNKNOWN_DATACENTER:
                        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to initialize with backend. unknown datacenter" );
                        return;

                    case NEXT_SERVER_INIT_RESPONSE_SDK_VERSION_TOO_OLD:
                        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to initialize with backend. sdk version too old" );
                        return;

                    case NEXT_SERVER_INIT_RESPONSE_SIGNATURE_CHECK_FAILED:
                        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to initialize with backend. signature check failed" );
                        return;

                    case NEXT_SERVER_INIT_RESPONSE_BUYER_NOT_ACTIVE:
                        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to initialize with backend. buyer not active" );
                        return;

                    case NEXT_SERVER_INIT_RESPONSE_DATACENTER_NOT_ENABLED:
                        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to initialize with backend. datacenter not enabled" );
                        return;

                    default:
                        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to initialize with backend" );
                        return;
                }
            }

            next_printf( NEXT_LOG_LEVEL_INFO, "welcome to network next :)" );

            server->received_init_response = true;

            memcpy( server->upcoming_magic, packet.upcoming_magic, 8 );
            memcpy( server->current_magic, packet.current_magic, 8 );
            memcpy( server->previous_magic, packet.previous_magic, 8 );

            next_printf( NEXT_LOG_LEVEL_DEBUG, "server initial magic: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x | %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x | %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
                packet.upcoming_magic[0],
                packet.upcoming_magic[1],
                packet.upcoming_magic[2],
                packet.upcoming_magic[3],
                packet.upcoming_magic[4],
                packet.upcoming_magic[5],
                packet.upcoming_magic[6],
                packet.upcoming_magic[7],
                packet.current_magic[0],
                packet.current_magic[1],
                packet.current_magic[2],
                packet.current_magic[3],
                packet.current_magic[4],
                packet.current_magic[5],
                packet.current_magic[6],
                packet.current_magic[7],
                packet.previous_magic[0],
                packet.previous_magic[1],
                packet.previous_magic[2],
                packet.previous_magic[3],
                packet.previous_magic[4],
                packet.previous_magic[5],
                packet.previous_magic[6],
                packet.previous_magic[7] );

            next_server_notify_magic_updated_t * notify = (next_server_notify_magic_updated_t*) next_malloc( server->context, sizeof( next_server_notify_magic_updated_t ) );
            notify->type = NEXT_SERVER_NOTIFY_MAGIC_UPDATED;
            memcpy( notify->current_magic, server->current_magic, 8 );
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_MAGIC_UPDATED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING                
                next_platform_mutex_guard( &server->notify_mutex );
                next_queue_push( server->notify_queue, notify );
            }

            return;
        }
    }

    // don't process network next packets until the server is initialized

    if ( server->state != NEXT_SERVER_STATE_INITIALIZED )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored network next packet because it is not initialized" );
        return;
    }

    // direct packet

    if ( packet_id == NEXT_DIRECT_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing direct packet" );

        const int packet_bytes = end - begin;

        if ( packet_bytes <= 9 )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored direct packet from %s. packet is too small to be valid", next_address_to_string( from, address_buffer ) );
            return;
        }

        if ( packet_bytes > NEXT_MTU + 9 )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored direct packet from %s. packet is too large to be valid", next_address_to_string( from, address_buffer ) );
            return;
        }

        const uint8_t * p = packet_data + begin;

        uint8_t packet_session_sequence = next_read_uint8( &p );

        uint64_t packet_sequence = next_read_uint64( &p );

        next_session_entry_t * entry = next_session_manager_find_by_address( server->session_manager, from );
        if ( !entry )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored direct packet from %s. could not find session for address", next_address_to_string( from, address_buffer ) );
            return;
        }

        if ( packet_session_sequence != entry->client_open_session_sequence )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored direct packet from %s. session mismatch", next_address_to_string( from, address_buffer ) );
            return;
        }

        if ( next_replay_protection_already_received( &entry->payload_replay_protection, packet_sequence ) )
            return;

        next_replay_protection_advance_sequence( &entry->payload_replay_protection, packet_sequence );

        next_packet_loss_tracker_packet_received( &entry->packet_loss_tracker, packet_sequence );

        next_out_of_order_tracker_packet_received( &entry->out_of_order_tracker, packet_sequence );

        next_jitter_tracker_packet_received( &entry->jitter_tracker, packet_sequence, next_platform_time() );

        next_server_notify_packet_received_t * notify = (next_server_notify_packet_received_t*) next_malloc( server->context, sizeof( next_server_notify_packet_received_t ) );
        notify->type = NEXT_SERVER_NOTIFY_PACKET_RECEIVED;
        notify->from = *from;
        notify->packet_bytes = packet_bytes - 9;
        next_assert( notify->packet_bytes > 0 );
        next_assert( notify->packet_bytes <= NEXT_MTU );
        memcpy( notify->packet_data, packet_data + begin + 9, size_t(notify->packet_bytes) );
        {
#if NEXT_SPIKE_TRACKING
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_PACKET_RECEIVED at %s:%d - from = %s, packet_bytes = %d", __FILE__, __LINE__, next_address_to_string( &notify->from, address_buffer ), notify->packet_bytes );
#endif // #if NEXT_SPIKE_TRACKING                
            next_platform_mutex_guard( &server->notify_mutex );
            next_queue_push( server->notify_queue, notify );
        }

        return;
    }

    // backend server response

    if ( packet_id == NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing server update response packet" );

        NextBackendServerUpdateResponsePacket packet;

        if ( next_read_backend_packet( packet_id, packet_data, begin, end, &packet, next_signed_packets, next_server_backend_public_key ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored server update response packet from backend. packet failed to read" );
            return;
        }

        if ( packet.request_id != server->server_update_request_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored server update response packet from backend. request id does not match" );
            return;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server received server update response packet from backend" );

        server->server_update_request_id = 0;
        server->server_update_resend_time = 0.0;

        if ( memcmp( packet.upcoming_magic, server->upcoming_magic, 8 ) != 0 )
        {
            memcpy( server->upcoming_magic, packet.upcoming_magic, 8 );
            memcpy( server->current_magic, packet.current_magic, 8 );
            memcpy( server->previous_magic, packet.previous_magic, 8 );

            next_printf( NEXT_LOG_LEVEL_DEBUG, "server updated magic: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x | %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x | %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
                packet.upcoming_magic[0],
                packet.upcoming_magic[1],
                packet.upcoming_magic[2],
                packet.upcoming_magic[3],
                packet.upcoming_magic[4],
                packet.upcoming_magic[5],
                packet.upcoming_magic[6],
                packet.upcoming_magic[7],
                packet.current_magic[0],
                packet.current_magic[1],
                packet.current_magic[2],
                packet.current_magic[3],
                packet.current_magic[4],
                packet.current_magic[5],
                packet.current_magic[6],
                packet.current_magic[7],
                packet.previous_magic[0],
                packet.previous_magic[1],
                packet.previous_magic[2],
                packet.previous_magic[3],
                packet.previous_magic[4],
                packet.previous_magic[5],
                packet.previous_magic[6],
                packet.previous_magic[7] );

            next_server_notify_magic_updated_t * notify = (next_server_notify_magic_updated_t*) next_malloc( server->context, sizeof( next_server_notify_magic_updated_t ) );
            notify->type = NEXT_SERVER_NOTIFY_MAGIC_UPDATED;
            memcpy( notify->current_magic, server->current_magic, 8 );
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_MAGIC_UPDATED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING                
                next_platform_mutex_guard( &server->notify_mutex );
                next_queue_push( server->notify_queue, notify );
            }
        }
    }

    // backend session response

    if ( packet_id == NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing session update response packet" );

        NextBackendSessionUpdateResponsePacket packet;

        if ( next_read_backend_packet( packet_id, packet_data, begin, end, &packet, next_signed_packets, next_server_backend_public_key ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored session update response packet from backend. packet failed to read" );
            return;
        }

        next_session_entry_t * entry = next_session_manager_find_by_session_id( server->session_manager, packet.session_id );
        if ( !entry )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored session update response packet from backend. could not find session %" PRIx64, packet.session_id );
            return;
        }

        if ( !entry->waiting_for_update_response )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored session update response packet from backend. not waiting for session response" );
            return;
        }

        if ( packet.slice_number != entry->update_sequence - 1 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored session update response packet from backend. wrong sequence number" );
            return;
        }

        const char * update_type = "???";

        switch ( packet.response_type )
        {
            case NEXT_UPDATE_TYPE_DIRECT:    update_type = "direct route";     break;
            case NEXT_UPDATE_TYPE_ROUTE:     update_type = "next route";       break;
            case NEXT_UPDATE_TYPE_CONTINUE:  update_type = "continue route";   break;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server received session update response from backend for session %" PRIx64 " (%s)", entry->session_id, update_type );

        bool multipath = packet.multipath;

        if ( multipath && !entry->multipath )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server multipath enabled for session %" PRIx64, entry->session_id );
            entry->multipath = true;
            {
                next_platform_mutex_guard( &server->session_mutex );
                entry->mutex_multipath = true;
            }
        }

        entry->update_dirty = true;

        entry->update_type = (uint8_t) packet.response_type;

        entry->update_num_tokens = packet.num_tokens;

        if ( packet.response_type == NEXT_UPDATE_TYPE_ROUTE )
        {
            memcpy( entry->update_tokens, packet.tokens, NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES * size_t(packet.num_tokens) );
        }
        else if ( packet.response_type == NEXT_UPDATE_TYPE_CONTINUE )
        {
            memcpy( entry->update_tokens, packet.tokens, NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES * size_t(packet.num_tokens) );
        }

        entry->update_has_near_relays = packet.has_near_relays;
        if ( packet.has_near_relays )
        {
            entry->update_num_near_relays = packet.num_near_relays;
            memcpy( entry->update_near_relay_ids, packet.near_relay_ids, 8 * size_t(packet.num_near_relays) );
            memcpy( entry->update_near_relay_addresses, packet.near_relay_addresses, sizeof(next_address_t) * size_t(packet.num_near_relays) );
            memcpy( entry->update_near_relay_ping_tokens, packet.near_relay_ping_tokens, packet.num_near_relays * NEXT_PING_TOKEN_BYTES );
            entry->update_near_relay_expire_timestamp = packet.near_relay_expire_timestamp;
        }

        entry->update_last_send_time = -1000.0;

        entry->session_data_bytes = packet.session_data_bytes;
        memcpy( entry->session_data, packet.session_data, packet.session_data_bytes );
        memcpy( entry->session_data_signature, packet.session_data_signature, NEXT_CRYPTO_SIGN_BYTES );

        entry->waiting_for_update_response = false;

        if ( packet.response_type == NEXT_UPDATE_TYPE_DIRECT )
        {
            bool session_transitions_to_direct = false;
            {
                next_platform_mutex_guard( &server->session_mutex );
                if ( entry->mutex_send_over_network_next )
                {
                    entry->mutex_send_over_network_next = false;
                    session_transitions_to_direct = true;
                }
            }

            if ( session_transitions_to_direct )
            {
                entry->has_previous_route = entry->has_current_route;
                entry->has_current_route = false;
                entry->previous_route_send_address = entry->current_route_send_address;
                memcpy( entry->previous_route_private_key, entry->current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
            }
        }

        entry->has_debug = packet.has_debug;
        memcpy( entry->debug, packet.debug, NEXT_MAX_SESSION_DEBUG );

        if ( entry->previous_session_events != 0 )
        {   
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server flushed session events %x to backend for session %" PRIx64 " at address %s", entry->previous_session_events, entry->session_id, next_address_to_string( from, address_buffer ));
            entry->previous_session_events = 0;
        }

        if ( entry->session_update_flush && entry->session_update_request_packet.client_ping_timed_out && packet.slice_number == entry->session_flush_update_sequence - 1 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server flushed session update for session %" PRIx64 " to backend", entry->session_id );
            entry->session_update_flush_finished = true;
            server->num_flushed_session_updates++;
        }

        return;
    }

    // upgrade response packet

    if ( packet_id == NEXT_UPGRADE_RESPONSE_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing upgrade response packet" );

        NextUpgradeResponsePacket packet;

        if ( next_read_packet( NEXT_UPGRADE_RESPONSE_PACKET, packet_data, begin, end, &packet, next_signed_packets, NULL, NULL, NULL, NULL, NULL ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response packet. did not read" );
            return;
        }

        NextUpgradeToken upgrade_token;

        // does the session already exist? if so we still need to reply with upgrade commit in case of server -> client packet loss

        bool upgraded = false;

        next_session_entry_t * existing_entry = next_session_manager_find_by_address( server->session_manager, from );

        if ( existing_entry )
        {
            if ( !upgrade_token.Read( packet.upgrade_token, existing_entry->ephemeral_private_key ) )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response from %s. could not decrypt upgrade token (existing entry)", next_address_to_string( from, address_buffer ) );
                return;
            }

            if ( upgrade_token.session_id != existing_entry->session_id )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response from %s. session id does not match existing entry", next_address_to_string( from, address_buffer ) );
                return;
            }

            if ( !next_address_equal( &upgrade_token.client_address, &existing_entry->address ) )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response from %s. client address does not match existing entry", next_address_to_string( from, address_buffer ) );
                return;
            }
        }
        else
        {
            // session does not exist yet. look up pending upgrade entry...

            next_pending_session_entry_t * pending_entry = next_pending_session_manager_find( server->pending_session_manager, from );
            if ( pending_entry == NULL )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response from %s. does not match any pending upgrade", next_address_to_string( from, address_buffer ) );
                return;
            }

            if ( !upgrade_token.Read( packet.upgrade_token, pending_entry->private_key ) )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response from %s. could not decrypt upgrade token", next_address_to_string( from, address_buffer ) );
                return;
            }

            if ( upgrade_token.session_id != pending_entry->session_id )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response from %s. session id does not match pending upgrade entry", next_address_to_string( from, address_buffer ) );
                return;
            }

            if ( !next_address_equal( &upgrade_token.client_address, &pending_entry->address ) )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response from %s. client address does not match pending upgrade entry", next_address_to_string( from, address_buffer ) );
                return;
            }

            uint8_t server_send_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
            uint8_t server_receive_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
            if ( next_crypto_kx_server_session_keys( server_receive_key, server_send_key, server->server_kx_public_key, server->server_kx_private_key, packet.client_kx_public_key ) != 0 )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server could not generate session keys from client public key" );
                return;
            }

            // remove from pending upgrade

            next_pending_session_manager_remove_by_address( server->pending_session_manager, from );

            // add to established sessions

            next_session_entry_t * entry = NULL;
            {
                next_platform_mutex_guard( &server->session_mutex );
                entry = next_session_manager_add( server->session_manager, &pending_entry->address, pending_entry->session_id, pending_entry->private_key, pending_entry->upgrade_token );
            }
            if ( entry == NULL )
            {
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored upgrade response from %s. failed to add session", next_address_to_string( from, address_buffer ) );
                return;
            }

            memcpy( entry->send_key, server_send_key, NEXT_CRYPTO_KX_SESSIONKEYBYTES );
            memcpy( entry->receive_key, server_receive_key, NEXT_CRYPTO_KX_SESSIONKEYBYTES );
            memcpy( entry->client_route_public_key, packet.client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
            entry->last_client_stats_update = next_platform_time();
            entry->user_hash = pending_entry->user_hash;
            entry->client_open_session_sequence = packet.client_open_session_sequence;
            entry->stats_platform_id = packet.platform_id;
            entry->stats_connection_type = packet.connection_type;
            entry->last_upgraded_packet_receive_time = next_platform_time();

            // notify session upgraded

            next_server_notify_session_upgraded_t * notify = (next_server_notify_session_upgraded_t*) next_malloc( server->context, sizeof( next_server_notify_session_upgraded_t ) );
            notify->type = NEXT_SERVER_NOTIFY_SESSION_UPGRADED;
            notify->address = entry->address;
            notify->session_id = entry->session_id;
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_SESSION_UPGRADED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING                
                next_platform_mutex_guard( &server->notify_mutex );
                next_queue_push( server->notify_queue, notify );
            }

            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server received upgrade response packet from client %s", next_address_to_string( from, address_buffer ) );

            upgraded = true;
        }

        if ( !next_address_equal( &upgrade_token.client_address, from ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response. client address does not match from address" );
            return;
        }

        if ( upgrade_token.expire_timestamp < uint64_t(next_platform_time()) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response. upgrade token expired" );
            return;
        }

        if ( !next_address_equal( &upgrade_token.client_address, from ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response. client address does not match from address" );
            return;
        }

        if ( !next_address_equal( &upgrade_token.server_address, &server->server_address ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored upgrade response. server address does not match" );
            return;
        }

        next_post_validate_packet( NEXT_UPGRADE_RESPONSE_PACKET, NULL, NULL, NULL );
        
        if ( !upgraded )
        {
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server received upgrade response packet from %s", next_address_to_string( from, address_buffer ) );
        }

        // reply with upgrade confirm

        NextUpgradeConfirmPacket response;
        response.upgrade_sequence = server->upgrade_sequence++;
        response.session_id = upgrade_token.session_id;
        response.server_address = server->server_address;
        memcpy( response.client_kx_public_key, packet.client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        memcpy( response.server_kx_public_key, server->server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );

        if ( next_server_internal_send_packet( server, from, NEXT_UPGRADE_CONFIRM_PACKET, &response ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "server could not send upgrade confirm packet" );
            return;
        }

        char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent upgrade confirm packet to client %s", next_address_to_string( from, address_buffer ) );

        return;
    }

    // -------------------
    // PACKETS FROM RELAYS
    // -------------------

    if ( packet_id == NEXT_ROUTE_REQUEST_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing route request packet" );

        const int packet_bytes = end - begin;

        if ( packet_bytes != NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored route request packet. wrong size" );
            return;
        }

        uint8_t * buffer = packet_data + begin;
        next_route_token_t route_token;
        if ( next_read_encrypted_route_token( &buffer, &route_token, next_relay_backend_public_key, server->server_route_private_key ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored route request packet. bad route" );
            return;
        }

        next_session_entry_t * entry = next_session_manager_find_by_session_id( server->session_manager, route_token.session_id );
        if ( !entry )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored route request packet. could not find session %" PRIx64, route_token.session_id );
            return;
        }

        if ( entry->has_current_route && route_token.expire_timestamp < entry->current_route_expire_timestamp )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored route request packet. expire timestamp is older than current route" );
            return;
        }

        if ( entry->has_current_route && next_sequence_greater_than( entry->most_recent_session_version, route_token.session_version ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored route request packet. route is older than most recent session (%d vs. %d)", route_token.session_version, entry->most_recent_session_version );
            return;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server received route request packet from relay for session %" PRIx64, route_token.session_id );

        if ( next_sequence_greater_than( route_token.session_version, entry->pending_route_session_version ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server added pending route for session %" PRIx64, route_token.session_id );
            entry->has_pending_route = true;
            entry->pending_route_session_version = route_token.session_version;
            entry->pending_route_expire_timestamp = route_token.expire_timestamp;
            entry->pending_route_expire_time = entry->has_current_route ? ( entry->current_route_expire_time + NEXT_SLICE_SECONDS * 2 ) : ( next_platform_time() + NEXT_SLICE_SECONDS * 2 );
            entry->pending_route_kbps_up = route_token.kbps_up;
            entry->pending_route_kbps_down = route_token.kbps_down;
            entry->pending_route_send_address = *from;
            memcpy( entry->pending_route_private_key, route_token.private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
            entry->most_recent_session_version = route_token.session_version;
        }

        uint64_t session_send_sequence = entry->special_send_sequence++;

        uint8_t from_address_data[4];
        uint8_t to_address_data[4];
        int from_address_bytes;
        int to_address_bytes;

        next_address_data( &server->server_address, from_address_data, &from_address_bytes );
        next_address_data( from, to_address_data, &to_address_bytes );

        uint8_t response_data[NEXT_MAX_PACKET_BYTES];

        int response_bytes = next_write_route_response_packet( response_data, session_send_sequence, entry->session_id, entry->pending_route_session_version, entry->pending_route_private_key, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

        next_assert( response_bytes > 0 );

        next_assert( next_basic_packet_filter( response_data, response_bytes ) );
        next_assert( next_advanced_packet_filter( response_data, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, response_bytes ) );

        next_server_internal_send_packet_to_address( server, from, response_data, response_bytes );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent route response packet to relay for session %" PRIx64, entry->session_id );

        return;
    }

    // continue request packet

    if ( packet_id == NEXT_CONTINUE_REQUEST_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing continue request packet" );

        const int packet_bytes = end - begin;

        if ( packet_bytes != NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored continue request packet. wrong size" );
            return;
        }

        uint8_t * buffer = packet_data + begin;
        next_continue_token_t continue_token;
        if ( next_read_encrypted_continue_token( &buffer, &continue_token, next_relay_backend_public_key, server->server_route_private_key ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored continue request packet from relay. bad token" );
            return;
        }

        next_session_entry_t * entry = next_session_manager_find_by_session_id( server->session_manager, continue_token.session_id );
        if ( !entry )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored continue request packet from relay. could not find session %" PRIx64, continue_token.session_id );
            return;
        }

        if ( !entry->has_current_route )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored continue request packet from relay. session has no route to continue" );
            return;
        }

        if ( continue_token.session_version != entry->current_route_session_version )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored continue request packet from relay. session version does not match" );
            return;
        }

        if ( continue_token.expire_timestamp < entry->current_route_expire_timestamp )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored continue request packet from relay. expire timestamp is older than current route" );
            return;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server received continue request packet from relay for session %" PRIx64, continue_token.session_id );

        entry->current_route_expire_timestamp = continue_token.expire_timestamp;
        entry->current_route_expire_time += NEXT_SLICE_SECONDS;
        entry->has_previous_route = false;

        uint64_t session_send_sequence = entry->special_send_sequence++;

        uint8_t from_address_data[4];
        uint8_t to_address_data[4];
        int from_address_bytes;
        int to_address_bytes;

        next_address_data( &server->server_address, from_address_data, &from_address_bytes );
        next_address_data( from, to_address_data, &to_address_bytes );

        uint8_t response_data[NEXT_MAX_PACKET_BYTES];

        int response_bytes = next_write_continue_response_packet( response_data, session_send_sequence, entry->session_id, entry->current_route_session_version, entry->current_route_private_key, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

        next_assert( response_bytes > 0 );

        next_assert( next_basic_packet_filter( response_data, response_bytes ) );
        next_assert( next_advanced_packet_filter( response_data, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, response_bytes ) );

        next_server_internal_send_packet_to_address( server, from, response_data, response_bytes );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent continue response packet to relay for session %" PRIx64, entry->session_id );

        return;
    }

    // client to server packet

    if ( packet_id == NEXT_CLIENT_TO_SERVER_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing client to server packet" );

        const int packet_bytes = end - begin;

        if ( packet_bytes <= NEXT_HEADER_BYTES )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored client to server packet. packet too small to be valid" );
            return;
        }

        next_session_entry_t * entry = next_server_internal_process_client_to_server_packet( server, packet_id, packet_data + begin, packet_bytes );
        if ( !entry )
        {
            // IMPORTANT: There is no need to log this case, because next_server_internal_process_client_to_server_packet already
            // logs all cases where it returns NULL to the debug log. Logging here duplicates the log and incorrectly prints
            // out an error when the packet has already been received on the direct path, when multipath is enabled.
            return;
        }

        next_server_notify_packet_received_t * notify = (next_server_notify_packet_received_t*) next_malloc( server->context, sizeof( next_server_notify_packet_received_t ) );
        notify->type = NEXT_SERVER_NOTIFY_PACKET_RECEIVED;
        notify->from = entry->address;
        notify->packet_bytes = packet_bytes - NEXT_HEADER_BYTES;
        next_assert( notify->packet_bytes > 0 );
        next_assert( notify->packet_bytes <= NEXT_MTU );
        memcpy( notify->packet_data, packet_data + begin + NEXT_HEADER_BYTES, size_t(notify->packet_bytes) );
        {
#if NEXT_SPIKE_TRACKING
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_PACKET_RECEIVED at %s:%d - from = %s, packet_bytes = %d", __FILE__, __LINE__, next_address_to_string( &notify->from, address_buffer ), notify->packet_bytes );
#endif // #if NEXT_SPIKE_TRACKING                
            next_platform_mutex_guard( &server->notify_mutex );
            next_queue_push( server->notify_queue, notify );
        }

        return;
    }

    // ping packet

    if ( packet_id == NEXT_PING_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing next ping packet" );

        const int packet_bytes = end - begin;

        if ( packet_bytes != NEXT_HEADER_BYTES + 8 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored next ping packet. bad packet size" );
            return;
        }

        next_session_entry_t * entry = next_server_internal_process_client_to_server_packet( server, packet_id, packet_data + begin, packet_bytes );
        if ( !entry )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored next ping packet. did not verify" );
            return;
        }

        const uint8_t * p = packet_data + begin + NEXT_HEADER_BYTES;

        uint64_t ping_sequence = next_read_uint64( &p );

        entry->last_client_next_ping = next_platform_time();

        uint64_t send_sequence = entry->special_send_sequence++;

        uint8_t from_address_data[4];
        uint8_t to_address_data[4];
        int from_address_bytes;
        int to_address_bytes;

        next_address_data( &server->server_address, from_address_data, &from_address_bytes );
        next_address_data( from, to_address_data, &to_address_bytes );

        uint8_t pong_packet_data[NEXT_MAX_PACKET_BYTES];

        int pong_packet_bytes = next_write_pong_packet( pong_packet_data, send_sequence, entry->session_id, entry->current_route_session_version, entry->current_route_private_key, ping_sequence, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

        next_assert( pong_packet_bytes > 0 );

        next_assert( next_basic_packet_filter( pong_packet_data, pong_packet_bytes ) );
        next_assert( next_advanced_packet_filter( pong_packet_data, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, pong_packet_bytes ) );

        next_server_internal_send_packet_to_address( server, from, pong_packet_data, pong_packet_bytes );

        return;
    }

    // ----------------------------------
    // ENCRYPTED CLIENT TO SERVER PACKETS
    // ----------------------------------

    next_session_entry_t * session = NULL;

    if ( next_encrypted_packets[packet_id] )
    {
        session = next_session_manager_find_by_address( server->session_manager, from );
        if ( !session )
        {
            next_printf( NEXT_LOG_LEVEL_SPAM, "server dropped encrypted packet because it couldn't find any session for it" );
            return;
        }

        session->last_upgraded_packet_receive_time = next_platform_time();
    }

    // direct ping packet

    if ( packet_id == NEXT_DIRECT_PING_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing direct ping packet" );

        next_assert( session );

        if ( session == NULL )
            return;

        uint64_t packet_sequence = 0;

        NextDirectPingPacket packet;
        if ( next_read_packet( NEXT_DIRECT_PING_PACKET, packet_data, begin, end, &packet, next_signed_packets, next_encrypted_packets, &packet_sequence, NULL, session->receive_key, &session->internal_replay_protection ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored direct ping packet. could not read" );
            return;
        }

        session->last_client_direct_ping = next_platform_time();

        next_post_validate_packet( NEXT_DIRECT_PING_PACKET, next_encrypted_packets, &packet_sequence, &session->internal_replay_protection );

        NextDirectPongPacket response;
        response.ping_sequence = packet.ping_sequence;

        if ( next_server_internal_send_packet( server, from, NEXT_DIRECT_PONG_PACKET, &response ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server could not send upgrade confirm packet" );
            return;
        }

        return;
    }

    // client stats packet

    if ( packet_id == NEXT_CLIENT_STATS_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing client stats packet" );

        next_assert( session );

        if ( session == NULL )
            return;

        NextClientStatsPacket packet;

        uint64_t packet_sequence = 0;

        if ( next_read_packet( NEXT_CLIENT_STATS_PACKET, packet_data, begin, end, &packet, next_signed_packets, next_encrypted_packets, &packet_sequence, NULL, session->receive_key, &session->internal_replay_protection ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored client stats packet. could not read" );
            return;
        }

        next_post_validate_packet( NEXT_CLIENT_STATS_PACKET, next_encrypted_packets, &packet_sequence, &session->internal_replay_protection );

        if ( packet_sequence > session->stats_sequence )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server received client stats packet for session %" PRIx64, session->session_id );

            if ( !session->stats_fallback_to_direct && packet.fallback_to_direct )
            {
                next_printf( NEXT_LOG_LEVEL_INFO, "server session fell back to direct %" PRIx64, session->session_id );
            }

            session->stats_sequence = packet_sequence;

            session->stats_reported = packet.reported;
            session->stats_multipath = packet.multipath;
            session->stats_fallback_to_direct = packet.fallback_to_direct;
            if ( packet.next_bandwidth_over_limit )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server session sees client over next bandwidth limit %" PRIx64, session->session_id );
                session->stats_client_bandwidth_over_limit = true;
            }

            session->stats_platform_id = packet.platform_id;
            session->stats_connection_type = packet.connection_type;
            session->stats_direct_kbps_up = packet.direct_kbps_up;
            session->stats_direct_kbps_down = packet.direct_kbps_down;
            session->stats_next_kbps_up = packet.next_kbps_up;
            session->stats_next_kbps_down = packet.next_kbps_down;
            session->stats_direct_rtt = packet.direct_rtt;
            session->stats_direct_jitter = packet.direct_jitter;
            session->stats_direct_packet_loss = packet.direct_packet_loss;
            session->stats_direct_max_packet_loss_seen = packet.direct_max_packet_loss_seen;
            session->stats_next = packet.next;
            session->stats_next_rtt = packet.next_rtt;
            session->stats_next_jitter = packet.next_jitter;
            session->stats_next_packet_loss = packet.next_packet_loss;
            session->stats_has_near_relay_pings = packet.num_near_relays > 0;
            session->stats_num_near_relays = packet.num_near_relays;
            for ( int i = 0; i < packet.num_near_relays; ++i )
            {
                session->stats_near_relay_ids[i] = packet.near_relay_ids[i];
                session->stats_near_relay_rtt[i] = packet.near_relay_rtt[i];
                session->stats_near_relay_jitter[i] = packet.near_relay_jitter[i];
                session->stats_near_relay_packet_loss[i] = packet.near_relay_packet_loss[i];
            }
            session->stats_packets_sent_client_to_server = packet.packets_sent_client_to_server;
            session->stats_packets_lost_server_to_client = packet.packets_lost_server_to_client;
            session->stats_jitter_server_to_client = packet.jitter_server_to_client;

            session->last_client_stats_update = next_platform_time();
        }

        return;
    }

    // route update ack packet

    if ( packet_id == NEXT_ROUTE_UPDATE_ACK_PACKET && session != NULL )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "server processing route update ack packet" );

        NextRouteUpdateAckPacket packet;

        uint64_t packet_sequence = 0;

        if ( next_read_packet( NEXT_ROUTE_UPDATE_ACK_PACKET, packet_data, begin, end, &packet, next_signed_packets, next_encrypted_packets, &packet_sequence, NULL, session->receive_key, &session->internal_replay_protection ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored client stats packet. could not read" );
            return;
        }

        if ( packet.sequence != session->update_sequence )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored route update ack packet. wrong update sequence number" );
            return;
        }

        next_post_validate_packet( NEXT_ROUTE_UPDATE_ACK_PACKET, next_encrypted_packets, &packet_sequence, &session->internal_replay_protection );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server received route update ack from client for session %" PRIx64, session->session_id );

        if ( session->update_dirty )
        {
            session->update_dirty = false;
        }

        return;
    }
}

void next_server_internal_process_passthrough_packet( next_server_internal_t * server, const next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( from );
    next_assert( packet_data );

    next_printf( NEXT_LOG_LEVEL_SPAM, "server processing passthrough packet" );

    next_server_internal_verify_sentinels( server );

    if ( packet_bytes > 0 && packet_bytes <= NEXT_MAX_PACKET_BYTES - 1 )
    {
        if ( server->payload_receive_callback )
        {
            void * callback_data = server->payload_receive_callback_data;
            if ( server->payload_receive_callback( callback_data, from, packet_data, packet_bytes ) )
                return;
        }

        next_server_notify_packet_received_t * notify = (next_server_notify_packet_received_t*) next_malloc( server->context, sizeof( next_server_notify_packet_received_t ) );
        notify->type = NEXT_SERVER_NOTIFY_PACKET_RECEIVED;
        notify->from = *from;
        notify->packet_bytes = packet_bytes;
        next_assert( packet_bytes > 0 );
        next_assert( packet_bytes <= NEXT_MAX_PACKET_BYTES - 1 );
        memcpy( notify->packet_data, packet_data, size_t(packet_bytes) );
        {
#if NEXT_SPIKE_TRACKING
            char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_PACKET_RECEIVED at %s:%d - from = %s, packet_bytes = %d", __FILE__, __LINE__, next_address_to_string( &notify->from, address_buffer ), notify->packet_bytes );
#endif // #if NEXT_SPIKE_TRACKING                
            next_platform_mutex_guard( &server->notify_mutex );
            next_queue_push( server->notify_queue, notify );
        }
    }
}

#if NEXT_DEVELOPMENT
extern bool next_packet_loss;
#endif // #if NEXT_DEVELOPMENT

void next_server_internal_block_and_receive_packet( next_server_internal_t * server )
{
    next_server_internal_verify_sentinels( server );

    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

    next_assert( ( size_t(packet_data) % 4 ) == 0 );

    next_address_t from;

#if NEXT_SPIKE_TRACKING
    next_printf( NEXT_LOG_LEVEL_SPAM, "server calls next_platform_socket_receive_packet on internal thread" );
#endif // #if NEXT_SPIKE_TRACKING

    const int packet_bytes = next_platform_socket_receive_packet( server->socket, &from, packet_data, NEXT_MAX_PACKET_BYTES );

#if NEXT_SPIKE_TRACKING
    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_printf( NEXT_LOG_LEVEL_SPAM, "server next_platform_socket_receive_packet returns with a %d byte packet from %s", next_address_to_string( &from, address_buffer ) );
#endif // #if NEXT_SPIKE_TRACKING

    if ( packet_bytes == 0 )
        return;

    next_assert( packet_bytes > 0 );

    int begin = 0;
    int end = packet_bytes;

    if ( server->packet_receive_callback )
    {
        void * callback_data = server->packet_receive_callback_data;

        server->packet_receive_callback( callback_data, &from, packet_data, &begin, &end );

        next_assert( begin >= 0 );
        next_assert( end <= NEXT_MAX_PACKET_BYTES );

        if ( end - begin <= 0 )
            return;        
    }

#if NEXT_DEVELOPMENT
    if ( next_packet_loss && ( rand() % 10 ) == 0 )
         return;
#endif // #if NEXT_DEVELOPMENT

    const uint8_t packet_type = packet_data[begin];

    if ( packet_type != NEXT_PASSTHROUGH_PACKET )
    {
        next_server_internal_process_network_next_packet( server, &from, packet_data, begin, end );
    }
    else
    {
        begin += 1;
        next_server_internal_process_passthrough_packet( server, &from, packet_data + begin, end - begin );
    }
}

void next_server_internal_upgrade_session( next_server_internal_t * server, const next_address_t * address, uint64_t session_id, uint64_t user_hash )
{
    next_assert( server );
    next_assert( address );

    next_server_internal_verify_sentinels( server );

    if ( next_global_config.disable_network_next )
        return;

    if ( server->state != NEXT_SERVER_STATE_INITIALIZED )
        return;

    next_assert( server->state == NEXT_SERVER_STATE_INITIALIZED || server->state == NEXT_SERVER_STATE_DIRECT_ONLY );

    if ( server->state == NEXT_SERVER_STATE_DIRECT_ONLY )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server cannot upgrade client. direct only mode" );
        return;
    }

    char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];

    next_printf( NEXT_LOG_LEVEL_DEBUG, "server upgrading client %s to session %" PRIx64, next_address_to_string( address, buffer ), session_id );

    NextUpgradeToken upgrade_token;

    upgrade_token.session_id = session_id;
    upgrade_token.expire_timestamp = uint64_t( next_platform_time() ) + 10;
    upgrade_token.client_address = *address;
    upgrade_token.server_address = server->server_address;

    unsigned char session_private_key[NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    next_crypto_secretbox_keygen( session_private_key );

    uint8_t upgrade_token_data[NEXT_UPGRADE_TOKEN_BYTES];

    upgrade_token.Write( upgrade_token_data, session_private_key );

    next_pending_session_manager_remove_by_address( server->pending_session_manager, address );

    next_session_manager_remove_by_address( server->session_manager, address );

    next_pending_session_entry_t * entry = next_pending_session_manager_add( server->pending_session_manager, address, upgrade_token.session_id, session_private_key, upgrade_token_data, next_platform_time() );

    if ( entry == NULL )
    {
        next_assert( !"could not add pending session entry. this should never happen!" );
        return;
    }

    entry->user_hash = user_hash;
}

void next_server_internal_session_events( next_server_internal_t * server, const next_address_t * address, uint64_t session_events )
{
    next_assert( server );
    next_assert( address );

    next_server_internal_verify_sentinels( server );

    if ( next_global_config.disable_network_next )
        return;

    if ( server->state != NEXT_SERVER_STATE_INITIALIZED )
        return;

    next_session_entry_t * entry = next_session_manager_find_by_address( server->session_manager, address );
    if ( !entry )
    {
        char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_printf( NEXT_LOG_LEVEL_DEBUG, "could not find session at address %s. not adding session event %x", next_address_to_string( address, buffer ), session_events );
        return;
    }

    entry->current_session_events |= session_events;
    char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_printf( NEXT_LOG_LEVEL_DEBUG, "server set session event %x for session %" PRIx64 " at address %s", session_events, entry->session_id, next_address_to_string( address, buffer ) );
}

void next_server_internal_flush_session_update( next_server_internal_t * server )
{
    next_assert( server );
    next_assert( server->session_manager );

    if ( next_global_config.disable_network_next )
        return;

    const int max_entry_index = server->session_manager->max_entry_index;

    for ( int i = 0; i <= max_entry_index; ++i )
    {
        if ( server->session_manager->session_ids[i] == 0 )
            continue;

        next_session_entry_t * session = &server->session_manager->entries[i];

        session->client_ping_timed_out = true;
        session->session_update_request_packet.client_ping_timed_out = true;

        // IMPORTANT: Make sure to only accept a backend session response for the next session update
        // sent out, not the current session update (if any is in flight). This way flush succeeds
        // even if it called in the middle of a session update in progress.
        session->session_flush_update_sequence = session->update_sequence + 1;
        session->session_update_flush = true;
        server->num_session_updates_to_flush++;
    }
}

void next_server_internal_flush( next_server_internal_t * server )
{
    next_assert( server );
    next_assert( server->session_manager );

    next_server_internal_verify_sentinels( server );

    if ( next_global_config.disable_network_next )
    {
        server->flushing = true;
        server->flushed = true;
        return;
    }

    if ( server->flushing )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "server ignored flush. already flushed" );
        return;
    }

    server->flushing = true;

    next_server_internal_flush_session_update( server );

    next_printf( NEXT_LOG_LEVEL_DEBUG, "server flush started. %d session updates to flush", server->num_session_updates_to_flush );
}

void next_server_internal_pump_commands( next_server_internal_t * server )
{
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "next_server_internal_pump_commands" );
#endif // #if NEXT_SPIKE_TRACKING

    while ( true )
    {
        next_server_internal_verify_sentinels( server );

        void * entry = NULL;
        {
            next_platform_mutex_guard( &server->command_mutex );
            entry = next_queue_pop( server->command_queue );
        }

        if ( entry == NULL )
            break;

        next_server_command_t * command = (next_server_command_t*) entry;

        switch ( command->type )
        {
            case NEXT_SERVER_COMMAND_UPGRADE_SESSION:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread receives NEXT_SERVER_COMMAND_UPGRADE_SESSION" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_command_upgrade_session_t * upgrade_session = (next_server_command_upgrade_session_t*) command;
                next_server_internal_upgrade_session( server, &upgrade_session->address, upgrade_session->session_id, upgrade_session->user_hash );
            }
            break;

            case NEXT_SERVER_COMMAND_SESSION_EVENT:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread receives NEXT_SERVER_COMMAND_SESSION_EVENT" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_command_session_event_t * event = (next_server_command_session_event_t*) command;
                next_server_internal_session_events( server, &event->address, event->session_events );
            }
            break;

            case NEXT_SERVER_COMMAND_FLUSH:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread receives NEXT_SERVER_COMMAND_FLUSH" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_internal_flush( server );
            }
            break;

            case NEXT_SERVER_COMMAND_SET_PACKET_RECEIVE_CALLBACK:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread receives NEXT_SERVER_COMMAND_SET_PACKET_RECEIVE_CALLBACK" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_command_set_packet_receive_callback_t * cmd = (next_server_command_set_packet_receive_callback_t*) command;
                server->packet_receive_callback = cmd->callback;
                server->packet_receive_callback_data = cmd->callback_data;
            }
            break;

            case NEXT_SERVER_COMMAND_SET_SEND_PACKET_TO_ADDRESS_CALLBACK:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread receives NEXT_SERVER_COMMAND_SET_SEND_PACKET_TO_ADDRESS_CALLBACK" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_command_set_send_packet_to_address_callback_t * cmd = (next_server_command_set_send_packet_to_address_callback_t*) command;
                server->send_packet_to_address_callback = cmd->callback;
                server->send_packet_to_address_callback_data = cmd->callback_data;
            }
            break;

            case NEXT_SERVER_COMMAND_SET_PAYLOAD_RECEIVE_CALLBACK:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread receives NEXT_SERVER_COMMAND_SET_PAYLOAD_RECEIVE_CALLBACK" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_command_set_payload_receive_callback_t * cmd = (next_server_command_set_payload_receive_callback_t*) command;
                server->payload_receive_callback = cmd->callback;
                server->payload_receive_callback_data = cmd->callback_data;
            }
            break;

            default: break;
        }

        next_free( server->context, command );
    }
}

static void next_server_internal_resolve_hostname_thread_function( void * context )
{
    next_assert( context );

    next_server_internal_t * server = (next_server_internal_t*) context;

    double start_time = next_platform_time();

    const char * hostname = next_global_config.server_backend_hostname;
    const char * port = NEXT_SERVER_BACKEND_PORT;
    const char * override_port = next_platform_getenv( "NEXT_SERVER_BACKEND_PORT" );
    if ( !override_port )
    {
        override_port = next_platform_getenv( "NEXT_PORT" );
    }
    if ( override_port )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "override server backend port: '%s'", override_port );
        port = override_port;
    }

    next_printf( NEXT_LOG_LEVEL_INFO, "server resolving backend hostname '%s'", hostname );

    next_address_t address;

    bool success = false;

    // first try to parse the hostname directly as an address, this is a common case in testbeds and there's no reason to actually run a DNS resolve on it

    if ( next_address_parse( &address, hostname ) == NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server backend hostname is an address" );
        next_assert( address.type == NEXT_ADDRESS_IPV4 || address.type == NEXT_ADDRESS_IPV6 );
        address.port = uint16_t( atoi(port) );
        success = true;
    }    
    else
    {
        // try to resolve the hostname, retry a few times if it doesn't succeed right away

        for ( int i = 0; i < 10; ++i )
        {
            if ( next_platform_hostname_resolve( hostname, port, &address ) == NEXT_OK )
            {
                next_assert( address.type == NEXT_ADDRESS_IPV4 || address.type == NEXT_ADDRESS_IPV6 );
                success = true;
                break;
            }
            else
            {
                next_printf( NEXT_LOG_LEVEL_WARN, "server failed to resolve hostname: '%s' (%d)", hostname, i );
                next_platform_sleep( 1.0 );
            }
        }
    }

#if NEXT_DEVELOPMENT
    if ( next_platform_getenv( "NEXT_FORCE_RESOLVE_HOSTNAME_TIMEOUT" ) )
    {
        next_platform_sleep( NEXT_SERVER_RESOLVE_HOSTNAME_TIMEOUT * 2 );
    }
#endif // #if NEXT_DEVELOPMENT

    if ( next_platform_time() - start_time > NEXT_SERVER_AUTODETECT_TIMEOUT )
    {
        // IMPORTANT: if we have timed out, don't grab the mutex or write results. 
        // our thread has been destroyed and if we are unlucky, the next_server_internal_t instance has as well.
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server resolve hostname thread aborted" );
        return;
    }

    if ( !success )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to resolve backend hostname: %s", hostname );
        next_platform_mutex_guard( &server->resolve_hostname_mutex );
        server->resolve_hostname_finished = true;
        memset( &server->resolve_hostname_result, 0, sizeof(next_address_t) );
        return;
    }

    next_platform_mutex_guard( &server->resolve_hostname_mutex );
    server->resolve_hostname_finished = true;
    server->resolve_hostname_result = address;

    next_printf( NEXT_LOG_LEVEL_DEBUG, "server resolve hostname thread finished" );
}

static bool next_server_internal_update_resolve_hostname( next_server_internal_t * server )
{
    next_assert( server );

    next_server_internal_verify_sentinels( server );

    next_assert( !next_global_config.disable_network_next );

    if ( !server->resolving_hostname )
        return true;

    bool finished = false;
    next_address_t result;
    memset( &result, 0, sizeof(next_address_t) );
    {
        next_platform_mutex_guard( &server->resolve_hostname_mutex );
        finished = server->resolve_hostname_finished;
        result = server->resolve_hostname_result;
    }

    if ( finished )
    {
        next_platform_thread_join( server->resolve_hostname_thread );
    }
    else
    {
        if ( next_platform_time() < server->resolve_hostname_start_time + NEXT_SERVER_RESOLVE_HOSTNAME_TIMEOUT )
        {
            // keep waiting
            return false;
        }
        else
        {
            // but don't wait forever...
            next_printf( NEXT_LOG_LEVEL_INFO, "resolve hostname timed out" );
        }
    }
    
    next_platform_thread_destroy( server->resolve_hostname_thread );
    
    server->resolve_hostname_thread = NULL;
    server->resolving_hostname = false;
    server->backend_address = result;

    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];

    if ( result.type != NEXT_ADDRESS_NONE )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server resolved backend hostname to %s", next_address_to_string( &result, address_buffer ) );
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server failed to resolve backend hostname" );
        server->resolving_hostname = false;
    }

    return true;
}

static void next_server_internal_autodetect_thread_function( void * context )
{
    next_assert( context );

    double start_time = next_platform_time();

    next_server_internal_t * server = (next_server_internal_t*) context;

    bool autodetect_result = false;
    bool autodetect_actually_did_something = false;
    char autodetect_output[NEXT_MAX_DATACENTER_NAME_LENGTH];

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC || NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    // autodetect datacenter is currently windows and linux only (mac is just for testing...)

    const char * autodetect_input = server->datacenter_name;
    
    char autodetect_address[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_address_t server_address_no_port = server->server_address;
    server_address_no_port.port = 0;
    next_address_to_string( &server_address_no_port, autodetect_address );

    if ( !next_global_config.disable_autodetect &&
         ( autodetect_input[0] == '\0' 
            ||
         ( autodetect_input[0] == 'c' &&
           autodetect_input[1] == 'l' &&
           autodetect_input[2] == 'o' &&
           autodetect_input[3] == 'u' &&
           autodetect_input[4] == 'd' &&
           autodetect_input[5] == '\0' ) 
            ||
         ( autodetect_input[0] == 'm' && 
           autodetect_input[1] == 'u' && 
           autodetect_input[2] == 'l' && 
           autodetect_input[3] == 't' && 
           autodetect_input[4] == 'i' && 
           autodetect_input[5] == 'p' && 
           autodetect_input[6] == 'l' && 
           autodetect_input[7] == 'a' && 
           autodetect_input[8] == 'y' && 
           autodetect_input[9] == '.' ) ) )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server attempting to autodetect datacenter" );

        autodetect_result = next_autodetect_datacenter( autodetect_input, autodetect_address, autodetect_output, sizeof(autodetect_output) );
        
        autodetect_actually_did_something = true;
    }

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC || NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

#if NEXT_DEVELOPMENT
    if ( next_platform_getenv( "NEXT_FORCE_AUTODETECT_TIMEOUT" ) )
    {
        next_platform_sleep( NEXT_SERVER_AUTODETECT_TIMEOUT * 1.25 );
    }
#endif // #if NEXT_DEVELOPMENT

    if ( next_platform_time() - start_time > NEXT_SERVER_RESOLVE_HOSTNAME_TIMEOUT )
    {
        // IMPORTANT: if we have timed out, don't grab the mutex or write results. 
        // our thread has been destroyed and if we are unlucky, the next_server_internal_t instance is as well.
        return;
    }

    next_platform_mutex_guard( &server->autodetect_mutex );
    next_copy_string( server->autodetect_result, autodetect_output, NEXT_MAX_DATACENTER_NAME_LENGTH );
    server->autodetect_finished = true;
    server->autodetect_succeeded = autodetect_result;
    server->autodetect_actually_did_something = autodetect_actually_did_something;
}

static bool next_server_internal_update_autodetect( next_server_internal_t * server )
{
    next_assert( server );

    next_server_internal_verify_sentinels( server );

    next_assert( !next_global_config.disable_network_next );

    if ( server->resolving_hostname )    // IMPORTANT: wait until resolving hostname is finished, before autodetect complete!
        return true;

    if ( !server->autodetecting )
        return true;

    bool finished = false;
    {
        next_platform_mutex_guard( &server->autodetect_mutex );
        finished = server->autodetect_finished;
    }

    if ( finished )
    {
        next_platform_thread_join( server->autodetect_thread );
    }
    else
    {
        if ( next_platform_time() < server->autodetect_start_time + NEXT_SERVER_AUTODETECT_TIMEOUT )
        {
            // keep waiting
            return false;
        }
        else
        {
            // but don't wait forever...
            next_printf( NEXT_LOG_LEVEL_INFO, "autodetect timed out. sticking with '%s' [%" PRIx64 "]", server->datacenter_name, server->datacenter_id );
        }
    }
    
    next_platform_thread_destroy( server->autodetect_thread );
    
    server->autodetect_thread = NULL;
    server->autodetecting = false;

    if ( server->autodetect_actually_did_something )
    {
        if ( server->autodetect_succeeded )
        {
            memset( server->datacenter_name, 0, sizeof(server->datacenter_name) );
            next_copy_string( server->datacenter_name, server->autodetect_result, NEXT_MAX_DATACENTER_NAME_LENGTH );
            server->datacenter_id = next_datacenter_id( server->datacenter_name );
            next_printf( NEXT_LOG_LEVEL_INFO, "server autodetected datacenter '%s' [%" PRIx64 "]", server->datacenter_name, server->datacenter_id );
        }
        else
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter failed. sticking with '%s' [%" PRIx64 "]", server->datacenter_name, server->datacenter_id );
        }
    }

    return true;
}

#if NEXT_DEVELOPMENT
bool raspberry_fake_latency = false;
#endif // #if NEXT_DEVELOPMENT

void next_server_internal_update_init( next_server_internal_t * server )
{
    next_server_internal_verify_sentinels( server );

    next_assert( server );

    next_assert( !next_global_config.disable_network_next );

    if ( server->state != NEXT_SERVER_STATE_INITIALIZING )
        return;

    // check for init timeout

    const double current_time = next_platform_time();

    if ( server->server_init_timeout_time <= current_time )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server init timed out. falling back to direct mode only :(" );

        server->state = NEXT_SERVER_STATE_DIRECT_ONLY;

        next_server_notify_ready_t * notify_ready = (next_server_notify_ready_t*) next_malloc( server->context, sizeof( next_server_notify_ready_t ) );
        notify_ready->type = NEXT_SERVER_NOTIFY_READY;
        next_copy_string( notify_ready->datacenter_name, server->datacenter_name, NEXT_MAX_DATACENTER_NAME_LENGTH );

        next_server_notify_direct_only_t * notify_direct_only = (next_server_notify_direct_only_t*) next_malloc( server->context, sizeof(next_server_notify_direct_only_t) );
        next_assert( notify_direct_only );
        notify_direct_only->type = NEXT_SERVER_NOTIFY_DIRECT_ONLY;

        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_DIRECT_ONLY and NEXT_SERVER_NOTIFY_READY at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING                
            next_platform_mutex_guard( &server->notify_mutex );
            next_queue_push( server->notify_queue, notify_direct_only );
            next_queue_push( server->notify_queue, notify_ready );
        }

        return;
    }

    // check for initializing -> initialized transition

    if ( server->resolve_hostname_finished && server->autodetect_finished && server->received_init_response )
    {
        next_assert( server->backend_address.type == NEXT_ADDRESS_IPV4 || server->backend_address.type == NEXT_ADDRESS_IPV6 );
        next_server_notify_ready_t * notify = (next_server_notify_ready_t*) next_malloc( server->context, sizeof( next_server_notify_ready_t ) );
        notify->type = NEXT_SERVER_NOTIFY_READY;
        next_copy_string( notify->datacenter_name, server->datacenter_name, NEXT_MAX_DATACENTER_NAME_LENGTH );
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_READY at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &server->notify_mutex );
            next_queue_push( server->notify_queue, notify );
        }
        server->state = NEXT_SERVER_STATE_INITIALIZED;
    }

    // wait until we have resolved the backend hostname

    if ( !server->resolve_hostname_finished )
        return;

    // wait until we have autodetected the datacenter

    if ( !server->autodetect_finished )
        return;

    // wait until the backend 

    // if we have started flushing, abort the init...

    if ( server->flushing )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server aborted init" );
        server->state = NEXT_SERVER_STATE_DIRECT_ONLY;
        next_server_notify_direct_only_t * notify_direct_only = (next_server_notify_direct_only_t*) next_malloc( server->context, sizeof(next_server_notify_direct_only_t) );
        next_assert( notify_direct_only );
        notify_direct_only->type = NEXT_SERVER_NOTIFY_DIRECT_ONLY;
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_DIRECT_ONLY at %s:%d", __FILE__, __FILE__ );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &server->notify_mutex );
            next_queue_push( server->notify_queue, notify_direct_only );
        }
        return;
    }

    // send init request packets repeatedly until we get a response or time out...

    if ( server->server_init_request_id != 0 && server->server_init_resend_time > current_time )
        return;

    while ( server->server_init_request_id == 0 )
    {
        server->server_init_request_id = next_random_uint64();
    }

    server->server_init_resend_time = current_time + 1.0;

    NextBackendServerInitRequestPacket packet;

    packet.request_id = server->server_init_request_id;
    packet.buyer_id = server->buyer_id;
    packet.datacenter_id = server->datacenter_id;
    next_copy_string( packet.datacenter_name, server->datacenter_name, NEXT_MAX_DATACENTER_NAME_LENGTH );
    packet.datacenter_name[NEXT_MAX_DATACENTER_NAME_LENGTH-1] = '\0';

    uint8_t magic[8];
    memset( magic, 0, sizeof(magic) );

    uint8_t from_address_data[32];
    uint8_t to_address_data[32];
    int from_address_bytes;
    int to_address_bytes;

    next_address_data( &server->server_address, from_address_data, &from_address_bytes );
    next_address_data( &server->backend_address, to_address_data, &to_address_bytes );

    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

    next_assert( ( size_t(packet_data) % 4 ) == 0 );

    int packet_bytes = 0;
    if ( next_write_backend_packet( NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET, &packet, packet_data, &packet_bytes, next_signed_packets, server->buyer_private_key, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to write server init request packet for backend" );
        return;
    }

    next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
    next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

    next_server_internal_send_packet_to_backend( server, packet_data, packet_bytes );

    next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent init request to backend" );
}

void next_server_internal_backend_update( next_server_internal_t * server )
{
    next_server_internal_verify_sentinels( server );

    next_assert( server );

    if ( next_global_config.disable_network_next )
        return;

    double current_time = next_platform_time();

    // don't do anything until we resolve the backend hostname

    if ( server->resolving_hostname )
        return;

    // tracker updates

    const int max_entry_index = server->session_manager->max_entry_index;

    for ( int i = 0; i <= max_entry_index; ++i )
    {
        if ( server->session_manager->session_ids[i] == 0 )
            continue;

        next_session_entry_t * session = &server->session_manager->entries[i];

        if ( session->stats_fallback_to_direct )
            continue;

        if ( session->next_tracker_update_time <= current_time )
        {
            const int packets_lost = next_packet_loss_tracker_update( &session->packet_loss_tracker );
            session->stats_packets_lost_client_to_server += packets_lost;
            session->stats_packets_out_of_order_client_to_server = session->out_of_order_tracker.num_out_of_order_packets;
            session->stats_jitter_client_to_server = session->jitter_tracker.jitter * 1000.0;
            session->next_tracker_update_time = current_time + NEXT_SECONDS_BETWEEN_PACKET_LOSS_UPDATES;
        }
    }

    if ( server->state != NEXT_SERVER_STATE_INITIALIZED )
        return;

    // server update

    bool first_server_update = server->server_update_first;

    if ( server->state != NEXT_SERVER_STATE_DIRECT_ONLY && server->server_update_last_time + NEXT_SECONDS_BETWEEN_SERVER_UPDATES <= current_time )
    {
        if ( server->server_update_request_id != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "server update response timed out. falling back to direct mode only :(" );
            server->state = NEXT_SERVER_STATE_DIRECT_ONLY;
            next_server_notify_direct_only_t * notify_direct_only = (next_server_notify_direct_only_t*) next_malloc( server->context, sizeof(next_server_notify_direct_only_t) );
            next_assert( notify_direct_only );
            notify_direct_only->type = NEXT_SERVER_NOTIFY_DIRECT_ONLY;
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server internal thread queued up NEXT_SERVER_NOTIFY_DIRECT_ONLY at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
                next_platform_mutex_guard( &server->notify_mutex );
                next_queue_push( server->notify_queue, notify_direct_only );
            }
            return;
        }

        while ( server->server_update_request_id == 0 )
        {
            server->server_update_request_id = next_random_uint64();
        }

        server->server_update_resend_time = current_time + 1.0;
        server->server_update_num_sessions = next_session_manager_num_entries( server->session_manager );

        NextBackendServerUpdateRequestPacket packet;

        packet.request_id = server->server_update_request_id;
        packet.buyer_id = server->buyer_id;
        packet.datacenter_id = server->datacenter_id;
        packet.num_sessions = server->server_update_num_sessions;
        packet.server_address = server->server_address;
        packet.uptime = uint64_t( time(NULL) - server->start_time );

        uint8_t magic[8];
        memset( magic, 0, sizeof(magic) );

        uint8_t from_address_data[32];
        uint8_t to_address_data[32];
        int from_address_bytes;
        int to_address_bytes;

        next_address_data( &server->server_address, from_address_data, &from_address_bytes );
        next_address_data( &server->backend_address, to_address_data, &to_address_bytes );

        uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

        next_assert( ( size_t(packet_data) % 4 ) == 0 );

        int packet_bytes = 0;
        if ( next_write_backend_packet( NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET, &packet, packet_data, &packet_bytes, next_signed_packets, server->buyer_private_key, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to write server update request packet for backend" );
            return;
        }

        next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

        next_server_internal_send_packet_to_backend( server, packet_data, packet_bytes );

        server->server_update_last_time = current_time;

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent server update packet to backend (%d sessions)", packet.num_sessions );

        server->server_update_first = false;
    }

    if ( first_server_update )
        return;

    // server update resend

    if ( server->server_update_request_id && server->server_update_resend_time <= current_time )
    {
        NextBackendServerUpdateRequestPacket packet;

        packet.request_id = server->server_update_request_id;
        packet.buyer_id = server->buyer_id;
        packet.datacenter_id = server->datacenter_id;
        packet.num_sessions = server->server_update_num_sessions;
        packet.server_address = server->server_address;

        uint8_t magic[8];
        memset( magic, 0, sizeof(magic) );

        uint8_t from_address_data[32];
        uint8_t to_address_data[32];
        int from_address_bytes;
        int to_address_bytes;

        next_address_data( &server->server_address, from_address_data, &from_address_bytes );
        next_address_data( &server->backend_address, to_address_data, &to_address_bytes );

        uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

        next_assert( ( size_t(packet_data) % 4 ) == 0 );

        int packet_bytes = 0;
        if ( next_write_backend_packet( NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET, &packet, packet_data, &packet_bytes, next_signed_packets, server->buyer_private_key, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to write server update packet for backend" );
            return;
        }

        next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

        next_server_internal_send_packet_to_backend( server, packet_data, packet_bytes );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "server resent server update packet to backend", packet.num_sessions );

        server->server_update_resend_time = current_time + 1.0;
    }

    // session updates

    for ( int i = 0; i <= max_entry_index; ++i )
    {
        if ( server->session_manager->session_ids[i] == 0 )
            continue;

        next_session_entry_t * session = &server->session_manager->entries[i];

        if ( !session->session_update_timed_out && ( ( session->next_session_update_time >= 0.0 && session->next_session_update_time <= current_time ) || ( session->session_update_flush && !session->session_update_flush_finished && !session->waiting_for_update_response ) ) )
        {
            NextBackendSessionUpdateRequestPacket packet;

            packet.Reset();

            packet.buyer_id = server->buyer_id;
            packet.datacenter_id = server->datacenter_id;
            packet.session_id = session->session_id;
            packet.slice_number = session->update_sequence++;
            packet.platform_id = session->stats_platform_id;
            packet.user_hash = session->user_hash;
            session->previous_session_events = session->current_session_events;
            session->current_session_events = 0;
            packet.session_events = session->previous_session_events;
            packet.reported = session->stats_reported;
            packet.fallback_to_direct = session->stats_fallback_to_direct;
            packet.client_bandwidth_over_limit = session->stats_client_bandwidth_over_limit;
            packet.server_bandwidth_over_limit = session->stats_server_bandwidth_over_limit;
            packet.client_ping_timed_out = session->client_ping_timed_out;
            packet.connection_type = session->stats_connection_type;
            packet.direct_kbps_up = session->stats_direct_kbps_up;
            packet.direct_kbps_down = session->stats_direct_kbps_down;
            packet.next_kbps_up = session->stats_next_kbps_up;
            packet.next_kbps_down = session->stats_next_kbps_down;
            packet.packets_sent_client_to_server = session->stats_packets_sent_client_to_server;
            {
                next_platform_mutex_guard( &server->session_mutex );
                packet.packets_sent_server_to_client = session->stats_packets_sent_server_to_client;
            }

            // IMPORTANT: hold near relay stats for the rest of the session
            if ( session->num_held_near_relays == 0 && session->stats_num_near_relays != 0 )
            {
                session->num_held_near_relays = session->stats_num_near_relays;
                for ( int j = 0; j < session->stats_num_near_relays; j++ )
                {
                    session->held_near_relay_ids[j] = session->stats_near_relay_ids[j];    
                    session->held_near_relay_rtt[j] = session->stats_near_relay_rtt[j];
                    session->held_near_relay_jitter[j] = session->stats_near_relay_jitter[j];
                    session->held_near_relay_packet_loss[j] = session->stats_near_relay_packet_loss[j];    
                }
            }

            packet.packets_lost_client_to_server = session->stats_packets_lost_client_to_server;
            packet.packets_lost_server_to_client = session->stats_packets_lost_server_to_client;
            packet.packets_out_of_order_client_to_server = session->stats_packets_out_of_order_client_to_server;
            packet.packets_out_of_order_server_to_client = session->stats_packets_out_of_order_server_to_client;

            packet.jitter_client_to_server = session->stats_jitter_client_to_server;
            packet.jitter_server_to_client = session->stats_jitter_server_to_client;
            packet.next = session->stats_next;
            packet.next_rtt = session->stats_next_rtt;
            packet.next_jitter = session->stats_next_jitter;
            packet.next_packet_loss = session->stats_next_packet_loss;
            packet.direct_rtt = session->stats_direct_rtt;
            packet.direct_jitter = session->stats_direct_jitter;
            packet.direct_packet_loss = session->stats_direct_packet_loss;
            packet.direct_max_packet_loss_seen = session->stats_direct_max_packet_loss_seen;
            packet.has_near_relay_pings = session->num_held_near_relays != 0;
            packet.num_near_relays = session->num_held_near_relays;
            for ( int j = 0; j < packet.num_near_relays; ++j )
            {
                packet.near_relay_ids[j] = session->held_near_relay_ids[j];
                packet.near_relay_rtt[j] = session->held_near_relay_rtt[j];
                packet.near_relay_jitter[j] = session->held_near_relay_jitter[j];
                packet.near_relay_packet_loss[j] = session->held_near_relay_packet_loss[j];
            }
            packet.client_address = session->address;
            packet.server_address = server->server_address;
            memcpy( packet.client_route_public_key, session->client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
            memcpy( packet.server_route_public_key, server->server_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );

            next_assert( session->session_data_bytes >= 0 );
            next_assert( session->session_data_bytes <= NEXT_MAX_SESSION_DATA_BYTES );
            packet.session_data_bytes = session->session_data_bytes;
            memcpy( packet.session_data, session->session_data, session->session_data_bytes );
            memcpy( packet.session_data_signature, session->session_data_signature, NEXT_CRYPTO_SIGN_BYTES );

            session->session_update_request_packet = packet;

#if NEXT_DEVELOPMENT
            // This is used by the raspberry pi clients in dev to give a normal distribution of latencies across all sessions, so I can test the portal
            if ( raspberry_fake_latency )
            {
                packet.next = ( session->session_id % 10 ) == 0;
                if ( packet.next )
                {
                    packet.direct_rtt = 100 + ( session->session_id % 100 );
                    packet.next_rtt = 1 + ( session->session_id % 79 );
                }
                else
                {
                    packet.direct_rtt = ( session->session_id % 500 );
                    packet.next_rtt = 0;
                }
            }
#endif // #if NEXT_DEVELOPMENT

            uint8_t magic[8];
            memset( magic, 0, sizeof(magic) );

            uint8_t from_address_data[32];
            uint8_t to_address_data[32];
            int from_address_bytes;
            int to_address_bytes;

            next_address_data( &server->server_address, from_address_data, &from_address_bytes );
            next_address_data( &server->backend_address, to_address_data, &to_address_bytes );

            uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

            next_assert( ( size_t(packet_data) % 4 ) == 0 );

            int packet_bytes = 0;
            if ( next_write_backend_packet( NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET, &packet, packet_data, &packet_bytes, next_signed_packets, server->buyer_private_key, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes ) != NEXT_OK )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to write server init request packet for backend" );
                return;
            }

            next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
            next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

            next_server_internal_send_packet_to_backend( server, packet_data, packet_bytes );

            next_printf( NEXT_LOG_LEVEL_DEBUG, "server sent session update packet to backend for session %" PRIx64, session->session_id );

            if ( session->next_session_update_time == 0.0 )
            {
                session->next_session_update_time = current_time + NEXT_SECONDS_BETWEEN_SESSION_UPDATES;
            }
            else
            {
                session->next_session_update_time += NEXT_SECONDS_BETWEEN_SESSION_UPDATES;
            }

            session->stats_client_bandwidth_over_limit = false;
            session->stats_server_bandwidth_over_limit = false;

            if ( !session->stats_fallback_to_direct )
            {
                session->waiting_for_update_response = true;
                session->next_session_resend_time = current_time + NEXT_SESSION_UPDATE_RESEND_TIME;
            }
            else
            {
                // IMPORTANT: don't send session update retries if we have fallen back to direct
                // otherwise, we swamp the server backend with increased load for the rest of the session
                session->waiting_for_update_response = false;
                session->next_session_update_time = -1.0;
            }
        }

        if ( session->waiting_for_update_response && session->next_session_resend_time <= current_time )
        {
            session->session_update_request_packet.retry_number++;

            next_printf( NEXT_LOG_LEVEL_DEBUG, "server resent session update packet to backend for session %" PRIx64 " (%d)", session->session_id, session->session_update_request_packet.retry_number );

            uint8_t magic[8];
            memset( magic, 0, sizeof(magic) );

            uint8_t from_address_data[32];
            uint8_t to_address_data[32];
            int from_address_bytes;
            int to_address_bytes;

            next_address_data( &server->server_address, from_address_data, &from_address_bytes );
            next_address_data( &server->backend_address, to_address_data, &to_address_bytes );

            uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

            next_assert( ( size_t(packet_data) % 4 ) == 0 );

            int packet_bytes = 0;
            if ( next_write_backend_packet( NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET, &session->session_update_request_packet, packet_data, &packet_bytes, next_signed_packets, server->buyer_private_key, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes ) != NEXT_OK )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "server failed to write session update request packet for backend" );
                return;
            }

            next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
            next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

            next_server_internal_send_packet_to_backend( server, packet_data, packet_bytes );

            session->next_session_resend_time += NEXT_SESSION_UPDATE_RESEND_TIME;
        }

        if ( !session->session_update_timed_out && session->waiting_for_update_response && session->next_session_update_time - NEXT_SECONDS_BETWEEN_SESSION_UPDATES + NEXT_SESSION_UPDATE_TIMEOUT <= current_time )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "server timed out waiting for backend response for session %" PRIx64, session->session_id );
            session->waiting_for_update_response = false;
            session->next_session_update_time = -1.0;
            session->session_update_timed_out = true;

            // IMPORTANT: Send packets direct from now on for this session
            {
                next_platform_mutex_guard( &server->session_mutex );
                session->mutex_send_over_network_next = false;
            }
        }
    }
}

static void next_server_update_internal( next_server_internal_t * server )
{
    next_assert( !next_global_config.disable_network_next );

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_server_internal_update_flush( server );

    next_server_internal_update_resolve_hostname( server );

    next_server_internal_update_autodetect( server );

    next_server_internal_update_init( server );

    next_server_internal_update_pending_upgrades( server );

    next_server_internal_update_route( server );

    next_server_internal_update_sessions( server );

    next_server_internal_backend_update( server );

    next_server_internal_pump_commands( server );

#if NEXT_SPIKE_TRACKING

    double finish_time = next_platform_time();

    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_server_update_internal spike %.2f milliseconds", ( finish_time - start_time ) * 1000.0 );
    }

#endif // #if NEXT_SPIKE_TRACKING
}

static void next_server_internal_thread_function( void * context )
{
    next_assert( context );

    next_server_internal_t * server = (next_server_internal_t*) context;

    double last_update_time = next_platform_time();

    while ( !server->quit )
    {
        next_server_internal_block_and_receive_packet( server );

        if ( !next_global_config.disable_network_next && next_platform_time() >= last_update_time + 0.1 )
        {
            next_server_update_internal( server );

            last_update_time = next_platform_time();
        }
    }
}

// ---------------------------------------------------------------

struct next_server_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    next_server_internal_t * internal;
    next_platform_thread_t * thread;
    next_proxy_session_manager_t * pending_session_manager;
    next_proxy_session_manager_t * session_manager;
    next_address_t address;
    uint16_t bound_port;
    bool ready;
    char datacenter_name[NEXT_MAX_DATACENTER_NAME_LENGTH];
    bool flushing;
    bool flushed;
    bool direct_only;

    NEXT_DECLARE_SENTINEL(1)

    uint8_t current_magic[8];

    NEXT_DECLARE_SENTINEL(2)

    void (*packet_received_callback)( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes );
    int (*send_packet_to_address_callback)( void * data, const next_address_t * address, const uint8_t * packet_data, int packet_bytes );
    void * send_packet_to_address_callback_data;

    NEXT_DECLARE_SENTINEL(3)
};

void next_server_initialize_sentinels( next_server_t * server )
{
    (void) server;
    next_assert( server );
    NEXT_INITIALIZE_SENTINEL( server, 0 )
    NEXT_INITIALIZE_SENTINEL( server, 1 )
    NEXT_INITIALIZE_SENTINEL( server, 2 )
    NEXT_INITIALIZE_SENTINEL( server, 3 )
}

void next_server_verify_sentinels( next_server_t * server )
{
    (void) server;
    next_assert( server );
    NEXT_VERIFY_SENTINEL( server, 0 )
    NEXT_VERIFY_SENTINEL( server, 1 )
    NEXT_VERIFY_SENTINEL( server, 2 )
    NEXT_VERIFY_SENTINEL( server, 3 )
    if ( server->session_manager )
        next_proxy_session_manager_verify_sentinels( server->session_manager );
    if ( server->pending_session_manager )
        next_proxy_session_manager_verify_sentinels( server->pending_session_manager );
}

void next_server_destroy( next_server_t * server );

next_server_t * next_server_create( void * context, const char * server_address, const char * bind_address, const char * datacenter, void (*packet_received_callback)( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes ) )
{
    next_assert( server_address );
    next_assert( bind_address );
    next_assert( packet_received_callback );

    next_server_t * server = (next_server_t*) next_malloc( context, sizeof(next_server_t) );
    if ( !server )
        return NULL;

    memset( server, 0, sizeof( next_server_t) );

    next_server_initialize_sentinels( server );

    server->context = context;

    server->internal = next_server_internal_create( context, server_address, bind_address, datacenter );
    if ( !server->internal )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create internal server" );
        next_server_destroy( server );
        return NULL;
    }

    server->address = server->internal->server_address;
    server->bound_port = server->internal->server_address.port;

    server->thread = next_platform_thread_create( server->context, next_server_internal_thread_function, server->internal );
    if ( !server->thread )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create server thread" );
        next_server_destroy( server );
        return NULL;
    }

    next_platform_server_thread_priority( server->thread );

    server->pending_session_manager = next_proxy_session_manager_create( context, NEXT_INITIAL_PENDING_SESSION_SIZE );
    if ( server->pending_session_manager == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create pending session manager (proxy)" );
        next_server_destroy( server );
        return NULL;
    }

    server->session_manager = next_proxy_session_manager_create( context, NEXT_INITIAL_SESSION_SIZE );
    if ( server->session_manager == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server could not create session manager (proxy)" );
        next_server_destroy( server );
        return NULL;
    }

    server->context = context;
    server->packet_received_callback = packet_received_callback;

    next_server_verify_sentinels( server );

    return server;
}

uint16_t next_server_port( next_server_t * server )
{
    next_server_verify_sentinels( server );

    return server->bound_port;
}

const next_address_t * next_server_address( next_server_t * server )
{
    next_server_verify_sentinels( server );

    return &server->address;
}

void next_server_destroy( next_server_t * server )
{
    next_server_verify_sentinels( server );

    if ( server->pending_session_manager )
    {
        next_proxy_session_manager_destroy( server->pending_session_manager );
    }

    if ( server->session_manager )
    {
        next_proxy_session_manager_destroy( server->session_manager );
    }

    if ( server->thread )
    {
        next_server_internal_quit( server->internal );
        next_platform_thread_join( server->thread );
        next_platform_thread_destroy( server->thread );
    }

    if ( server->internal )
    {
        next_server_internal_destroy( server->internal );
    }

    next_clear_and_free( server->context, server, sizeof(next_server_t) );
}

void next_server_update( next_server_t * server )
{
    next_server_verify_sentinels( server );

#if NEXT_SPIKE_TRACKING
    next_printf( NEXT_LOG_LEVEL_SPAM, "next_server_update" );
#endif // #if NEXT_SPIKE_TRACKING

    while ( true )
    {
        void * queue_entry = NULL;
        {
            next_platform_mutex_guard( &server->internal->notify_mutex );
            queue_entry = next_queue_pop( server->internal->notify_queue );
        }

        if ( queue_entry == NULL )
            break;

        next_server_notify_t * notify = (next_server_notify_t*) queue_entry;

        switch ( notify->type )
        {
            case NEXT_SERVER_NOTIFY_PACKET_RECEIVED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_PACKET_RECEIVED" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_notify_packet_received_t * packet_received = (next_server_notify_packet_received_t*) notify;
                next_assert( packet_received->packet_data );
                next_assert( packet_received->packet_bytes > 0 );
                next_assert( packet_received->packet_bytes <= NEXT_MAX_PACKET_BYTES - 1 );
#if NEXT_SPIKE_TRACKING
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_SPAM, "server calling packet received callback: from = %s, packet_bytes = %d", next_address_to_string( &packet_received->from, address_buffer ), packet_received->packet_bytes );
#endif // #if NEXT_SPIKE_TRACKING
                server->packet_received_callback( server, server->context, &packet_received->from, packet_received->packet_data, packet_received->packet_bytes );
            }
            break;

            case NEXT_SERVER_NOTIFY_SESSION_UPGRADED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_SESSION_UPDATED" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_notify_session_upgraded_t * session_upgraded = (next_server_notify_session_upgraded_t*) notify;
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_INFO, "server upgraded client %s to session %" PRIx64, next_address_to_string( &session_upgraded->address, address_buffer ), session_upgraded->session_id );
                next_proxy_session_entry_t * proxy_entry = next_proxy_session_manager_find( server->pending_session_manager, &session_upgraded->address );
                if ( proxy_entry && proxy_entry->session_id == session_upgraded->session_id )
                {
                    next_proxy_session_manager_remove_by_address( server->session_manager, &session_upgraded->address );
                    next_proxy_session_manager_remove_by_address( server->pending_session_manager, &session_upgraded->address );
                    proxy_entry = next_proxy_session_manager_add( server->session_manager, &session_upgraded->address, session_upgraded->session_id );
                }
            }
            break;

            case NEXT_SERVER_NOTIFY_PENDING_SESSION_TIMED_OUT:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_PENDING_SESSION_TIMED_OUT" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_notify_pending_session_timed_out_t * pending_session_timed_out = (next_server_notify_pending_session_timed_out_t*) notify;
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_DEBUG, "server timed out pending upgrade of client %s to session %" PRIx64, next_address_to_string( &pending_session_timed_out->address, address_buffer ), pending_session_timed_out->session_id );
                next_proxy_session_entry_t * pending_entry = next_proxy_session_manager_find( server->pending_session_manager, &pending_session_timed_out->address );
                if ( pending_entry && pending_entry->session_id == pending_session_timed_out->session_id )
                {
                    next_proxy_session_manager_remove_by_address( server->pending_session_manager, &pending_session_timed_out->address );
                    next_proxy_session_manager_remove_by_address( server->session_manager, &pending_session_timed_out->address );
                }
            }
            break;

            case NEXT_SERVER_NOTIFY_SESSION_TIMED_OUT:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_SESSION_TIMED_OUT" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_notify_session_timed_out_t * session_timed_out = (next_server_notify_session_timed_out_t*) notify;
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_INFO, "server timed out client %s from session %" PRIx64, next_address_to_string( &session_timed_out->address, address_buffer ), session_timed_out->session_id );
                next_proxy_session_entry_t * proxy_session_entry = next_proxy_session_manager_find( server->session_manager, &session_timed_out->address );
                if ( proxy_session_entry && proxy_session_entry->session_id == session_timed_out->session_id )
                {
                    next_proxy_session_manager_remove_by_address( server->session_manager, &session_timed_out->address );
                }
            }
            break;

            case NEXT_SERVER_NOTIFY_READY:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_READY" );
#endif // #if NEXT_SPIKE_TRACKING
                next_server_notify_ready_t * ready = (next_server_notify_ready_t*) notify;
                next_copy_string( server->datacenter_name, ready->datacenter_name, NEXT_MAX_DATACENTER_NAME_LENGTH );
                server->ready = true;
                next_printf( NEXT_LOG_LEVEL_INFO, "server datacenter is '%s'", ready->datacenter_name );
                next_printf( NEXT_LOG_LEVEL_INFO, "server is ready to receive client connections" );
            }
            break;

            case NEXT_SERVER_NOTIFY_FLUSH_FINISHED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_FLUSH_FINISHED" );
#endif // #if NEXT_SPIKE_TRACKING
                server->flushed = true;
            }
            break;

            case NEXT_SERVER_NOTIFY_MAGIC_UPDATED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_MAGIC_UPDATED" );
#endif // #if NEXT_SPIKE_TRACKING

                next_server_notify_magic_updated_t * magic_updated = (next_server_notify_magic_updated_t*) notify;

                memcpy( server->current_magic, magic_updated->current_magic, 8 );

                next_printf( NEXT_LOG_LEVEL_DEBUG, "server current magic: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
                    server->current_magic[0],
                    server->current_magic[1],
                    server->current_magic[2],
                    server->current_magic[3],
                    server->current_magic[4],
                    server->current_magic[5],
                    server->current_magic[6],
                    server->current_magic[7] );
            }
            break;

            case NEXT_SERVER_NOTIFY_DIRECT_ONLY:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "server received NEXT_SERVER_NOTIFY_DIRECT_ONLY" );
#endif // #if NEXT_SPIKE_TRACKING
                server->direct_only = true;
            }
            break;

            default: break;
        }

        next_free( server->context, queue_entry );
    }
}

uint64_t next_generate_session_id()
{
    uint64_t session_id = 0;
    while ( session_id == 0 )
    {
        next_crypto_random_bytes( (uint8_t*) &session_id, 8 );
    }
    return session_id;
}

uint64_t next_server_upgrade_session( next_server_t * server, const next_address_t * address, const char * user_id )
{
    next_server_verify_sentinels( server );

    next_assert( server->internal );

    // send upgrade session command to internal server

    next_server_command_upgrade_session_t * command = (next_server_command_upgrade_session_t*) next_malloc( server->context, sizeof( next_server_command_upgrade_session_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server upgrade session failed. could not create upgrade session command" );
        return 0;
    }

    uint64_t session_id = next_generate_session_id();

    uint64_t user_hash = ( user_id != NULL ) ? next_hash_string( user_id ) : 0;

    command->type = NEXT_SERVER_COMMAND_UPGRADE_SESSION;
    command->address = *address;
    command->user_hash = user_hash;
    command->session_id = session_id;

    {
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "server queues up NEXT_SERVER_COMMAND_UPGRADE_SESSION from %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &server->internal->command_mutex );
        next_queue_push( server->internal->command_queue, command );
    }

    // remove any existing entry for this address. latest upgrade takes precedence

    next_proxy_session_manager_remove_by_address( server->session_manager, address );
    next_proxy_session_manager_remove_by_address( server->pending_session_manager, address );

    // add a new pending session entry for this address

    next_proxy_session_entry_t * entry = next_proxy_session_manager_add( server->pending_session_manager, address, session_id );

    if ( entry == NULL )
    {
        next_assert( !"could not add pending session entry. this should never happen!" );
        return 0;
    }

    return session_id;
}

bool next_server_session_upgraded( next_server_t * server, const next_address_t * address )
{
    next_server_verify_sentinels( server );

    next_assert( server->internal );

    next_proxy_session_entry_t * pending_entry = next_proxy_session_manager_find( server->pending_session_manager, address );
    if ( pending_entry != NULL )
        return true;

    next_proxy_session_entry_t * entry = next_proxy_session_manager_find( server->session_manager, address );
    if ( entry != NULL )
        return true;

    return false;
}

void next_server_send_packet_to_address( next_server_t * server, const next_address_t * address, const uint8_t * packet_data, int packet_bytes )
{
    next_server_verify_sentinels( server );

    next_assert( address );
    next_assert( address->type != NEXT_ADDRESS_NONE );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    if ( server->send_packet_to_address_callback )
    {
        void * callback_data = server->send_packet_to_address_callback_data;
        if ( server->send_packet_to_address_callback( callback_data, address, packet_data, packet_bytes ) != 0 )
            return;
    }

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( server->internal->socket, address, packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING
}

void next_server_send_packet( next_server_t * server, const next_address_t * to_address, const uint8_t * packet_data, int packet_bytes )
{
    next_server_verify_sentinels( server );

    next_assert( to_address );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    if ( next_global_config.disable_network_next )
    {
        next_server_send_packet_direct( server, to_address, packet_data, packet_bytes );
        return;
    }

    if ( packet_bytes > NEXT_MAX_PACKET_BYTES - 1 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server can't send packet because packet size is too large" );
        return;
    }

    next_proxy_session_entry_t * entry = next_proxy_session_manager_find( server->session_manager, to_address );

    bool send_over_network_next = false;
    bool send_upgraded_direct = false;

    if ( entry && packet_bytes <= NEXT_MTU )
    {
        bool multipath = false;
        int envelope_kbps_down = 0;
        uint8_t open_session_sequence = 0;
        uint64_t send_sequence = 0;
        uint64_t session_id = 0;
        uint8_t session_version = 0;
        next_address_t session_address;
        double last_upgraded_packet_receive_time = 0.0;
        uint8_t session_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

        next_session_entry_t * internal_entry = NULL;
        {
            next_platform_mutex_guard( &server->internal->session_mutex );
            internal_entry = next_session_manager_find_by_address( server->internal->session_manager, to_address );
            if ( internal_entry )
            {
                last_upgraded_packet_receive_time = internal_entry->last_upgraded_packet_receive_time;
            }
        }

        // IMPORTANT: If we haven't received any upgraded packets in the last second send passthrough packets.
        // This makes reconnect robust when a client reconnects using the same port number.
        if ( !internal_entry || last_upgraded_packet_receive_time + 1.0 < next_platform_time() )
        {
            next_server_send_packet_direct( server, to_address, packet_data, packet_bytes );
            return;
        }

        {
            next_platform_mutex_guard( &server->internal->session_mutex );
            multipath = internal_entry->mutex_multipath;
            envelope_kbps_down = internal_entry->mutex_envelope_kbps_down;
            send_over_network_next = internal_entry->mutex_send_over_network_next;
            send_upgraded_direct = !send_over_network_next;
            send_sequence = internal_entry->mutex_payload_send_sequence++;
            open_session_sequence = internal_entry->client_open_session_sequence;
            session_id = internal_entry->mutex_session_id;
            session_version = internal_entry->mutex_session_version;
            session_address = internal_entry->mutex_send_address;
            memcpy( session_private_key, internal_entry->mutex_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
            internal_entry->stats_packets_sent_server_to_client++;
        }

        if ( multipath )
        {
            send_upgraded_direct = true;
        }

        if ( send_over_network_next )
        {
            const int wire_packet_bits = next_wire_packet_bits( packet_bytes );

            bool over_budget = next_bandwidth_limiter_add_packet( &entry->send_bandwidth, next_platform_time(), envelope_kbps_down, wire_packet_bits );

            if ( over_budget )
            {
                next_printf( NEXT_LOG_LEVEL_WARN, "server exceeded bandwidth budget for session %" PRIx64 " (%d kbps)", session_id, envelope_kbps_down );
                {
                    next_platform_mutex_guard( &server->internal->session_mutex );
                    internal_entry->stats_server_bandwidth_over_limit = true;
                }
                send_over_network_next = false;
                if ( !multipath )
                {
                    send_upgraded_direct = true;
                }
            }
        }

        if ( send_over_network_next )
        {
            // send over network next

            uint8_t from_address_data[32];
            uint8_t to_address_data[32];
            int from_address_bytes;
            int to_address_bytes;

            next_address_data( &server->address, from_address_data, &from_address_bytes );
            next_address_data( &session_address, to_address_data, &to_address_bytes );

            uint8_t next_packet_data[NEXT_MAX_PACKET_BYTES];

            int next_packet_bytes = next_write_server_to_client_packet( next_packet_data, send_sequence, session_id, session_version, session_private_key, packet_data, packet_bytes, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

            next_assert( next_packet_bytes > 0 );

            next_assert( next_basic_packet_filter( next_packet_data, next_packet_bytes ) );
            next_assert( next_advanced_packet_filter( next_packet_data, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, next_packet_bytes ) );

            next_server_send_packet_to_address( server, &session_address, next_packet_data, next_packet_bytes );
        }

        if ( send_upgraded_direct )
        {
            // direct packet

            uint8_t from_address_data[32];
            uint8_t to_address_data[32];
            int from_address_bytes = 0;
            int to_address_bytes = 0;

            next_address_data( &server->address, from_address_data, &from_address_bytes );
            next_address_data( to_address, to_address_data, &to_address_bytes );

            uint8_t direct_packet_data[NEXT_MAX_PACKET_BYTES];

            int direct_packet_bytes = next_write_direct_packet( direct_packet_data, open_session_sequence, send_sequence, packet_data, packet_bytes, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

            next_assert( direct_packet_bytes >= 27 );
            next_assert( direct_packet_bytes <= NEXT_MTU + 27 );
            next_assert( direct_packet_data[0] == NEXT_DIRECT_PACKET );

            next_assert( next_basic_packet_filter( direct_packet_data, direct_packet_bytes ) );
            next_assert( next_advanced_packet_filter( direct_packet_data, server->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, direct_packet_bytes ) );

            next_server_send_packet_to_address( server, to_address, direct_packet_data, direct_packet_bytes );
        }
    }
    else
    {
        // passthrough packet

        next_server_send_packet_direct( server, to_address, packet_data, packet_bytes );
    }
}

void next_server_send_packet_direct( next_server_t * server, const next_address_t * to_address, const uint8_t * packet_data, int packet_bytes )
{
    next_server_verify_sentinels( server );

    next_assert( to_address );
    next_assert( to_address->type != NEXT_ADDRESS_NONE );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    if ( packet_bytes > NEXT_MAX_PACKET_BYTES - 1 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server can't send packet because packet size is too large\n" );
        return;
    }

    uint8_t buffer[NEXT_MAX_PACKET_BYTES];
    buffer[0] = NEXT_PASSTHROUGH_PACKET;
    memcpy( buffer + 1, packet_data, packet_bytes );
    next_server_send_packet_to_address( server, to_address, buffer, packet_bytes + 1 );
}

void next_server_send_packet_raw( struct next_server_t * server, const struct next_address_t * to_address, const uint8_t * packet_data, int packet_bytes )
{
    next_server_verify_sentinels( server );

    next_assert( to_address );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( server->internal->socket, to_address, packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING
}

bool next_server_stats( next_server_t * server, const next_address_t * address, next_server_stats_t * stats )
{
    next_assert( server );
    next_assert( address );
    next_assert( stats );

    next_platform_mutex_guard( &server->internal->session_mutex );

    next_session_entry_t * entry = next_session_manager_find_by_address( server->internal->session_manager, address );
    if ( !entry )
        return false;

    stats->session_id = entry->session_id;
    stats->user_hash = entry->user_hash;
    stats->platform_id = entry->stats_platform_id;
    stats->connection_type = entry->stats_connection_type;
    stats->next = entry->stats_next;
    stats->multipath = entry->stats_multipath;
    stats->reported = entry->stats_reported;
    stats->fallback_to_direct = entry->stats_fallback_to_direct;
    stats->direct_rtt = entry->stats_direct_rtt;
    stats->direct_jitter = entry->stats_direct_jitter;
    stats->direct_packet_loss = entry->stats_direct_packet_loss;
    stats->direct_max_packet_loss_seen = entry->stats_direct_max_packet_loss_seen;
    stats->next_rtt = entry->stats_next_rtt;
    stats->next_jitter = entry->stats_next_jitter;
    stats->next_packet_loss = entry->stats_next_packet_loss;
    stats->direct_kbps_up = entry->stats_direct_kbps_up;
    stats->direct_kbps_down = entry->stats_direct_kbps_down;
    stats->next_kbps_up = entry->stats_next_kbps_up;
    stats->next_kbps_down = entry->stats_next_kbps_down;
    stats->packets_sent_client_to_server = entry->stats_packets_sent_client_to_server;
    stats->packets_sent_server_to_client = entry->stats_packets_sent_server_to_client;
    stats->packets_lost_client_to_server = entry->stats_packets_lost_client_to_server;
    stats->packets_lost_server_to_client = entry->stats_packets_lost_server_to_client;
    stats->packets_out_of_order_client_to_server = entry->stats_packets_out_of_order_client_to_server;
    stats->packets_out_of_order_server_to_client = entry->stats_packets_out_of_order_server_to_client;
    stats->jitter_client_to_server = entry->stats_jitter_client_to_server;
    stats->jitter_server_to_client = entry->stats_jitter_server_to_client;

    return true;
}

bool next_server_ready( next_server_t * server ) 
{
    next_server_verify_sentinels( server );
    return ( next_global_config.disable_network_next || server->ready ) ? true : false;
}

const char * next_server_datacenter( next_server_t * server )
{
    next_server_verify_sentinels( server );

    return server->datacenter_name;
}

void next_server_session_event( struct next_server_t * server, const struct next_address_t * address, uint64_t session_events )
{
    next_assert( server );
    next_assert( address );
    next_assert( server->internal );

    if ( server->flushing )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignoring session event. server is flushed" );
        return;
    }
    
    // send session event command to internal server

    next_server_command_session_event_t * command = (next_server_command_session_event_t*) next_malloc( server->context, sizeof( next_server_command_session_event_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "session event failed. could not create session event command" );
        return;
    }

    command->type = NEXT_SERVER_COMMAND_SESSION_EVENT;
    command->address = *address;
    command->session_events = session_events;

    {    
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "server queues up NEXT_SERVER_COMMAND_SERVER_EVENT from %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &server->internal->command_mutex );
        next_queue_push( server->internal->command_queue, command );
    }
}

void next_server_flush( struct next_server_t * server )
{
    next_assert( server );

    if ( next_global_config.disable_network_next == true )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignoring server flush. network next is disabled" );
        return;
    }

    if ( server->flushing )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignoring server flush. server is already flushed" );
        return;
    }

    // send flush command to internal server

    next_server_command_flush_t * command = (next_server_command_flush_t*) next_malloc( server->context, sizeof( next_server_command_flush_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server flush failed. could not create server flush command" );
        return;
    }

    command->type = NEXT_SERVER_COMMAND_FLUSH;

    {    
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "server queues up NEXT_SERVER_COMMAND_FLUSH from %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &server->internal->command_mutex );
        next_queue_push( server->internal->command_queue, command );
    }

    server->flushing = true;

    next_printf( NEXT_LOG_LEVEL_INFO, "server flush started" );

    double flush_timeout = next_platform_time() + NEXT_SERVER_FLUSH_TIMEOUT;

    while ( !server->flushed && next_platform_time() < flush_timeout )
    {
        next_server_update( server );
        
        next_platform_sleep( 0.1 );
    }

    if ( next_platform_time() > flush_timeout )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server flush timed out :(" );
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server flush finished" );    
    }
}

void next_server_set_packet_receive_callback( struct next_server_t * server, void (*callback) ( void * data, next_address_t * from, uint8_t * packet_data, int * begin, int * end ), void * callback_data )
{
    next_assert( server );

    next_server_command_set_packet_receive_callback_t * command = (next_server_command_set_packet_receive_callback_t*) next_malloc( server->context, sizeof( next_server_command_set_packet_receive_callback_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server set packet receive callback failed. could not create command" );
        return;
    }

    command->type = NEXT_SERVER_COMMAND_SET_PACKET_RECEIVE_CALLBACK;
    command->callback = callback;
    command->callback_data = callback_data;

    {    
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "server queues up NEXT_SERVER_COMMAND_SET_PACKET_RECEIVE_CALLBACK from %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &server->internal->command_mutex );
        next_queue_push( server->internal->command_queue, command );
    }
}

void next_server_set_send_packet_to_address_callback( struct next_server_t * server, int (*callback) ( void * data, const next_address_t * from, const uint8_t * packet_data, int packet_bytes ), void * callback_data )
{
    next_assert( server );

    server->send_packet_to_address_callback = callback;
    server->send_packet_to_address_callback_data = callback_data;

    next_server_command_set_send_packet_to_address_callback_t * command = (next_server_command_set_send_packet_to_address_callback_t*) next_malloc( server->context, sizeof( next_server_command_set_send_packet_to_address_callback_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server set send packet to address callback failed. could not create command" );
        return;
    }

    command->type = NEXT_SERVER_COMMAND_SET_SEND_PACKET_TO_ADDRESS_CALLBACK;
    command->callback = callback;
    command->callback_data = callback_data;

    {    
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "server queues up NEXT_SERVER_COMMAND_SEND_PACKET_TO_ADDRESS_CALLBACK from %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &server->internal->command_mutex );
        next_queue_push( server->internal->command_queue, command );
    }
}

void next_server_set_payload_receive_callback( struct next_server_t * server, int (*callback) ( void * data, const next_address_t * client_address, const uint8_t * payload_data, int payload_bytes ), void * callback_data )
{
    next_assert( server );

    next_server_command_set_payload_receive_callback_t * command = (next_server_command_set_payload_receive_callback_t*) next_malloc( server->context, sizeof( next_server_command_set_payload_receive_callback_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server set payload receive callback failed. could not create command" );
        return;
    }

    command->type = NEXT_SERVER_COMMAND_SET_PAYLOAD_RECEIVE_CALLBACK;
    command->callback = callback;
    command->callback_data = callback_data;

    {    
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "server queues up NEXT_SERVER_COMMAND_SEND_PACKET_TO_ADDRESS_CALLBACK from %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &server->internal->command_mutex );
        next_queue_push( server->internal->command_queue, command );
    }
}

bool next_server_direct_only( struct next_server_t * server )
{
    next_assert( server );
    return server->direct_only;
}

// ---------------------------------------------------------------

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

#include "next_client.h"
#include "next_memory_checks.h"
#include "next_queue.h"
#include "next_platform.h"
#include "next_relay_manager.h"
#include "next_route_manager.h"
#include "next_packet_loss_tracker.h"
#include "next_out_of_order_tracker.h"
#include "next_jitter_tracker.h"
#include "next_bandwidth_limiter.h"
#include "next_replay_protection.h"
#include "next_read_write.h"
#include "next_header.h"
#include "next_internal_config.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>

// ---------------------------------------------------------------

#define NEXT_CLIENT_COMMAND_OPEN_SESSION            0
#define NEXT_CLIENT_COMMAND_CLOSE_SESSION           1
#define NEXT_CLIENT_COMMAND_DESTROY                 2
#define NEXT_CLIENT_COMMAND_REPORT_SESSION          3

struct next_client_command_t
{
    int type;
};

struct next_client_command_open_session_t : public next_client_command_t
{
    next_address_t server_address;
};

struct next_client_command_close_session_t : public next_client_command_t
{
    // ...
};

struct next_client_command_destroy_t : public next_client_command_t
{
    // ...
};

struct next_client_command_report_session_t : public next_client_command_t
{
    // ...
};

// ---------------------------------------------------------------

#define NEXT_CLIENT_NOTIFY_PACKET_RECEIVED          0
#define NEXT_CLIENT_NOTIFY_UPGRADED                 1
#define NEXT_CLIENT_NOTIFY_STATS_UPDATED            2
#define NEXT_CLIENT_NOTIFY_MAGIC_UPDATED            3
#define NEXT_CLIENT_NOTIFY_READY                    4

struct next_client_notify_t
{
    int type;
};

struct next_client_notify_packet_received_t : public next_client_notify_t
{
    bool direct;
    bool already_received;
    int payload_bytes;
    uint8_t payload_data[NEXT_MAX_PACKET_BYTES-1];
};

struct next_client_notify_upgraded_t : public next_client_notify_t
{
    uint64_t session_id;
    next_address_t client_external_address;
    uint8_t current_magic[8];
};

struct next_client_notify_stats_updated_t : public next_client_notify_t
{
    next_client_stats_t stats;
    bool fallback_to_direct;
};

struct next_client_notify_magic_updated_t : public next_client_notify_t
{
    uint8_t current_magic[8];
};

struct next_client_notify_ready_t : public next_client_notify_t
{
};

// ---------------------------------------------------------------

struct next_client_internal_t;

void next_client_internal_initialize_sentinels( next_client_internal_t * client );

void next_client_internal_verify_sentinels( next_client_internal_t * client );

void next_client_internal_destroy( next_client_internal_t * client );

next_client_internal_t * next_client_internal_create( void * context, const char * bind_address_string );

void next_client_internal_destroy( next_client_internal_t * client );

int next_client_internal_send_packet_to_server( next_client_internal_t * client, uint8_t packet_id, void * packet_object );

void next_client_internal_process_network_next_packet( next_client_internal_t * client, const next_address_t * from, uint8_t * packet_data, int packet_bytes, double packet_receive_time );

void next_client_internal_process_passthrough_packet( next_client_internal_t * client, const next_address_t * from, uint8_t * packet_data, int packet_bytes );

void next_client_internal_block_and_receive_packet( next_client_internal_t * client );

bool next_client_internal_pump_commands( next_client_internal_t * client );

void next_client_internal_update_stats( next_client_internal_t * client );

void next_client_internal_update_direct_pings( next_client_internal_t * client );

void next_client_internal_update_next_pings( next_client_internal_t * client );

void next_client_internal_send_pings_to_near_relays( next_client_internal_t * client );

void next_client_internal_update_fallback_to_direct( next_client_internal_t * client );

void next_client_internal_update_route_manager( next_client_internal_t * client );

void next_client_internal_update_upgrade_response( next_client_internal_t * client );

void next_client_internal_update( next_client_internal_t * client );

// ---------------------------------------------------------------

extern next_internal_config_t next_global_config;

extern int next_signed_packets[256];

extern int next_encrypted_packets[256];

// ---------------------------------------------------------------

struct next_client_internal_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    next_queue_t * command_queue;
    next_queue_t * notify_queue;
    next_platform_socket_t * socket;
    next_platform_mutex_t command_mutex;
    next_platform_mutex_t notify_mutex;
    next_address_t server_address;
    next_address_t client_external_address;     // IMPORTANT: only known post-upgrade
    uint16_t bound_port;
    bool session_open;
    bool upgraded;
    bool reported;
    bool fallback_to_direct;
    bool multipath;
    uint8_t open_session_sequence;
    uint64_t upgrade_sequence;
    uint64_t session_id;
    uint64_t special_send_sequence;
    uint64_t internal_send_sequence;
    double last_next_ping_time;
    double last_next_pong_time;
    double last_direct_ping_time;
    double last_direct_pong_time;
    double last_stats_update_time;
    double last_stats_report_time;
    double last_route_switch_time;
    double route_update_timeout_time;
    uint64_t route_update_sequence;
    uint8_t upcoming_magic[8];
    uint8_t current_magic[8];
    uint8_t previous_magic[8];

    NEXT_DECLARE_SENTINEL(1)

    std::atomic<uint64_t> packets_sent;

    NEXT_DECLARE_SENTINEL(2)

    next_relay_manager_t * near_relay_manager;
    next_route_manager_t * route_manager;
    next_platform_mutex_t route_manager_mutex;

    NEXT_DECLARE_SENTINEL(3)

    next_packet_loss_tracker_t packet_loss_tracker;
    next_out_of_order_tracker_t out_of_order_tracker;
    next_jitter_tracker_t jitter_tracker;

    NEXT_DECLARE_SENTINEL(4)

    uint8_t buyer_public_key[NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
    uint8_t client_kx_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];
    uint8_t client_kx_private_key[NEXT_CRYPTO_KX_SECRETKEYBYTES];
    uint8_t client_send_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    uint8_t client_receive_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    uint8_t client_route_public_key[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];
    uint8_t client_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(5)

    next_client_stats_t client_stats;

    NEXT_DECLARE_SENTINEL(6)

    next_relay_stats_t near_relay_stats;

    NEXT_DECLARE_SENTINEL(7)

    next_ping_history_t next_ping_history;
    next_ping_history_t direct_ping_history;

    NEXT_DECLARE_SENTINEL(8)

    next_replay_protection_t payload_replay_protection;
    next_replay_protection_t special_replay_protection;
    next_replay_protection_t internal_replay_protection;

    NEXT_DECLARE_SENTINEL(9)

    next_platform_mutex_t direct_bandwidth_mutex;
    float direct_bandwidth_usage_kbps_up;
    float direct_bandwidth_usage_kbps_down;

    NEXT_DECLARE_SENTINEL(10)

    next_platform_mutex_t next_bandwidth_mutex;
    bool next_bandwidth_over_limit;
    float next_bandwidth_usage_kbps_up;
    float next_bandwidth_usage_kbps_down;
    float next_bandwidth_envelope_kbps_up;
    float next_bandwidth_envelope_kbps_down;

    NEXT_DECLARE_SENTINEL(11)

    bool sending_upgrade_response;
    double upgrade_response_start_time;
    double last_upgrade_response_send_time;
    int upgrade_response_packet_bytes;
    uint8_t upgrade_response_packet_data[NEXT_MAX_PACKET_BYTES];

    NEXT_DECLARE_SENTINEL(12)

    std::atomic<uint64_t> counters[NEXT_CLIENT_COUNTER_MAX];

    NEXT_DECLARE_SENTINEL(13)
};

void next_client_internal_initialize_sentinels( next_client_internal_t * client )
{
    (void) client;
    next_assert( client );
    NEXT_INITIALIZE_SENTINEL( client, 0 )
    NEXT_INITIALIZE_SENTINEL( client, 1 )
    NEXT_INITIALIZE_SENTINEL( client, 2 )
    NEXT_INITIALIZE_SENTINEL( client, 3 )
    NEXT_INITIALIZE_SENTINEL( client, 4 )
    NEXT_INITIALIZE_SENTINEL( client, 5 )
    NEXT_INITIALIZE_SENTINEL( client, 6 )
    NEXT_INITIALIZE_SENTINEL( client, 7 )
    NEXT_INITIALIZE_SENTINEL( client, 8 )
    NEXT_INITIALIZE_SENTINEL( client, 9 )
    NEXT_INITIALIZE_SENTINEL( client, 10 )
    NEXT_INITIALIZE_SENTINEL( client, 11 )
    NEXT_INITIALIZE_SENTINEL( client, 12 )
    NEXT_INITIALIZE_SENTINEL( client, 13 )

    next_replay_protection_initialize_sentinels( &client->payload_replay_protection );
    next_replay_protection_initialize_sentinels( &client->special_replay_protection );
    next_replay_protection_initialize_sentinels( &client->internal_replay_protection );

    next_relay_stats_initialize_sentinels( &client->near_relay_stats );

    next_ping_history_initialize_sentinels( &client->next_ping_history );
    next_ping_history_initialize_sentinels( &client->direct_ping_history );
}

void next_client_internal_verify_sentinels( next_client_internal_t * client )
{
    (void) client;

    next_assert( client );

    NEXT_VERIFY_SENTINEL( client, 0 )
    NEXT_VERIFY_SENTINEL( client, 1 )
    NEXT_VERIFY_SENTINEL( client, 2 )
    NEXT_VERIFY_SENTINEL( client, 3 )
    NEXT_VERIFY_SENTINEL( client, 4 )
    NEXT_VERIFY_SENTINEL( client, 5 )
    NEXT_VERIFY_SENTINEL( client, 6 )
    NEXT_VERIFY_SENTINEL( client, 7 )
    NEXT_VERIFY_SENTINEL( client, 8 )
    NEXT_VERIFY_SENTINEL( client, 9 )
    NEXT_VERIFY_SENTINEL( client, 10 )
    NEXT_VERIFY_SENTINEL( client, 11 )
    NEXT_VERIFY_SENTINEL( client, 12 )
    NEXT_VERIFY_SENTINEL( client, 13 )

    if ( client->command_queue )
        next_queue_verify_sentinels( client->command_queue );

    if ( client->notify_queue )
        next_queue_verify_sentinels( client->notify_queue );

    next_replay_protection_verify_sentinels( &client->payload_replay_protection );
    next_replay_protection_verify_sentinels( &client->special_replay_protection );
    next_replay_protection_verify_sentinels( &client->internal_replay_protection );

    next_relay_stats_verify_sentinels( &client->near_relay_stats );

    if ( client->near_relay_manager )
        next_relay_manager_verify_sentinels( client->near_relay_manager );

    next_ping_history_verify_sentinels( &client->next_ping_history );
    next_ping_history_verify_sentinels( &client->direct_ping_history );

    if ( client->route_manager )
        next_route_manager_verify_sentinels( client->route_manager );
}

next_client_internal_t * next_client_internal_create( void * context, const char * bind_address_string )
{
#if !NEXT_DEVELOPMENT
    next_printf( NEXT_LOG_LEVEL_INFO, "client sdk version is %s", NEXT_VERSION_FULL );
#endif // #if !NEXT_DEVELOPMENT

    next_address_t bind_address;
    if ( next_address_parse( &bind_address, bind_address_string ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client failed to parse bind address: %s", bind_address_string );
        return NULL;
    }

    next_client_internal_t * client = (next_client_internal_t*) next_malloc( context, sizeof(next_client_internal_t) );
    if ( !client )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "could not create internal client" );
        return NULL;
    }

    char * just_clear_it_and_dont_complain = (char*) client;
    memset( just_clear_it_and_dont_complain, 0, sizeof(next_client_internal_t) );

    next_client_internal_initialize_sentinels( client );

    next_client_internal_verify_sentinels( client );

    client->context = context;

    memcpy( client->buyer_public_key, next_global_config.buyer_public_key, NEXT_CRYPTO_SIGN_PUBLICKEYBYTES );

    next_client_internal_verify_sentinels( client );

    client->command_queue = next_queue_create( context, NEXT_COMMAND_QUEUE_LENGTH );
    if ( !client->command_queue )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create client command queue" );
        next_client_internal_destroy( client );
        return NULL;
    }

    next_client_internal_verify_sentinels( client );

    client->notify_queue = next_queue_create( context, NEXT_NOTIFY_QUEUE_LENGTH );
    if ( !client->notify_queue )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create client notify queue" );
        next_client_internal_destroy( client );
        return NULL;
    }

    next_client_internal_verify_sentinels( client );

    // IMPORTANT: for many platforms it's best practice to bind to ipv6 and go dual stack on the client
    if ( next_platform_client_dual_stack() )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "client socket is dual stack ipv4 and ipv6" );
        bind_address.type = NEXT_ADDRESS_IPV6;
        memset( bind_address.data.ipv6, 0, sizeof(bind_address.data.ipv6) );
    }

    // IMPORTANT: some platforms (GDK) have a preferred port that we must use to access packet tagging
    // If the bind address has set port of 0, substitute the preferred client port here
    if ( bind_address.port == 0 )
    {
        int preferred_client_port = next_platform_preferred_client_port();
        if ( preferred_client_port != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "client socket using preferred port %d", preferred_client_port );
            bind_address.port = preferred_client_port;
        }
    }

    client->socket = next_platform_socket_create( client->context, &bind_address, NEXT_PLATFORM_SOCKET_BLOCKING, 0.1f, next_global_config.socket_send_buffer_size, next_global_config.socket_receive_buffer_size, true );
    if ( client->socket == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create socket" );
        next_client_internal_destroy( client );
        return NULL;
    }

    char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_printf( NEXT_LOG_LEVEL_INFO, "client bound to %s", next_address_to_string( &bind_address, address_string ) );
    client->bound_port = bind_address.port;

    int result = next_platform_mutex_create( &client->command_mutex );
    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create command mutex" );
        next_client_internal_destroy( client );
        return NULL;
    }

    result = next_platform_mutex_create( &client->notify_mutex );
    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create notify mutex" );
        next_client_internal_destroy( client );
        return NULL;
    }

    client->near_relay_manager = next_relay_manager_create( context );
    if ( !client->near_relay_manager )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create near relay manager" );
        next_client_internal_destroy( client );
        return NULL;
    }

    client->route_manager = next_route_manager_create( context );
    if ( !client->route_manager )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create route manager" );
        next_client_internal_destroy( client );
        return NULL;
    }

    result = next_platform_mutex_create( &client->route_manager_mutex );
    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create client route manager mutex" );
        next_client_internal_destroy( client );
        return NULL;
    }

    result = next_platform_mutex_create( &client->direct_bandwidth_mutex );
    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create direct bandwidth mutex" );
        next_client_internal_destroy( client );
        return NULL;
    }

    result = next_platform_mutex_create( &client->next_bandwidth_mutex );
    if ( result != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create next bandwidth mutex" );
        next_client_internal_destroy( client );
        return NULL;
    }

    next_ping_history_clear( &client->next_ping_history );
    next_ping_history_clear( &client->direct_ping_history );

    next_replay_protection_reset( &client->payload_replay_protection );
    next_replay_protection_reset( &client->special_replay_protection );
    next_replay_protection_reset( &client->internal_replay_protection );

    next_packet_loss_tracker_reset( &client->packet_loss_tracker );
    next_out_of_order_tracker_reset( &client->out_of_order_tracker );
    next_jitter_tracker_reset( &client->jitter_tracker );

    next_client_internal_verify_sentinels( client );

    client->special_send_sequence = 1;
    client->internal_send_sequence = 1;

    return client;
}

void next_client_internal_destroy( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    if ( client->socket )
    {
        next_platform_socket_destroy( client->socket );
    }
    if ( client->command_queue )
    {
        next_queue_destroy( client->command_queue );
    }
    if ( client->notify_queue )
    {
        next_queue_destroy( client->notify_queue );
    }
    if ( client->near_relay_manager )
    {
        next_relay_manager_destroy( client->near_relay_manager );
    }
    if ( client->route_manager )
    {
        next_route_manager_destroy( client->route_manager );
    }

    next_platform_mutex_destroy( &client->command_mutex );
    next_platform_mutex_destroy( &client->notify_mutex );
    next_platform_mutex_destroy( &client->route_manager_mutex );
    next_platform_mutex_destroy( &client->direct_bandwidth_mutex );
    next_platform_mutex_destroy( &client->next_bandwidth_mutex );

    next_clear_and_free( client->context, client, sizeof(next_client_internal_t) );
}

int next_client_internal_send_packet_to_server( next_client_internal_t * client, uint8_t packet_id, void * packet_object )
{
    next_client_internal_verify_sentinels( client );

    next_assert( packet_object );
    next_assert( client->session_open );

    if ( !client->session_open )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client can't send internal packet to server because no session is open" );
        return NEXT_ERROR;
    }

    int packet_bytes = 0;

    uint8_t buffer[NEXT_MAX_PACKET_BYTES];

    uint8_t from_address_data[32];
    uint8_t to_address_data[32];
    int from_address_bytes;
    int to_address_bytes;

    next_address_data( &client->client_external_address, from_address_data, &from_address_bytes );
    next_address_data( &client->server_address, to_address_data, &to_address_bytes );

    if ( next_write_packet( packet_id, packet_object, buffer, &packet_bytes, next_signed_packets, next_encrypted_packets, &client->internal_send_sequence, NULL, client->client_send_key, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client failed to write internal packet type %d", packet_id );
        return NEXT_ERROR;
    }

    next_assert( next_basic_packet_filter( buffer, sizeof(buffer) ) );
    next_assert( next_advanced_packet_filter( buffer, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( client->socket, &client->server_address, buffer, packet_bytes );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING

    return NEXT_OK;
}

void next_client_internal_process_network_next_packet( next_client_internal_t * client, const next_address_t * from, uint8_t * packet_data, int packet_bytes, double packet_receive_time )
{
    next_client_internal_verify_sentinels( client );

    next_assert( from );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_BYTES );

    const bool from_server_address = client->server_address.type != 0 && next_address_equal( from, &client->server_address );

    const int packet_id = packet_data[0];

#if NEXT_ASSERTS
    {
        char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing packet type %d from %s (%d bytes)", packet_id, next_address_to_string( &client->server_address, address_buffer ), packet_bytes );
    }
#endif // #if NEXT_ASSERTS

    // run packet filters
    {
        if ( !next_basic_packet_filter( packet_data, packet_bytes ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client basic packet filter dropped packet (%d)", packet_id );
            return;
        }

        uint8_t from_address_data[32];
        uint8_t to_address_data[32];
        int from_address_bytes = 0;
        int to_address_bytes = 0;

        next_address_data( from, from_address_data, &from_address_bytes );
        next_address_data( &client->client_external_address, to_address_data, &to_address_bytes );

        if ( packet_id != NEXT_UPGRADE_REQUEST_PACKET )
        {
            if ( !next_advanced_packet_filter( packet_data, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) )
            {
                if ( !next_advanced_packet_filter( packet_data, client->upcoming_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) )
                {
                    if ( !next_advanced_packet_filter( packet_data, client->previous_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) )
                    {
                        next_printf( NEXT_LOG_LEVEL_DEBUG, "client advanced packet filter dropped packet (%d)", packet_id );
                    }
                    return;
                }
            }
        }
        else
        {
            uint8_t magic[8];
            memset( magic, 0, sizeof(magic) );
            to_address_bytes = 0;
            if ( !next_advanced_packet_filter( packet_data, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "client advanced packet filter dropped packet (upgrade request)" );
                return;
            }
        }
    }

    // upgrade request packet (not encrypted)

    if ( !client->upgraded && from_server_address && packet_id == NEXT_UPGRADE_REQUEST_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing upgrade request packet" );

        if ( !next_address_equal( from, &client->server_address ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. packet does not come from server address" );
            return;
        }

        if ( client->fallback_to_direct )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. in fallback to direct state" );
            return;
        }

        if ( next_global_config.disable_network_next )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. network next is disabled" );
            return;
        }

        // todo: header

        NextUpgradeRequestPacket packet;
        int begin = 16;
        int end = packet_bytes - 2;
        if ( next_read_packet( NEXT_UPGRADE_REQUEST_PACKET, packet_data, begin, end, &packet, NULL, NULL, NULL, NULL, NULL, NULL ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. failed to read" );
            return;
        }

        if ( packet.protocol_version != next_protocol_version() )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. protocol version mismatch" );
            return;
        }

        next_post_validate_packet( NEXT_UPGRADE_REQUEST_PACKET, NULL, NULL, NULL );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client received upgrade request packet from server" );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client initial magic: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x | %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x | %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
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

        memcpy( client->upcoming_magic, packet.upcoming_magic, 8 );
        memcpy( client->current_magic, packet.current_magic, 8 );
        memcpy( client->previous_magic, packet.previous_magic, 8 );

        client->client_external_address = packet.client_address;

        char address_buffer[256];
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client external address is %s", next_address_to_string( &client->client_external_address, address_buffer ) );

        NextUpgradeResponsePacket response;

        response.client_open_session_sequence = client->open_session_sequence;
        memcpy( response.client_kx_public_key, client->client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        memcpy( response.client_route_public_key, client->client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
        memcpy( response.upgrade_token, packet.upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
        response.platform_id = next_platform_id();

        response.connection_type = next_platform_connection_type();

        if ( next_client_internal_send_packet_to_server( client, NEXT_UPGRADE_RESPONSE_PACKET, &response ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "client failed to send upgrade response packet to server" );
            return;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client sent upgrade response packet to server" );

        // IMPORTANT: Cache upgrade response and keep sending it until we get an upgrade confirm.
        // Without this, under very rare packet loss conditions it's possible for the client to get
        // stuck in an undefined state.

        uint8_t from_address_data[32];
        uint8_t to_address_data[32];
        int from_address_bytes = 0;
        int to_address_bytes = 0;

        next_address_data( &client->client_external_address, from_address_data, &from_address_bytes );
        next_address_data( &client->server_address, to_address_data, &to_address_bytes );

        client->upgrade_response_packet_bytes = 0;
        const int result = next_write_packet( NEXT_UPGRADE_RESPONSE_PACKET, &response, client->upgrade_response_packet_data, &client->upgrade_response_packet_bytes, NULL, NULL, NULL, NULL, NULL, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

        if ( result != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "client failed to write upgrade response packet" );
            return;
        }

#if NEXT_ASSERTS

        const uint8_t * debug_packet_data = client->upgrade_response_packet_data;
        const int debug_packet_bytes = client->upgrade_response_packet_bytes;

        next_assert( debug_packet_data );
        next_assert( debug_packet_bytes > 0 );

        next_assert( next_basic_packet_filter( debug_packet_data, debug_packet_bytes ) );
        next_assert( next_advanced_packet_filter( debug_packet_data, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, debug_packet_bytes ) );

#endif // #if NEXT_ASSERTS

        client->sending_upgrade_response = true;
        client->upgrade_response_start_time = next_platform_time();
        client->last_upgrade_response_send_time = next_platform_time();

        return;
    }

    // upgrade confirm packet

    if ( !client->upgraded && packet_id == NEXT_UPGRADE_CONFIRM_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing upgrade confirm packet" );

        if ( !client->sending_upgrade_response )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade confirm packet from server. unexpected" );
            return;
        }

        if ( client->fallback_to_direct )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. in fallback to direct state" );
            return;
        }

        if ( !next_address_equal( from, &client->server_address ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. not from server address" );
            return;
        }

        // todo: header

        NextUpgradeConfirmPacket packet;
        int begin = 16;
        int end = packet_bytes - 2;
        if ( next_read_packet( NEXT_UPGRADE_CONFIRM_PACKET, packet_data, begin, end, &packet, NULL, NULL, NULL, NULL, NULL, NULL ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade request packet from server. could not read packet" );
            return;
        }

        if ( memcmp( packet.client_kx_public_key, client->client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES ) != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade confirm packet from server. client public key does not match" );
            return;
        }

        if ( client->upgraded && client->upgrade_sequence >= packet.upgrade_sequence )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade confirm packet from server. client already upgraded" );
            return;
        }

        uint8_t client_send_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
        uint8_t client_receive_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
        if ( next_crypto_kx_client_session_keys( client_receive_key, client_send_key, client->client_kx_public_key, client->client_kx_private_key, packet.server_kx_public_key ) != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored upgrade confirm packet from server. could not generate session keys from server public key" );
            return;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client received upgrade confirm packet from server" );

        next_post_validate_packet( NEXT_UPGRADE_CONFIRM_PACKET, NULL, NULL, NULL );

        client->upgraded = true;
        client->upgrade_sequence = packet.upgrade_sequence;
        client->session_id = packet.session_id;
        client->last_direct_pong_time = next_platform_time();
        client->last_next_pong_time = next_platform_time();
        memcpy( client->client_send_key, client_send_key, NEXT_CRYPTO_KX_SESSIONKEYBYTES );
        memcpy( client->client_receive_key, client_receive_key, NEXT_CRYPTO_KX_SESSIONKEYBYTES );

        next_client_notify_upgraded_t * notify = (next_client_notify_upgraded_t*) next_malloc( client->context, sizeof(next_client_notify_upgraded_t) );
        next_assert( notify );
        notify->type = NEXT_CLIENT_NOTIFY_UPGRADED;
        notify->session_id = client->session_id;
        notify->client_external_address = client->client_external_address;
        memcpy( notify->current_magic, client->current_magic, 8 );
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread queues up NEXT_CLIENT_NOTIFY_UPGRADED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &client->notify_mutex );
            next_queue_push( client->notify_queue, notify );
        }

        client->counters[NEXT_CLIENT_COUNTER_UPGRADE_SESSION]++;

        client->sending_upgrade_response = false;

        client->route_update_timeout_time = next_platform_time() + NEXT_CLIENT_ROUTE_UPDATE_TIMEOUT;

        return;
    }

    // direct packet

    if ( packet_id == NEXT_DIRECT_PACKET && client->upgraded && from_server_address )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing direct packet" );

        // todo: header

        packet_data += 16;
        packet_bytes -= 18;

        if ( packet_bytes <= 9 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored direct packet. packet is too small to be valid" );
            return;
        }

        if ( packet_bytes > NEXT_MTU + 9 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored direct packet. packet is too large to be valid" );
            return;
        }

        const uint8_t * p = packet_data;

        uint8_t packet_session_sequence = next_read_uint8( &p );

        if ( packet_session_sequence != client->open_session_sequence )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored direct packet. session mismatch" );
            return;
        }

        uint64_t packet_sequence = next_read_uint64( &p );

        const bool already_received = next_replay_protection_already_received( &client->payload_replay_protection, packet_sequence );

        if ( !already_received )
        {
            next_replay_protection_advance_sequence( &client->payload_replay_protection, packet_sequence );

            next_packet_loss_tracker_packet_received( &client->packet_loss_tracker, packet_sequence );

            next_out_of_order_tracker_packet_received( &client->out_of_order_tracker, packet_sequence );

            next_jitter_tracker_packet_received( &client->jitter_tracker, packet_sequence, packet_receive_time );
        }

        next_client_notify_packet_received_t * notify = (next_client_notify_packet_received_t*) next_malloc( client->context, sizeof( next_client_notify_packet_received_t ) );
        notify->type = NEXT_CLIENT_NOTIFY_PACKET_RECEIVED;
        notify->direct = true;
        notify->already_received = already_received;
        notify->payload_bytes = packet_bytes - 9;
        next_assert( notify->payload_bytes > 0 );
        memcpy( notify->payload_data, packet_data + 9, size_t(notify->payload_bytes) );
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread queues up NEXT_CLIENT_NOTIFY_PACKET_RECEIVED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &client->notify_mutex );
            next_queue_push( client->notify_queue, notify );
        }
        client->counters[NEXT_CLIENT_COUNTER_PACKET_RECEIVED_DIRECT]++;

        return;
    }

    // -------------------
    // PACKETS FROM RELAYS
    // -------------------

    if ( packet_id == NEXT_ROUTE_RESPONSE_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing route response packet" );

        // todo: header

        packet_data += 16;
        packet_bytes -= 18;

        if ( packet_bytes != NEXT_HEADER_BYTES )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route response packet from relay. bad packet size" );
            return;
        }

        bool fallback_to_direct;
        bool pending_route;
        uint64_t pending_route_session_id;
        uint8_t pending_route_session_version;
        uint8_t pending_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            next_route_manager_get_pending_route_data( client->route_manager, &fallback_to_direct, &pending_route, &pending_route_session_id, &pending_route_session_version, &pending_route_private_key[0] );
        }

        uint64_t packet_sequence = 0;
        uint64_t packet_session_id = 0;
        uint8_t packet_session_version = 0;

        if ( next_read_header( packet_id, &packet_sequence, &packet_session_id, &packet_session_version, pending_route_private_key, packet_data, packet_bytes ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route response packet from relay. could not read header" );
            return;
        }

        if ( fallback_to_direct )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route response packet from relay. in fallback to direct state" );
            return;
        }

        if ( !pending_route )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route response packet from relay. no pending route" );
            return;
        }

        next_platform_mutex_guard( &client->route_manager_mutex );

        next_replay_protection_t * replay_protection = &client->special_replay_protection;

        if ( next_replay_protection_already_received( replay_protection, packet_sequence ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route response packet from relay. sequence already received (%" PRIx64 " vs. %" PRIx64 ")", packet_sequence, replay_protection->most_recent_sequence );
            return;
        }

        if ( packet_session_id != pending_route_session_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route response packet from relay. session id mismatch" );
            return;
        }

        if ( packet_session_version != pending_route_session_version )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route response packet from relay. session version mismatch" );
            return;
        }

        next_replay_protection_advance_sequence( replay_protection, packet_sequence );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client received route response from relay" );

        int route_kbps_up = 0; 
        int route_kbps_down = 0;

        next_route_manager_confirm_pending_route( client->route_manager, &route_kbps_up, &route_kbps_down );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client network next route is confirmed" );

        client->last_route_switch_time = next_platform_time();
        {
            next_platform_mutex_guard( &client->next_bandwidth_mutex );
            client->next_bandwidth_envelope_kbps_up = route_kbps_up;
            client->next_bandwidth_envelope_kbps_down = route_kbps_down;
        }

        return;
    }

    // continue response packet

    if ( packet_id == NEXT_CONTINUE_RESPONSE_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing continue response packet" );

        // todo: header

        packet_data += 16;
        packet_bytes -= 18;

        if ( packet_bytes != NEXT_HEADER_BYTES )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. bad packet size" );
            return;
        }

        bool fallback_to_direct;
        bool current_route;
        bool pending_continue;
        uint64_t current_route_session_id;
        uint8_t current_route_session_version;
        uint8_t current_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            next_route_manager_get_current_route_data( client->route_manager, &fallback_to_direct, &current_route, &pending_continue, &current_route_session_id, &current_route_session_version, &current_route_private_key[0] );
        }

        if ( fallback_to_direct )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. in fallback to direct state" );
            return;
        }

        if ( !current_route )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. no current route" );
            return;
        }

        if ( !pending_continue )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. no pending continue" );
            return;
        }

        uint64_t packet_sequence = 0;
        uint64_t packet_session_id = 0;
        uint8_t packet_session_version = 0;

        if ( next_read_header( packet_id, &packet_sequence, &packet_session_id, &packet_session_version, current_route_private_key, packet_data, packet_bytes ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. could not read header" );
            return;
        }

        next_replay_protection_t * replay_protection = &client->special_replay_protection;

        if ( next_replay_protection_already_received( replay_protection, packet_sequence ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. sequence already received (%" PRIx64 " vs. %" PRIx64 ")", packet_sequence, replay_protection->most_recent_sequence );
            return;
        }

        if ( packet_session_id != current_route_session_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. session id mismatch" );
            return;
        }

        if ( packet_session_version != current_route_session_version )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored continue response packet from relay. session version mismatch" );
            return;
        }

        next_replay_protection_advance_sequence( replay_protection, packet_sequence );

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client received continue response from relay" );
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            next_route_manager_confirm_continue_route( client->route_manager );
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client continue network next route is confirmed" );

        return;
    }

    // server to client packet

    if ( packet_id == NEXT_SERVER_TO_CLIENT_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing server to client packet" );

        // todo: header

        packet_data += 16;
        packet_bytes -= 18;

        uint64_t payload_sequence = 0;

        bool result = false;
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            result = next_route_manager_process_server_to_client_packet( client->route_manager, packet_id, packet_data, packet_bytes, &payload_sequence );
        }

        if ( !result )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. could not verify" );
            return;
        }

        const bool already_received = next_replay_protection_already_received( &client->payload_replay_protection, payload_sequence ) != 0;

        if ( !already_received )
        {
            next_replay_protection_advance_sequence( &client->payload_replay_protection, payload_sequence );

            next_packet_loss_tracker_packet_received( &client->packet_loss_tracker, payload_sequence );

            next_out_of_order_tracker_packet_received( &client->out_of_order_tracker, payload_sequence );

            next_jitter_tracker_packet_received( &client->jitter_tracker, payload_sequence, next_platform_time() );
        }

        next_client_notify_packet_received_t * notify = (next_client_notify_packet_received_t*) next_malloc( client->context, sizeof( next_client_notify_packet_received_t ) );
        notify->type = NEXT_CLIENT_NOTIFY_PACKET_RECEIVED;
        notify->direct = false;
        notify->already_received = already_received;
        notify->payload_bytes = packet_bytes - NEXT_HEADER_BYTES;
        memcpy( notify->payload_data, packet_data + NEXT_HEADER_BYTES, size_t(packet_bytes) - NEXT_HEADER_BYTES );
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread queues up NEXT_CLIENT_NOTIFY_PACKET_RECEIVED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &client->notify_mutex );
            next_queue_push( client->notify_queue, notify );
        }

        client->counters[NEXT_CLIENT_COUNTER_PACKET_RECEIVED_NEXT]++;

        return;
    }

    // session pong packet

    if ( packet_id == NEXT_SESSION_PONG_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing session pong packet" );

        // todo: header

        packet_data += 16;
        packet_bytes -= 18;

        uint64_t payload_sequence = 0;

        bool result = false;
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            result = next_route_manager_process_server_to_client_packet( client->route_manager, packet_id, packet_data, packet_bytes, &payload_sequence );
        }

        if ( !result )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored session pong packet. could not verify" );
            return;
        }

        // todo: special replay protection. check what is done

        if ( next_replay_protection_already_received( &client->special_replay_protection, payload_sequence ) )
            return;

        next_replay_protection_advance_sequence( &client->special_replay_protection, payload_sequence );

        const uint8_t * p = packet_data + NEXT_HEADER_BYTES;

        uint64_t ping_sequence = next_read_uint64( &p );

        next_ping_history_pong_received( &client->next_ping_history, ping_sequence, next_platform_time() );

        client->last_next_pong_time = next_platform_time();

        return;
    }

    // client pong packet from near relay

    if ( packet_id == NEXT_CLIENT_PONG_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing client pong packet" );

        if ( !client->upgraded )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored client pong packet. not upgraded yet" );
            return;
        }

        // todo: header

        packet_data += 16;
        packet_bytes -= 18;

        const uint8_t * p = packet_data;

        uint64_t ping_sequence = next_read_uint64( &p );
        uint64_t ping_session_id = next_read_uint64( &p );

        if ( ping_session_id != client->session_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignoring client pong packet. session id does not match" );
            return;
        }

        next_relay_manager_process_pong( client->near_relay_manager, from, ping_sequence );

        return;
    }

    // -------------------
    // PACKETS FROM SERVER
    // -------------------

    if ( !next_address_equal( from, &client->server_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client ignoring packet because it's not from the server" );
        return;
    }

    // direct pong packet

    if ( packet_id == NEXT_DIRECT_PONG_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing direct packet" );

        NextDirectPongPacket packet;

        uint64_t packet_sequence = 0;

        // todo: header

        int begin = 16;
        int end = packet_bytes - 2;

        if ( next_read_packet( NEXT_DIRECT_PONG_PACKET, packet_data, begin, end, &packet, next_signed_packets, next_encrypted_packets, &packet_sequence, NULL, client->client_receive_key, &client->internal_replay_protection ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored direct pong packet. could not read" );
            return;
        }

        next_ping_history_pong_received( &client->direct_ping_history, packet.ping_sequence, next_platform_time() );

        next_post_validate_packet( NEXT_DIRECT_PONG_PACKET, next_encrypted_packets, &packet_sequence, &client->internal_replay_protection );

        client->last_direct_pong_time = next_platform_time();

        return;
    }

    // route update packet

    if ( packet_id == NEXT_ROUTE_UPDATE_PACKET )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client processing route update packet" );

        if ( client->fallback_to_direct )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route update packet from server. in fallback to direct state (1)" );
            return;
        }

        NextRouteUpdatePacket packet;

        uint64_t packet_sequence = 0;

        // todo: header

        int begin = 16;
        int end = packet_bytes - 2;

        if ( next_read_packet( NEXT_ROUTE_UPDATE_PACKET, packet_data, begin, end, &packet, next_signed_packets, next_encrypted_packets, &packet_sequence, NULL, client->client_receive_key, &client->internal_replay_protection ) != packet_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route update packet. could not read" );
            return;
        }

        if ( packet.sequence < client->route_update_sequence )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route update packet from server. sequence is old" );
            return;
        }

        next_post_validate_packet( NEXT_ROUTE_UPDATE_PACKET, next_encrypted_packets, &packet_sequence, &client->internal_replay_protection );

        bool fallback_to_direct = false;

        if ( packet.sequence > client->route_update_sequence )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client received route update packet from server" );

            // todo: near relays in another packet
            /*
            if ( packet.has_near_relays )
            {
                // enable near relay pings

                next_printf( NEXT_LOG_LEVEL_INFO, "client pinging %d near relays", packet.num_near_relays );

                next_relay_manager_update( client->near_relay_manager, packet.num_near_relays, packet.near_relay_ids, packet.near_relay_addresses, packet.near_relay_ping_tokens, packet.near_relay_expire_timestamp );
            }
            else
            {
                // disable near relay pings (and clear any ping data)
                
                if ( client->near_relay_manager->num_relays != 0 )
                {
                    next_printf( NEXT_LOG_LEVEL_INFO, "client near relay pings completed" );
                
                    next_relay_manager_update( client->near_relay_manager, 0, packet.near_relay_ids, packet.near_relay_addresses, NULL, 0 );
                }
            }
            */

            {
                next_platform_mutex_guard( &client->route_manager_mutex );
                next_route_manager_update( client->route_manager, packet.update_type, packet.num_tokens, packet.tokens, next_relay_backend_public_key, client->client_route_private_key, client->current_magic, &client->client_external_address );
                fallback_to_direct = next_route_manager_get_fallback_to_direct( client->route_manager );
            }

            if ( !client->fallback_to_direct && fallback_to_direct )
            {
                client->counters[NEXT_CLIENT_COUNTER_FALLBACK_TO_DIRECT]++;
            }

            client->fallback_to_direct = fallback_to_direct;

            if ( !fallback_to_direct )
            {
                if ( packet.multipath && !client->multipath )
                {
                    next_printf( NEXT_LOG_LEVEL_INFO, "client multipath enabled" );
                    client->multipath = true;
                    client->counters[NEXT_CLIENT_COUNTER_MULTIPATH]++;
                }

                client->route_update_sequence = packet.sequence;
                client->client_stats.packets_sent_server_to_client = packet.packets_sent_server_to_client;
                client->client_stats.packets_lost_client_to_server = packet.packets_lost_client_to_server;
                client->client_stats.packets_out_of_order_client_to_server = packet.packets_out_of_order_client_to_server;
                client->client_stats.jitter_client_to_server = packet.jitter_client_to_server;
                client->counters[NEXT_CLIENT_COUNTER_PACKETS_LOST_CLIENT_TO_SERVER] = packet.packets_lost_client_to_server;
                client->counters[NEXT_CLIENT_COUNTER_PACKETS_OUT_OF_ORDER_CLIENT_TO_SERVER] = packet.packets_out_of_order_client_to_server;
                client->route_update_timeout_time = next_platform_time() + NEXT_CLIENT_ROUTE_UPDATE_TIMEOUT;

                if ( memcmp( client->upcoming_magic, packet.upcoming_magic, 8 ) != 0 )
                {
                    next_printf( NEXT_LOG_LEVEL_DEBUG, "client updated magic: %x,%x,%x,%x,%x,%x,%x,%x | %x,%x,%x,%x,%x,%x,%x,%x | %x,%x,%x,%x,%x,%x,%x,%x",
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

                    memcpy( client->upcoming_magic, packet.upcoming_magic, 8 );
                    memcpy( client->current_magic, packet.current_magic, 8 );
                    memcpy( client->previous_magic, packet.previous_magic, 8 );

                    next_client_notify_magic_updated_t * notify = (next_client_notify_magic_updated_t*) next_malloc( client->context, sizeof(next_client_notify_magic_updated_t) );
                    next_assert( notify );
                    notify->type = NEXT_CLIENT_NOTIFY_MAGIC_UPDATED;
                    memcpy( notify->current_magic, client->current_magic, 8 );
                    {
#if NEXT_SPIKE_TRACKING
                        next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread queues up NEXT_CLIENT_NOTIFY_MAGIC_UPDATED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
                        next_platform_mutex_guard( &client->notify_mutex );
                        next_queue_push( client->notify_queue, notify );
                    }
                }
            }
        }

        if ( fallback_to_direct )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored route update packet from server. in fallback to direct state (2)" );
            return;
        }

        NextRouteUpdateAckPacket ack;
        ack.sequence = packet.sequence;

        if ( next_client_internal_send_packet_to_server( client, NEXT_ROUTE_UPDATE_ACK_PACKET, &ack ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "client failed to send route update ack packet to server" );
            return;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "client sent route update ack packet to server" );

        return;
    }
}

void next_client_internal_process_passthrough_packet( next_client_internal_t * client, const next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_client_internal_verify_sentinels( client );

    next_printf( NEXT_LOG_LEVEL_SPAM, "client processing passthrough packet" );

    next_assert( from );
    next_assert( packet_data );

    const bool from_server_address = client->server_address.type != 0 && next_address_equal( from, &client->server_address );

#if NEXT_SPIKE_TRACKING
    next_printf( NEXT_LOG_LEVEL_SPAM, "client drops passthrough packet at %s:%d because it does not think it comes from the server", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING

    if ( packet_bytes <= NEXT_MAX_PACKET_BYTES - 1 && from_server_address )
    {
        next_client_notify_packet_received_t * notify = (next_client_notify_packet_received_t*) next_malloc( client->context, sizeof( next_client_notify_packet_received_t ) );
        notify->type = NEXT_CLIENT_NOTIFY_PACKET_RECEIVED;
        notify->direct = true;
        notify->payload_bytes = packet_bytes;
        notify->already_received = false;
        next_assert( notify->payload_bytes >= 0 );
        next_assert( notify->payload_bytes <= NEXT_MAX_PACKET_BYTES - 1 );
        memcpy( notify->payload_data, packet_data, size_t(packet_bytes) );
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread queues up NEXT_CLIENT_NOTIFY_PACKET_RECEIVED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &client->notify_mutex );
            next_queue_push( client->notify_queue, notify );
        }
        client->counters[NEXT_CLIENT_COUNTER_PACKET_RECEIVED_PASSTHROUGH]++;
    }
}

#if NEXT_DEVELOPMENT
extern bool next_packet_loss;
#endif // #if NEXT_DEVELOPMENT

void next_client_internal_block_and_receive_packet( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

    next_assert( ( size_t(packet_data) % 4 ) == 0 );

    next_address_t from;

#if NEXT_SPIKE_TRACKING
    next_printf( NEXT_LOG_LEVEL_SPAM, "client calls next_platform_socket_receive_packet on internal thread" );
#endif // #if NEXT_SPIKE_TRACKING

    int packet_bytes = next_platform_socket_receive_packet( client->socket, &from, packet_data, NEXT_MAX_PACKET_BYTES );

#if NEXT_SPIKE_TRACKING
    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_printf( NEXT_LOG_LEVEL_SPAM, "client next_platform_socket_receive_packet returns with a %d byte packet from %s", packet_bytes, next_address_to_string( &from, address_buffer ) );
#endif // #if NEXT_SPIKE_TRACKING

    double packet_receive_time = next_platform_time();

    next_assert( packet_bytes >= 0 );

    if ( packet_bytes <= 1 )
        return;
    
#if NEXT_DEVELOPMENT
    if ( next_packet_loss && ( rand() % 10 ) == 0 )
        return;
#endif // #if NEXT_DEVELOPMENT

    if ( packet_data[0] != NEXT_PASSTHROUGH_PACKET )
    {
        next_client_internal_process_network_next_packet( client, &from, packet_data, packet_bytes, packet_receive_time );
    }
    else
    {
        next_client_internal_process_passthrough_packet( client, &from, packet_data + 1, packet_bytes - 1 );
    }
}

bool next_client_internal_pump_commands( next_client_internal_t * client )
{
#if NEXT_SPIKE_TRACKING
    next_printf( NEXT_LOG_LEVEL_SPAM, "next_client_internal_pump_commands" );
#endif // #if NEXT_SPIKE_TRACKING

    next_client_internal_verify_sentinels( client );

    bool quit = false;

    while ( true )
    {
        void * entry = NULL;
        {
            next_platform_mutex_guard( &client->command_mutex );
            entry = next_queue_pop( client->command_queue );
        }

        if ( entry == NULL )
            break;

        next_client_command_t * command = (next_client_command_t*) entry;

        switch ( command->type )
        {
            case NEXT_CLIENT_COMMAND_OPEN_SESSION:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread received NEXT_CLIENT_COMMAND_OPEN_SESSION" );
#endif // #if NEXT_SPIKE_TRACKING

                next_client_command_open_session_t * open_session_command = (next_client_command_open_session_t*) entry;
                client->server_address = open_session_command->server_address;
                client->session_open = true;
                client->open_session_sequence++;
                client->last_direct_ping_time = next_platform_time();
                client->last_stats_update_time = next_platform_time();
                client->last_stats_report_time = next_platform_time() + next_random_float();
                next_crypto_kx_keypair( client->client_kx_public_key, client->client_kx_private_key );
                next_crypto_box_keypair( client->client_route_public_key, client->client_route_private_key );
                char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_INFO, "client opened session to %s", next_address_to_string( &open_session_command->server_address, buffer ) );
                client->counters[NEXT_CLIENT_COUNTER_OPEN_SESSION]++;
                {
                    next_platform_mutex_guard( &client->route_manager_mutex );
                    next_route_manager_reset( client->route_manager );
                    next_route_manager_direct_route( client->route_manager, true );
                }

                // IMPORTANT: Fire back ready when the client is ready to start sending packets and we're all dialed in for this session
                next_client_notify_ready_t * notify = (next_client_notify_ready_t*) next_malloc( client->context, sizeof(next_client_notify_ready_t) );
                next_assert( notify );
                notify->type = NEXT_CLIENT_NOTIFY_READY;
                {
#if NEXT_SPIKE_TRACKING
                    next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread queues up NEXT_CLIENT_NOTIFY_READY at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
                    next_platform_mutex_guard( &client->notify_mutex );
                    next_queue_push( client->notify_queue, notify );
                }
            }
            break;

            case NEXT_CLIENT_COMMAND_CLOSE_SESSION:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread received NEXT_CLIENT_COMMAND_CLOSE_SESSION" );
#endif // #if NEXT_SPIKE_TRACKING

                if ( !client->session_open )
                    break;

                char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_INFO, "client closed session to %s", next_address_to_string( &client->server_address, buffer ) );

                memset( client->upcoming_magic, 0, 8 );
                memset( client->current_magic, 0, 8 );
                memset( client->previous_magic, 0, 8 );
                memset( &client->server_address, 0, sizeof(next_address_t) );
                memset( &client->client_external_address, 0, sizeof(next_address_t) );

                client->session_open = false;
                client->upgraded = false;
                client->reported = false;
                client->fallback_to_direct = false;
                client->multipath = false;
                client->upgrade_sequence = 0;
                client->session_id = 0;
                client->internal_send_sequence = 0;
                client->last_next_ping_time = 0.0;
                client->last_next_pong_time = 0.0;
                client->last_direct_ping_time = 0.0;
                client->last_direct_pong_time = 0.0;
                client->last_stats_update_time = 0.0;
                client->last_stats_report_time = 0.0;
                client->last_route_switch_time = 0.0;
                client->route_update_timeout_time = 0.0;
                client->route_update_sequence = 0;
                client->sending_upgrade_response = false;
                client->upgrade_response_packet_bytes = 0;
                memset( client->upgrade_response_packet_data, 0, sizeof(client->upgrade_response_packet_data) );
                client->upgrade_response_start_time = 0.0;
                client->last_upgrade_response_send_time = 0.0;

                client->packets_sent = 0;

                memset( &client->client_stats, 0, sizeof(next_client_stats_t) );
                memset( &client->near_relay_stats, 0, sizeof(next_relay_stats_t ) );
                next_relay_stats_initialize_sentinels( &client->near_relay_stats );

                next_relay_manager_reset( client->near_relay_manager );

                memset( client->client_kx_public_key, 0, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
                memset( client->client_kx_private_key, 0, NEXT_CRYPTO_KX_SECRETKEYBYTES );
                memset( client->client_send_key, 0, NEXT_CRYPTO_KX_SESSIONKEYBYTES );
                memset( client->client_receive_key, 0, NEXT_CRYPTO_KX_SESSIONKEYBYTES );
                memset( client->client_route_public_key, 0, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
                memset( client->client_route_private_key, 0, NEXT_CRYPTO_BOX_SECRETKEYBYTES );

                next_ping_history_clear( &client->next_ping_history );
                next_ping_history_clear( &client->direct_ping_history );

                next_replay_protection_reset( &client->payload_replay_protection );
                next_replay_protection_reset( &client->special_replay_protection );
                next_replay_protection_reset( &client->internal_replay_protection );

                {
                    next_platform_mutex_guard( &client->direct_bandwidth_mutex );
                    client->direct_bandwidth_usage_kbps_up = 0;
                    client->direct_bandwidth_usage_kbps_down = 0;
                }

                {
                    next_platform_mutex_guard( &client->next_bandwidth_mutex );
                    client->next_bandwidth_over_limit = false;
                    client->next_bandwidth_usage_kbps_up = 0;
                    client->next_bandwidth_usage_kbps_down = 0;
                    client->next_bandwidth_envelope_kbps_up = 0;
                    client->next_bandwidth_envelope_kbps_down = 0;
                }

                {
                    next_platform_mutex_guard( &client->route_manager_mutex );
                    next_route_manager_reset( client->route_manager );
                }

                next_packet_loss_tracker_reset( &client->packet_loss_tracker );
                next_out_of_order_tracker_reset( &client->out_of_order_tracker );
                next_jitter_tracker_reset( &client->jitter_tracker );

                client->counters[NEXT_CLIENT_COUNTER_CLOSE_SESSION]++;
            }
            break;

            case NEXT_CLIENT_COMMAND_DESTROY:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread received NEXT_CLIENT_COMMAND_DESTROY" );
#endif // #if NEXT_SPIKE_TRACKING
                quit = true;
            }
            break;

            case NEXT_CLIENT_COMMAND_REPORT_SESSION:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread received NEXT_CLIENT_COMMAND_REPORT_SESSION" );
#endif // #if NEXT_SPIKE_TRACKING
                if ( client->session_id != 0 && !client->reported )
                {
                    next_printf( NEXT_LOG_LEVEL_INFO, "client reported session %" PRIx64, client->session_id );
                    client->reported = true;
                }
            }
            break;

            default: break;
        }

        next_free( client->context, command );
    }

    return quit;
}

#if NEXT_DEVELOPMENT
bool next_fake_fallback_to_direct = false;
float next_fake_direct_packet_loss = 0.0f;
float next_fake_direct_rtt = 0.0f;
float next_fake_next_packet_loss = 0.0f;
float next_fake_next_rtt = 0.0f;
#endif // #if !NEXT_DEVELOPMENT

void next_client_internal_update_stats( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    next_assert( !next_global_config.disable_network_next );

    double current_time = next_platform_time();

    if ( client->last_stats_update_time + ( 1.0 / NEXT_CLIENT_STATS_UPDATES_PER_SECOND ) < current_time )
    {
        bool network_next = false;
        bool fallback_to_direct = false;
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            network_next = next_route_manager_has_network_next_route( client->route_manager );
            fallback_to_direct = next_route_manager_get_fallback_to_direct( client->route_manager );
        }
        
        client->client_stats.next = network_next;
        client->client_stats.upgraded = client->upgraded;
        client->client_stats.reported = client->reported;
        client->client_stats.fallback_to_direct = client->fallback_to_direct;
        client->client_stats.multipath = client->multipath;
        client->client_stats.platform_id = next_platform_id();

        client->client_stats.connection_type = next_platform_connection_type();

        double start_time = current_time - NEXT_CLIENT_STATS_WINDOW;
        if ( start_time < client->last_route_switch_time + NEXT_PING_SAFETY )
        {
            start_time = client->last_route_switch_time + NEXT_PING_SAFETY;
        }

        next_route_stats_t next_route_stats;
        next_route_stats_from_ping_history( &client->next_ping_history, current_time - NEXT_CLIENT_STATS_WINDOW, current_time, &next_route_stats );

        next_route_stats_t direct_route_stats;
        next_route_stats_from_ping_history( &client->direct_ping_history, current_time - NEXT_CLIENT_STATS_WINDOW, current_time, &direct_route_stats );

        {
            next_platform_mutex_guard( &client->direct_bandwidth_mutex );
            client->client_stats.direct_kbps_up = client->direct_bandwidth_usage_kbps_up;
            client->client_stats.direct_kbps_down = client->direct_bandwidth_usage_kbps_down;
        }

        if ( network_next )
        {
            client->client_stats.next_rtt = next_route_stats.rtt;
            client->client_stats.next_jitter = next_route_stats.jitter;
            client->client_stats.next_packet_loss = next_route_stats.packet_loss;
            {
                next_platform_mutex_guard( &client->next_bandwidth_mutex );
                client->client_stats.next_kbps_up = client->next_bandwidth_usage_kbps_up;
                client->client_stats.next_kbps_down = client->next_bandwidth_usage_kbps_down;
            }
        }
        else
        {
            client->client_stats.next_rtt = 0.0f;
            client->client_stats.next_jitter = 0.0f;
            client->client_stats.next_packet_loss = 0.0f;
            client->client_stats.next_kbps_up = 0;
            client->client_stats.next_kbps_down = 0;
        }

        client->client_stats.direct_rtt = direct_route_stats.rtt;
        client->client_stats.direct_jitter = direct_route_stats.jitter;
        client->client_stats.direct_packet_loss = direct_route_stats.packet_loss;

        if ( direct_route_stats.packet_loss > client->client_stats.direct_max_packet_loss_seen )
        {
            client->client_stats.direct_max_packet_loss_seen = direct_route_stats.packet_loss;
        }

#if NEXT_DEVELOPMENT
        if ( !fallback_to_direct && next_fake_fallback_to_direct )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "client fakes fallback to direct" );
            {
                next_platform_mutex_guard( &client->route_manager_mutex );
                next_route_manager_fallback_to_direct( client->route_manager, NEXT_FLAGS_ROUTE_UPDATE_TIMED_OUT );
            }
        }
        client->client_stats.direct_rtt += next_fake_direct_rtt;
        client->client_stats.direct_packet_loss += next_fake_direct_packet_loss;
        client->client_stats.next_rtt += next_fake_next_rtt;
        client->client_stats.next_packet_loss += next_fake_next_packet_loss;
 #endif // #if NEXT_DEVELOPMENT

        if ( !fallback_to_direct )
        {
            const int packets_lost = next_packet_loss_tracker_update( &client->packet_loss_tracker );
            client->client_stats.packets_lost_server_to_client += packets_lost;
            client->counters[NEXT_CLIENT_COUNTER_PACKETS_LOST_SERVER_TO_CLIENT] += packets_lost;

            client->client_stats.packets_out_of_order_server_to_client = client->out_of_order_tracker.num_out_of_order_packets;
            client->counters[NEXT_CLIENT_COUNTER_PACKETS_OUT_OF_ORDER_SERVER_TO_CLIENT] = client->out_of_order_tracker.num_out_of_order_packets;

            client->client_stats.jitter_server_to_client = float( client->jitter_tracker.jitter * 1000.0 );
        }

        client->client_stats.packets_sent_client_to_server = client->packets_sent;

        next_relay_manager_get_stats( client->near_relay_manager, &client->near_relay_stats );

        next_client_notify_stats_updated_t * notify = (next_client_notify_stats_updated_t*) next_malloc( client->context, sizeof( next_client_notify_stats_updated_t ) );
        notify->type = NEXT_CLIENT_NOTIFY_STATS_UPDATED;
        notify->stats = client->client_stats;
        notify->fallback_to_direct = fallback_to_direct;
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "client internal thread queues up NEXT_CLIENT_NOTIFY_STATS_UPDATED at %s:%d", __FILE__, __LINE__ );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &client->notify_mutex );
            next_queue_push( client->notify_queue, notify );
        }

        client->last_stats_update_time = current_time;
    }

    if ( client->last_stats_report_time + 1.0 < current_time && client->client_stats.direct_rtt > 0.0f )
    {
        NextClientStatsPacket packet;

        packet.reported = client->reported;
        packet.fallback_to_direct = client->fallback_to_direct;
        packet.multipath = client->multipath;
        packet.platform_id = client->client_stats.platform_id;
        packet.connection_type = client->client_stats.connection_type;

        {
            next_platform_mutex_guard( &client->direct_bandwidth_mutex );
            packet.direct_kbps_up = (int) ceil( client->direct_bandwidth_usage_kbps_up );
            packet.direct_kbps_down = (int) ceil( client->direct_bandwidth_usage_kbps_down );
        }

        {
            next_platform_mutex_guard( &client->next_bandwidth_mutex );
            packet.next_bandwidth_over_limit = client->next_bandwidth_over_limit;
            packet.next_kbps_up = (int) ceil( client->next_bandwidth_usage_kbps_up );
            packet.next_kbps_down = (int) ceil( client->next_bandwidth_usage_kbps_down );
            client->next_bandwidth_over_limit = false;
        }

        if ( !client->client_stats.next )
        {
            packet.next_kbps_up = 0;
            packet.next_kbps_down = 0;
        }

        packet.next = client->client_stats.next;
        packet.next_rtt = client->client_stats.next_rtt;
        packet.next_jitter = client->client_stats.next_jitter;
        packet.next_packet_loss = client->client_stats.next_packet_loss;

        packet.direct_rtt = client->client_stats.direct_rtt;
        packet.direct_jitter = client->client_stats.direct_jitter;
        packet.direct_packet_loss = client->client_stats.direct_packet_loss;
        packet.direct_max_packet_loss_seen = client->client_stats.direct_max_packet_loss_seen;

        if ( !client->fallback_to_direct )
        {
            packet.num_near_relays = client->near_relay_stats.num_relays;
            for ( int i = 0; i < packet.num_near_relays; ++i )
            {
                int rtt = (int) ceil( client->near_relay_stats.relay_rtt[i] );
                int jitter = (int) ceil( client->near_relay_stats.relay_jitter[i] );
                float packet_loss = client->near_relay_stats.relay_packet_loss[i];

                if ( rtt > 255 )
                    rtt = 255;

                if ( jitter > 255 )
                    jitter = 255;

                if ( packet_loss > 100 )
                    packet_loss = 100;

                packet.near_relay_ids[i] = client->near_relay_stats.relay_ids[i];
                packet.near_relay_rtt[i] = uint8_t( rtt );
                packet.near_relay_jitter[i] = uint8_t( jitter );
                packet.near_relay_packet_loss[i] = packet_loss;
            }
        }

        packet.packets_sent_client_to_server = client->packets_sent;

        packet.packets_lost_server_to_client = client->client_stats.packets_lost_server_to_client;
        packet.packets_out_of_order_server_to_client = client->client_stats.packets_out_of_order_server_to_client;
        packet.jitter_server_to_client = client->client_stats.jitter_server_to_client;

        if ( next_client_internal_send_packet_to_server( client, NEXT_CLIENT_STATS_PACKET, &packet ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "client failed to send stats packet to server" );
            return;
        }

        client->last_stats_report_time = current_time;
    }
}

void next_client_internal_update_direct_pings( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    next_assert( !next_global_config.disable_network_next );

    if ( !client->upgraded )
        return;

    if ( client->fallback_to_direct )
        return;

    double current_time = next_platform_time();

    if ( client->last_direct_pong_time + NEXT_CLIENT_SESSION_TIMEOUT < current_time )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client direct pong timed out. falling back to direct" );
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            next_route_manager_fallback_to_direct( client->route_manager, NEXT_FLAGS_DIRECT_PONG_TIMED_OUT );
        }
        return;
    }

    if ( client->last_direct_ping_time + ( 1.0 / NEXT_DIRECT_PINGS_PER_SECOND ) <= current_time )
    {
        NextDirectPingPacket packet;
        packet.ping_sequence = next_ping_history_ping_sent( &client->direct_ping_history, current_time );

        if ( next_client_internal_send_packet_to_server( client, NEXT_DIRECT_PING_PACKET, &packet ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "client failed to send direct ping packet to server" );
            return;
        }

        client->last_direct_ping_time = current_time;
    }
}

void next_client_internal_update_next_pings( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    next_assert( !next_global_config.disable_network_next );

    if ( !client->upgraded )
        return;

    if ( client->fallback_to_direct )
        return;

    double current_time = next_platform_time();

    bool has_next_route = false;
    {
        next_platform_mutex_guard( &client->route_manager_mutex );
        has_next_route = next_route_manager_has_network_next_route( client->route_manager );
    }

    if ( !has_next_route )
    {
        client->last_next_pong_time = current_time;
    }

    if ( client->last_next_pong_time + NEXT_CLIENT_SESSION_TIMEOUT < current_time )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client next pong timed out. falling back to direct" );
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            next_route_manager_fallback_to_direct( client->route_manager, NEXT_FLAGS_NEXT_PONG_TIMED_OUT );
        }
        return;
    }

    if ( client->last_next_ping_time + ( 1.0 / NEXT_PINGS_PER_SECOND ) <= current_time )
    {
        if ( !has_next_route )
            return;

        uint64_t session_id;
        uint8_t session_version;
        next_address_t to;
        uint8_t private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            next_route_manager_get_next_route_data( client->route_manager, &session_id, &session_version, &to, private_key );
        }

        uint64_t sequence = client->special_send_sequence++;

        uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

        uint8_t from_address_data[32];
        uint8_t to_address_data[32];
        int from_address_bytes;
        int to_address_bytes;

        next_address_data( &client->client_external_address, from_address_data, &from_address_bytes );
        next_address_data( &to, to_address_data, &to_address_bytes );

        const uint64_t ping_sequence = next_ping_history_ping_sent( &client->next_ping_history, current_time );

        int packet_bytes = next_write_session_ping_packet( packet_data, sequence, session_id, session_version, private_key, ping_sequence, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

        next_assert( packet_bytes > 0 );

        next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
        next_assert( next_advanced_packet_filter( packet_data, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

#if NEXT_SPIKE_TRACKING
        double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

        next_platform_socket_send_packet( client->socket, &to, packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
        double finish_time = next_platform_time();
        if ( finish_time - start_time > 0.001 )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
        }
#endif // #if NEXT_SPIKE_TRACKING

        client->last_next_ping_time = current_time;
    }
}

void next_client_internal_send_pings_to_near_relays( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    if ( next_global_config.disable_network_next )
        return;

    if ( !client->upgraded )
        return;

    if ( client->fallback_to_direct )
        return;

    next_relay_manager_send_pings( client->near_relay_manager, client->socket, client->session_id, client->current_magic, &client->client_external_address );
}

void next_client_internal_update_fallback_to_direct( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    next_assert( !next_global_config.disable_network_next );

    bool fallback_to_direct = false;
    {
        next_platform_mutex_guard( &client->route_manager_mutex );
        if ( client->upgraded )
        {
            next_route_manager_check_for_timeouts( client->route_manager );
        }
        fallback_to_direct = next_route_manager_get_fallback_to_direct( client->route_manager );
    }

    if ( !client->fallback_to_direct && fallback_to_direct )
    {
        client->counters[NEXT_CLIENT_COUNTER_FALLBACK_TO_DIRECT]++;
        client->fallback_to_direct = fallback_to_direct;
        return;
    }

    if ( !client->fallback_to_direct && client->upgraded && client->route_update_timeout_time > 0.0 )
    {
        if ( next_platform_time() > client->route_update_timeout_time )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "client route update timeout. falling back to direct" );
            {
                next_platform_mutex_guard( &client->route_manager_mutex );
                next_route_manager_fallback_to_direct( client->route_manager, NEXT_FLAGS_ROUTE_UPDATE_TIMED_OUT );
            }
            client->counters[NEXT_CLIENT_COUNTER_FALLBACK_TO_DIRECT]++;
            client->fallback_to_direct = true;
        }
    }
}

void next_client_internal_update_route_manager( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    next_assert( !next_global_config.disable_network_next );

    if ( !client->upgraded )
        return;

    if ( client->fallback_to_direct )
        return;

    next_address_t route_request_to;
    next_address_t continue_request_to;

    int route_request_packet_bytes;
    int continue_request_packet_bytes;

    uint8_t route_request_packet_data[NEXT_MAX_PACKET_BYTES];
    uint8_t continue_request_packet_data[NEXT_MAX_PACKET_BYTES];

    bool send_route_request = false;
    bool send_continue_request = false;
    {
        next_platform_mutex_guard( &client->route_manager_mutex );
        send_route_request = next_route_manager_send_route_request( client->route_manager, &route_request_to, route_request_packet_data, &route_request_packet_bytes );
        send_continue_request = next_route_manager_send_continue_request( client->route_manager, &continue_request_to, continue_request_packet_data, &continue_request_packet_bytes );
    }

    if ( send_route_request )
    {
        char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client sent route request to relay: %s", next_address_to_string( &route_request_to, buffer ) );

#if NEXT_SPIKE_TRACKING
        double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

        next_platform_socket_send_packet( client->socket, &route_request_to, route_request_packet_data, route_request_packet_bytes );

#if NEXT_SPIKE_TRACKING
        double finish_time = next_platform_time();
        if ( finish_time - start_time > 0.001 )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
        }
#endif // #if NEXT_SPIKE_TRACKING
    }

    if ( send_continue_request )
    {
        char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client sent continue request to relay: %s", next_address_to_string( &continue_request_to, buffer ) );

#if NEXT_SPIKE_TRACKING
        double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

        next_platform_socket_send_packet( client->socket, &continue_request_to, continue_request_packet_data, continue_request_packet_bytes );

#if NEXT_SPIKE_TRACKING
        double finish_time = next_platform_time();
        if ( finish_time - start_time > 0.001 )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
        }
#endif // #if NEXT_SPIKE_TRACKING
    }
}

void next_client_internal_update_upgrade_response( next_client_internal_t * client )
{
    next_client_internal_verify_sentinels( client );

    next_assert( !next_global_config.disable_network_next );

    if ( client->fallback_to_direct )
        return;

    if ( !client->sending_upgrade_response )
        return;

    const double current_time = next_platform_time();

    if ( client->last_upgrade_response_send_time + 1.0 > current_time )
        return;

    next_assert( client->upgrade_response_packet_bytes > 0 );

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( client->socket, &client->server_address, client->upgrade_response_packet_data, client->upgrade_response_packet_bytes );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING

    next_printf( NEXT_LOG_LEVEL_DEBUG, "client sent cached upgrade response packet to server" );

    client->last_upgrade_response_send_time = current_time;

    if ( client->upgrade_response_start_time + 5.0 <= current_time )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "upgrade response timed out" );
        {
            next_platform_mutex_guard( &client->route_manager_mutex );
            next_route_manager_fallback_to_direct( client->route_manager, NEXT_FLAGS_UPGRADE_RESPONSE_TIMED_OUT );
        }
        client->fallback_to_direct = true;
    }
}

void next_client_internal_update( next_client_internal_t * client )
{
    if ( next_global_config.disable_network_next )
        return;
  
#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_client_internal_update_direct_pings( client );

    next_client_internal_update_next_pings( client );

    next_client_internal_send_pings_to_near_relays( client );

    next_client_internal_update_stats( client );

    next_client_internal_update_fallback_to_direct( client );

    next_client_internal_update_route_manager( client );

    next_client_internal_update_upgrade_response( client );

#if NEXT_SPIKE_TRACKING

    double finish_time = next_platform_time();

    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_client_internal_update spike %.2f milliseconds", ( finish_time - start_time ) * 1000.0 );
    }

#endif // #if NEXT_SPIKE_TRACKING
}

static void next_client_internal_thread_function( void * context )
{
    next_client_internal_t * client = (next_client_internal_t*) context;

    next_assert( client );

    bool quit = false;

    double last_update_time = next_platform_time();

    while ( !quit )
    {
        next_client_internal_block_and_receive_packet( client );

        if ( next_platform_time() > last_update_time + 0.01 )
        {
            next_client_internal_update( client );

            quit = next_client_internal_pump_commands( client );

            last_update_time = next_platform_time();
        }
    }
}

// ---------------------------------------------------------------

struct next_client_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    int state;
    bool ready;
    bool upgraded;
    bool fallback_to_direct;
    uint8_t open_session_sequence;
    uint8_t current_magic[8];
    uint16_t bound_port;
    uint64_t session_id;
    next_address_t server_address;
    next_address_t client_external_address;
    next_client_internal_t * internal;
    next_platform_thread_t * thread;
    void (*packet_received_callback)( next_client_t * client, void * context, const struct next_address_t * from, const uint8_t * packet_data, int packet_bytes );

    NEXT_DECLARE_SENTINEL(1)

    next_client_stats_t client_stats;

    NEXT_DECLARE_SENTINEL(2)

    next_bandwidth_limiter_t direct_send_bandwidth;
    next_bandwidth_limiter_t direct_receive_bandwidth;
    next_bandwidth_limiter_t next_send_bandwidth;
    next_bandwidth_limiter_t next_receive_bandwidth;

    NEXT_DECLARE_SENTINEL(3)

    uint64_t counters[NEXT_CLIENT_COUNTER_MAX];

    NEXT_DECLARE_SENTINEL(4)
};

void next_client_initialize_sentinels( next_client_t * client )
{
    (void) client;
    next_assert( client );
    NEXT_INITIALIZE_SENTINEL( client, 0 )
    NEXT_INITIALIZE_SENTINEL( client, 1 )
    NEXT_INITIALIZE_SENTINEL( client, 2 )
    NEXT_INITIALIZE_SENTINEL( client, 3 )
    NEXT_INITIALIZE_SENTINEL( client, 4 )
}

void next_client_verify_sentinels( next_client_t * client )
{
    (void) client;
    next_assert( client );
    NEXT_VERIFY_SENTINEL( client, 0 )
    NEXT_VERIFY_SENTINEL( client, 1 )
    NEXT_VERIFY_SENTINEL( client, 2 )
    NEXT_VERIFY_SENTINEL( client, 3 )
    NEXT_VERIFY_SENTINEL( client, 4 )
}

void next_client_destroy( next_client_t * client );

next_client_t * next_client_create( void * context, const char * bind_address, void (*packet_received_callback)( next_client_t * client, void * context, const struct next_address_t * from, const uint8_t * packet_data, int packet_bytes ) )
{
    next_assert( bind_address );
    next_assert( packet_received_callback );

    next_client_t * client = (next_client_t*) next_malloc( context, sizeof(next_client_t) );
    if ( !client )
        return NULL;

    memset( client, 0, sizeof( next_client_t) );

    next_client_initialize_sentinels( client );

    client->context = context;
    client->packet_received_callback = packet_received_callback;

    client->internal = next_client_internal_create( client->context, bind_address );
    if ( !client->internal )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create internal client" );
        next_client_destroy( client );
        return NULL;
    }

    client->bound_port = client->internal->bound_port;

    client->thread = next_platform_thread_create( client->context, next_client_internal_thread_function, client->internal );
    next_assert( client->thread );
    if ( !client->thread )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client could not create thread" );
        next_client_destroy( client );
        return NULL;
    }

    next_platform_client_thread_priority( client->thread );

    next_bandwidth_limiter_reset( &client->direct_send_bandwidth );
    next_bandwidth_limiter_reset( &client->direct_receive_bandwidth );
    next_bandwidth_limiter_reset( &client->next_send_bandwidth );
    next_bandwidth_limiter_reset( &client->next_receive_bandwidth );

    next_client_verify_sentinels( client );

    return client;
}

uint16_t next_client_port( next_client_t * client )
{
    next_client_verify_sentinels( client );

    return client->bound_port;
}

void next_client_destroy( next_client_t * client )
{
    next_client_verify_sentinels( client );

    if ( client->thread )
    {
        next_client_command_destroy_t * command = (next_client_command_destroy_t*) next_malloc( client->context, sizeof( next_client_command_destroy_t ) );
        if ( !command )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "client destroy failed. could not create destroy command" );
            return;
        }
        command->type = NEXT_CLIENT_COMMAND_DESTROY;
        {
#if NEXT_SPIKE_TRACKING
            next_printf( NEXT_LOG_LEVEL_SPAM, "client sent NEXT_CLIENT_COMMAND_DESTROY" );
#endif // #if NEXT_SPIKE_TRACKING
            next_platform_mutex_guard( &client->internal->command_mutex );
            next_queue_push( client->internal->command_queue, command );
        }

        next_platform_thread_join( client->thread );
        next_platform_thread_destroy( client->thread );
    }

    if ( client->internal )
    {
        next_client_internal_destroy( client->internal );
    }

    next_clear_and_free( client->context, client, sizeof(next_client_t) );
}

void next_client_open_session( next_client_t * client, const char * server_address_string )
{
    next_client_verify_sentinels( client );

    next_assert( client->internal );

    next_client_close_session( client );

    next_address_t server_address;
    if ( next_address_parse( &server_address, server_address_string ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client open session failed. could not parse server address: %s", server_address_string );
        client->state = NEXT_CLIENT_STATE_ERROR;
        return;
    }

    next_client_command_open_session_t * command = (next_client_command_open_session_t*) next_malloc( client->context, sizeof( next_client_command_open_session_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client open session failed. could not create open session command" );
        client->state = NEXT_CLIENT_STATE_ERROR;
        return;
    }

    command->type = NEXT_CLIENT_COMMAND_OPEN_SESSION;
    command->server_address = server_address;

    {
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "client sent NEXT_CLIENT_COMMAND_OPEN_SESSION" );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &client->internal->command_mutex );
        next_queue_push( client->internal->command_queue, command );
    }

    client->state = NEXT_CLIENT_STATE_OPEN;
    client->server_address = server_address;
    client->open_session_sequence++;
}

bool next_client_is_session_open( next_client_t * client )
{
    next_client_verify_sentinels( client );

    return client->state == NEXT_CLIENT_STATE_OPEN;
}

int next_client_state( next_client_t * client )
{
    next_client_verify_sentinels( client );

    return client->state;
}

void next_client_close_session( next_client_t * client )
{
    next_client_verify_sentinels( client );

    next_assert( client->internal );

    next_client_command_close_session_t * command = (next_client_command_close_session_t*) next_malloc( client->context, sizeof( next_client_command_close_session_t ) );
    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client close session failed. could not create close session command" );
        client->state = NEXT_CLIENT_STATE_ERROR;
        return;
    }

    command->type = NEXT_CLIENT_COMMAND_CLOSE_SESSION;
    {
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "client sent NEXT_CLIENT_COMMAND_CLOSE_SESSION" );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &client->internal->command_mutex );
        next_queue_push( client->internal->command_queue, command );
    }

    client->ready = false;
    client->upgraded = false;
    client->fallback_to_direct = false;
    client->session_id = 0;
    memset( &client->client_stats, 0, sizeof(next_client_stats_t ) );
    memset( &client->server_address, 0, sizeof(next_address_t) );
    memset( &client->client_external_address, 0, sizeof(next_address_t) );
    next_bandwidth_limiter_reset( &client->direct_send_bandwidth );
    next_bandwidth_limiter_reset( &client->direct_receive_bandwidth );
    next_bandwidth_limiter_reset( &client->next_send_bandwidth );
    next_bandwidth_limiter_reset( &client->next_receive_bandwidth );
    client->state = NEXT_CLIENT_STATE_CLOSED;
    memset( client->current_magic, 0, sizeof(client->current_magic) );
}

void next_client_update( next_client_t * client )
{
    next_client_verify_sentinels( client );

#if NEXT_SPIKE_TRACKING
    next_printf( NEXT_LOG_LEVEL_SPAM, "next_client_update" );
#endif // #if NEXT_SPIKE_TRACKING

    while ( true )
    {
        void * entry = NULL;
        {
            next_platform_mutex_guard( &client->internal->notify_mutex );
            entry = next_queue_pop( client->internal->notify_queue );
        }

        if ( entry == NULL )
            break;

        next_client_notify_t * notify = (next_client_notify_t*) entry;

        switch ( notify->type )
        {
            case NEXT_CLIENT_NOTIFY_PACKET_RECEIVED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "NEXT_CLIENT_NOTIFY_STATS_UPGRADED" );
#endif // #if NEXT_SPIKE_TRACKING

                next_client_notify_packet_received_t * packet_received = (next_client_notify_packet_received_t*) notify;

#if NEXT_SPIKE_TRACKING
                char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_printf( NEXT_LOG_LEVEL_SPAM, "client calling packet received callback: from = %s, payload = %d bytes", next_address_to_string( &client->server_address, address_buffer ), packet_received->payload_bytes );
#endif // #if NEXT_SPIKE_TRACKING

                const bool direct = packet_received->direct;
                const bool already_received = packet_received->already_received;

                if ( !already_received )
                {
                    client->packet_received_callback( client, client->context, &client->server_address, packet_received->payload_data, packet_received->payload_bytes );
                }

                const int wire_packet_bits = next_wire_packet_bits( packet_received->payload_bytes );

                if ( direct )
                {
                    next_bandwidth_limiter_add_packet( &client->direct_receive_bandwidth, next_platform_time(), 0, wire_packet_bits );

                    double direct_kbps_down = next_bandwidth_limiter_usage_kbps( &client->direct_receive_bandwidth );

                    {
                        next_platform_mutex_guard( &client->internal->direct_bandwidth_mutex );
                        client->internal->direct_bandwidth_usage_kbps_down = direct_kbps_down;
                    }
                }
                else
                {
                    int envelope_kbps_down;
                    {
                        next_platform_mutex_guard( &client->internal->next_bandwidth_mutex );
                        envelope_kbps_down = client->internal->next_bandwidth_envelope_kbps_down;
                    }

                    next_bandwidth_limiter_add_packet( &client->next_receive_bandwidth, next_platform_time(), envelope_kbps_down, wire_packet_bits );

                    double next_kbps_down = next_bandwidth_limiter_usage_kbps( &client->next_receive_bandwidth );

                    {
                        next_platform_mutex_guard( &client->internal->next_bandwidth_mutex );
                        client->internal->next_bandwidth_usage_kbps_down = next_kbps_down;
                    }
                }
            }
            break;

            case NEXT_CLIENT_NOTIFY_UPGRADED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "NEXT_CLIENT_NOTIFY_STATS_UPGRADED" );
#endif // #if NEXT_SPIKE_TRACKING
                next_client_notify_upgraded_t * upgraded = (next_client_notify_upgraded_t*) notify;
                client->upgraded = true;
                client->session_id = upgraded->session_id;
                client->client_external_address = upgraded->client_external_address;
                memcpy( client->current_magic, upgraded->current_magic, 8 );
                next_printf( NEXT_LOG_LEVEL_INFO, "client upgraded to session %" PRIx64, client->session_id );
            }
            break;

            case NEXT_CLIENT_NOTIFY_STATS_UPDATED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "NEXT_CLIENT_NOTIFY_STATS_UPDATED" );
#endif // #if NEXT_SPIKE_TRACKING
                next_client_notify_stats_updated_t * stats_updated = (next_client_notify_stats_updated_t*) notify;
                client->client_stats = stats_updated->stats;
                client->fallback_to_direct = stats_updated->fallback_to_direct;
            }
            break;

            case NEXT_CLIENT_NOTIFY_MAGIC_UPDATED:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "NEXT_CLIENT_NOTIFY_MAGIC_UPDATED" );
#endif // #if NEXT_SPIKE_TRACKING
                next_client_notify_magic_updated_t * magic_updated = (next_client_notify_magic_updated_t*) notify;
                memcpy( client->current_magic, magic_updated->current_magic, 8 );
            }
            break;

            case NEXT_CLIENT_NOTIFY_READY:
            {
#if NEXT_SPIKE_TRACKING
                next_printf( NEXT_LOG_LEVEL_SPAM, "NEXT_CLIENT_NOTIFY_READY" );
#endif // #if NEXT_SPIKE_TRACKING
                client->ready = true;
            }
            break;

            default: break;
        }

        next_free( client->context, entry );
    }
}

bool next_client_ready( next_client_t * client )
{
    next_assert( client );
    return ( next_global_config.disable_network_next || client->ready ) ? true : false;
}

bool next_client_fallback_to_direct( struct next_client_t * client )
{
    next_assert( client );
    return client->client_stats.fallback_to_direct;
}

void next_client_send_packet( next_client_t * client, const uint8_t * packet_data, int packet_bytes )
{
    next_client_verify_sentinels( client );

    next_assert( client->internal );
    next_assert( client->internal->socket );
    next_assert( packet_bytes > 0 );

    if ( client->state != NEXT_CLIENT_STATE_OPEN )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client can't send packet because no session is open" );
        return;
    }

    if ( packet_bytes > NEXT_MAX_PACKET_BYTES - 1 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client can't send packet because packet is too large" );
        return;
    }

    if ( next_global_config.disable_network_next || client->fallback_to_direct )
    {
        next_client_send_packet_direct( client, packet_data, packet_bytes );
        return;
    }

#if NEXT_DEVELOPMENT
    if ( next_packet_loss && ( rand() % 10 ) == 0 )
        return;
#endif // #if NEXT_DEVELOPMENT

    if ( client->upgraded && packet_bytes <= NEXT_MTU )
    {
        uint64_t send_sequence = 0;
        bool send_over_network_next = false;
        {
            next_platform_mutex_guard( &client->internal->route_manager_mutex );
            send_sequence = next_route_manager_next_send_sequence( client->internal->route_manager );
            send_over_network_next = next_route_manager_has_network_next_route( client->internal->route_manager );
        }

        bool send_direct = !send_over_network_next;
        bool multipath = client->client_stats.multipath;
        if ( send_over_network_next && multipath )
        {
            send_direct = true;
        }

        // track direct send bandwidth

        const int wire_packet_bits = next_wire_packet_bits( packet_bytes );

        next_bandwidth_limiter_add_packet( &client->direct_send_bandwidth, next_platform_time(), 0, wire_packet_bits );

        double direct_usage_kbps_up = next_bandwidth_limiter_usage_kbps( &client->direct_send_bandwidth );

        {
            next_platform_mutex_guard( &client->internal->direct_bandwidth_mutex );
            client->internal->direct_bandwidth_usage_kbps_up = direct_usage_kbps_up;
        }

        // track next send bandwidth and don't send over network next if we're over the bandwidth budget

        if ( send_over_network_next )
        {
            int next_envelope_kbps_up;
            {
                next_platform_mutex_guard( &client->internal->next_bandwidth_mutex );
                next_envelope_kbps_up = client->internal->next_bandwidth_envelope_kbps_up;
            }

            bool over_budget = next_bandwidth_limiter_add_packet( &client->next_send_bandwidth, next_platform_time(), next_envelope_kbps_up, wire_packet_bits );

            double next_usage_kbps_up = next_bandwidth_limiter_usage_kbps( &client->next_send_bandwidth );

            {
                next_platform_mutex_guard( &client->internal->next_bandwidth_mutex );
                client->internal->next_bandwidth_usage_kbps_up = next_usage_kbps_up;
                if ( over_budget )
                    client->internal->next_bandwidth_over_limit = true;
            }

            if ( over_budget )
            {
                next_printf( NEXT_LOG_LEVEL_WARN, "client exceeded bandwidth budget (%d kbps)", next_envelope_kbps_up );
                send_over_network_next = false;
                send_direct = true;
            }
        }

        if ( send_over_network_next )
        {
            // send over network next

            int next_packet_bytes = 0;
            next_address_t next_to;
            uint8_t next_packet_data[NEXT_MAX_PACKET_BYTES];

            bool result = false;
            {
                next_platform_mutex_guard( &client->internal->route_manager_mutex );
                result = next_route_manager_prepare_send_packet( client->internal->route_manager, send_sequence, &next_to, packet_data, packet_bytes, next_packet_data, &next_packet_bytes, client->current_magic, &client->client_external_address );
            }

            if ( result )
            {
#if NEXT_SPIKE_TRACKING
                double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

                next_platform_socket_send_packet( client->internal->socket, &next_to, next_packet_data, next_packet_bytes );

#if NEXT_SPIKE_TRACKING
                double finish_time = next_platform_time();
                if ( finish_time - start_time > 0.001 )
                {
                    next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
                }
#endif // #if NEXT_SPIKE_TRACKING

                client->counters[NEXT_CLIENT_COUNTER_PACKET_SENT_NEXT]++;
            }
            else
            {
                // could not send over network next
                send_direct = true;
            }
        }

        if ( send_direct )
        {
            // send direct from client to server

            uint8_t from_address_data[32];
            uint8_t to_address_data[32];
            int from_address_bytes;
            int to_address_bytes;

            next_address_data( &client->client_external_address, from_address_data, &from_address_bytes );
            next_address_data( &client->server_address, to_address_data, &to_address_bytes );

            uint8_t direct_packet_data[NEXT_MAX_PACKET_BYTES];

            const int direct_packet_bytes = next_write_direct_packet( direct_packet_data, client->open_session_sequence, send_sequence, packet_data, packet_bytes, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

            next_assert( direct_packet_bytes >= 0 );

            next_assert( next_basic_packet_filter( direct_packet_data, direct_packet_bytes ) );
            next_assert( next_advanced_packet_filter( direct_packet_data, client->current_magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, direct_packet_bytes ) );

            (void) direct_packet_data;
            (void) direct_packet_bytes;

#if NEXT_SPIKE_TRACKING 
            double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

            next_platform_socket_send_packet( client->internal->socket, &client->server_address, direct_packet_data, direct_packet_bytes );

#if NEXT_SPIKE_TRACKING
            double finish_time = next_platform_time();
            if ( finish_time - start_time > 0.001 )
            {
                next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
            }
#endif // #if NEXT_SPIKE_TRACKING

            client->counters[NEXT_CLIENT_COUNTER_PACKET_SENT_DIRECT]++;
        }

        client->internal->packets_sent++;
    }
    else
    {
        // passthrough packet

        next_client_send_packet_direct( client, packet_data, packet_bytes );
    }
}

void next_client_send_packet_direct( next_client_t * client, const uint8_t * packet_data, int packet_bytes )
{
    next_client_verify_sentinels( client );

    next_assert( client->internal );
    next_assert( client->internal->socket );
    next_assert( packet_bytes > 0 );

    if ( client->state != NEXT_CLIENT_STATE_OPEN )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client can't send packet because no session is open" );
        return;
    }

    if ( packet_bytes <= 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client can't send packet because packet size <= 0" );
        return;
    }

    if ( packet_bytes > NEXT_MAX_PACKET_BYTES - 1 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client can't send packet because packet is too large" );
        return;
    }

    uint8_t buffer[NEXT_MAX_PACKET_BYTES];
    buffer[0] = NEXT_PASSTHROUGH_PACKET;
    memcpy( buffer + 1, packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( client->internal->socket, &client->server_address, buffer, packet_bytes + 1 );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING

    client->counters[NEXT_CLIENT_COUNTER_PACKET_SENT_PASSTHROUGH]++;

    client->internal->packets_sent++;
}

void next_client_send_packet_raw( next_client_t * client, const next_address_t * to_address, const uint8_t * packet_data, int packet_bytes )
{
    next_client_verify_sentinels( client );

    next_assert( client->internal );
    next_assert( client->internal->socket );
    next_assert( to_address );
    next_assert( packet_bytes > 0 );

#if NEXT_SPIKE_TRACKING
    double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

    next_platform_socket_send_packet( client->internal->socket, to_address, packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
    double finish_time = next_platform_time();
    if ( finish_time - start_time > 0.001 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
    }
#endif // #if NEXT_SPIKE_TRACKING
}

void next_client_report_session( next_client_t * client )
{
    next_client_verify_sentinels( client );

    next_client_command_report_session_t * command = (next_client_command_report_session_t*) next_malloc( client->context, sizeof( next_client_command_report_session_t ) );

    if ( !command )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "report session failed. could not create report session command" );
        return;
    }

    command->type = NEXT_CLIENT_COMMAND_REPORT_SESSION;
    {
#if NEXT_SPIKE_TRACKING
        next_printf( NEXT_LOG_LEVEL_SPAM, "client sent NEXT_CLIENT_COMMAND_REPORT_SESSION" );
#endif // #if NEXT_SPIKE_TRACKING
        next_platform_mutex_guard( &client->internal->command_mutex );
        next_queue_push( client->internal->command_queue, command );
    }
}

uint64_t next_client_session_id( next_client_t * client )
{
    next_client_verify_sentinels( client );

    return client->session_id;
}

const next_client_stats_t * next_client_stats( next_client_t * client )
{
    next_client_verify_sentinels( client );

    return &client->client_stats;
}

const next_address_t * next_client_server_address( next_client_t * client )
{
    next_assert( client );
    return &client->server_address;
}

void next_client_counters( next_client_t * client, uint64_t * counters )
{
    next_client_verify_sentinels( client );
    memcpy( counters, client->counters, sizeof(uint64_t) * NEXT_CLIENT_COUNTER_MAX );
    for ( int i = 0; i < NEXT_CLIENT_COUNTER_MAX; ++i )
        counters[i] += client->internal->counters[i];
}

// ---------------------------------------------------------------

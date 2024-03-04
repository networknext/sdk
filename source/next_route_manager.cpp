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

#include "next_route_manager.h"
#include "next_memory_checks.h"
#include "next_address.h"
#include "next_crypto.h"
#include "next_config.h"
#include "next_route_token.h"
#include "next_continue_token.h"
#include "next_platform.h"
#include "next_packet_filter.h"
#include "next_packets.h"
#include "next_header.h"

#include <memory.h>

struct next_route_data_t
{
    NEXT_DECLARE_SENTINEL(0)

    bool current_route;
    double current_route_expire_time;
    uint64_t current_route_session_id;
    uint8_t current_route_session_version;
    int current_route_kbps_up;
    int current_route_kbps_down;
    next_address_t current_route_next_address;

    NEXT_DECLARE_SENTINEL(1)

    uint8_t current_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(2)

    bool previous_route;
    uint64_t previous_route_session_id;
    uint8_t previous_route_session_version;

    NEXT_DECLARE_SENTINEL(3)

    uint8_t previous_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(4)

    bool pending_route;
    double pending_route_start_time;
    double pending_route_last_send_time;
    uint64_t pending_route_session_id;
    uint8_t pending_route_session_version;
    int pending_route_kbps_up;
    int pending_route_kbps_down;
    int pending_route_request_packet_bytes;
    next_address_t pending_route_next_address;

    NEXT_DECLARE_SENTINEL(5)

    uint8_t pending_route_request_packet_data[NEXT_MAX_PACKET_BYTES];

    NEXT_DECLARE_SENTINEL(6)

    uint8_t pending_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(7)

    bool pending_continue;
    double pending_continue_start_time;
    double pending_continue_last_send_time;
    int pending_continue_request_packet_bytes;

    NEXT_DECLARE_SENTINEL(8)

    uint8_t pending_continue_request_packet_data[NEXT_MAX_PACKET_BYTES];

    NEXT_DECLARE_SENTINEL(9)
};

void next_route_data_initialize_sentinels( next_route_data_t * route_data )
{
    (void) route_data;
    next_assert( route_data );
    NEXT_INITIALIZE_SENTINEL( route_data, 0 )
    NEXT_INITIALIZE_SENTINEL( route_data, 1 )
    NEXT_INITIALIZE_SENTINEL( route_data, 2 )
    NEXT_INITIALIZE_SENTINEL( route_data, 3 )
    NEXT_INITIALIZE_SENTINEL( route_data, 4 )
    NEXT_INITIALIZE_SENTINEL( route_data, 5 )
    NEXT_INITIALIZE_SENTINEL( route_data, 6 )
    NEXT_INITIALIZE_SENTINEL( route_data, 7 )
    NEXT_INITIALIZE_SENTINEL( route_data, 8 )
    NEXT_INITIALIZE_SENTINEL( route_data, 9 )
}

void next_route_data_verify_sentinels( next_route_data_t * route_data )
{
    (void) route_data;
    next_assert( route_data );
    NEXT_VERIFY_SENTINEL( route_data, 0 )
    NEXT_VERIFY_SENTINEL( route_data, 1 )
    NEXT_VERIFY_SENTINEL( route_data, 2 )
    NEXT_VERIFY_SENTINEL( route_data, 3 )
    NEXT_VERIFY_SENTINEL( route_data, 4 )
    NEXT_VERIFY_SENTINEL( route_data, 5 )
    NEXT_VERIFY_SENTINEL( route_data, 6 )
    NEXT_VERIFY_SENTINEL( route_data, 7 )
    NEXT_VERIFY_SENTINEL( route_data, 8 )
    NEXT_VERIFY_SENTINEL( route_data, 9 )
}

struct next_route_manager_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    uint64_t send_sequence;
    bool fallback_to_direct;
    next_route_data_t route_data;
    double last_route_update_time;
    uint32_t flags;

    NEXT_DECLARE_SENTINEL(1)
};

void next_route_manager_initialize_sentinels( next_route_manager_t * route_manager )
{
    (void) route_manager;
    next_assert( route_manager );
    NEXT_INITIALIZE_SENTINEL( route_manager, 0 )
    NEXT_INITIALIZE_SENTINEL( route_manager, 1 )
    next_route_data_initialize_sentinels( &route_manager->route_data );
}

void next_route_manager_verify_sentinels( next_route_manager_t * route_manager )
{
    (void) route_manager;
    next_assert( route_manager );
    NEXT_VERIFY_SENTINEL( route_manager, 0 )
    NEXT_VERIFY_SENTINEL( route_manager, 1 )
    next_route_data_verify_sentinels( &route_manager->route_data );
}

next_route_manager_t * next_route_manager_create( void * context )
{
    next_route_manager_t * route_manager = (next_route_manager_t*) next_malloc( context, sizeof(next_route_manager_t) );
    if ( !route_manager )
        return NULL;
    memset( route_manager, 0, sizeof(next_route_manager_t) );
    next_route_manager_initialize_sentinels( route_manager );
    route_manager->context = context;
    return route_manager;
}

void next_route_manager_reset( next_route_manager_t * route_manager )
{
    next_route_manager_verify_sentinels( route_manager );

    route_manager->send_sequence = 0;
    route_manager->fallback_to_direct = false;
    route_manager->last_route_update_time = 0.0;

    memset( &route_manager->route_data, 0, sizeof(next_route_data_t) );

    next_route_manager_initialize_sentinels( route_manager );

    route_manager->flags = 0;

    next_route_manager_verify_sentinels( route_manager );
}

void next_route_manager_fallback_to_direct( next_route_manager_t * route_manager, uint32_t flags )
{
    next_route_manager_verify_sentinels( route_manager );

    route_manager->flags |= flags;

    if ( route_manager->fallback_to_direct )
        return;

    route_manager->fallback_to_direct = true;

    next_printf( NEXT_LOG_LEVEL_INFO, "client fallback to direct" );

    route_manager->route_data.previous_route = route_manager->route_data.current_route;
    route_manager->route_data.previous_route_session_id = route_manager->route_data.current_route_session_id;
    route_manager->route_data.previous_route_session_version = route_manager->route_data.current_route_session_version;
    memcpy( route_manager->route_data.previous_route_private_key, route_manager->route_data.current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );

    route_manager->route_data.current_route = false;
}

void next_route_manager_direct_route( next_route_manager_t * route_manager, bool quiet )
{
    next_route_manager_verify_sentinels( route_manager );

    if ( route_manager->fallback_to_direct )
        return;

    if ( !quiet )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "client direct route" );
    }

    route_manager->route_data.previous_route = route_manager->route_data.current_route;
    route_manager->route_data.previous_route_session_id = route_manager->route_data.current_route_session_id;
    route_manager->route_data.previous_route_session_version = route_manager->route_data.current_route_session_version;
    memcpy( route_manager->route_data.previous_route_private_key, route_manager->route_data.current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );

    route_manager->route_data.current_route = false;
}

void next_route_manager_begin_next_route( next_route_manager_t * route_manager, int num_tokens, uint8_t * tokens, const uint8_t * client_secret_key, const uint8_t * magic, const next_address_t * client_external_address )
{
    next_route_manager_verify_sentinels( route_manager );

    next_assert( tokens );
    next_assert( num_tokens >= 2 );
    next_assert( num_tokens <= NEXT_MAX_TOKENS );

    if ( route_manager->fallback_to_direct )
        return;

    uint8_t * p = tokens;
    next_route_token_t route_token;
    if ( next_read_encrypted_route_token( &p, &route_token, client_secret_key ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client received bad route token" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_BAD_ROUTE_TOKEN );
        return;
    }

    next_printf( NEXT_LOG_LEVEL_INFO, "client next route" );

    route_manager->route_data.pending_route = true;
    route_manager->route_data.pending_route_start_time = next_platform_time();
    route_manager->route_data.pending_route_last_send_time = -1000.0;
    route_manager->route_data.pending_route_next_address.type = NEXT_ADDRESS_IPV4;
    route_manager->route_data.pending_route_next_address.data.ip = route_token.next_address;
    route_manager->route_data.pending_route_next_address.port = route_token.next_port;
    route_manager->route_data.pending_route_session_id = route_token.session_id;
    route_manager->route_data.pending_route_session_version = route_token.session_version;
    route_manager->route_data.pending_route_kbps_up = route_token.kbps_up;
    route_manager->route_data.pending_route_kbps_down = route_token.kbps_down;

    memcpy( route_manager->route_data.pending_route_request_packet_data + 1, tokens + NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES, ( size_t(num_tokens) - 1 ) * NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES );
    memcpy( route_manager->route_data.pending_route_private_key, route_token.private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );

    const uint8_t * token_data = tokens + NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES;
    const int token_bytes = ( num_tokens - 1 ) * NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES;

    uint8_t from_address_data[4];
    uint8_t to_address_data[4];

    next_address_data( client_external_address, from_address_data );
    memcpy( to_address_data, &route_token.next_address, 4 );

    route_manager->route_data.pending_route_request_packet_bytes = next_write_route_request_packet( route_manager->route_data.pending_route_request_packet_data, token_data, token_bytes, magic, from_address_data, to_address_data );

    next_assert( route_manager->route_data.pending_route_request_packet_bytes > 0 );
    next_assert( route_manager->route_data.pending_route_request_packet_bytes <= NEXT_MAX_PACKET_BYTES );

    const uint8_t * packet_data = route_manager->route_data.pending_route_request_packet_data;
    const int packet_bytes = route_manager->route_data.pending_route_request_packet_bytes;

    next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
    next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, to_address_data, packet_bytes ) );

    (void) packet_data;
    (void) packet_bytes;
}

void next_route_manager_continue_next_route( next_route_manager_t * route_manager, int num_tokens, uint8_t * tokens, const uint8_t * secret_key, const uint8_t * magic, const next_address_t * client_external_address )
{
    next_route_manager_verify_sentinels( route_manager );

    next_assert( tokens );
    next_assert( num_tokens >= 2 );
    next_assert( num_tokens <= NEXT_MAX_TOKENS );

    if ( route_manager->fallback_to_direct )
        return;

    if ( !route_manager->route_data.current_route )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client has no route to continue" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_NO_ROUTE_TO_CONTINUE );
        return;
    }

    if ( route_manager->route_data.pending_route || route_manager->route_data.pending_continue )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client previous update still pending" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_PREVIOUS_UPDATE_STILL_PENDING );
        return;
    }

    uint8_t * p = tokens;
    next_continue_token_t continue_token;
    if ( next_read_encrypted_continue_token( &p, &continue_token, secret_key ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client received bad continue token" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_BAD_CONTINUE_TOKEN );
        return;
    }

    route_manager->route_data.pending_continue = true;
    route_manager->route_data.pending_continue_start_time = next_platform_time();
    route_manager->route_data.pending_continue_last_send_time = -1000.0;

    uint8_t from_address_data[4];
    uint8_t to_address_data[4];

    next_address_data( client_external_address, from_address_data );
    next_address_data( &route_manager->route_data.current_route_next_address, to_address_data );

    const uint8_t * token_data = tokens + NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES;
    const int token_bytes = ( num_tokens - 1 ) * NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES;

    route_manager->route_data.pending_continue_request_packet_bytes = next_write_continue_request_packet( route_manager->route_data.pending_continue_request_packet_data, token_data, token_bytes, magic, from_address_data, to_address_data );

    next_assert( route_manager->route_data.pending_continue_request_packet_bytes >= 0 );
    next_assert( route_manager->route_data.pending_continue_request_packet_bytes <= NEXT_MAX_PACKET_BYTES );

    const uint8_t * packet_data = route_manager->route_data.pending_continue_request_packet_data;
    const int packet_bytes = route_manager->route_data.pending_continue_request_packet_bytes;

    next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
    next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, to_address_data, packet_bytes ) );

    (void) packet_data;
    (void) packet_bytes;

    next_printf( NEXT_LOG_LEVEL_INFO, "client continues route" );
}

void next_route_manager_update( next_route_manager_t * route_manager, int update_type, int num_tokens, uint8_t * tokens, const uint8_t * client_secret_key, const uint8_t * magic, const next_address_t * client_external_address )
{
    next_route_manager_verify_sentinels( route_manager );

    next_assert( client_secret_key );

    if ( update_type == NEXT_UPDATE_TYPE_DIRECT )
    {
        next_route_manager_direct_route( route_manager, false );
    }
    else if ( update_type == NEXT_UPDATE_TYPE_ROUTE )
    {
        next_route_manager_begin_next_route( route_manager, num_tokens, tokens, client_secret_key, magic, client_external_address );
    }
    else if ( update_type == NEXT_UPDATE_TYPE_CONTINUE )
    {
        next_route_manager_continue_next_route( route_manager, num_tokens, tokens, client_secret_key, magic, client_external_address );
    }
}

bool next_route_manager_has_network_next_route( next_route_manager_t * route_manager )
{
    next_route_manager_verify_sentinels( route_manager );
    return route_manager->route_data.current_route;
}

uint64_t next_route_manager_next_send_sequence( next_route_manager_t * route_manager )
{
    next_route_manager_verify_sentinels( route_manager );
    return route_manager->send_sequence++;
}

bool next_route_manager_prepare_send_packet( next_route_manager_t * route_manager, uint64_t sequence, next_address_t * to, const uint8_t * payload_data, int payload_bytes, uint8_t * packet_data, int * packet_bytes, const uint8_t * magic, const next_address_t * client_external_address )
{
    next_route_manager_verify_sentinels( route_manager );

    if ( !route_manager->route_data.current_route )
        return false;

    next_assert( route_manager->route_data.current_route );
    next_assert( to );
    next_assert( payload_data );
    next_assert( payload_bytes );
    next_assert( packet_data );
    next_assert( packet_bytes );

    *to = route_manager->route_data.current_route_next_address;

    uint8_t from_address_data[4];
    uint8_t to_address_data[4];

    next_address_data( client_external_address, from_address_data );
    next_address_data( to, to_address_data );

    *packet_bytes = next_write_client_to_server_packet( packet_data, sequence, route_manager->route_data.current_route_session_id, route_manager->route_data.current_route_session_version, route_manager->route_data.current_route_private_key, payload_data, payload_bytes, magic, from_address_data, to_address_data );

    if ( *packet_bytes == 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client failed to write client to server packet header" );
        return false;
    }

    next_assert( next_basic_packet_filter( packet_data, *packet_bytes ) );
    next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, to_address_data, *packet_bytes ) );

    next_assert( *packet_bytes < NEXT_MAX_PACKET_BYTES );

    return true;
}

bool next_route_manager_process_server_to_client_packet( next_route_manager_t * route_manager, uint8_t packet_type, uint8_t * packet_data, int packet_bytes, uint64_t * payload_sequence )
{
    next_route_manager_verify_sentinels( route_manager );

    next_assert( packet_data );
    next_assert( payload_sequence );

    uint64_t packet_sequence = 0;
    uint64_t packet_session_id = 0;
    uint8_t packet_session_version = 0;

    bool from_current_route = true;

    if ( next_read_header( packet_type, &packet_sequence, &packet_session_id, &packet_session_version, route_manager->route_data.current_route_private_key, packet_data, packet_bytes ) != NEXT_OK )
    {
        from_current_route = false;
        if ( next_read_header( packet_type, &packet_sequence, &packet_session_id, &packet_session_version, route_manager->route_data.previous_route_private_key, packet_data, packet_bytes ) != NEXT_OK )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. could not read header" );
            return false;
        }
    }

    if ( !route_manager->route_data.current_route && !route_manager->route_data.previous_route )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. no current or previous route" );
        return false;
    }

    if ( from_current_route )
    {
        if ( packet_session_id != route_manager->route_data.current_route_session_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. session id mismatch (current route)" );
            return false;
        }

        if ( packet_session_version != route_manager->route_data.current_route_session_version )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. session version mismatch (current route)" );
            return false;
        }
    }
    else
    {
        if ( packet_session_id != route_manager->route_data.previous_route_session_id )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. session id mismatch (previous route)" );
            return false;
        }

        if ( packet_session_version != route_manager->route_data.previous_route_session_version )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. session version mismatch (previous route)" );
            return false;
        }
    }

    *payload_sequence = packet_sequence;

    int payload_bytes = packet_bytes - NEXT_HEADER_BYTES;

    if ( payload_bytes > NEXT_MTU )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored server to client packet. too large (%d>%d)", payload_bytes, NEXT_MTU );
        return false;
    }

    (void) payload_bytes;

    return true;
}

void next_route_manager_check_for_timeouts( next_route_manager_t * route_manager )
{
    next_route_manager_verify_sentinels( route_manager );

    if ( route_manager->fallback_to_direct )
        return;

    const double current_time = next_platform_time();

    if ( route_manager->last_route_update_time > 0.0 && route_manager->last_route_update_time + NEXT_CLIENT_ROUTE_TIMEOUT < current_time )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client route timed out" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_ROUTE_TIMED_OUT );
        return;
    }

    if ( route_manager->route_data.current_route && route_manager->route_data.current_route_expire_time <= current_time )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client route expired" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_ROUTE_EXPIRED );
        return;
    }

    if ( route_manager->route_data.pending_route && route_manager->route_data.pending_route_start_time + NEXT_ROUTE_REQUEST_TIMEOUT <= current_time )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client route request timed out" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_ROUTE_REQUEST_TIMED_OUT );
        return;
    }

    if ( route_manager->route_data.pending_continue && route_manager->route_data.pending_continue_start_time + NEXT_CONTINUE_REQUEST_TIMEOUT <= current_time )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client continue request timed out" );
        next_route_manager_fallback_to_direct( route_manager, NEXT_FLAGS_CONTINUE_REQUEST_TIMED_OUT );
        return;
    }
}

bool next_route_manager_send_route_request( next_route_manager_t * route_manager, next_address_t * to, uint8_t * packet_data, int * packet_bytes )
{
    next_route_manager_verify_sentinels( route_manager );

    next_assert( to );
    next_assert( packet_data );
    next_assert( packet_bytes );

    if ( route_manager->fallback_to_direct )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client not sending route request. fallback to direct" );
        return false;
    }

    if ( !route_manager->route_data.pending_route )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client not sending route request. pending route" );
        return false;
    }

    double current_time = next_platform_time();

    if ( route_manager->route_data.pending_route_last_send_time + NEXT_ROUTE_REQUEST_SEND_TIME > current_time )
    {
        next_printf( NEXT_LOG_LEVEL_SPAM, "client not sending route request. not yet" );
        return false;
    }

    *to = route_manager->route_data.pending_route_next_address;
    route_manager->route_data.pending_route_last_send_time = current_time;
    *packet_bytes = route_manager->route_data.pending_route_request_packet_bytes;
    memcpy( packet_data, route_manager->route_data.pending_route_request_packet_data, route_manager->route_data.pending_route_request_packet_bytes );

    return true;
}

bool next_route_manager_send_continue_request( next_route_manager_t * route_manager, next_address_t * to, uint8_t * packet_data, int * packet_bytes )
{
    next_route_manager_verify_sentinels( route_manager );

    next_assert( to );
    next_assert( packet_data );
    next_assert( packet_bytes );

    if ( route_manager->fallback_to_direct )
        return false;

    if ( !route_manager->route_data.current_route || !route_manager->route_data.pending_continue )
        return false;

    double current_time = next_platform_time();

    if ( route_manager->route_data.pending_continue_last_send_time + NEXT_CONTINUE_REQUEST_SEND_TIME > current_time )
        return false;

    *to = route_manager->route_data.current_route_next_address;
    route_manager->route_data.pending_continue_last_send_time = current_time;
    *packet_bytes = route_manager->route_data.pending_continue_request_packet_bytes;
    memcpy( packet_data, route_manager->route_data.pending_continue_request_packet_data, route_manager->route_data.pending_continue_request_packet_bytes );

    return true;
}

void next_route_manager_destroy( next_route_manager_t * route_manager )
{
    next_route_manager_verify_sentinels( route_manager );

    next_free( route_manager->context, route_manager );
}

void next_route_manager_get_pending_route_data( next_route_manager_t * route_manager, bool * fallback_to_direct, bool * pending_route, uint64_t * pending_route_session_id, uint8_t * pending_route_session_version, uint8_t * pending_route_private_key )
{
    next_assert( route_manager );
    *fallback_to_direct = route_manager->fallback_to_direct;
    *pending_route = route_manager->route_data.pending_route;
    *pending_route_session_id = route_manager->route_data.pending_route_session_id;
    *pending_route_session_version = route_manager->route_data.pending_route_session_version;
    memcpy( pending_route_private_key, route_manager->route_data.pending_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
}

void next_route_manager_confirm_pending_route( next_route_manager_t * route_manager, int * route_kbps_up, int * route_kbps_down )
{
    if ( route_manager->route_data.current_route )
    {
        route_manager->route_data.previous_route = route_manager->route_data.current_route;
        route_manager->route_data.previous_route_session_id = route_manager->route_data.current_route_session_id;
        route_manager->route_data.previous_route_session_version = route_manager->route_data.current_route_session_version;
        memcpy( route_manager->route_data.previous_route_private_key, route_manager->route_data.current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
    }

    route_manager->route_data.current_route_session_id = route_manager->route_data.pending_route_session_id;
    route_manager->route_data.current_route_session_version = route_manager->route_data.pending_route_session_version;
    route_manager->route_data.current_route_kbps_up = route_manager->route_data.pending_route_kbps_up;
    route_manager->route_data.current_route_kbps_down = route_manager->route_data.pending_route_kbps_down;
    route_manager->route_data.current_route_next_address = route_manager->route_data.pending_route_next_address;
    memcpy( route_manager->route_data.current_route_private_key, route_manager->route_data.pending_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );

    if ( !route_manager->route_data.current_route )
    {
        route_manager->route_data.current_route_expire_time = route_manager->route_data.pending_route_start_time + 2.0 * NEXT_SLICE_SECONDS;
    }
    else
    {
        route_manager->route_data.current_route_expire_time += 2.0 * NEXT_SLICE_SECONDS;
    }

    route_manager->route_data.current_route = true;
    route_manager->route_data.pending_route = false;

    *route_kbps_up = route_manager->route_data.current_route_kbps_up;
    *route_kbps_down = route_manager->route_data.current_route_kbps_down;
}

void next_route_manager_get_current_route_data( next_route_manager_t * route_manager, bool * fallback_to_direct, bool * current_route, bool * pending_continue, uint64_t * current_route_session_id, uint8_t * current_route_session_version, uint8_t * current_route_private_key )
{
    next_assert( route_manager );
    *fallback_to_direct = route_manager->fallback_to_direct;
    *current_route = route_manager->route_data.current_route;
    *pending_continue = route_manager->route_data.pending_continue;
    *current_route_session_id = route_manager->route_data.current_route_session_id;
    *current_route_session_version = route_manager->route_data.current_route_session_version;
    memcpy( current_route_private_key, route_manager->route_data.current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
}

void next_route_manager_confirm_continue_route( next_route_manager_t * route_manager )
{
    next_assert( route_manager );
    route_manager->route_data.current_route_expire_time += NEXT_SLICE_SECONDS;
    route_manager->route_data.pending_continue = false;
}

bool next_route_manager_get_fallback_to_direct( next_route_manager_t * route_manager )
{
    next_assert( route_manager );
    return route_manager->fallback_to_direct;
}

void next_route_manager_get_next_route_data( next_route_manager_t * route_manager, uint64_t * session_id, uint8_t * session_version, next_address_t * to, uint8_t * private_key )
{
    *session_id = route_manager->route_data.current_route_session_id;
    *session_version = route_manager->route_data.current_route_session_version;
    *to = route_manager->route_data.current_route_next_address;
    memcpy( private_key, route_manager->route_data.current_route_private_key, NEXT_CRYPTO_BOX_SECRETKEYBYTES );
}

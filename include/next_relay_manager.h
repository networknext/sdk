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

#ifndef NEXT_RELAY_MANAGER_H
#define NEXT_RELAY_MANAGER_H

#include "next_config.h"
#include "next_address.h"
#include "next_ping_history.h"
#include "next_packet_filter.h"
#include "next_packets.h"

#include <memory.h>
#include <stdio.h>

// ---------------------------------------------------------------

struct next_relay_stats_t
{
    NEXT_DECLARE_SENTINEL(0)

    bool has_pings;
    int num_relays;

    NEXT_DECLARE_SENTINEL(1)

    uint64_t relay_ids[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(2)

    float relay_rtt[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(3)

    float relay_jitter[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(4)

    float relay_packet_loss[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(5)
};

inline void next_relay_stats_initialize_sentinels( next_relay_stats_t * stats )
{
    (void) stats;
    next_assert( stats );
    NEXT_INITIALIZE_SENTINEL( stats, 0 )
    NEXT_INITIALIZE_SENTINEL( stats, 1 )
    NEXT_INITIALIZE_SENTINEL( stats, 2 )
    NEXT_INITIALIZE_SENTINEL( stats, 3 )
    NEXT_INITIALIZE_SENTINEL( stats, 4 )
    NEXT_INITIALIZE_SENTINEL( stats, 5 )
}

inline void next_relay_stats_verify_sentinels( next_relay_stats_t * stats )
{
    (void) stats;
    next_assert( stats );
    NEXT_VERIFY_SENTINEL( stats, 0 )
    NEXT_VERIFY_SENTINEL( stats, 1 )
    NEXT_VERIFY_SENTINEL( stats, 2 )
    NEXT_VERIFY_SENTINEL( stats, 3 )
    NEXT_VERIFY_SENTINEL( stats, 4 )
    NEXT_VERIFY_SENTINEL( stats, 5 )
}

// ---------------------------------------------------------------

struct next_relay_manager_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    int num_relays;

    NEXT_DECLARE_SENTINEL(1)

    uint64_t relay_ids[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(2)

    double relay_last_ping_time[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(3)

    next_address_t relay_addresses[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(4)

    uint8_t relay_ping_tokens[NEXT_MAX_NEAR_RELAYS * NEXT_PING_TOKEN_BYTES];

    NEXT_DECLARE_SENTINEL(5)

    uint64_t relay_ping_expire_timestamp;

    NEXT_DECLARE_SENTINEL(6)

    next_ping_history_t relay_ping_history[NEXT_MAX_NEAR_RELAYS];

    NEXT_DECLARE_SENTINEL(7)
};

inline void next_relay_manager_initialize_sentinels( next_relay_manager_t * manager )
{
    (void) manager;

    next_assert( manager );

    NEXT_INITIALIZE_SENTINEL( manager, 0 )
    NEXT_INITIALIZE_SENTINEL( manager, 1 )
    NEXT_INITIALIZE_SENTINEL( manager, 2 )
    NEXT_INITIALIZE_SENTINEL( manager, 3 )
    NEXT_INITIALIZE_SENTINEL( manager, 4 )
    NEXT_INITIALIZE_SENTINEL( manager, 5 )
    NEXT_INITIALIZE_SENTINEL( manager, 6 )
    NEXT_INITIALIZE_SENTINEL( manager, 7 )

    for ( int i = 0; i < NEXT_MAX_NEAR_RELAYS; ++i )
        next_ping_history_initialize_sentinels( &manager->relay_ping_history[i] );
}

inline void next_relay_manager_verify_sentinels( next_relay_manager_t * manager )
{
    (void) manager;
#if NEXT_ENABLE_MEMORY_CHECKS
    next_assert( manager );
    NEXT_VERIFY_SENTINEL( manager, 0 )
    NEXT_VERIFY_SENTINEL( manager, 1 )
    NEXT_VERIFY_SENTINEL( manager, 2 )
    NEXT_VERIFY_SENTINEL( manager, 3 )
    NEXT_VERIFY_SENTINEL( manager, 4 )
    NEXT_VERIFY_SENTINEL( manager, 5 )
    NEXT_VERIFY_SENTINEL( manager, 6 )
    NEXT_VERIFY_SENTINEL( manager, 7 )
    for ( int i = 0; i < NEXT_MAX_NEAR_RELAYS; ++i )
        next_ping_history_verify_sentinels( &manager->relay_ping_history[i] );
#endif // #if NEXT_ENABLE_MEMORY_CHECKS
}

void next_relay_manager_reset( next_relay_manager_t * manager );

inline next_relay_manager_t * next_relay_manager_create( void * context )
{
    next_relay_manager_t * manager = (next_relay_manager_t*) next_malloc( context, sizeof(next_relay_manager_t) );
    if ( !manager )
        return NULL;

    memset( manager, 0, sizeof(next_relay_manager_t) );

    manager->context = context;

    next_relay_manager_initialize_sentinels( manager );

    next_relay_manager_reset( manager );

    next_relay_manager_verify_sentinels( manager );

    return manager;
}

inline void next_relay_manager_reset( next_relay_manager_t * manager )
{
    next_relay_manager_verify_sentinels( manager );

    manager->num_relays = 0;

    memset( manager->relay_ids, 0, sizeof(manager->relay_ids) );
    memset( manager->relay_last_ping_time, 0, sizeof(manager->relay_last_ping_time) );
    memset( manager->relay_addresses, 0, sizeof(manager->relay_addresses) );
    memset( manager->relay_ping_tokens, 0, sizeof(manager->relay_ping_tokens) );
    manager->relay_ping_expire_timestamp = 0;

    for ( int i = 0; i < NEXT_MAX_NEAR_RELAYS; ++i )
    {
        next_ping_history_clear( &manager->relay_ping_history[i] );
    }
}

inline void next_relay_manager_update( next_relay_manager_t * manager, int num_relays, const uint64_t * relay_ids, const next_address_t * relay_addresses, const uint8_t * relay_ping_tokens, uint64_t relay_ping_expire_timestamp )
{
    next_relay_manager_verify_sentinels( manager );

    next_assert( num_relays >= 0 );
    next_assert( num_relays <= NEXT_MAX_NEAR_RELAYS );
    next_assert( relay_ids );
    next_assert( relay_addresses );

    // reset relay manager

    next_relay_manager_reset( manager );

    // copy across all relay data

    manager->num_relays = num_relays;

    for ( int i = 0; i < num_relays; ++i )
    {
        manager->relay_ids[i] = relay_ids[i];
        manager->relay_addresses[i] = relay_addresses[i];
    }

    memcpy( manager->relay_ping_tokens, relay_ping_tokens, num_relays * NEXT_PING_TOKEN_BYTES );

    manager->relay_ping_expire_timestamp = relay_ping_expire_timestamp;

    // make sure all ping times are evenly distributed to avoid clusters of ping packets

    double current_time = next_platform_time();

    const double ping_time = 1.0 / NEXT_NEAR_RELAY_PINGS_PER_SECOND;

    for ( int i = 0; i < manager->num_relays; ++i )
    {
        manager->relay_last_ping_time[i] = current_time - ping_time + i * ping_time / manager->num_relays;
    }

    next_relay_manager_verify_sentinels( manager );
}

inline void next_relay_manager_send_pings( next_relay_manager_t * manager, next_platform_socket_t * socket, uint64_t session_id, const uint8_t * magic, const next_address_t * client_external_address )
{
    next_relay_manager_verify_sentinels( manager );

    next_assert( socket );

    uint8_t packet_data[NEXT_MAX_PACKET_BYTES];

    double current_time = next_platform_time();

    for ( int i = 0; i < manager->num_relays; ++i )
    {
        const double ping_time = 1.0 / NEXT_NEAR_RELAY_PINGS_PER_SECOND;

        if ( manager->relay_last_ping_time[i] + ping_time <= current_time )
        {
            uint64_t ping_sequence = next_ping_history_ping_sent( &manager->relay_ping_history[i], next_platform_time() );

            const uint8_t * ping_token = manager->relay_ping_tokens + i * NEXT_PING_TOKEN_BYTES;

            uint8_t from_address_data[32];
            uint8_t to_address_data[32];
            int from_address_bytes;
            int to_address_bytes;

            next_address_data( client_external_address, from_address_data, &from_address_bytes );
            next_address_data( &manager->relay_addresses[i], to_address_data, &to_address_bytes );

            int packet_bytes = next_write_relay_ping_packet( packet_data, ping_token, ping_sequence, session_id, manager->relay_ping_expire_timestamp, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes );

            next_assert( packet_bytes > 0 );

            next_assert( next_basic_packet_filter( packet_data, packet_bytes ) );
            next_assert( next_advanced_packet_filter( packet_data, magic, from_address_data, from_address_bytes, to_address_data, to_address_bytes, packet_bytes ) );

#if NEXT_SPIKE_TRACKING
            double start_time = next_platform_time();
#endif // #if NEXT_SPIKE_TRACKING

            next_platform_socket_send_packet( socket, &manager->relay_addresses[i], packet_data, packet_bytes );

#if NEXT_SPIKE_TRACKING
            double finish_time = next_platform_time();
            if ( finish_time - start_time > 0.001 )
            {
                next_printf( NEXT_LOG_LEVEL_WARN, "next_platform_socket_send_packet spiked %.2f milliseconds at %s:%d", ( finish_time - start_time ) * 1000.0, __FILE__, __LINE__ );
            }
#endif // #if NEXT_SPIKE_TRACKING

            manager->relay_last_ping_time[i] = current_time;
        }
    }
}

inline void next_relay_manager_process_pong( next_relay_manager_t * manager, const next_address_t * from, uint64_t sequence )
{
    next_relay_manager_verify_sentinels( manager );

    next_assert( from );

    for ( int i = 0; i < manager->num_relays; ++i )
    {
        if ( next_address_equal( from, &manager->relay_addresses[i] ) )
        {
            next_ping_history_pong_received( &manager->relay_ping_history[i], sequence, next_platform_time() );
            return;
        }
    }
}

inline void next_relay_manager_get_stats( next_relay_manager_t * manager, next_relay_stats_t * stats )
{
    next_relay_manager_verify_sentinels( manager );

    next_assert( stats );

    double current_time = next_platform_time();

    next_relay_stats_initialize_sentinels( stats );

    stats->num_relays = manager->num_relays;
    stats->has_pings = stats->num_relays > 0;

    for ( int i = 0; i < stats->num_relays; ++i )
    {
        next_route_stats_t route_stats;

        next_route_stats_from_ping_history( &manager->relay_ping_history[i], current_time - NEXT_CLIENT_STATS_WINDOW, current_time, &route_stats );

        stats->relay_ids[i] = manager->relay_ids[i];
        stats->relay_rtt[i] = route_stats.rtt;
        stats->relay_jitter[i] = route_stats.jitter;
        stats->relay_packet_loss[i] = route_stats.packet_loss;
    }

    next_relay_stats_verify_sentinels( stats );
}

inline void next_relay_manager_destroy( next_relay_manager_t * manager )
{
    next_relay_manager_verify_sentinels( manager );

    next_free( manager->context, manager );
}

#endif // #ifndef NEXT_H

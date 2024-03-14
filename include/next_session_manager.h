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

#ifndef NEXT_SESSION_MANAGER_H
#define NEXT_SESSION_MANAGER_H

#include "next.h"
#include "next_packets.h"
#include "next_memory_checks.h"
#include "next_replay_protection.h"
#include "next_packet_loss_tracker.h"
#include "next_out_of_order_tracker.h"
#include "next_jitter_tracker.h"
#include "next_platform.h"

struct next_session_entry_t
{
    NEXT_DECLARE_SENTINEL(0)

    next_address_t address;
    uint64_t session_id;
    uint8_t most_recent_session_version;
    uint64_t special_send_sequence;
    uint64_t internal_send_sequence;
    uint64_t stats_sequence;
    uint64_t user_hash;
    uint64_t previous_session_events;
    uint64_t current_session_events;
    uint8_t client_open_session_sequence;

    NEXT_DECLARE_SENTINEL(1)

    bool stats_reported;
    bool stats_multipath;
    bool stats_fallback_to_direct;
    bool stats_client_bandwidth_over_limit;
    bool stats_server_bandwidth_over_limit;
    int stats_platform_id;
    int stats_connection_type;
    float stats_direct_kbps_up;
    float stats_direct_kbps_down;
    float stats_next_kbps_up;
    float stats_next_kbps_down;
    float stats_direct_rtt;
    float stats_direct_jitter;
    float stats_direct_packet_loss;
    float stats_direct_max_packet_loss_seen;
    bool stats_next;
    float stats_next_rtt;
    float stats_next_jitter;
    float stats_next_packet_loss;

    NEXT_DECLARE_SENTINEL(2)

    bool stats_has_client_relay_pings;
    bool stats_client_relay_pings_have_changed;
    uint64_t stats_last_client_relay_request_id;
    uint64_t stats_last_server_relay_request_id;
    int stats_num_client_relays;
    uint64_t stats_client_relay_ids[NEXT_MAX_CLIENT_RELAYS];
    uint8_t stats_client_relay_rtt[NEXT_MAX_CLIENT_RELAYS];
    uint8_t stats_client_relay_jitter[NEXT_MAX_CLIENT_RELAYS];
    float stats_client_relay_packet_loss[NEXT_MAX_CLIENT_RELAYS];

    NEXT_DECLARE_SENTINEL(3)

    uint64_t stats_packets_sent_client_to_server;
    uint64_t stats_packets_sent_server_to_client;
    uint64_t stats_packets_lost_client_to_server;
    uint64_t stats_packets_lost_server_to_client;
    uint64_t stats_packets_out_of_order_client_to_server;
    uint64_t stats_packets_out_of_order_server_to_client;

    float stats_jitter_client_to_server;
    float stats_jitter_server_to_client;

    double next_tracker_update_time;
    double next_session_update_time;
    double next_session_resend_time;
    double last_client_stats_update;
    double last_upgraded_packet_receive_time;

    NEXT_DECLARE_SENTINEL(4)

    uint64_t update_sequence;
    bool update_dirty;
    bool waiting_for_update_response;
    bool multipath;
    double update_last_send_time;
    uint8_t update_type;
    int update_num_tokens;
    bool session_update_timed_out;

    NEXT_DECLARE_SENTINEL(5)

    uint8_t update_tokens[NEXT_MAX_TOKENS*NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES];

    NEXT_DECLARE_SENTINEL(6)

    NextBackendSessionUpdateRequestPacket session_update_request_packet;

    NEXT_DECLARE_SENTINEL(7)

    bool has_pending_route;
    uint8_t pending_route_session_version;
    uint64_t pending_route_expire_timestamp;
    double pending_route_expire_time;
    int pending_route_kbps_up;
    int pending_route_kbps_down;
    next_address_t pending_route_send_address;

    NEXT_DECLARE_SENTINEL(8)

    uint8_t pending_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(9)

    bool has_current_route;
    uint8_t current_route_session_version;
    uint64_t current_route_expire_timestamp;
    double current_route_expire_time;
    int current_route_kbps_up;
    int current_route_kbps_down;
    next_address_t current_route_send_address;

    NEXT_DECLARE_SENTINEL(10)

    uint8_t current_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(11)

    bool has_previous_route;
    next_address_t previous_route_send_address;

    NEXT_DECLARE_SENTINEL(12)

    uint8_t previous_route_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(13)

    uint8_t ephemeral_private_key[NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    uint8_t send_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    uint8_t receive_key[NEXT_CRYPTO_KX_SESSIONKEYBYTES];
    uint8_t client_route_public_key[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];

    NEXT_DECLARE_SENTINEL(14)

    uint8_t upgrade_token[NEXT_UPGRADE_TOKEN_BYTES];

    NEXT_DECLARE_SENTINEL(15)

    next_replay_protection_t payload_replay_protection;
    next_replay_protection_t special_replay_protection;
    next_replay_protection_t internal_replay_protection;

    NEXT_DECLARE_SENTINEL(16)

    next_packet_loss_tracker_t packet_loss_tracker;
    next_out_of_order_tracker_t out_of_order_tracker;
    next_jitter_tracker_t jitter_tracker;

    NEXT_DECLARE_SENTINEL(17)

    bool mutex_multipath;
    int mutex_envelope_kbps_up;
    int mutex_envelope_kbps_down;
    uint64_t mutex_payload_send_sequence;
    uint64_t mutex_session_id;
    uint8_t mutex_session_version;
    bool mutex_send_over_network_next;
    next_address_t mutex_send_address;

    NEXT_DECLARE_SENTINEL(18)

    uint8_t mutex_private_key[NEXT_CRYPTO_BOX_SECRETKEYBYTES];

    NEXT_DECLARE_SENTINEL(19)

    int session_data_bytes;
    uint8_t session_data[NEXT_MAX_SESSION_DATA_BYTES];
    uint8_t session_data_signature[NEXT_CRYPTO_SIGN_BYTES];

    NEXT_DECLARE_SENTINEL(20)

    bool client_ping_timed_out;
    double last_client_direct_ping;
    double last_client_next_ping;

    NEXT_DECLARE_SENTINEL(21)

    uint32_t session_flush_update_sequence;
    bool session_update_flush;
    bool session_update_flush_finished;

    NEXT_DECLARE_SENTINEL(22)

    bool requesting_client_relays;
    double next_client_relay_request_time;
    double next_client_relay_request_packet_send_time;
    double client_relay_request_timeout_time;
    NextBackendClientRelayRequestPacket client_relay_request_packet;
    NextBackendClientRelayResponsePacket client_relay_response_packet;

    NEXT_DECLARE_SENTINEL(23)

    bool sending_client_relay_update_down_to_client;
    double next_client_relay_update_packet_send_time;
    double client_relay_update_timeout_time;
    NextClientRelayUpdatePacket client_relay_update_packet;

    NEXT_DECLARE_SENTINEL(24)
};

inline void next_session_entry_initialize_sentinels( next_session_entry_t * entry )
{
    (void) entry;
    next_assert( entry );
    NEXT_INITIALIZE_SENTINEL( entry, 0 )
    NEXT_INITIALIZE_SENTINEL( entry, 1 )
    NEXT_INITIALIZE_SENTINEL( entry, 2 )
    NEXT_INITIALIZE_SENTINEL( entry, 3 )
    NEXT_INITIALIZE_SENTINEL( entry, 4 )
    NEXT_INITIALIZE_SENTINEL( entry, 5 )
    NEXT_INITIALIZE_SENTINEL( entry, 6 )
    NEXT_INITIALIZE_SENTINEL( entry, 7 )
    NEXT_INITIALIZE_SENTINEL( entry, 8 )
    NEXT_INITIALIZE_SENTINEL( entry, 9 )
    NEXT_INITIALIZE_SENTINEL( entry, 10 )
    NEXT_INITIALIZE_SENTINEL( entry, 11 )
    NEXT_INITIALIZE_SENTINEL( entry, 12 )
    NEXT_INITIALIZE_SENTINEL( entry, 13 )
    NEXT_INITIALIZE_SENTINEL( entry, 14 )
    NEXT_INITIALIZE_SENTINEL( entry, 15 )
    NEXT_INITIALIZE_SENTINEL( entry, 16 )
    NEXT_INITIALIZE_SENTINEL( entry, 17 )
    NEXT_INITIALIZE_SENTINEL( entry, 18 )
    NEXT_INITIALIZE_SENTINEL( entry, 19 )
    NEXT_INITIALIZE_SENTINEL( entry, 20 )
    NEXT_INITIALIZE_SENTINEL( entry, 21 )
    NEXT_INITIALIZE_SENTINEL( entry, 22 )
    NEXT_INITIALIZE_SENTINEL( entry, 23 )
    NEXT_INITIALIZE_SENTINEL( entry, 24 )
    next_replay_protection_initialize_sentinels( &entry->payload_replay_protection );
    next_replay_protection_initialize_sentinels( &entry->special_replay_protection );
    next_replay_protection_initialize_sentinels( &entry->internal_replay_protection );
    next_packet_loss_tracker_initialize_sentinels( &entry->packet_loss_tracker );
    next_out_of_order_tracker_initialize_sentinels( &entry->out_of_order_tracker );
    next_jitter_tracker_initialize_sentinels( &entry->jitter_tracker );
}

inline void next_session_entry_verify_sentinels( next_session_entry_t * entry )
{
    (void) entry;
    next_assert( entry );
    NEXT_VERIFY_SENTINEL( entry, 0 )
    NEXT_VERIFY_SENTINEL( entry, 1 )
    NEXT_VERIFY_SENTINEL( entry, 2 )
    NEXT_VERIFY_SENTINEL( entry, 3 )
    NEXT_VERIFY_SENTINEL( entry, 4 )
    NEXT_VERIFY_SENTINEL( entry, 5 )
    NEXT_VERIFY_SENTINEL( entry, 6 )
    NEXT_VERIFY_SENTINEL( entry, 7 )
    NEXT_VERIFY_SENTINEL( entry, 8 )
    NEXT_VERIFY_SENTINEL( entry, 9 )
    NEXT_VERIFY_SENTINEL( entry, 10 )
    NEXT_VERIFY_SENTINEL( entry, 11 )
    NEXT_VERIFY_SENTINEL( entry, 12 )
    NEXT_VERIFY_SENTINEL( entry, 13 )
    NEXT_VERIFY_SENTINEL( entry, 14 )
    NEXT_VERIFY_SENTINEL( entry, 15 )
    NEXT_VERIFY_SENTINEL( entry, 16 )
    NEXT_VERIFY_SENTINEL( entry, 17 )
    NEXT_VERIFY_SENTINEL( entry, 18 )
    NEXT_VERIFY_SENTINEL( entry, 19 )
    NEXT_VERIFY_SENTINEL( entry, 20 )
    NEXT_VERIFY_SENTINEL( entry, 21 )
    NEXT_VERIFY_SENTINEL( entry, 22 )
    NEXT_VERIFY_SENTINEL( entry, 23 )
    NEXT_VERIFY_SENTINEL( entry, 24 )
    next_replay_protection_verify_sentinels( &entry->payload_replay_protection );
    next_replay_protection_verify_sentinels( &entry->special_replay_protection );
    next_replay_protection_verify_sentinels( &entry->internal_replay_protection );
    next_packet_loss_tracker_verify_sentinels( &entry->packet_loss_tracker );
    next_out_of_order_tracker_verify_sentinels( &entry->out_of_order_tracker );
    next_jitter_tracker_verify_sentinels( &entry->jitter_tracker );
}

struct next_session_manager_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    int size;
    int max_entry_index;
    uint64_t * session_ids;
    next_address_t * addresses;
    next_session_entry_t * entries;

    NEXT_DECLARE_SENTINEL(1)
};

inline void next_session_manager_initialize_sentinels( next_session_manager_t * session_manager )
{
    (void) session_manager;
    next_assert( session_manager );
    NEXT_INITIALIZE_SENTINEL( session_manager, 0 )
    NEXT_INITIALIZE_SENTINEL( session_manager, 1 )
}

inline void next_session_manager_verify_sentinels( next_session_manager_t * session_manager )
{
    (void) session_manager;
#if NEXT_ENABLE_MEMORY_CHECKS
    next_assert( session_manager );
    NEXT_VERIFY_SENTINEL( session_manager, 0 )
    NEXT_VERIFY_SENTINEL( session_manager, 1 )
    const int size = session_manager->size;
    for ( int i = 0; i < size; ++i )
    {
        if ( session_manager->session_ids[i] != 0 )
        {
            next_session_entry_verify_sentinels( &session_manager->entries[i] );
        }
    }
#endif // #if NEXT_ENABLE_MEMORY_CHECKS
}

void next_session_manager_destroy( next_session_manager_t * session_manager );

inline next_session_manager_t * next_session_manager_create( void * context, int initial_size )
{
    next_session_manager_t * session_manager = (next_session_manager_t*) next_malloc( context, sizeof(next_session_manager_t) );

    next_assert( session_manager );
    if ( !session_manager )
        return NULL;

    memset( session_manager, 0, sizeof(next_session_manager_t) );

    next_session_manager_initialize_sentinels( session_manager );

    session_manager->context = context;
    session_manager->size = initial_size;
    session_manager->session_ids = (uint64_t*) next_malloc( context, size_t(initial_size) * 8 );
    session_manager->addresses = (next_address_t*) next_malloc( context, size_t(initial_size) * sizeof(next_address_t) );
    session_manager->entries = (next_session_entry_t*) next_malloc( context, size_t(initial_size) * sizeof(next_session_entry_t) );

    next_assert( session_manager->session_ids );
    next_assert( session_manager->addresses );
    next_assert( session_manager->entries );

    if ( session_manager->session_ids == NULL || session_manager->addresses == NULL || session_manager->entries == NULL )
    {
        next_session_manager_destroy( session_manager );
        return NULL;
    }

    memset( session_manager->session_ids, 0, size_t(initial_size) * 8 );
    memset( session_manager->addresses, 0, size_t(initial_size) * sizeof(next_address_t) );
    memset( session_manager->entries, 0, size_t(initial_size) * sizeof(next_session_entry_t) );

    next_session_manager_verify_sentinels( session_manager );

    return session_manager;
}

inline void next_session_manager_destroy( next_session_manager_t * session_manager )
{
    next_session_manager_verify_sentinels( session_manager );

    next_free( session_manager->context, session_manager->session_ids );
    next_free( session_manager->context, session_manager->addresses );
    next_free( session_manager->context, session_manager->entries );

    next_clear_and_free( session_manager->context, session_manager, sizeof(next_session_manager_t) );
}

inline bool next_session_manager_expand( next_session_manager_t * session_manager )
{
    next_assert( session_manager );

    next_session_manager_verify_sentinels( session_manager );

    int new_size = session_manager->size * 2;

    uint64_t * new_session_ids = (uint64_t*) next_malloc( session_manager->context, size_t(new_size) * 8 );
    next_address_t * new_addresses = (next_address_t*) next_malloc( session_manager->context, size_t(new_size) * sizeof(next_address_t) );
    next_session_entry_t * new_entries = (next_session_entry_t*) next_malloc( session_manager->context, size_t(new_size) * sizeof(next_session_entry_t) );

    next_assert( new_session_ids );
    next_assert( new_addresses );
    next_assert( new_entries );

    if ( new_session_ids == NULL || new_addresses == NULL || new_entries == NULL )
    {
        next_free( session_manager->context, new_session_ids );
        next_free( session_manager->context, new_addresses );
        next_free( session_manager->context, new_entries );
        return false;
    }

    memset( new_session_ids, 0, size_t(new_size) * 8 );
    memset( new_addresses, 0, size_t(new_size) * sizeof(next_address_t) );
    memset( new_entries, 0, size_t(new_size) * sizeof(next_session_entry_t) );

    int index = 0;
    const int current_size = session_manager->size;
    for ( int i = 0; i < current_size; ++i )
    {
        if ( session_manager->session_ids[i] != 0 )
        {
            memcpy( &new_session_ids[index], &session_manager->session_ids[i], 8 );
            memcpy( &new_addresses[index], &session_manager->addresses[i], sizeof(next_address_t) );
            memcpy( &new_entries[index], &session_manager->entries[i], sizeof(next_session_entry_t) );
            index++;
        }
    }

    next_free( session_manager->context, session_manager->session_ids );
    next_free( session_manager->context, session_manager->addresses );
    next_free( session_manager->context, session_manager->entries );

    session_manager->session_ids = new_session_ids;
    session_manager->addresses = new_addresses;
    session_manager->entries = new_entries;
    session_manager->size = new_size;
    session_manager->max_entry_index = index - 1;

    return true;
}

inline void next_clear_session_entry( next_session_entry_t * entry, const next_address_t * address, uint64_t session_id )
{
    memset( entry, 0, sizeof(next_session_entry_t) );

    next_session_entry_initialize_sentinels( entry );

    entry->address = *address;
    entry->session_id = session_id;

    next_replay_protection_reset( &entry->payload_replay_protection );
    next_replay_protection_reset( &entry->special_replay_protection );
    next_replay_protection_reset( &entry->internal_replay_protection );

    next_packet_loss_tracker_reset( &entry->packet_loss_tracker );
    next_out_of_order_tracker_reset( &entry->out_of_order_tracker );
    next_jitter_tracker_reset( &entry->jitter_tracker );

    next_session_entry_verify_sentinels( entry );

    entry->special_send_sequence = 1;
    entry->internal_send_sequence = 1;

    const double current_time = next_platform_time();

    entry->last_client_direct_ping = current_time;
    entry->last_client_next_ping = current_time;
}

inline next_session_entry_t * next_session_manager_add( next_session_manager_t * session_manager, const next_address_t * address, uint64_t session_id, const uint8_t * ephemeral_private_key, const uint8_t * upgrade_token )
{
    next_session_manager_verify_sentinels( session_manager );

    next_assert( session_id != 0 );
    next_assert( address );
    next_assert( address->type != NEXT_ADDRESS_NONE );

    // first scan existing entries and see if we can insert there

    const int size = session_manager->size;

    for ( int i = 0; i < size; ++i )
    {
        if ( session_manager->session_ids[i] == 0 )
        {
            session_manager->session_ids[i] = session_id;
            session_manager->addresses[i] = *address;
            next_session_entry_t * entry = &session_manager->entries[i];
            next_clear_session_entry( entry, address, session_id );
            memcpy( entry->ephemeral_private_key, ephemeral_private_key, NEXT_CRYPTO_SECRETBOX_KEYBYTES );
            memcpy( entry->upgrade_token, upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
            if ( i > session_manager->max_entry_index )
            {
                session_manager->max_entry_index = i;
            }
            return entry;
        }
    }

    // ok, we need to grow, expand and add at the end (expand compacts existing entries)

    if ( !next_session_manager_expand( session_manager ) )
        return NULL;

    const int i = ++session_manager->max_entry_index;

    session_manager->session_ids[i] = session_id;
    session_manager->addresses[i] = *address;
    next_session_entry_t * entry = &session_manager->entries[i];
    next_clear_session_entry( entry, address, session_id );
    memcpy( entry->ephemeral_private_key, ephemeral_private_key, NEXT_CRYPTO_SECRETBOX_KEYBYTES );
    memcpy( entry->upgrade_token, upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );

    next_session_manager_verify_sentinels( session_manager );

    return entry;
}

inline void next_session_manager_remove_at_index( next_session_manager_t * session_manager, int index )
{
    next_session_manager_verify_sentinels( session_manager );

    next_assert( index >= 0 );
    next_assert( index <= session_manager->max_entry_index );

    const int max_index = session_manager->max_entry_index;
    session_manager->session_ids[index] = 0;
    session_manager->addresses[index].type = NEXT_ADDRESS_NONE;
    if ( index == max_index )
    {
        while ( index > 0 && session_manager->session_ids[index] == 0 )
        {
            index--;
        }
        session_manager->max_entry_index = index;
    }

    next_session_manager_verify_sentinels( session_manager );
}

inline void next_session_manager_remove_by_address( next_session_manager_t * session_manager, const next_address_t * address )
{
    next_session_manager_verify_sentinels( session_manager );

    next_assert( address );

    const int max_index = session_manager->max_entry_index;
    for ( int i = 0; i <= max_index; ++i )
    {
        if ( next_address_equal( address, &session_manager->addresses[i] ) == 1 )
        {
            next_session_manager_remove_at_index( session_manager, i );
            return;
        }
    }

    next_session_manager_verify_sentinels( session_manager );
}

inline next_session_entry_t * next_session_manager_find_by_address( next_session_manager_t * session_manager, const next_address_t * address )
{
    next_session_manager_verify_sentinels( session_manager );
    next_assert( address );
    const int max_index = session_manager->max_entry_index;
    for ( int i = 0; i <= max_index; ++i )
    {
        if ( next_address_equal( address, &session_manager->addresses[i] ) == 1 )
        {
            return &session_manager->entries[i];
        }
    }
    return NULL;
}

inline next_session_entry_t * next_session_manager_find_by_session_id( next_session_manager_t * session_manager, uint64_t session_id )
{
    next_session_manager_verify_sentinels( session_manager );
    next_assert( session_id );
    if ( session_id == 0 )
    {
        return NULL;
    }
    const int max_index = session_manager->max_entry_index;
    for ( int i = 0; i <= max_index; ++i )
    {
        if ( session_id == session_manager->session_ids[i] )
        {
            return &session_manager->entries[i];
        }
    }
    return NULL;
}

inline int next_session_manager_num_entries( next_session_manager_t * session_manager )
{
    next_session_manager_verify_sentinels( session_manager );
    int num_entries = 0;
    const int max_index = session_manager->max_entry_index;
    for ( int i = 0; i <= max_index; ++i )
    {
        if ( session_manager->session_ids[i] != 0 )
        {
            num_entries++;
        }
    }
    return num_entries;
}

#endif // #ifndef NEXT_SESSION_MANAGER_H

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

#ifndef NEXT_PENDING_SESSION_MANAGER_H
#define NEXT_PENDING_SESSION_MANAGER_H

#include "next.h"
#include "next_memory_checks.h"

struct next_pending_session_entry_t
{
    NEXT_DECLARE_SENTINEL(0)

    next_address_t address;
    uint64_t session_id;
    uint64_t user_hash;
    double upgrade_time;
    double last_packet_send_time;
    uint8_t private_key[NEXT_CRYPTO_SECRETBOX_KEYBYTES];
    uint8_t upgrade_token[NEXT_UPGRADE_TOKEN_BYTES];

    NEXT_DECLARE_SENTINEL(1)
};

inline void next_pending_session_entry_initialize_sentinels( next_pending_session_entry_t * entry )
{
    (void) entry;
    next_assert( entry );
    NEXT_INITIALIZE_SENTINEL( entry, 0 )
    NEXT_INITIALIZE_SENTINEL( entry, 1 )
}

inline void next_pending_session_entry_verify_sentinels( next_pending_session_entry_t * entry )
{
    (void) entry;
    next_assert( entry );
    NEXT_VERIFY_SENTINEL( entry, 0 )
    NEXT_VERIFY_SENTINEL( entry, 1 )
}

struct next_pending_session_manager_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    int size;
    int max_entry_index;
    next_address_t * addresses;
    next_pending_session_entry_t * entries;

    NEXT_DECLARE_SENTINEL(1)
};

inline void next_pending_session_manager_initialize_sentinels( next_pending_session_manager_t * session_manager )
{
    (void) session_manager;
    next_assert( session_manager );
    NEXT_INITIALIZE_SENTINEL( session_manager, 0 )
    NEXT_INITIALIZE_SENTINEL( session_manager, 1 )
}

inline void next_pending_session_manager_verify_sentinels( next_pending_session_manager_t * session_manager )
{
    (void) session_manager;
#if NEXT_ENABLE_MEMORY_CHECKS
    next_assert( session_manager );
    NEXT_VERIFY_SENTINEL( session_manager, 0 )
    NEXT_VERIFY_SENTINEL( session_manager, 1 )
    const int size = session_manager->size;
    for ( int i = 0; i < size; ++i )
    {
        if ( session_manager->addresses[i].type != 0 )
        {
            next_pending_session_entry_verify_sentinels( &session_manager->entries[i] );
        }
    }
#endif // #if NEXT_ENABLE_MEMORY_CHECKS
}

void next_pending_session_manager_destroy( next_pending_session_manager_t * pending_session_manager );

inline next_pending_session_manager_t * next_pending_session_manager_create( void * context, int initial_size )
{
    next_pending_session_manager_t * pending_session_manager = (next_pending_session_manager_t*) next_malloc( context, sizeof(next_pending_session_manager_t) );

    next_assert( pending_session_manager );
    if ( !pending_session_manager )
        return NULL;

    memset( pending_session_manager, 0, sizeof(next_pending_session_manager_t) );

    next_pending_session_manager_initialize_sentinels( pending_session_manager );

    pending_session_manager->context = context;
    pending_session_manager->size = initial_size;
    pending_session_manager->addresses = (next_address_t*) next_malloc( context, initial_size * sizeof(next_address_t) );
    pending_session_manager->entries = (next_pending_session_entry_t*) next_malloc( context, initial_size * sizeof(next_pending_session_entry_t) );

    next_assert( pending_session_manager->addresses );
    next_assert( pending_session_manager->entries );

    if ( pending_session_manager->addresses == NULL || pending_session_manager->entries == NULL )
    {
        next_pending_session_manager_destroy( pending_session_manager );
        return NULL;
    }

    memset( pending_session_manager->addresses, 0, initial_size * sizeof(next_address_t) );
    memset( pending_session_manager->entries, 0, initial_size * sizeof(next_pending_session_entry_t) );

    for ( int i = 0; i < initial_size; i++ )
        next_pending_session_entry_initialize_sentinels( &pending_session_manager->entries[i] );

    next_pending_session_manager_verify_sentinels( pending_session_manager );

    return pending_session_manager;
}

inline void next_pending_session_manager_destroy( next_pending_session_manager_t * pending_session_manager )
{
    next_pending_session_manager_verify_sentinels( pending_session_manager );

    next_free( pending_session_manager->context, pending_session_manager->addresses );
    next_free( pending_session_manager->context, pending_session_manager->entries );

    next_clear_and_free( pending_session_manager->context, pending_session_manager, sizeof(next_pending_session_manager_t) );
}

inline bool next_pending_session_manager_expand( next_pending_session_manager_t * pending_session_manager )
{
    next_pending_session_manager_verify_sentinels( pending_session_manager );

    int new_size = pending_session_manager->size * 2;

    next_address_t * new_addresses = (next_address_t*) next_malloc( pending_session_manager->context, new_size * sizeof(next_address_t) );

    next_pending_session_entry_t * new_entries = (next_pending_session_entry_t*) next_malloc( pending_session_manager->context, new_size * sizeof(next_pending_session_entry_t) );

    next_assert( pending_session_manager->addresses );
    next_assert( pending_session_manager->entries );

    if ( pending_session_manager->addresses == NULL || pending_session_manager->entries == NULL )
    {
        next_free( pending_session_manager->context, new_addresses );
        next_free( pending_session_manager->context, new_entries );
        return false;
    }

    memset( new_addresses, 0, new_size * sizeof(next_address_t) );
    memset( new_entries, 0, new_size * sizeof(next_pending_session_entry_t) );

    for ( int i = 0; i < new_size; ++i )
        next_pending_session_entry_initialize_sentinels( &new_entries[i] );

    int index = 0;
    const int current_size = pending_session_manager->size;
    for ( int i = 0; i < current_size; ++i )
    {
        if ( pending_session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            memcpy( &new_addresses[index], &pending_session_manager->addresses[i], sizeof(next_address_t) );
            memcpy( &new_entries[index], &pending_session_manager->entries[i], sizeof(next_pending_session_entry_t) );
            index++;
        }
    }

    next_free( pending_session_manager->context, pending_session_manager->addresses );
    next_free( pending_session_manager->context, pending_session_manager->entries );

    pending_session_manager->addresses = new_addresses;
    pending_session_manager->entries = new_entries;
    pending_session_manager->size = new_size;
    pending_session_manager->max_entry_index = index - 1;

    next_pending_session_manager_verify_sentinels( pending_session_manager );

    return true;
}

inline next_pending_session_entry_t * next_pending_session_manager_add( next_pending_session_manager_t * pending_session_manager, const next_address_t * address, uint64_t session_id, const uint8_t * private_key, const uint8_t * upgrade_token, double current_time )
{
    next_pending_session_manager_verify_sentinels( pending_session_manager );

    next_assert( session_id != 0 );
    next_assert( address );
    next_assert( address->type != NEXT_ADDRESS_NONE );

    // first scan existing entries and see if we can insert there

    const int size = pending_session_manager->size;

    for ( int i = 0; i < size; ++i )
    {
        if ( pending_session_manager->addresses[i].type == NEXT_ADDRESS_NONE )
        {
            pending_session_manager->addresses[i] = *address;
            next_pending_session_entry_t * entry = &pending_session_manager->entries[i];
            entry->address = *address;
            entry->session_id = session_id;
            entry->upgrade_time = current_time;
            entry->last_packet_send_time = -1000.0;
            memcpy( entry->private_key, private_key, NEXT_CRYPTO_SECRETBOX_KEYBYTES );
            memcpy( entry->upgrade_token, upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
            if ( i > pending_session_manager->max_entry_index )
            {
                pending_session_manager->max_entry_index = i;
            }
            return entry;
        }
    }

    // ok, we need to grow, expand and add at the end (expand compacts existing entries)

    if ( !next_pending_session_manager_expand( pending_session_manager ) )
        return NULL;

    const int i = ++pending_session_manager->max_entry_index;
    pending_session_manager->addresses[i] = *address;
    next_pending_session_entry_t * entry = &pending_session_manager->entries[i];
    entry->address = *address;
    entry->session_id = session_id;
    entry->upgrade_time = current_time;
    entry->last_packet_send_time = -1000.0;
    memcpy( entry->private_key, private_key, NEXT_CRYPTO_SECRETBOX_KEYBYTES );
    memcpy( entry->upgrade_token, upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );

    next_pending_session_manager_verify_sentinels( pending_session_manager );

    return entry;
}

inline void next_pending_session_manager_remove_at_index( next_pending_session_manager_t * pending_session_manager, int index )
{
    next_pending_session_manager_verify_sentinels( pending_session_manager );

    next_assert( index >= 0 );
    next_assert( index <= pending_session_manager->max_entry_index );

    const int max_index = pending_session_manager->max_entry_index;

    pending_session_manager->addresses[index].type = NEXT_ADDRESS_NONE;

    if ( index == max_index )
    {
        while ( index > 0 && pending_session_manager->addresses[index].type == NEXT_ADDRESS_NONE )
        {
            index--;
        }
        pending_session_manager->max_entry_index = index;
    }
}

inline void next_pending_session_manager_remove_by_address( next_pending_session_manager_t * pending_session_manager, const next_address_t * address )
{
    next_pending_session_manager_verify_sentinels( pending_session_manager );

    next_assert( address );

    const int max_index = pending_session_manager->max_entry_index;

    for ( int i = 0; i <= max_index; ++i )
    {
        if ( next_address_equal( address, &pending_session_manager->addresses[i] ) == 1 )
        {
            next_pending_session_manager_remove_at_index( pending_session_manager, i );
            return;
        }
    }
}

inline next_pending_session_entry_t * next_pending_session_manager_find( next_pending_session_manager_t * pending_session_manager, const next_address_t * address )
{
    next_pending_session_manager_verify_sentinels( pending_session_manager );

    next_assert( address );

    const int max_index = pending_session_manager->max_entry_index;

    for ( int i = 0; i <= max_index; ++i )
    {
        if ( next_address_equal( address, &pending_session_manager->addresses[i] ) == 1 )
        {
            return &pending_session_manager->entries[i];
        }
    }

    return NULL;
}

inline int next_pending_session_manager_num_entries( next_pending_session_manager_t * pending_session_manager )
{
    next_pending_session_manager_verify_sentinels( pending_session_manager );

    int num_entries = 0;

    const int max_index = pending_session_manager->max_entry_index;

    for ( int i = 0; i <= max_index; ++i )
    {
        if ( pending_session_manager->addresses[i].type != 0 )
        {
            num_entries++;
        }
    }

    return num_entries;
}

#endif // #ifndef NEXT_PENDING_SESSION_MANAGER_H

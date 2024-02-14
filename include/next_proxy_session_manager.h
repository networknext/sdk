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

#ifndef NEXT_PROXY_SESSION_MANAGER_H
#define NEXT_PROXY_SESSION_MANAGER_H

#include "next.h"
#include "next_memory_checks.h"
#include "next_bandwidth_limiter.h"

struct next_proxy_session_entry_t
{
    NEXT_DECLARE_SENTINEL(0)

    next_address_t address;
    uint64_t session_id;

    NEXT_DECLARE_SENTINEL(1)

    next_bandwidth_limiter_t send_bandwidth;

    NEXT_DECLARE_SENTINEL(2)
};

inline void next_proxy_session_entry_initialize_sentinels( next_proxy_session_entry_t * entry )
{
    (void) entry;
    next_assert( entry );
    NEXT_INITIALIZE_SENTINEL( entry, 0 )
    NEXT_INITIALIZE_SENTINEL( entry, 1 )
    NEXT_INITIALIZE_SENTINEL( entry, 2 )
}

inline void next_proxy_session_entry_verify_sentinels( next_proxy_session_entry_t * entry )
{
    (void) entry;
    next_assert( entry );
    NEXT_VERIFY_SENTINEL( entry, 0 )
    NEXT_VERIFY_SENTINEL( entry, 1 )
    NEXT_VERIFY_SENTINEL( entry, 2 )
}

struct next_proxy_session_manager_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    int size;
    int max_entry_index;
    next_address_t * addresses;
    next_proxy_session_entry_t * entries;

    NEXT_DECLARE_SENTINEL(1)
};

inline void next_proxy_session_manager_initialize_sentinels( next_proxy_session_manager_t * session_manager )
{
    (void) session_manager;
    next_assert( session_manager );
    NEXT_INITIALIZE_SENTINEL( session_manager, 0 )
    NEXT_INITIALIZE_SENTINEL( session_manager, 1 )
}

inline void next_proxy_session_manager_verify_sentinels( next_proxy_session_manager_t * session_manager )
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
            next_proxy_session_entry_verify_sentinels( &session_manager->entries[i] );
        }
    }
#endif // #if NEXT_ENABLE_MEMORY_CHECKS
}

void next_proxy_session_manager_destroy( next_proxy_session_manager_t * session_manager );

inline next_proxy_session_manager_t * next_proxy_session_manager_create( void * context, int initial_size )
{
    next_proxy_session_manager_t * session_manager = (next_proxy_session_manager_t*) next_malloc( context, sizeof(next_proxy_session_manager_t) );

    next_assert( session_manager );

    if ( !session_manager )
        return NULL;

    memset( session_manager, 0, sizeof(next_proxy_session_manager_t) );

    next_proxy_session_manager_initialize_sentinels( session_manager );

    session_manager->context = context;
    session_manager->size = initial_size;
    session_manager->addresses = (next_address_t*) next_malloc( context, initial_size * sizeof(next_address_t) );
    session_manager->entries = (next_proxy_session_entry_t*) next_malloc( context, initial_size * sizeof(next_proxy_session_entry_t) );

    next_assert( session_manager->addresses );
    next_assert( session_manager->entries );

    if ( session_manager->addresses == NULL || session_manager->entries == NULL )
    {
        next_proxy_session_manager_destroy( session_manager );
        return NULL;
    }

    memset( session_manager->addresses, 0, initial_size * sizeof(next_address_t) );
    memset( session_manager->entries, 0, initial_size * sizeof(next_proxy_session_entry_t) );

    for ( int i = 0; i < initial_size; ++i )
        next_proxy_session_entry_initialize_sentinels( &session_manager->entries[i] );

    next_proxy_session_manager_verify_sentinels( session_manager );

    return session_manager;
}

inline void next_proxy_session_manager_destroy( next_proxy_session_manager_t * session_manager )
{
    next_proxy_session_manager_verify_sentinels( session_manager );

    next_free( session_manager->context, session_manager->addresses );
    next_free( session_manager->context, session_manager->entries );

    next_clear_and_free( session_manager->context, session_manager, sizeof(next_proxy_session_manager_t) );
}

inline bool next_proxy_session_manager_expand( next_proxy_session_manager_t * session_manager )
{
    next_proxy_session_manager_verify_sentinels( session_manager );

    int new_size = session_manager->size * 2;
    next_address_t * new_addresses = (next_address_t*) next_malloc( session_manager->context, new_size * sizeof(next_address_t) );
    next_proxy_session_entry_t * new_entries = (next_proxy_session_entry_t*) next_malloc( session_manager->context, new_size * sizeof(next_proxy_session_entry_t) );

    next_assert( session_manager->addresses );
    next_assert( session_manager->entries );

    if ( session_manager->addresses == NULL || session_manager->entries == NULL )
    {
        next_free( session_manager->context, new_addresses );
        next_free( session_manager->context, new_entries );
        return false;
    }

    memset( new_addresses, 0, new_size * sizeof(next_address_t) );
    memset( new_entries, 0, new_size * sizeof(next_proxy_session_entry_t) );

    for ( int i = 0; i < new_size; ++i )
        next_proxy_session_entry_initialize_sentinels( &new_entries[i] );

    int index = 0;
    const int current_size = session_manager->size;
    for ( int i = 0; i < current_size; ++i )
    {
        if ( session_manager->addresses[i].type != NEXT_ADDRESS_NONE )
        {
            memcpy( &new_addresses[index], &session_manager->addresses[i], sizeof(next_address_t) );
            memcpy( &new_entries[index], &session_manager->entries[i], sizeof(next_proxy_session_entry_t) );
            index++;
        }
    }

    next_free( session_manager->context, session_manager->addresses );
    next_free( session_manager->context, session_manager->entries );

    session_manager->addresses = new_addresses;
    session_manager->entries = new_entries;
    session_manager->size = new_size;
    session_manager->max_entry_index = index - 1;

    next_proxy_session_manager_verify_sentinels( session_manager );

    return true;
}

inline next_proxy_session_entry_t * next_proxy_session_manager_add( next_proxy_session_manager_t * session_manager, const next_address_t * address, uint64_t session_id )
{
    next_proxy_session_manager_verify_sentinels( session_manager );

    next_assert( session_id != 0 );
    next_assert( address );
    next_assert( address->type != NEXT_ADDRESS_NONE );

    // first scan existing entries and see if we can insert there

    const int size = session_manager->size;

    for ( int i = 0; i < size; ++i )
    {
        if ( session_manager->addresses[i].type == NEXT_ADDRESS_NONE )
        {
            session_manager->addresses[i] = *address;
            next_proxy_session_entry_t * entry = &session_manager->entries[i];
            entry->address = *address;
            entry->session_id = session_id;
            next_bandwidth_limiter_reset( &entry->send_bandwidth );
            if ( i > session_manager->max_entry_index )
            {
                session_manager->max_entry_index = i;
            }
            return entry;
        }
    }

    // ok, we need to grow, expand and add at the end (expand compacts existing entries)

    if ( !next_proxy_session_manager_expand( session_manager ) )
        return NULL;

    const int i = ++session_manager->max_entry_index;
    session_manager->addresses[i] = *address;
    next_proxy_session_entry_t * entry = &session_manager->entries[i];
    entry->address = *address;
    entry->session_id = session_id;
    next_bandwidth_limiter_reset( &entry->send_bandwidth );

    next_proxy_session_manager_verify_sentinels( session_manager );

    return entry;
}

inline void next_proxy_session_manager_remove_at_index( next_proxy_session_manager_t * session_manager, int index )
{
    next_proxy_session_manager_verify_sentinels( session_manager );

    next_assert( index >= 0 );
    next_assert( index <= session_manager->max_entry_index );
    const int max_index = session_manager->max_entry_index;
    session_manager->addresses[index].type = NEXT_ADDRESS_NONE;
    if ( index == max_index )
    {
        while ( index > 0 && session_manager->addresses[index].type == NEXT_ADDRESS_NONE )
        {
            index--;
        }
        session_manager->max_entry_index = index;
    }

    next_proxy_session_manager_verify_sentinels( session_manager );
}

inline void next_proxy_session_manager_remove_by_address( next_proxy_session_manager_t * session_manager, const next_address_t * address )
{
    next_proxy_session_manager_verify_sentinels( session_manager );

    next_assert( address );

    const int max_index = session_manager->max_entry_index;
    for ( int i = 0; i <= max_index; ++i )
    {
        if ( next_address_equal( address, &session_manager->addresses[i] ) == 1 )
        {
            next_proxy_session_manager_remove_at_index( session_manager, i );
            next_proxy_session_manager_verify_sentinels( session_manager );
            return;
        }
    }
}

inline next_proxy_session_entry_t * next_proxy_session_manager_find( next_proxy_session_manager_t * session_manager, const next_address_t * address )
{
    next_proxy_session_manager_verify_sentinels( session_manager );

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

inline int next_proxy_session_manager_num_entries( next_proxy_session_manager_t * session_manager )
{
    next_proxy_session_manager_verify_sentinels( session_manager );

    int num_entries = 0;

    const int max_index = session_manager->max_entry_index;
    for ( int i = 0; i <= max_index; ++i )
    {
        if ( session_manager->addresses[i].type != 0 )
        {
            num_entries++;
        }
    }

    return num_entries;
}

#endif // #ifndef NEXT_PROXY_SESSION_MANAGER_H

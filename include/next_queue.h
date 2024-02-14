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

#ifndef NEXT_QUEUE_H
#define NEXT_QUEUE_H

#include "next.h"
#include "next_memory_checks.h"

struct next_queue_t
{
    NEXT_DECLARE_SENTINEL(0)

    void * context;
    int size;
    int num_entries;
    int start_index;
    void ** entries;

    NEXT_DECLARE_SENTINEL(1)
};

inline void next_queue_initialize_sentinels( next_queue_t * queue )
{
    (void) queue;
    next_assert( queue );
    NEXT_INITIALIZE_SENTINEL( queue, 0 )
    NEXT_INITIALIZE_SENTINEL( queue, 1 )
}

inline void next_queue_verify_sentinels( next_queue_t * queue )
{
    (void) queue;
    next_assert( queue );
    NEXT_VERIFY_SENTINEL( queue, 0 )
    NEXT_VERIFY_SENTINEL( queue, 1 )
}

inline void next_queue_destroy( next_queue_t * queue );

inline next_queue_t * next_queue_create( void * context, int size )
{
    next_queue_t * queue = (next_queue_t*) next_malloc( context, sizeof(next_queue_t) );
    next_assert( queue );
    if ( !queue )
        return NULL;

    next_queue_initialize_sentinels( queue );

    queue->context = context;
    queue->size = size;
    queue->num_entries = 0;
    queue->start_index = 0;
    queue->entries = (void**) next_malloc( context, size * sizeof(void*) );

    next_assert( queue->entries );

    if ( !queue->entries )
    {
        next_queue_destroy( queue );
        return NULL;
    }

    next_queue_verify_sentinels( queue );

    return queue;
}

inline void next_queue_clear( next_queue_t * queue )
{
    next_queue_verify_sentinels( queue );

    const int queue_size = queue->size;
    const int start_index = queue->start_index;

    for ( int i = 0; i < queue->num_entries; ++i )
    {
        const int index = (start_index + i ) % queue_size;
        next_free( queue->context, queue->entries[index] );
        queue->entries[index] = NULL;
    }

    queue->num_entries = 0;
    queue->start_index = 0;
}

inline void next_queue_destroy( next_queue_t * queue )
{
    next_queue_verify_sentinels( queue );

    next_queue_clear( queue );

    next_free( queue->context, queue->entries );

    next_clear_and_free( queue->context, queue, sizeof( queue ) );
}

inline int next_queue_push( next_queue_t * queue, void * entry )
{
    next_queue_verify_sentinels( queue );

    next_assert( entry );

    if ( queue->num_entries == queue->size )
    {
        next_free( queue->context, entry );
        return NEXT_ERROR;
    }

    int index = ( queue->start_index + queue->num_entries ) % queue->size;

    queue->entries[index] = entry;
    queue->num_entries++;

    return NEXT_OK;
}

inline void * next_queue_pop( next_queue_t * queue )
{
    next_queue_verify_sentinels( queue );

    if ( queue->num_entries == 0 )
        return NULL;

    void * entry = queue->entries[queue->start_index];

    queue->start_index = ( queue->start_index + 1 ) % queue->size;
    queue->num_entries--;

    return entry;
}

#endif // #ifndef NEXT_QUEUE_H

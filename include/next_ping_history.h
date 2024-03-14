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

#ifndef NEXT_PING_HISTORY_H
#define NEXT_PING_HISTORY_H

#include "next.h"

#include <math.h>
#include <float.h>

struct next_route_stats_t
{
    float rtt;                          // rtt (ms)
    float jitter;                       // jitter (ms)
    float packet_loss;                  // packet loss %
};

struct next_ping_history_entry_t
{
    uint64_t sequence;
    double time_ping_sent;
    double time_pong_received;
};

struct next_ping_history_t
{
    NEXT_DECLARE_SENTINEL(0)

    uint64_t sequence;

    NEXT_DECLARE_SENTINEL(1)

    next_ping_history_entry_t entries[NEXT_PING_HISTORY_ENTRY_COUNT];

    NEXT_DECLARE_SENTINEL(2)
};

inline void next_ping_history_initialize_sentinels( next_ping_history_t * history )
{
    (void) history;
    next_assert( history );
    NEXT_INITIALIZE_SENTINEL( history, 0 )
    NEXT_INITIALIZE_SENTINEL( history, 1 )
    NEXT_INITIALIZE_SENTINEL( history, 2 )
}

inline void next_ping_history_verify_sentinels( const next_ping_history_t * history )
{
    (void) history;
    next_assert( history );
    NEXT_VERIFY_SENTINEL( history, 0 )
    NEXT_VERIFY_SENTINEL( history, 1 )
    NEXT_VERIFY_SENTINEL( history, 2 )
}

inline void next_ping_history_clear( next_ping_history_t * history )
{
    next_assert( history );

    next_ping_history_initialize_sentinels( history );

    history->sequence = 0;

    for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; ++i )
    {
        history->entries[i].sequence = 0xFFFFFFFFFFFFFFFFULL;
        history->entries[i].time_ping_sent = -1.0;
        history->entries[i].time_pong_received = -1.0;
    }

    next_ping_history_verify_sentinels( history );
}

inline uint64_t next_ping_history_ping_sent( next_ping_history_t * history, double time )
{
    next_ping_history_verify_sentinels( history );

    const int index = history->sequence % NEXT_PING_HISTORY_ENTRY_COUNT;

    next_ping_history_entry_t * entry = &history->entries[index];

    entry->sequence = history->sequence;
    entry->time_ping_sent = time;
    entry->time_pong_received = -1.0;

    history->sequence++;

    return entry->sequence;
}

inline void next_ping_history_pong_received( next_ping_history_t * history, uint64_t sequence, double time )
{
    next_ping_history_verify_sentinels( history );

    const int index = sequence % NEXT_PING_HISTORY_ENTRY_COUNT;

    next_ping_history_entry_t * entry = &history->entries[index];

    if ( entry->sequence == sequence )
    {
        entry->time_pong_received = time;
    }
}

inline void next_route_stats_from_ping_history( const next_ping_history_t * history, double start, double end, next_route_stats_t * stats, double safety = NEXT_PING_SAFETY )
{
    next_ping_history_verify_sentinels( history );

    next_assert( stats );

    if ( start < safety )
    {
        start = safety;
    }

    stats->rtt = 0.0f;
    stats->jitter = 0.0f;
    stats->packet_loss = 100.0f;

    // IMPORTANT: Instead of searching across the whole range then considering any ping with a pong older than ping safety
    // (typically one second) to be lost, look for the time of the most recent ping that has received a pong, subtract ping
    // safety from this, and then look for packet loss only in this range. This avoids turning every ping that receives a
    // pong more than 1 second later as packet loss, which was behavior we saw with previous versions of this code.

    double most_recent_ping_that_received_pong_time = 0.0;

    for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; i++ )
    {
        const next_ping_history_entry_t * entry = &history->entries[i];

        if ( entry->time_ping_sent >= start && entry->time_ping_sent <= end && entry->time_pong_received >= entry->time_ping_sent )
        {
            if ( entry->time_pong_received > most_recent_ping_that_received_pong_time )
            {
                most_recent_ping_that_received_pong_time = entry->time_pong_received;
            }
        }
    }

    if ( most_recent_ping_that_received_pong_time > 0.0 )
    {
        end = most_recent_ping_that_received_pong_time - safety;
    }
    else
    {
        return;
    }

    // calculate ping stats

    double min_rtt = FLT_MAX;

    int num_pings_sent = 0;
    int num_pongs_received = 0;

    for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; i++ )
    {
        const next_ping_history_entry_t * entry = &history->entries[i];

        if ( entry->time_ping_sent >= start && entry->time_ping_sent <= end )
        {
            num_pings_sent++;

            if ( entry->time_pong_received >= entry->time_ping_sent )
            {
                double rtt = ( entry->time_pong_received - entry->time_ping_sent );

                if ( rtt < min_rtt )
                {
                    min_rtt = rtt;
                }

                num_pongs_received++;
            }
        }
    }

    if ( num_pings_sent > 0 && num_pongs_received > 0 )
    {
        next_assert( min_rtt >= 0.0 );

        stats->rtt = float( min_rtt ) * 1000.0f;

        stats->packet_loss = (float) ( 100.0 * ( 1.0 - ( double( num_pongs_received ) / double( num_pings_sent ) ) ) );

        int num_jitter_samples = 0;

        double stddev_rtt = 0.0;

        for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; i++ )
        {
            const next_ping_history_entry_t * entry = &history->entries[i];

            if ( entry->time_ping_sent >= start && entry->time_ping_sent <= end )
            {
                if ( entry->time_pong_received > entry->time_ping_sent )
                {
                    // pong received
                    double rtt = ( entry->time_pong_received - entry->time_ping_sent );
                    double error = rtt - min_rtt;
                    stddev_rtt += error * error;
                    num_jitter_samples++;
                }
            }
        }

        if ( num_jitter_samples > 0 )
        {
            stats->jitter = (float) sqrt( stddev_rtt / num_jitter_samples ) * 1000.0f;
        }
    }

    next_ping_history_verify_sentinels( history );
}

#endif // #ifndef NEXT_PING_HISTORY_H

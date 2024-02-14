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

#ifndef NEXT_PACKET_LOSS_TRACKER_H
#define NEXT_PACKET_LOSS_TRACKER_H

struct next_packet_loss_tracker_t
{
    NEXT_DECLARE_SENTINEL(0)

    uint64_t last_packet_processed;
    uint64_t most_recent_packet_received;

    NEXT_DECLARE_SENTINEL(1)

    uint64_t received_packets[NEXT_PACKET_LOSS_TRACKER_HISTORY];

    NEXT_DECLARE_SENTINEL(2)
};

inline void next_packet_loss_tracker_initialize_sentinels( next_packet_loss_tracker_t * tracker )
{
    (void) tracker;
    next_assert( tracker );
    NEXT_INITIALIZE_SENTINEL( tracker, 0 )
    NEXT_INITIALIZE_SENTINEL( tracker, 1 )
    NEXT_INITIALIZE_SENTINEL( tracker, 2 )
}

inline void next_packet_loss_tracker_verify_sentinels( next_packet_loss_tracker_t * tracker )
{
    (void) tracker;
    next_assert( tracker );
    NEXT_VERIFY_SENTINEL( tracker, 0 )
    NEXT_VERIFY_SENTINEL( tracker, 1 )
    NEXT_VERIFY_SENTINEL( tracker, 2 )
}

inline void next_packet_loss_tracker_reset( next_packet_loss_tracker_t * tracker )
{
    next_assert( tracker );

    next_packet_loss_tracker_initialize_sentinels( tracker );

    tracker->last_packet_processed = 0;
    tracker->most_recent_packet_received = 0;

    for ( int i = 0; i < NEXT_PACKET_LOSS_TRACKER_HISTORY; ++i )
    {
        tracker->received_packets[i] = 0xFFFFFFFFFFFFFFFFUL;
    }

    next_packet_loss_tracker_verify_sentinels( tracker );
}

inline void next_packet_loss_tracker_packet_received( next_packet_loss_tracker_t * tracker, uint64_t sequence )
{
    next_packet_loss_tracker_verify_sentinels( tracker );

    sequence++;

    const int index = int( sequence % NEXT_PACKET_LOSS_TRACKER_HISTORY );

    tracker->received_packets[index] = sequence;
    tracker->most_recent_packet_received = sequence;
}

inline int next_packet_loss_tracker_update( next_packet_loss_tracker_t * tracker )
{
    next_packet_loss_tracker_verify_sentinels( tracker );

    int lost_packets = 0;

    uint64_t start = tracker->last_packet_processed + 1;
    uint64_t finish = ( tracker->most_recent_packet_received > NEXT_PACKET_LOSS_TRACKER_SAFETY ) ? ( tracker->most_recent_packet_received - NEXT_PACKET_LOSS_TRACKER_SAFETY ) : 0;

    if ( finish > start && finish - start > NEXT_PACKET_LOSS_TRACKER_HISTORY )
    {
        tracker->last_packet_processed = tracker->most_recent_packet_received;
        return 0;
    }

    for ( uint64_t sequence = start; sequence <= finish; ++sequence )
    {
        const int index = int( sequence % NEXT_PACKET_LOSS_TRACKER_HISTORY );
        if ( tracker->received_packets[index] != sequence )
        {
            lost_packets++;
        }
    }

    tracker->last_packet_processed = finish;

    return lost_packets;
}

#endif // #ifndef NEXT_PACKET_LOSS_TRACKER_H

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

#ifndef NEXT_OUT_OF_ORDER_TRACKER_H
#define NEXT_OUT_OF_ORDER_TRACKER_H

struct next_out_of_order_tracker_t
{
    NEXT_DECLARE_SENTINEL(0)

    uint64_t last_packet_processed;
    uint64_t num_out_of_order_packets;

    NEXT_DECLARE_SENTINEL(1)
};

inline void next_out_of_order_tracker_initialize_sentinels( next_out_of_order_tracker_t * tracker )
{
    (void) tracker;
    next_assert( tracker );
    NEXT_INITIALIZE_SENTINEL( tracker, 0 )
    NEXT_INITIALIZE_SENTINEL( tracker, 1 )
}

inline void next_out_of_order_tracker_verify_sentinels( next_out_of_order_tracker_t * tracker )
{
    (void) tracker;
    next_assert( tracker );
    NEXT_VERIFY_SENTINEL( tracker, 0 )
    NEXT_VERIFY_SENTINEL( tracker, 1 )
}

inline void next_out_of_order_tracker_reset( next_out_of_order_tracker_t * tracker )
{
    next_assert( tracker );

    next_out_of_order_tracker_initialize_sentinels( tracker );

    tracker->last_packet_processed = 0;
    tracker->num_out_of_order_packets = 0;

    next_out_of_order_tracker_verify_sentinels( tracker );
}

inline void next_out_of_order_tracker_packet_received( next_out_of_order_tracker_t * tracker, uint64_t sequence )
{
    next_out_of_order_tracker_verify_sentinels( tracker );

    if ( sequence < tracker->last_packet_processed )
    {
        tracker->num_out_of_order_packets++;
        return;
    }

    tracker->last_packet_processed = sequence;
}

#endif // #ifndef NEXT_OUT_OF_ORDER_TRACKER_H

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

#ifndef NEXT_BANDWIDTH_LIMITER_H
#define NEXT_BANDWIDTH_LIMITER_H

#include <stdio.h>

inline int next_wire_packet_bits( int payload_bytes )
{
    return ( NEXT_IPV4_HEADER_BYTES + NEXT_UDP_HEADER_BYTES + 1 + 15 + NEXT_HEADER_BYTES + payload_bytes + 2 ) * 8;
}

struct next_bandwidth_limiter_t
{
    uint64_t bits_sent;
    double last_check_time;
    double average_kbps;
};

inline void next_bandwidth_limiter_reset( next_bandwidth_limiter_t * bandwidth_limiter )
{
    next_assert( bandwidth_limiter );
    bandwidth_limiter->last_check_time = -100.0;
    bandwidth_limiter->bits_sent = 0;
    bandwidth_limiter->average_kbps = 0.0;
}

inline void next_bandwidth_limiter_add_sample( next_bandwidth_limiter_t * bandwidth_limiter, double kbps )
{
    if ( bandwidth_limiter->average_kbps == 0.0 && kbps != 0.0 )
    {
        bandwidth_limiter->average_kbps = kbps;
        return;
    }

    if ( bandwidth_limiter->average_kbps != 0.0 && kbps == 0.0 )
    {
        bandwidth_limiter->average_kbps = 0.0;
        return;
    }

    const double delta = kbps - bandwidth_limiter->average_kbps;

    if ( delta < 0.000001f )
    {
        bandwidth_limiter->average_kbps = kbps;
        return;
    }

    bandwidth_limiter->average_kbps += delta * 0.1f;
}

inline bool next_bandwidth_limiter_add_packet( next_bandwidth_limiter_t * bandwidth_limiter, double current_time, uint32_t kbps_allowed, uint32_t packet_bits )
{
    next_assert( bandwidth_limiter );

    const bool invalid = bandwidth_limiter->last_check_time < 0.0;

    const bool new_period = ( current_time - bandwidth_limiter->last_check_time ) >= NEXT_BANDWIDTH_LIMITER_INTERVAL - 0.00001;

    if ( invalid || new_period )
    {
        if ( new_period )
        {
            const double kbps = bandwidth_limiter->bits_sent / ( current_time - bandwidth_limiter->last_check_time) / 1000.0;
            next_bandwidth_limiter_add_sample( bandwidth_limiter, kbps );
        }
        bandwidth_limiter->bits_sent = 0;
        bandwidth_limiter->last_check_time = current_time;
    }

    bandwidth_limiter->bits_sent += packet_bits;

    return bandwidth_limiter->bits_sent > uint64_t(kbps_allowed) * 1000 * NEXT_BANDWIDTH_LIMITER_INTERVAL;
}

inline double next_bandwidth_limiter_usage_kbps( next_bandwidth_limiter_t * bandwidth_limiter )
{
    next_assert( bandwidth_limiter );
    return bandwidth_limiter->average_kbps;
}

#endif // #ifndef NEXT_BANDWIDTH_LIMITER_H

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

#ifndef NEXT_REPLAY_PROTECTION_H
#define NEXT_REPLAY_PROTECTION_H

#include "next.h"
#include "next_config.h"
#include "next_memory_checks.h"

struct next_replay_protection_t
{
    NEXT_DECLARE_SENTINEL(0)

    uint64_t most_recent_sequence;
    uint64_t received_packet[NEXT_REPLAY_PROTECTION_BUFFER_SIZE];

    NEXT_DECLARE_SENTINEL(1)
};

inline void next_replay_protection_initialize_sentinels( next_replay_protection_t * replay_protection )
{
    (void) replay_protection;
    next_assert( replay_protection );
    NEXT_INITIALIZE_SENTINEL( replay_protection, 0 )
    NEXT_INITIALIZE_SENTINEL( replay_protection, 1 )
}

inline void next_replay_protection_verify_sentinels( next_replay_protection_t * replay_protection )
{
    (void) replay_protection;
    next_assert( replay_protection );
    NEXT_VERIFY_SENTINEL( replay_protection, 0 )
    NEXT_VERIFY_SENTINEL( replay_protection, 1 )
}

inline void next_replay_protection_reset( next_replay_protection_t * replay_protection )
{
    next_replay_protection_initialize_sentinels( replay_protection );

    replay_protection->most_recent_sequence = 0;

    memset( replay_protection->received_packet, 0xFF, sizeof( replay_protection->received_packet ) );

    next_replay_protection_verify_sentinels( replay_protection );
}

inline bool next_replay_protection_already_received( next_replay_protection_t * replay_protection, uint64_t sequence )
{
    next_replay_protection_verify_sentinels( replay_protection );

    if ( sequence + NEXT_REPLAY_PROTECTION_BUFFER_SIZE <= replay_protection->most_recent_sequence )
    {
        return true;
    }

    int index = (int) ( sequence % NEXT_REPLAY_PROTECTION_BUFFER_SIZE );

    if ( replay_protection->received_packet[index] == 0xFFFFFFFFFFFFFFFFLL )
    {
        replay_protection->received_packet[index] = sequence;
        return false;
    }

    if ( replay_protection->received_packet[index] >= sequence )
    {
        return true;
    }

    return false;
}

inline void next_replay_protection_advance_sequence( next_replay_protection_t * replay_protection, uint64_t sequence )
{
    next_replay_protection_verify_sentinels( replay_protection );

    if ( sequence > replay_protection->most_recent_sequence )
    {
        replay_protection->most_recent_sequence = sequence;
    }

    int index = (int) ( sequence % NEXT_REPLAY_PROTECTION_BUFFER_SIZE );

    replay_protection->received_packet[index] = sequence;
}

#endif // #ifndef NEXT_REPLAY_PROTECTION_H

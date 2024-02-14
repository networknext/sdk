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

#ifndef NEXT_MEMORY_CHECKS_H
#define NEXT_MEMORY_CHECKS_H

#include "next.h"

#if NEXT_ENABLE_MEMORY_CHECKS

    #define NEXT_DECLARE_SENTINEL(n) uint32_t next_sentinel_##n[64];

    #define NEXT_INITIALIZE_SENTINEL(pointer,n) for ( int i = 0; i < 64; ++i ) { pointer->next_sentinel_##n[i] = 0xBAADF00D; }

    #define NEXT_VERIFY_SENTINEL(pointer,n) for ( int i = 0; i < 64; ++i ) { next_assert( pointer->next_sentinel_##n[i] == 0xBAADF00D ); }

#else // #if NEXT_ENABLE_MEMORY_CHECKS

    #define NEXT_DECLARE_SENTINEL(n)

    #define NEXT_INITIALIZE_SENTINEL(pointer,n)

    #define NEXT_VERIFY_SENTINEL(pointer,n)

#endif // #if NEXT_ENABLE_MEMORY_CHECKS

#endif // #ifndef NEXT_MEMORY_CHECKS_H

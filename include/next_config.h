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

#ifndef NEXT_CONFIG_H
#define NEXT_CONFIG_H

#include "next.h"
#include "next_constants.h"

#define NEXT_PROD_SERVER_BACKEND_HOSTNAME "server.virtualgo.net"
#define NEXT_PROD_SERVER_BACKEND_PUBLIC_KEY "K9BMSBeNK0hmxz+KkT1HouhMbleG4G+zj+Z/3eLI+zg="
#define NEXT_PROD_RELAY_BACKEND_PUBLIC_KEY "JWQL5GKNSqBJSbc6Ybga9oxFheKgLEmNjZwyTI/nJg0="

#define NEXT_DEV_SERVER_BACKEND_HOSTNAME "server-dev.virtualgo.net"
#define NEXT_DEV_SERVER_BACKEND_PUBLIC_KEY "emXNUJn34nPyGKWEAs7z18VXL+M8uo0QAZFHEGipeds="
#define NEXT_DEV_RELAY_BACKEND_PUBLIC_KEY "OBzjZnC9Dpr8W85ITOjsTfjoX9NJTux1vHzf2blXSkk="

#if !NEXT_DEVELOPMENT
#define NEXT_SERVER_BACKEND_HOSTNAME   NEXT_PROD_SERVER_BACKEND_HOSTNAME
#define NEXT_SERVER_BACKEND_PUBLIC_KEY NEXT_PROD_SERVER_BACKEND_PUBLIC_KEY
#define NEXT_RELAY_BACKEND_PUBLIC_KEY  NEXT_PROD_RELAY_BACKEND_PUBLIC_KEY
#else // #if !NEXT_DEVELOPMENT
#define NEXT_SERVER_BACKEND_HOSTNAME   NEXT_DEV_SERVER_BACKEND_HOSTNAME
#define NEXT_SERVER_BACKEND_PUBLIC_KEY NEXT_DEV_SERVER_BACKEND_PUBLIC_KEY
#define NEXT_RELAY_BACKEND_PUBLIC_KEY  NEXT_DEV_RELAY_BACKEND_PUBLIC_KEY
#endif // #if !NEXT_DEVELOPMENT

#define NEXT_CONFIG_BUCKET_NAME "xdp_network_next_sdk_config"

extern uint8_t next_server_backend_public_key[];

extern uint8_t next_relay_backend_public_key[];

#endif // #ifndef NEXT_CONFIG_H

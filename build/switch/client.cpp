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

#include <nn/nn_Log.h>
#include <nn/socket.h>
#include <nn/ro.h>
#include <nn/nn_Assert.h>
#include <nn/fs.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "next.h"
#include "next_tests.h"
#include "next_platform.h"

const char * server_address = "35.232.190.226:30000";

const char * buyer_public_key = "zkaPRGcAuThG6poXMJ8di/yKzgZEbqyQ6Ky951reRq4sgCm83lV24g==";

void client_packet_received( next_client_t * client, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
{
    (void)client; (void)context; (void)from; (void)packet_data; (void)packet_bytes;
}

extern "C" void nnMain()
{
    next_config_t config;
    next_default_config( &config );

    next_init( NULL, &config );

    printf( "\nRunning tests...\n\n" );

    next_log_level( NEXT_LOG_LEVEL_NONE );

    next_run_tests();
    
    printf( "\nAll tests passed successfully!\n\n" );

    next_term();

    printf( "Starting client...\n\n" );

    next_log_level( NEXT_LOG_LEVEL_INFO );

    strncpy( config.buyer_public_key, buyer_public_key, sizeof(config.buyer_public_key) - 1 );

    next_init( NULL, &config );

    next_client_t * client = next_client_create( NULL, "0.0.0.0:0", client_packet_received );
    if ( !client )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to create network next client" );
        exit( 1 );
    }

    next_client_open_session( client, server_address );

    while ( true )
    {
        next_client_update( client );

        uint8_t packet_data[32];
        memset( packet_data, 0, sizeof(packet_data) );
        next_client_send_packet( client, packet_data, sizeof( packet_data ) );

        next_platform_sleep( 1.0f / 60.0f );
    }

    printf( "Shutting down...\n\n" );

    next_client_destroy( client );

    next_term();
}

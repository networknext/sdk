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

#ifndef NEXT_SERVER_H
#define NEXT_SERVER_H

#include "next.h"
#include "next_config.h"
#include "next_address.h"
#include "next_session_manager.h"

struct next_server_t;

void next_server_initialize_sentinels( next_server_t * server );

void next_server_verify_sentinels( next_server_t * server );

next_server_t * next_server_create( void * context, const char * server_address, const char * bind_address, const char * datacenter, void (*packet_received_callback)( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes ) );

uint16_t next_server_port( next_server_t * server );

const next_address_t * next_server_address( next_server_t * server );

void next_server_destroy( next_server_t * server );

void next_server_update( next_server_t * server );

uint64_t next_server_upgrade_session( next_server_t * server, const next_address_t * address, const char * user_id );

bool next_server_session_upgraded( next_server_t * server, const next_address_t * address );

void next_server_send_packet_to_address( next_server_t * server, const next_address_t * address, const uint8_t * packet_data, int packet_bytes );

void next_server_send_packet( next_server_t * server, const next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

void next_server_send_packet_direct( next_server_t * server, const next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

void next_server_send_packet_raw( struct next_server_t * server, const struct next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

bool next_server_stats( next_server_t * server, const next_address_t * address, next_server_stats_t * stats );

bool next_server_ready( next_server_t * server );

const char * next_server_datacenter( next_server_t * server );

void next_server_session_event( struct next_server_t * server, const struct next_address_t * address, uint64_t session_events );

void next_server_flush( struct next_server_t * server );

void next_server_set_packet_receive_callback( struct next_server_t * server, void (*callback) ( void * data, next_address_t * from, uint8_t * packet_data, int * begin, int * end ), void * callback_data );

void next_server_set_send_packet_to_address_callback( struct next_server_t * server, int (*callback) ( void * data, const next_address_t * from, const uint8_t * packet_data, int packet_bytes ), void * callback_data );

void next_server_set_payload_receive_callback( struct next_server_t * server, int (*callback) ( void * data, const next_address_t * client_address, const uint8_t * payload_data, int payload_bytes ), void * callback_data );

bool next_server_direct_only( struct next_server_t * server );

#endif // #ifndef NEXT_SERVER_H

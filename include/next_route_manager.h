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

#ifndef NEXT_ROUTE_MANAGER_H
#define NEXT_ROUTE_MANAGER_H

#include "next.h"

struct next_route_manager_t;

void next_route_manager_initialize_sentinels( next_route_manager_t * route_manager );

void next_route_manager_verify_sentinels( next_route_manager_t * route_manager );

next_route_manager_t * next_route_manager_create( void * context );

void next_route_manager_destroy( next_route_manager_t * route_manager );

void next_route_manager_reset( next_route_manager_t * route_manager );

void next_route_manager_fallback_to_direct( next_route_manager_t * route_manager, uint32_t flags );

void next_route_manager_direct_route( next_route_manager_t * route_manager, bool quiet );

void next_route_manager_begin_next_route( next_route_manager_t * route_manager, int num_tokens, uint8_t * tokens, const uint8_t * public_key, const uint8_t * private_key, const uint8_t * magic, const next_address_t * client_external_address );

void next_route_manager_continue_next_route( next_route_manager_t * route_manager, int num_tokens, uint8_t * tokens, const uint8_t * public_key, const uint8_t * private_key, const uint8_t * magic, const next_address_t * client_external_address );

void next_route_manager_update( next_route_manager_t * route_manager, int update_type, int num_tokens, uint8_t * tokens, const uint8_t * public_key, const uint8_t * private_key, const uint8_t * magic, const next_address_t * client_external_address );

bool next_route_manager_has_network_next_route( next_route_manager_t * route_manager );

uint64_t next_route_manager_next_send_sequence( next_route_manager_t * route_manager );

bool next_route_manager_prepare_send_packet( next_route_manager_t * route_manager, uint64_t sequence, next_address_t * to, const uint8_t * payload_data, int payload_bytes, uint8_t * packet_data, int * packet_bytes, const uint8_t * magic, const next_address_t * client_external_address );

bool next_route_manager_process_server_to_client_packet( next_route_manager_t * route_manager, uint8_t packet_type, uint8_t * packet_data, int packet_bytes, uint64_t * payload_sequence );

void next_route_manager_check_for_timeouts( next_route_manager_t * route_manager );

bool next_route_manager_send_route_request( next_route_manager_t * route_manager, next_address_t * to, uint8_t * packet_data, int * packet_bytes );

bool next_route_manager_send_continue_request( next_route_manager_t * route_manager, next_address_t * to, uint8_t * packet_data, int * packet_bytes );

void next_route_manager_get_pending_route_data( next_route_manager_t * route_manager, bool * fallback_to_direct, bool * pending_route, uint64_t * pending_route_session_id, uint8_t * pending_route_session_version, uint8_t * pending_route_private_key );

void next_route_manager_confirm_pending_route( next_route_manager_t * route_manager, int * route_kbps_up, int * route_kbps_down );

void next_route_manager_get_current_route_data( next_route_manager_t * route_manager, bool * fallback_to_direct, bool * pending_route, bool * pending_continue, uint64_t * pending_route_session_id, uint8_t * pending_route_session_version, uint8_t * current_route_private_key );

void next_route_manager_confirm_continue_route( next_route_manager_t * route_manager );

bool next_route_manager_get_fallback_to_direct( next_route_manager_t * route_manager );

void next_route_manager_get_next_route_data( next_route_manager_t * route_manager, uint64_t * session_id, uint8_t * session_version, next_address_t * to, uint8_t * private_key );

#endif // #ifndef NEXT_ROUTE_MANAGER_H

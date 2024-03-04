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

#define NEXT_PROD_SERVER_BACKEND_HOSTNAME "server.virtualgo.net"
#define NEXT_PROD_SERVER_BACKEND_PUBLIC_KEY "Nb/JECiiftr9zuSlttiybGnjHzHlTBWE7JFwStPdIZ4="
#define NEXT_PROD_RELAY_BACKEND_PUBLIC_KEY "n7yB7ag9URvrKAUFLJYxaKi/HWN+O16MEQoE/bbf9xM="

#define NEXT_DEV_SERVER_BACKEND_HOSTNAME "server-dev.virtualgo.net"
#define NEXT_DEV_SERVER_BACKEND_PUBLIC_KEY "qtXDPQZ4St9XihqsNs6hP8QuSCHpr/63aKIOJehTNSg="
#define NEXT_DEV_RELAY_BACKEND_PUBLIC_KEY "QvHkCNNjQos2A9s1ufDJilvanYgQXNtB5E/eb6M9PDc="

#if !NEXT_DEVELOPMENT
#define NEXT_SERVER_BACKEND_HOSTNAME   NEXT_PROD_SERVER_BACKEND_HOSTNAME
#define NEXT_SERVER_BACKEND_PUBLIC_KEY NEXT_PROD_SERVER_BACKEND_PUBLIC_KEY
#define NEXT_RELAY_BACKEND_PUBLIC_KEY  NEXT_PROD_RELAY_BACKEND_PUBLIC_KEY
#else // #if !NEXT_DEVELOPMENT
#define NEXT_SERVER_BACKEND_HOSTNAME   NEXT_DEV_SERVER_BACKEND_HOSTNAME
#define NEXT_SERVER_BACKEND_PUBLIC_KEY NEXT_DEV_SERVER_BACKEND_PUBLIC_KEY
#define NEXT_RELAY_BACKEND_PUBLIC_KEY  NEXT_DEV_RELAY_BACKEND_PUBLIC_KEY
#endif // #if !NEXT_DEVELOPMENT

#define NEXT_CONFIG_BUCKET_NAME "consoles_network_next_sdk_config"

#define NEXT_SERVER_BACKEND_PORT                                  "40000"
#define NEXT_SERVER_INIT_TIMEOUT                                     10.0
#define NEXT_SERVER_AUTODETECT_TIMEOUT                                9.0
#define NEXT_SERVER_RESOLVE_HOSTNAME_TIMEOUT                         10.0
#define NEXT_ADDRESS_IPV4_BYTES                                         6
#define NEXT_ADDRESS_BYTES                                             19
#define NEXT_ADDRESS_BUFFER_SAFETY                                     32
#define NEXT_DEFAULT_SOCKET_SEND_BUFFER_SIZE                      1000000
#define NEXT_DEFAULT_SOCKET_RECEIVE_BUFFER_SIZE                   1000000
#define NEXT_REPLAY_PROTECTION_BUFFER_SIZE                           1024
#define NEXT_PING_HISTORY_ENTRY_COUNT                                1024
#define NEXT_CLIENT_STATS_WINDOW                                     10.0
#define NEXT_PING_SAFETY                                              1.0
#define NEXT_UPGRADE_TIMEOUT                                          5.0
#define NEXT_CLIENT_SESSION_TIMEOUT                                   5.0
#define NEXT_CLIENT_ROUTE_TIMEOUT                                    16.5
#define NEXT_SERVER_PING_TIMEOUT                                      5.0
#define NEXT_SERVER_SESSION_TIMEOUT                                  60.0
#define NEXT_SERVER_INIT_TIMEOUT                                     10.0
#define NEXT_INITIAL_PENDING_SESSION_SIZE                              64
#define NEXT_INITIAL_SESSION_SIZE                                      64
#define NEXT_PINGS_PER_SECOND                                           5
#define NEXT_DIRECT_PINGS_PER_SECOND                                    5
#define NEXT_COMMAND_QUEUE_LENGTH                                    1024
#define NEXT_NOTIFY_QUEUE_LENGTH                                     1024
#define NEXT_CLIENT_STATS_UPDATES_PER_SECOND                            5
#define NEXT_SECONDS_BETWEEN_SERVER_UPDATES                          10.0
#define NEXT_SECONDS_BETWEEN_SESSION_UPDATES                         10.0
#define NEXT_UPGRADE_TOKEN_BYTES                                      128
#define NEXT_MAX_NEAR_RELAYS                                           16
#define NEXT_ROUTE_TOKEN_BYTES                                         71
#define NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES                              111
#define NEXT_CONTINUE_TOKEN_BYTES                                      17
#define NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES                            57
#define NEXT_PING_TOKEN_BYTES                                          32
#define NEXT_UPDATE_TYPE_DIRECT                                         0
#define NEXT_UPDATE_TYPE_ROUTE                                          1
#define NEXT_UPDATE_TYPE_CONTINUE                                       2
#define NEXT_MAX_TOKENS                                                 7
#define NEXT_UPDATE_SEND_TIME                                        0.25
#define NEXT_ROUTE_REQUEST_SEND_TIME                                 0.25
#define NEXT_CONTINUE_REQUEST_SEND_TIME                              0.25
#define NEXT_SLICE_SECONDS                                           10.0
#define NEXT_ROUTE_REQUEST_TIMEOUT                                      5
#define NEXT_CONTINUE_REQUEST_TIMEOUT                                   5
#define NEXT_SESSION_UPDATE_RESEND_TIME                               1.0
#define NEXT_SESSION_UPDATE_TIMEOUT                                  10.0
#define NEXT_BANDWIDTH_LIMITER_INTERVAL                               1.0
#define NEXT_SERVER_FLUSH_TIMEOUT                                    30.0

#define NEXT_CLIENT_COUNTER_OPEN_SESSION                                0
#define NEXT_CLIENT_COUNTER_CLOSE_SESSION                               1
#define NEXT_CLIENT_COUNTER_UPGRADE_SESSION                             2
#define NEXT_CLIENT_COUNTER_FALLBACK_TO_DIRECT                          3
#define NEXT_CLIENT_COUNTER_PACKET_SENT_PASSTHROUGH                     4
#define NEXT_CLIENT_COUNTER_PACKET_RECEIVED_PASSTHROUGH                 5
#define NEXT_CLIENT_COUNTER_PACKET_SENT_DIRECT                          6
#define NEXT_CLIENT_COUNTER_PACKET_RECEIVED_DIRECT                      7
#define NEXT_CLIENT_COUNTER_PACKET_SENT_NEXT                            8
#define NEXT_CLIENT_COUNTER_PACKET_RECEIVED_NEXT                        9
#define NEXT_CLIENT_COUNTER_MULTIPATH                                  10
#define NEXT_CLIENT_COUNTER_PACKETS_LOST_CLIENT_TO_SERVER              11
#define NEXT_CLIENT_COUNTER_PACKETS_LOST_SERVER_TO_CLIENT              12
#define NEXT_CLIENT_COUNTER_PACKETS_OUT_OF_ORDER_CLIENT_TO_SERVER      13
#define NEXT_CLIENT_COUNTER_PACKETS_OUT_OF_ORDER_SERVER_TO_CLIENT      14

#define NEXT_CLIENT_COUNTER_MAX                                        64

#define NEXT_PACKET_LOSS_TRACKER_HISTORY                             1024
#define NEXT_PACKET_LOSS_TRACKER_SAFETY                                30
#define NEXT_SECONDS_BETWEEN_PACKET_LOSS_UPDATES                      0.1

#define NEXT_SERVER_INIT_RESPONSE_OK                                    0
#define NEXT_SERVER_INIT_RESPONSE_UNKNOWN_BUYER                         1
#define NEXT_SERVER_INIT_RESPONSE_UNKNOWN_DATACENTER                    2
#define NEXT_SERVER_INIT_RESPONSE_SDK_VERSION_TOO_OLD                   3
#define NEXT_SERVER_INIT_RESPONSE_SIGNATURE_CHECK_FAILED                4
#define NEXT_SERVER_INIT_RESPONSE_BUYER_NOT_ACTIVE                      5
#define NEXT_SERVER_INIT_RESPONSE_DATACENTER_NOT_ENABLED                6

#define NEXT_FLAGS_BAD_ROUTE_TOKEN                                 (1<<0)
#define NEXT_FLAGS_NO_ROUTE_TO_CONTINUE                            (1<<1)
#define NEXT_FLAGS_PREVIOUS_UPDATE_STILL_PENDING                   (1<<2)
#define NEXT_FLAGS_BAD_CONTINUE_TOKEN                              (1<<3)
#define NEXT_FLAGS_ROUTE_EXPIRED                                   (1<<4)
#define NEXT_FLAGS_ROUTE_REQUEST_TIMED_OUT                         (1<<5)
#define NEXT_FLAGS_CONTINUE_REQUEST_TIMED_OUT                      (1<<6)
#define NEXT_FLAGS_ROUTE_TIMED_OUT                                 (1<<7)
#define NEXT_FLAGS_UPGRADE_RESPONSE_TIMED_OUT                      (1<<8)
#define NEXT_FLAGS_ROUTE_UPDATE_TIMED_OUT                          (1<<9)
#define NEXT_FLAGS_DIRECT_PONG_TIMED_OUT                          (1<<10)
#define NEXT_FLAGS_NEXT_PONG_TIMED_OUT                            (1<<11)
#define NEXT_FLAGS_COUNT                                               12

#define NEXT_MAX_DATACENTER_NAME_LENGTH                               256

#define NEXT_MAX_SESSION_DATA_BYTES                                  1024

#define NEXT_MAX_SESSION_UPDATE_RETRIES                                10

#define NEXT_PASSTHROUGH_PACKET                                         0

#define NEXT_ROUTE_REQUEST_PACKET                                       1
#define NEXT_ROUTE_RESPONSE_PACKET                                      2
#define NEXT_CLIENT_TO_SERVER_PACKET                                    3
#define NEXT_SERVER_TO_CLIENT_PACKET                                    4
#define NEXT_SESSION_PING_PACKET                                        5
#define NEXT_SESSION_PONG_PACKET                                        6
#define NEXT_CONTINUE_REQUEST_PACKET                                    7
#define NEXT_CONTINUE_RESPONSE_PACKET                                   8
#define NEXT_CLIENT_PING_PACKET                                         9
#define NEXT_CLIENT_PONG_PACKET                                        10
#define NEXT_RESERVED_PACKET_1                                         11
#define NEXT_RESERVED_PACKET_2                                         12
#define NEXT_SERVER_PING_PACKET                                        13
#define NEXT_SERVER_PONG_PACKET                                        14

#define NEXT_DIRECT_PACKET                                             20
#define NEXT_DIRECT_PING_PACKET                                        21
#define NEXT_DIRECT_PONG_PACKET                                        22
#define NEXT_UPGRADE_REQUEST_PACKET                                    23
#define NEXT_UPGRADE_RESPONSE_PACKET                                   24
#define NEXT_UPGRADE_CONFIRM_PACKET                                    25
#define NEXT_ROUTE_UPDATE_PACKET                                       26
#define NEXT_ROUTE_UPDATE_ACK_PACKET                                   27
#define NEXT_CLIENT_STATS_PACKET                                       28

#define NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET                        50
#define NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET                       51
#define NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET                      52
#define NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET                     53
#define NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET                     54
#define NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET                    55

#define NEXT_CLIENT_ROUTE_UPDATE_TIMEOUT                               15

#define NEXT_MAX_SESSION_DEBUG                                       1024

#define NEXT_NEAR_RELAY_PINGS_PER_SECOND                                2

#define NEXT_IPV4_HEADER_BYTES                                         20
#define NEXT_UDP_HEADER_BYTES                                           8
#define NEXT_HEADER_BYTES                                              33

extern uint8_t next_server_backend_public_key[];

extern uint8_t next_relay_backend_public_key[];

#endif // #ifndef NEXT_CONFIG_H

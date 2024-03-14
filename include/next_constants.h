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

#ifndef NEXT_CONSTANTS_H
#define NEXT_CONSTANTS_H

#include "next.h"

#define NEXT_SERVER_BACKEND_PORT                                  "40000"
#define NEXT_SERVER_INIT_TIMEOUT                                      9.0
#define NEXT_SERVER_AUTODETECT_TIMEOUT                                9.0
#define NEXT_SERVER_RESOLVE_HOSTNAME_TIMEOUT                         10.0
#define NEXT_ADDRESS_BYTES_IPV4                                         6
#define NEXT_ADDRESS_BYTES                                             19
#define NEXT_ADDRESS_BUFFER_SAFETY                                     32
#define NEXT_DEFAULT_SOCKET_SEND_BUFFER_SIZE                      1000000
#define NEXT_DEFAULT_SOCKET_RECEIVE_BUFFER_SIZE                   1000000
#define NEXT_REPLAY_PROTECTION_BUFFER_SIZE                           1024
#define NEXT_PING_HISTORY_ENTRY_COUNT                                1024
#define NEXT_PING_STATS_WINDOW                                       10.0
#define NEXT_PING_SAFETY                                              1.0
#define NEXT_UPGRADE_TIMEOUT                                          5.0
#define NEXT_CLIENT_SESSION_TIMEOUT                                   5.0
#define NEXT_CLIENT_ROUTE_TIMEOUT                                    16.5
#define NEXT_SERVER_PING_TIMEOUT                                      5.0
#define NEXT_SERVER_SESSION_TIMEOUT                                  60.0
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
#define NEXT_MAX_CLIENT_RELAYS                                         16
#define NEXT_MAX_SERVER_RELAYS                                          8
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

#define NEXT_MAX_SESSION_DATA_BYTES                                   256

#define NEXT_MAX_SESSION_UPDATE_RETRIES                                10

#define NEXT_CLIENT_ROUTE_UPDATE_TIMEOUT                               15

#define NEXT_CLIENT_RELAY_PINGS_PER_SECOND                              2
#define NEXT_SERVER_RELAY_PINGS_PER_SECOND                             60

#define NEXT_IPV4_HEADER_BYTES                                         20
#define NEXT_UDP_HEADER_BYTES                                           8
#define NEXT_HEADER_BYTES                                              25

#define NEXT_SESSION_PRIVATE_KEY_BYTES                                 32

#define NEXT_PING_KEY_BYTES                                            32

#define NEXT_SECRET_KEY_BYTES                                          32

#define NEXT_SERVER_RELAYS_TIMEOUT                                      5
#define NEXT_SERVER_RELAYS_UPDATE_TIME_BASE                           300
#define NEXT_SERVER_RELAYS_UPDATE_TIME_VARIATION                      300
#define NEXT_SERVER_RELAYS_REQUEST_SEND_RATE                          1.0

#define NEXT_CLIENT_RELAYS_TIMEOUT                                      5
#define NEXT_CLIENT_RELAYS_UPDATE_TIME_BASE                           300
#define NEXT_CLIENT_RELAYS_UPDATE_TIME_VARIATION                      300
#define NEXT_CLIENT_RELAYS_REQUEST_SEND_RATE                          1.0

#define NEXT_SERVER_READY_TIMEOUT                                      15

#define NEXT_CLIENT_RELAY_UPDATE_SEND_RATE                            0.1
#define NEXT_CLIENT_RELAY_UPDATE_TIMEOUT                                5

#define NEXT_CLIENT_RELAY_PING_TIME                                     6

#endif // #ifndef NEXT_CONSTANTS_H

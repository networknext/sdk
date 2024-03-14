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

#ifndef NEXT_PACKETS_H
#define NEXT_PACKETS_H

#include "next.h"
#include "next_config.h"
#include "next_address.h"
#include "next_crypto.h"
#include "next_serialize.h"

struct next_replay_protection_t;

// ------------------------------------------------------------------------------------------------------

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
#define NEXT_ROUTE_ACK_PACKET                                          27
#define NEXT_CLIENT_STATS_PACKET                                       28
#define NEXT_CLIENT_RELAY_UPDATE_PACKET                                29
#define NEXT_CLIENT_RELAY_ACK_PACKET                                   30

#define NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET                        50
#define NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET                       51
#define NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET                      52
#define NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET                     53
#define NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET                     54
#define NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET                    55
#define NEXT_BACKEND_CLIENT_RELAY_REQUEST_PACKET                       56
#define NEXT_BACKEND_CLIENT_RELAY_RESPONSE_PACKET                      57
#define NEXT_BACKEND_SERVER_RELAY_REQUEST_PACKET                       58
#define NEXT_BACKEND_SERVER_RELAY_RESPONSE_PACKET                      59

// ------------------------------------------------------------------------------------------------------

struct NextUpgradeRequestPacket
{
    uint64_t protocol_version;
    uint64_t session_id;
    next_address_t client_address;
    next_address_t server_address;
    uint8_t server_kx_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];
    uint8_t upgrade_token[NEXT_UPGRADE_TOKEN_BYTES];
    uint8_t upcoming_magic[8];
    uint8_t current_magic[8];
    uint8_t previous_magic[8];

    NextUpgradeRequestPacket()
    {
        memset( this, 0, sizeof(NextUpgradeRequestPacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, protocol_version );
        serialize_uint64( stream, session_id );
        serialize_address( stream, client_address );
        serialize_address( stream, server_address );
        serialize_bytes( stream, server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        serialize_bytes( stream, upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
        serialize_bytes( stream, upcoming_magic, 8 );
        serialize_bytes( stream, current_magic, 8 );
        serialize_bytes( stream, previous_magic, 8 );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextUpgradeResponsePacket
{
    uint8_t client_open_session_sequence;
    uint8_t client_kx_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];
    uint8_t client_route_public_key[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];
    uint8_t upgrade_token[NEXT_UPGRADE_TOKEN_BYTES];
    int platform_id;
    int connection_type;

    NextUpgradeResponsePacket()
    {
        memset( this, 0, sizeof(NextUpgradeResponsePacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bits( stream, client_open_session_sequence, 8 );
        serialize_bytes( stream, client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        serialize_bytes( stream, client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
        serialize_bytes( stream, upgrade_token, NEXT_UPGRADE_TOKEN_BYTES );
        serialize_int( stream, platform_id, NEXT_PLATFORM_UNKNOWN, NEXT_PLATFORM_MAX );
        serialize_int( stream, connection_type, NEXT_CONNECTION_TYPE_UNKNOWN, NEXT_CONNECTION_TYPE_MAX );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextUpgradeConfirmPacket
{
    uint64_t upgrade_sequence;
    uint64_t session_id;
    next_address_t server_address;
    uint8_t client_kx_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];
    uint8_t server_kx_public_key[NEXT_CRYPTO_KX_PUBLICKEYBYTES];

    NextUpgradeConfirmPacket()
    {
        memset( this, 0, sizeof(NextUpgradeConfirmPacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, upgrade_sequence );
        serialize_uint64( stream, session_id );
        serialize_address( stream, server_address );
        serialize_bytes( stream, client_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        serialize_bytes( stream, server_kx_public_key, NEXT_CRYPTO_KX_PUBLICKEYBYTES );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextDirectPingPacket
{
    uint64_t ping_sequence;

    NextDirectPingPacket()
    {
        ping_sequence = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, ping_sequence );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextDirectPongPacket
{
    uint64_t ping_sequence;

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, ping_sequence );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextClientStatsPacket
{
    bool fallback_to_direct;
    bool next;
    bool multipath;
    bool reported;
    bool next_bandwidth_over_limit;
    int platform_id;
    int connection_type;
    float direct_kbps_up;
    float direct_kbps_down;
    float next_kbps_up;
    float next_kbps_down;
    float direct_rtt;
    float direct_jitter;
    float direct_packet_loss;
    float direct_max_packet_loss_seen;
    float next_rtt;
    float next_jitter;
    float next_packet_loss;
    float max_jitter_seen;
    int num_client_relays;
    uint64_t client_relay_ids[NEXT_MAX_CLIENT_RELAYS];
    uint8_t client_relay_rtt[NEXT_MAX_CLIENT_RELAYS];
    uint8_t client_relay_jitter[NEXT_MAX_CLIENT_RELAYS];
    float client_relay_packet_loss[NEXT_MAX_CLIENT_RELAYS];
    uint64_t packets_sent_client_to_server;
    uint64_t packets_lost_server_to_client;
    uint64_t packets_out_of_order_server_to_client;
    float jitter_server_to_client;
    uint64_t client_relay_request_id;

    NextClientStatsPacket()
    {
        memset( this, 0, sizeof(NextClientStatsPacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bool( stream, fallback_to_direct );
        serialize_bool( stream, next );
        serialize_bool( stream, multipath );
        serialize_bool( stream, reported );
        serialize_bool( stream, next_bandwidth_over_limit );
        serialize_int( stream, platform_id, NEXT_PLATFORM_UNKNOWN, NEXT_PLATFORM_MAX );
        serialize_int( stream, connection_type, NEXT_CONNECTION_TYPE_UNKNOWN, NEXT_CONNECTION_TYPE_MAX );
        serialize_float( stream, direct_kbps_up );
        serialize_float( stream, direct_kbps_down );
        serialize_float( stream, next_kbps_up );
        serialize_float( stream, next_kbps_down );
        serialize_float( stream, direct_rtt );
        serialize_float( stream, direct_jitter );
        serialize_float( stream, direct_packet_loss );
        serialize_float( stream, direct_max_packet_loss_seen );
        if ( next )
        {
            serialize_float( stream, next_rtt );
            serialize_float( stream, next_jitter );
            serialize_float( stream, next_packet_loss );
        }
        serialize_int( stream, num_client_relays, 0, NEXT_MAX_CLIENT_RELAYS );
        bool has_client_relay_pings = false;
        if ( Stream::IsWriting )
        {
            has_client_relay_pings = num_client_relays > 0;
        }
        serialize_bool( stream, has_client_relay_pings );
        if ( has_client_relay_pings )
        {
            for ( int i = 0; i < num_client_relays; ++i )
            {
                serialize_uint64( stream, client_relay_ids[i] );
                serialize_int( stream, client_relay_rtt[i], 0, 255 );
                serialize_int( stream, client_relay_jitter[i], 0, 255 );
                serialize_float( stream, client_relay_packet_loss[i] );
            }
        }
        serialize_uint64( stream, packets_sent_client_to_server );
        serialize_uint64( stream, packets_lost_server_to_client );
        serialize_uint64( stream, packets_out_of_order_server_to_client );
        serialize_float( stream, jitter_server_to_client );
        serialize_uint64( stream, client_relay_request_id );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextClientRelayUpdatePacket
{
    uint64_t request_id;
    int num_client_relays;
    uint64_t client_relay_ids[NEXT_MAX_CLIENT_RELAYS];
    next_address_t client_relay_addresses[NEXT_MAX_CLIENT_RELAYS];
    uint8_t client_relay_ping_tokens[NEXT_MAX_CLIENT_RELAYS][NEXT_PING_TOKEN_BYTES];
    uint64_t expire_timestamp;

    NextClientRelayUpdatePacket()
    {
        memset( this, 0, sizeof(NextClientRelayUpdatePacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, request_id );
        serialize_int( stream, num_client_relays, 0, NEXT_MAX_CLIENT_RELAYS );
        for ( int i = 0; i < num_client_relays; i++ )
        {
            serialize_uint64( stream, client_relay_ids[i] );
            serialize_address( stream, client_relay_addresses[i] );
            serialize_bytes( stream, client_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES );
        }
        serialize_uint64( stream, expire_timestamp );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextClientRelayAckPacket
{
    uint64_t request_id;

    NextClientRelayAckPacket()
    {
        request_id = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, request_id );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextRouteUpdatePacket
{
    uint64_t sequence;
    bool multipath;
    uint8_t update_type;
    int num_tokens;
    uint8_t tokens[NEXT_MAX_TOKENS*NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES];
    uint64_t packets_sent_server_to_client;
    uint64_t packets_lost_client_to_server;
    uint64_t packets_out_of_order_client_to_server;
    float jitter_client_to_server;
    uint8_t upcoming_magic[8];
    uint8_t current_magic[8];
    uint8_t previous_magic[8];

    NextRouteUpdatePacket()
    {
        memset( this, 0, sizeof(NextRouteUpdatePacket ) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, sequence );

        serialize_int( stream, update_type, 0, NEXT_UPDATE_TYPE_CONTINUE );

        if ( update_type != NEXT_UPDATE_TYPE_DIRECT )
        {
            serialize_int( stream, num_tokens, 0, NEXT_MAX_TOKENS );
            serialize_bool( stream, multipath );
        }
        if ( update_type == NEXT_UPDATE_TYPE_ROUTE )
        {
            serialize_bytes( stream, tokens, num_tokens * NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES );
        }
        else if ( update_type == NEXT_UPDATE_TYPE_CONTINUE )
        {
            serialize_bytes( stream, tokens, num_tokens * NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES );
        }

        serialize_uint64( stream, packets_sent_server_to_client );
        serialize_uint64( stream, packets_lost_client_to_server );

        serialize_uint64( stream, packets_out_of_order_client_to_server );

        serialize_float( stream, jitter_client_to_server );

        serialize_bytes( stream, upcoming_magic, 8 );
        serialize_bytes( stream, current_magic, 8 );
        serialize_bytes( stream, previous_magic, 8 );

        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextRouteAckPacket
{
    uint64_t sequence;

    NextRouteAckPacket()
    {
        sequence = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, sequence );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendServerInitRequestPacket
{
    int version_major;
    int version_minor;
    int version_patch;
    uint64_t buyer_id;
    uint64_t request_id;
    uint64_t datacenter_id;
    char datacenter_name[NEXT_MAX_DATACENTER_NAME_LENGTH];

    NextBackendServerInitRequestPacket()
    {
        version_major = NEXT_VERSION_MAJOR_INT;
        version_minor = NEXT_VERSION_MINOR_INT;
        version_patch = NEXT_VERSION_PATCH_INT;
        buyer_id = 0;
        request_id = 0;
        datacenter_id = 0;
        datacenter_name[0] = '\0';
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bits( stream, version_major, 8 );
        serialize_bits( stream, version_minor, 8 );
        serialize_bits( stream, version_patch, 8 );
        serialize_uint64( stream, buyer_id );
        serialize_uint64( stream, request_id );
        serialize_uint64( stream, datacenter_id );
        serialize_string( stream, datacenter_name, NEXT_MAX_DATACENTER_NAME_LENGTH );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendServerInitResponsePacket
{
    uint64_t request_id;
    uint32_t response;
    uint8_t upcoming_magic[8];
    uint8_t current_magic[8];
    uint8_t previous_magic[8];

    NextBackendServerInitResponsePacket()
    {
        memset( this, 0, sizeof(NextBackendServerInitResponsePacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, request_id );
        serialize_bits( stream, response, 8 );
        serialize_bytes( stream, upcoming_magic, 8 );
        serialize_bytes( stream, current_magic, 8 );
        serialize_bytes( stream, previous_magic, 8 );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendServerUpdateRequestPacket
{
    int version_major;
    int version_minor;
    int version_patch;
    uint64_t buyer_id;
    uint64_t request_id;
    uint64_t datacenter_id;
    uint32_t num_sessions;
    next_address_t server_address;
    uint64_t uptime;

    NextBackendServerUpdateRequestPacket()
    {
        version_major = NEXT_VERSION_MAJOR_INT;
        version_minor = NEXT_VERSION_MINOR_INT;
        version_patch = NEXT_VERSION_PATCH_INT;
        buyer_id = 0;
        request_id = 0;
        datacenter_id = 0;
        num_sessions = 0;
        memset( &server_address, 0, sizeof(next_address_t) );
        uptime = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bits( stream, version_major, 8 );
        serialize_bits( stream, version_minor, 8 );
        serialize_bits( stream, version_patch, 8 );
        serialize_uint64( stream, buyer_id );
        serialize_uint64( stream, request_id );
        serialize_uint64( stream, datacenter_id );
        serialize_uint32( stream, num_sessions );
        serialize_address( stream, server_address );
        serialize_uint64( stream, uptime );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendServerUpdateResponsePacket
{
    uint64_t request_id;
    uint8_t upcoming_magic[8];
    uint8_t current_magic[8];
    uint8_t previous_magic[8];

    NextBackendServerUpdateResponsePacket()
    {
        memset( this, 0, sizeof(NextBackendServerUpdateResponsePacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, request_id );
        serialize_bytes( stream, upcoming_magic, 8 );
        serialize_bytes( stream, current_magic, 8 );
        serialize_bytes( stream, previous_magic, 8 );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendClientRelayRequestPacket
{
    int version_major;
    int version_minor;
    int version_patch;
    uint64_t buyer_id;
    uint64_t request_id;
    uint64_t datacenter_id;
    next_address_t client_address;
   
    NextBackendClientRelayRequestPacket()
    {
        version_major = NEXT_VERSION_MAJOR_INT;
        version_minor = NEXT_VERSION_MINOR_INT;
        version_patch = NEXT_VERSION_PATCH_INT;
        buyer_id = 0;
        request_id = 0;
        datacenter_id = 0;
        memset( &client_address, 0, sizeof(next_address_t) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bits( stream, version_major, 8 );
        serialize_bits( stream, version_minor, 8 );
        serialize_bits( stream, version_patch, 8 );
        serialize_uint64( stream, buyer_id );
        serialize_uint64( stream, request_id );
        serialize_uint64( stream, datacenter_id );
        serialize_address( stream, client_address );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendClientRelayResponsePacket
{
    next_address_t client_address;
    uint64_t request_id;
    float latitude;
    float longitude;
    int num_client_relays;
    uint64_t client_relay_ids[NEXT_MAX_CLIENT_RELAYS];
    next_address_t client_relay_addresses[NEXT_MAX_CLIENT_RELAYS];
    uint8_t client_relay_ping_tokens[NEXT_MAX_CLIENT_RELAYS][NEXT_PING_TOKEN_BYTES];
    uint64_t expire_timestamp;

    NextBackendClientRelayResponsePacket()
    {
        memset( this, 0, sizeof(NextBackendClientRelayResponsePacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_address( stream, client_address );
        serialize_uint64( stream, request_id );
        serialize_float( stream, latitude );
        serialize_float( stream, longitude );
        serialize_int( stream, num_client_relays, 0, NEXT_MAX_CLIENT_RELAYS );
        for ( int i = 0; i < num_client_relays; i++ )
        {
            serialize_uint64( stream, client_relay_ids[i] );
            serialize_address( stream, client_relay_addresses[i] );
            serialize_bytes( stream, client_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES );
        }
        serialize_uint64( stream, expire_timestamp );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendServerRelayRequestPacket
{
    int version_major;
    int version_minor;
    int version_patch;
    uint64_t buyer_id;
    uint64_t request_id;
    uint64_t datacenter_id;
   
    NextBackendServerRelayRequestPacket()
    {
        version_major = NEXT_VERSION_MAJOR_INT;
        version_minor = NEXT_VERSION_MINOR_INT;
        version_patch = NEXT_VERSION_PATCH_INT;
        buyer_id = 0;
        request_id = 0;
        datacenter_id = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bits( stream, version_major, 8 );
        serialize_bits( stream, version_minor, 8 );
        serialize_bits( stream, version_patch, 8 );
        serialize_uint64( stream, buyer_id );
        serialize_uint64( stream, request_id );
        serialize_uint64( stream, datacenter_id );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendServerRelayResponsePacket
{
    uint64_t request_id;
    int num_server_relays;
    uint64_t server_relay_ids[NEXT_MAX_SERVER_RELAYS];
    next_address_t server_relay_addresses[NEXT_MAX_SERVER_RELAYS];
    uint8_t server_relay_ping_tokens[NEXT_MAX_SERVER_RELAYS][NEXT_PING_TOKEN_BYTES];
    uint64_t expire_timestamp;

    NextBackendServerRelayResponsePacket()
    {
        memset( this, 0, sizeof(NextBackendServerRelayResponsePacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, request_id );
        serialize_int( stream, num_server_relays, 0, NEXT_MAX_SERVER_RELAYS );
        for ( int i = 0; i < num_server_relays; i++ )
        {
            serialize_uint64( stream, server_relay_ids[i] );
            serialize_address( stream, server_relay_addresses[i] );
            serialize_bytes( stream, server_relay_ping_tokens[i], NEXT_PING_TOKEN_BYTES );
        }
        serialize_uint64( stream, expire_timestamp );
        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendSessionUpdateRequestPacket
{
    int version_major;
    int version_minor;
    int version_patch;
    uint64_t buyer_id;
    uint64_t datacenter_id;
    uint64_t session_id;
    uint32_t slice_number;
    uint32_t retry_number;
    int session_data_bytes;
    uint8_t session_data[NEXT_MAX_SESSION_DATA_BYTES];
    uint8_t session_data_signature[NEXT_CRYPTO_SIGN_BYTES];
    next_address_t client_address;
    next_address_t server_address;
    uint8_t client_route_public_key[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];
    uint8_t server_route_public_key[NEXT_CRYPTO_BOX_PUBLICKEYBYTES];
    uint64_t user_hash;
    int platform_id;
    int connection_type;
    bool next;
    bool reported;
    bool fallback_to_direct;
    bool client_bandwidth_over_limit;
    bool server_bandwidth_over_limit;
    bool client_ping_timed_out;
    bool has_client_relay_pings;
    bool has_server_relay_pings;
    bool client_relay_pings_have_changed;
    bool server_relay_pings_have_changed;
    uint64_t session_events;
    uint64_t internal_events;
    float direct_rtt;
    float direct_jitter;
    float direct_packet_loss;
    float direct_max_packet_loss_seen;
    float next_rtt;
    float next_jitter;
    float next_packet_loss;
    int num_client_relays;
    uint64_t client_relay_ids[NEXT_MAX_CLIENT_RELAYS];
    uint8_t client_relay_rtt[NEXT_MAX_CLIENT_RELAYS];
    uint8_t client_relay_jitter[NEXT_MAX_CLIENT_RELAYS];
    float client_relay_packet_loss[NEXT_MAX_CLIENT_RELAYS];
    int num_server_relays;
    uint64_t server_relay_ids[NEXT_MAX_SERVER_RELAYS];
    uint8_t server_relay_rtt[NEXT_MAX_SERVER_RELAYS];
    uint8_t server_relay_jitter[NEXT_MAX_SERVER_RELAYS];
    float server_relay_packet_loss[NEXT_MAX_SERVER_RELAYS];
    uint32_t direct_kbps_up;
    uint32_t direct_kbps_down;
    uint32_t next_kbps_up;
    uint32_t next_kbps_down;
    uint64_t packets_sent_client_to_server;
    uint64_t packets_sent_server_to_client;
    uint64_t packets_lost_client_to_server;
    uint64_t packets_lost_server_to_client;
    uint64_t packets_out_of_order_client_to_server;
    uint64_t packets_out_of_order_server_to_client;
    float jitter_client_to_server;
    float jitter_server_to_client;

    void Reset()
    {
        memset( this, 0, sizeof(NextBackendSessionUpdateRequestPacket) );
        version_major = NEXT_VERSION_MAJOR_INT;
        version_minor = NEXT_VERSION_MINOR_INT;
        version_patch = NEXT_VERSION_PATCH_INT;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bits( stream, version_major, 8 );
        serialize_bits( stream, version_minor, 8 );
        serialize_bits( stream, version_patch, 8 );

        serialize_uint64( stream, buyer_id );
        serialize_uint64( stream, datacenter_id );
        serialize_uint64( stream, session_id );

        serialize_uint32( stream, slice_number );

        serialize_int( stream, retry_number, 0, NEXT_MAX_SESSION_UPDATE_RETRIES );

        serialize_int( stream, session_data_bytes, 0, NEXT_MAX_SESSION_DATA_BYTES );
        if ( session_data_bytes > 0 )
        {
            serialize_bytes( stream, session_data, session_data_bytes );
            serialize_bytes( stream, session_data_signature, NEXT_CRYPTO_SIGN_BYTES );
        }

        serialize_address( stream, client_address );
        serialize_address( stream, server_address );

        serialize_bytes( stream, client_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );
        serialize_bytes( stream, server_route_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES );

        serialize_uint64( stream, user_hash );

        serialize_int( stream, platform_id, NEXT_PLATFORM_UNKNOWN, NEXT_PLATFORM_MAX );

        serialize_int( stream, connection_type, NEXT_CONNECTION_TYPE_UNKNOWN, NEXT_CONNECTION_TYPE_MAX );

        serialize_bool( stream, next );
        serialize_bool( stream, reported );
        serialize_bool( stream, fallback_to_direct );
        serialize_bool( stream, client_bandwidth_over_limit );
        serialize_bool( stream, server_bandwidth_over_limit );
        serialize_bool( stream, client_ping_timed_out );
        serialize_bool( stream, has_client_relay_pings );
        serialize_bool( stream, has_server_relay_pings );
        serialize_bool( stream, client_relay_pings_have_changed );
        serialize_bool( stream, server_relay_pings_have_changed );

        bool has_session_events = Stream::IsWriting && session_events != 0;
        bool has_internal_events = Stream::IsWriting && internal_events != 0;
        bool has_lost_packets = Stream::IsWriting && ( packets_lost_client_to_server + packets_lost_server_to_client ) > 0;
        bool has_out_of_order_packets = Stream::IsWriting && ( packets_out_of_order_client_to_server + packets_out_of_order_server_to_client ) > 0;

        serialize_bool( stream, has_session_events );
        serialize_bool( stream, has_internal_events );
        serialize_bool( stream, has_lost_packets );
        serialize_bool( stream, has_out_of_order_packets );

        if ( has_session_events )
        {
            serialize_uint64( stream, session_events );
        }

        if ( has_internal_events )
        {
            serialize_uint64( stream, internal_events );
        }

        serialize_float( stream, direct_rtt );
        serialize_float( stream, direct_jitter );
        serialize_float( stream, direct_packet_loss );
        serialize_float( stream, direct_max_packet_loss_seen );

        if ( next )
        {
            serialize_float( stream, next_rtt );
            serialize_float( stream, next_jitter );
            serialize_float( stream, next_packet_loss );
        }

        if ( has_client_relay_pings )
        {
            serialize_int( stream, num_client_relays, 0, NEXT_MAX_CLIENT_RELAYS );

            for ( int i = 0; i < num_client_relays; ++i )
            {
                serialize_uint64( stream, client_relay_ids[i] );
                if ( has_client_relay_pings )
                {
                    serialize_int( stream, client_relay_rtt[i], 0, 255 );
                    serialize_int( stream, client_relay_jitter[i], 0, 255 );
                    serialize_float( stream, client_relay_packet_loss[i] );
                }
            }
        }

        if ( has_server_relay_pings )
        {
            serialize_int( stream, num_server_relays, 0, NEXT_MAX_SERVER_RELAYS );

            for ( int i = 0; i < num_server_relays; ++i )
            {
                serialize_uint64( stream, server_relay_ids[i] );
                if ( has_server_relay_pings )
                {
                    serialize_int( stream, server_relay_rtt[i], 0, 255 );
                    serialize_int( stream, server_relay_jitter[i], 0, 255 );
                    serialize_float( stream, server_relay_packet_loss[i] );
                }
            }
        }

        serialize_uint32( stream, direct_kbps_up );
        serialize_uint32( stream, direct_kbps_down );

        if ( next )
        {
            serialize_uint32( stream, next_kbps_up );
            serialize_uint32( stream, next_kbps_down );
        }

        serialize_uint64( stream, packets_sent_client_to_server );
        serialize_uint64( stream, packets_sent_server_to_client );

        if ( has_lost_packets )
        {
            serialize_uint64( stream, packets_lost_client_to_server );
            serialize_uint64( stream, packets_lost_server_to_client );
        }

        if ( has_out_of_order_packets )
        {
            serialize_uint64( stream, packets_out_of_order_client_to_server );
            serialize_uint64( stream, packets_out_of_order_server_to_client );
        }

        serialize_float( stream, jitter_client_to_server );
        serialize_float( stream, jitter_server_to_client );

        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

struct NextBackendSessionUpdateResponsePacket
{
    uint64_t session_id;
    uint32_t slice_number;
    int session_data_bytes;
    uint8_t session_data[NEXT_MAX_SESSION_DATA_BYTES];
    uint8_t session_data_signature[NEXT_CRYPTO_SIGN_BYTES];
    uint8_t response_type;

    int num_tokens;
    uint8_t tokens[NEXT_MAX_TOKENS*NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES];
    bool multipath;

    NextBackendSessionUpdateResponsePacket()
    {
        memset( this, 0, sizeof(NextBackendSessionUpdateResponsePacket) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, session_id );

        serialize_uint32( stream, slice_number );

        serialize_int( stream, session_data_bytes, 0, NEXT_MAX_SESSION_DATA_BYTES );
        if ( session_data_bytes > 0 )
        {
            serialize_bytes( stream, session_data, session_data_bytes );
            serialize_bytes( stream, session_data_signature, NEXT_CRYPTO_SIGN_BYTES );
        }

        serialize_int( stream, response_type, 0, NEXT_UPDATE_TYPE_CONTINUE );

        if ( response_type != NEXT_UPDATE_TYPE_DIRECT )
        {
            serialize_bool( stream, multipath );
            serialize_int( stream, num_tokens, 0, NEXT_MAX_TOKENS );
        }

        if ( response_type == NEXT_UPDATE_TYPE_ROUTE )
        {
            serialize_bytes( stream, tokens, num_tokens * NEXT_ENCRYPTED_ROUTE_TOKEN_BYTES );
        }

        if ( response_type == NEXT_UPDATE_TYPE_CONTINUE )
        {
            serialize_bytes( stream, tokens, num_tokens * NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES );
        }

        return true;
    }
};

// ------------------------------------------------------------------------------------------------------

int next_write_direct_packet( uint8_t * packet_data, uint8_t open_session_sequence, uint64_t send_sequence, const uint8_t * game_packet_data, int game_packet_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_route_request_packet( uint8_t * packet_data, const uint8_t * token_data, int token_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_continue_request_packet( uint8_t * packet_data, const uint8_t * token_data, int token_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_route_response_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_client_to_server_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * game_packet_data, int game_packet_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_server_to_client_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * game_packet_data, int game_packet_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_session_ping_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, uint64_t ping_sequence, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_session_pong_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, uint64_t ping_sequence, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_continue_response_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_client_ping_packet( uint8_t * packet_data, const uint8_t * ping_token, uint64_t ping_sequence, uint64_t session_id, uint64_t expire_timestamp, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_client_pong_packet( uint8_t * packet_data, uint64_t ping_sequence, uint64_t session_id, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_server_ping_packet( uint8_t * packet_data, const uint8_t * ping_token, uint64_t ping_sequence, uint64_t expire_timestamp, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_server_pong_packet( uint8_t * packet_data, uint64_t ping_sequence, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_write_packet( uint8_t packet_id, void * packet_object, uint8_t * packet_data, int * packet_bytes, const int * signed_packet, const int * encrypted_packet, uint64_t * sequence, const uint8_t * sign_private_key, const uint8_t * encrypt_private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

bool next_is_payload_packet( uint8_t packet_id );

int next_read_packet( uint8_t packet_id, uint8_t * packet_data, int begin, int end, void * packet_object, const int * signed_packet, const int * encrypted_packet, uint64_t * sequence, const uint8_t * sign_public_key, const uint8_t * encrypt_private_key, next_replay_protection_t * replay_protection );

void next_post_validate_packet( uint8_t packet_id, const int * encrypted_packet, uint64_t * sequence, next_replay_protection_t * replay_protection );

int next_write_backend_packet( uint8_t packet_id, void * packet_object, uint8_t * packet_data, int * packet_bytes, const int * signed_packet, const uint8_t * sign_private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address );

int next_read_backend_packet( uint8_t packet_id, uint8_t * packet_data, int begin, int end, void * packet_object, const int * signed_packet, const uint8_t * sign_public_key );

// ------------------------------------------------------------------------------------------------------

#endif // #ifndef NEXT_PACKETS_H

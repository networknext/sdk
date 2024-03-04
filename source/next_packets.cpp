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

#include "next_packets.h"
#include "next_read_write.h"
#include "next_header.h"
#include "next_packet_filter.h"
#include "next_serialize.h"
#include "next_replay_protection.h"

#include <stdlib.h>
#if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS || NEXT_PLATFORM == NEXT_PLATFORM_GDK
#include <malloc.h>
#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS || NEXT_PLATFORM == NEXT_PLATFORM_GDK

int next_write_direct_packet( uint8_t * packet_data, uint8_t open_session_sequence, uint64_t send_sequence, const uint8_t * game_packet_data, int game_packet_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( game_packet_data );
    next_assert( game_packet_bytes >= 0 );
    next_assert( game_packet_bytes <= NEXT_MTU );
    
    packet_data[0] = NEXT_DIRECT_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    next_write_uint8( &p, open_session_sequence );
    next_write_uint64( &p, send_sequence );

    next_write_bytes( &p, game_packet_data, game_packet_bytes );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_route_request_packet( uint8_t * packet_data, const uint8_t * token_data, int token_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( token_data );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_ROUTE_REQUEST_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    next_write_bytes( &p, token_data, token_bytes );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_continue_request_packet( uint8_t * packet_data, const uint8_t * token_data, int token_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( token_data );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_CONTINUE_REQUEST_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    next_write_bytes( &p, token_data, token_bytes );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_route_response_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( private_key );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_ROUTE_RESPONSE_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    if ( next_write_header( NEXT_ROUTE_RESPONSE_PACKET, send_sequence, session_id, session_version, private_key, p ) != NEXT_OK )
        return 0;

    p += NEXT_HEADER_BYTES;

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_client_to_server_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * game_packet_data, int game_packet_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( private_key );
    next_assert( game_packet_data );
    next_assert( game_packet_bytes >= 0 );
    next_assert( game_packet_bytes <= NEXT_MTU );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_CLIENT_TO_SERVER_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    if ( next_write_header( NEXT_CLIENT_TO_SERVER_PACKET, send_sequence, session_id, session_version, private_key, p ) != NEXT_OK )
        return 0;

    p += NEXT_HEADER_BYTES;

    next_write_bytes( &p, game_packet_data, game_packet_bytes );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_server_to_client_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * game_packet_data, int game_packet_bytes, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( private_key );
    next_assert( game_packet_data );
    next_assert( game_packet_bytes >= 0 );
    next_assert( game_packet_bytes <= NEXT_MTU );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_SERVER_TO_CLIENT_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    if ( next_write_header( NEXT_SERVER_TO_CLIENT_PACKET, send_sequence, session_id, session_version, private_key, p ) != NEXT_OK )
        return 0;

    p += NEXT_HEADER_BYTES;

    next_write_bytes( &p, game_packet_data, game_packet_bytes );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_session_ping_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, uint64_t ping_sequence, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( private_key );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_SESSION_PING_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    if ( next_write_header( NEXT_SESSION_PING_PACKET, send_sequence, session_id, session_version, private_key, p ) != NEXT_OK )
        return 0;

    p += NEXT_HEADER_BYTES;

    next_write_uint64( &p, ping_sequence );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_session_pong_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, uint64_t ping_sequence, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( private_key );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_SESSION_PONG_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    if ( next_write_header( NEXT_SESSION_PONG_PACKET, send_sequence, session_id, session_version, private_key, p ) != NEXT_OK )
        return 0;

    p += NEXT_HEADER_BYTES;

    next_write_uint64( &p, ping_sequence );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_continue_response_packet( uint8_t * packet_data, uint64_t send_sequence, uint64_t session_id, uint8_t session_version, const uint8_t * private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_data );
    next_assert( private_key );
    next_assert( magic );
    next_assert( from_address );
    next_assert( to_address );

    packet_data[0] = NEXT_CONTINUE_RESPONSE_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    if ( next_write_header( NEXT_CONTINUE_RESPONSE_PACKET, send_sequence, session_id, session_version, private_key, p ) != NEXT_OK )
        return 0;

    p += NEXT_HEADER_BYTES;

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_client_ping_packet( uint8_t * packet_data, const uint8_t * ping_token, uint64_t ping_sequence, uint64_t session_id, uint64_t expire_timestamp, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    packet_data[0] = NEXT_CLIENT_PING_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    next_write_uint64( &p, ping_sequence );
    next_write_uint64( &p, session_id );
    next_write_uint64( &p, expire_timestamp );
    next_write_bytes( &p, ping_token, NEXT_PING_TOKEN_BYTES );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_client_pong_packet( uint8_t * packet_data, uint64_t ping_sequence, uint64_t session_id, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    packet_data[0] = NEXT_CLIENT_PONG_PACKET;
    uint8_t * a = packet_data + 1;
    uint8_t * b = packet_data + 3;
    uint8_t * p = packet_data + 18;

    next_write_uint64( &p, ping_sequence );
    next_write_uint64( &p, session_id );

    int packet_length = p - packet_data;
    next_generate_pittle( a, from_address, to_address, packet_length );
    next_generate_chonkle( b, magic, from_address, to_address, packet_length );
    return packet_length;
}

int next_write_packet( uint8_t packet_id, void * packet_object, uint8_t * packet_data, int * packet_bytes, const int * signed_packet, const int * encrypted_packet, uint64_t * sequence, const uint8_t * sign_private_key, const uint8_t * encrypt_private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    next_assert( packet_object );
    next_assert( packet_data );
    next_assert( packet_bytes );

    next::WriteStream stream( packet_data, NEXT_MAX_PACKET_BYTES );

    typedef next::WriteStream Stream;

    serialize_bits( stream, packet_id, 8 );

    for ( int i = 0; i < 17; ++i )
    {
        uint8_t dummy = 0;
        serialize_bits( stream, dummy, 8 );
    }

    if ( encrypted_packet && encrypted_packet[packet_id] != 0 )
    {
        next_assert( sequence );
        next_assert( encrypt_private_key );
        serialize_uint64( stream, *sequence );
    }

    switch ( packet_id )
    {
        case NEXT_UPGRADE_REQUEST_PACKET:
        {
            NextUpgradeRequestPacket * packet = (NextUpgradeRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write upgrade request packet" );
                return NEXT_ERROR;
            }
        }
        break;

        case NEXT_UPGRADE_RESPONSE_PACKET:
        {
            NextUpgradeResponsePacket * packet = (NextUpgradeResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write upgrade response packet" );
                return NEXT_ERROR;
            }
        }
        break;

        case NEXT_UPGRADE_CONFIRM_PACKET:
        {
            NextUpgradeConfirmPacket * packet = (NextUpgradeConfirmPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write upgrade confirm packet" );
                return NEXT_ERROR;
            }
        }
        break;

        case NEXT_DIRECT_PING_PACKET:
        {
            NextDirectPingPacket * packet = (NextDirectPingPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write direct ping packet" );
                return NEXT_ERROR;
            }
        }
        break;

        case NEXT_DIRECT_PONG_PACKET:
        {
            NextDirectPongPacket * packet = (NextDirectPongPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write direct pong packet" );
                return NEXT_ERROR;
            }
        }
        break;

        case NEXT_CLIENT_STATS_PACKET:
        {
            NextClientStatsPacket * packet = (NextClientStatsPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write client stats packet" );
                return NEXT_ERROR;
            }
        }
        break;

        case NEXT_ROUTE_UPDATE_PACKET:
        {
            NextRouteUpdatePacket * packet = (NextRouteUpdatePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write route update packet" );
                return NEXT_ERROR;
            }
        }
        break;

        case NEXT_ROUTE_UPDATE_ACK_PACKET:
        {
            NextRouteUpdateAckPacket * packet = (NextRouteUpdateAckPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to write route update ack packet" );
                return NEXT_ERROR;
            }
        }
        break;

        default:
            return NEXT_ERROR;
    }

    stream.Flush();

    *packet_bytes = stream.GetBytesProcessed();

    if ( signed_packet && signed_packet[packet_id] )
    {
        next_assert( sign_private_key );
        next_crypto_sign_state_t state;
        next_crypto_sign_init( &state );
        next_crypto_sign_update( &state, packet_data, 1 );
        next_crypto_sign_update( &state, packet_data + 18, *packet_bytes - 18 );
        next_crypto_sign_final_create( &state, packet_data + *packet_bytes, NULL, sign_private_key );
        *packet_bytes += NEXT_CRYPTO_SIGN_BYTES;
    }

    if ( encrypted_packet && encrypted_packet[packet_id] )
    {
        next_assert( !( signed_packet && signed_packet[packet_id] ) );

        uint8_t * additional = packet_data;
        uint8_t * nonce = packet_data + 18;
        uint8_t * message = packet_data + 18 + 8;
        int message_length = *packet_bytes - 18 - 8;

        unsigned long long encrypted_bytes = 0;

        next_crypto_aead_chacha20poly1305_encrypt( message, &encrypted_bytes,
                                                   message, message_length,
                                                   additional, 1,
                                                   NULL, nonce, encrypt_private_key );

        next_assert( encrypted_bytes == uint64_t(message_length) + NEXT_CRYPTO_AEAD_CHACHA20POLY1305_ABYTES );

        *packet_bytes = 18 + 8 + encrypted_bytes;

        (*sequence)++;
    }

    int packet_length = *packet_bytes;

    next_generate_pittle( packet_data + 1, from_address, to_address, packet_length );
    next_generate_chonkle( packet_data + 3, magic, from_address, to_address, packet_length );

    return NEXT_OK;
}

bool next_is_payload_packet( uint8_t packet_id )
{
    return packet_id == NEXT_CLIENT_TO_SERVER_PACKET ||
           packet_id == NEXT_SERVER_TO_CLIENT_PACKET;
}

int next_read_packet( uint8_t packet_id, uint8_t * packet_data, int begin, int end, void * packet_object, const int * signed_packet, const int * encrypted_packet, uint64_t * sequence, const uint8_t * sign_public_key, const uint8_t * encrypt_private_key, next_replay_protection_t * replay_protection )
{
    // todo: header

    next_assert( packet_data );
    next_assert( packet_object );

    next::ReadStream stream( packet_data, end );

    uint8_t * dummy = (uint8_t*) alloca( begin );
    serialize_bytes( stream, dummy, begin );

    if ( signed_packet && signed_packet[packet_id] )
    {
        next_assert( sign_public_key );

        const int packet_bytes = end - begin;

        if ( packet_bytes < int( NEXT_CRYPTO_SIGN_BYTES ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "signed packet is too small to be valid" );
            return NEXT_ERROR;
        }

        next_crypto_sign_state_t state;
        next_crypto_sign_init( &state );
        next_crypto_sign_update( &state, &packet_id, 1 );
        next_crypto_sign_update( &state, packet_data + begin, packet_bytes - NEXT_CRYPTO_SIGN_BYTES );
        if ( next_crypto_sign_final_verify( &state, packet_data + end - NEXT_CRYPTO_SIGN_BYTES, sign_public_key ) != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "signed packet did not verify" );
            return NEXT_ERROR;
        }
    }

    if ( encrypted_packet && encrypted_packet[packet_id] )
    {
        next_assert( !( signed_packet && signed_packet[packet_id] ) );

        next_assert( sequence );
        next_assert( encrypt_private_key );
        next_assert( replay_protection );

        const int packet_bytes = end - begin;

        if ( packet_bytes <= (int) ( 8 + NEXT_CRYPTO_AEAD_CHACHA20POLY1305_ABYTES ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "encrypted packet is too small to be valid" );
            return NEXT_ERROR;
        }

        const uint8_t * p = packet_data + begin;

        *sequence = next_read_uint64( &p );

        uint8_t * nonce = packet_data + begin;
        uint8_t * message = packet_data + begin + 8;
        uint8_t * additional = &packet_id;

        int message_length = end - ( begin + 8 );

        unsigned long long decrypted_bytes;

        if ( next_crypto_aead_chacha20poly1305_decrypt( message, &decrypted_bytes,
                                                        NULL,
                                                        message, message_length,
                                                        additional, 1,
                                                        nonce, encrypt_private_key ) != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "encrypted packet failed to decrypt" );
            return NEXT_ERROR;
        }

        next_assert( decrypted_bytes == uint64_t(message_length) - NEXT_CRYPTO_AEAD_CHACHA20POLY1305_ABYTES );

        serialize_bytes( stream, dummy, 8 );

        if ( next_replay_protection_already_received( replay_protection, *sequence ) )
            return NEXT_ERROR;
    }

    switch ( packet_id )
    {
        case NEXT_UPGRADE_REQUEST_PACKET:
        {
            NextUpgradeRequestPacket * packet = (NextUpgradeRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_UPGRADE_RESPONSE_PACKET:
        {
            NextUpgradeResponsePacket * packet = (NextUpgradeResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_UPGRADE_CONFIRM_PACKET:
        {
            NextUpgradeConfirmPacket * packet = (NextUpgradeConfirmPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_DIRECT_PING_PACKET:
        {
            NextDirectPingPacket * packet = (NextDirectPingPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_DIRECT_PONG_PACKET:
        {
            NextDirectPongPacket * packet = (NextDirectPongPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_CLIENT_STATS_PACKET:
        {
            NextClientStatsPacket * packet = (NextClientStatsPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_ROUTE_UPDATE_PACKET:
        {
            NextRouteUpdatePacket * packet = (NextRouteUpdatePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_ROUTE_UPDATE_ACK_PACKET:
        {
            NextRouteUpdateAckPacket * packet = (NextRouteUpdateAckPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        default:
            return NEXT_ERROR;
    }

    return (int) packet_id;
}

void next_post_validate_packet( uint8_t packet_id, const int * encrypted_packet, uint64_t * sequence, next_replay_protection_t * replay_protection )
{
    // todo: look into this. what about the non-payload packets with sequences?
    
    const bool payload_packet = next_is_payload_packet( packet_id );

    if ( payload_packet && encrypted_packet && encrypted_packet[packet_id] )
    {
        next_replay_protection_advance_sequence( replay_protection, *sequence );
    }
}

int next_write_backend_packet( uint8_t packet_id, void * packet_object, uint8_t * packet_data, int * packet_bytes, const int * signed_packet, const uint8_t * sign_private_key, const uint8_t * magic, const uint8_t * from_address, const uint8_t * to_address )
{
    // todo: header

    next_assert( packet_object );
    next_assert( packet_data );
    next_assert( packet_bytes );

    next::WriteStream stream( packet_data, NEXT_MAX_PACKET_BYTES );

    typedef next::WriteStream Stream;

    serialize_bits( stream, packet_id, 8 );

    uint8_t dummy = 0;
    for ( int i = 0; i < 17; ++i )
    {
        serialize_bits( stream, dummy, 8 );
    }

    switch ( packet_id )
    {
        case NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET:
        {
            NextBackendServerInitRequestPacket * packet = (NextBackendServerInitRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET:
        {
            NextBackendServerInitResponsePacket * packet = (NextBackendServerInitResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET:
        {
            NextBackendServerUpdateRequestPacket * packet = (NextBackendServerUpdateRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET:
        {
            NextBackendServerUpdateResponsePacket * packet = (NextBackendServerUpdateResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET:
        {
            NextBackendSessionUpdateRequestPacket * packet = (NextBackendSessionUpdateRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET:
        {
            NextBackendSessionUpdateResponsePacket * packet = (NextBackendSessionUpdateResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        default:
            return NEXT_ERROR;
    }

    stream.Flush();

    *packet_bytes = stream.GetBytesProcessed();

    next_assert( *packet_bytes >= 0 );
    next_assert( *packet_bytes < NEXT_MAX_PACKET_BYTES );

    if ( signed_packet && signed_packet[packet_id] )
    {
        next_assert( sign_private_key );
        next_crypto_sign_state_t state;
        next_crypto_sign_init( &state );
        next_crypto_sign_update( &state, packet_data, 1 );
        next_crypto_sign_update( &state, packet_data + 18, size_t(*packet_bytes) - 18 );
        next_crypto_sign_final_create( &state, packet_data + *packet_bytes, NULL, sign_private_key );
        *packet_bytes += NEXT_CRYPTO_SIGN_BYTES;
    }

    next_generate_pittle( packet_data + 1, from_address, to_address, *packet_bytes );
    next_generate_chonkle( packet_data + 3, magic, from_address, to_address, *packet_bytes );

    return NEXT_OK;
}

int next_read_backend_packet( uint8_t packet_id, uint8_t * packet_data, int begin, int end, void * packet_object, const int * signed_packet, const uint8_t * sign_public_key )
{
    next_assert( packet_data );
    next_assert( packet_object );

    next::ReadStream stream( packet_data, end );

    uint8_t * dummy = (uint8_t*) alloca( begin );

    serialize_bytes( stream, dummy, begin );

    if ( signed_packet && signed_packet[packet_id] )
    {
        next_assert( sign_public_key );

        const int packet_bytes = end - begin;

        if ( packet_bytes < int( NEXT_CRYPTO_SIGN_BYTES ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "signed backend packet is too small to be valid" );
            return NEXT_ERROR;
        }

        next_crypto_sign_state_t state;
        next_crypto_sign_init( &state );
        next_crypto_sign_update( &state, &packet_id, 1 );
        next_crypto_sign_update( &state, packet_data + begin, packet_bytes - NEXT_CRYPTO_SIGN_BYTES );
        if ( next_crypto_sign_final_verify( &state, packet_data + end - NEXT_CRYPTO_SIGN_BYTES, sign_public_key ) != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "signed backend packet did not verify" );
            return NEXT_ERROR;
        }
    }

    switch ( packet_id )
    {
        case NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET:
        {
            NextBackendServerInitRequestPacket * packet = (NextBackendServerInitRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET:
        {
            NextBackendServerInitResponsePacket * packet = (NextBackendServerInitResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET:
        {
            NextBackendServerUpdateRequestPacket * packet = (NextBackendServerUpdateRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET:
        {
            NextBackendServerUpdateResponsePacket * packet = (NextBackendServerUpdateResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET:
        {
            NextBackendSessionUpdateRequestPacket * packet = (NextBackendSessionUpdateRequestPacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        case NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET:
        {
            NextBackendSessionUpdateResponsePacket * packet = (NextBackendSessionUpdateResponsePacket*) packet_object;
            if ( !packet->Serialize( stream ) )
                return NEXT_ERROR;
        }
        break;

        default:
            return NEXT_ERROR;
    }

    return (int) packet_id;
}

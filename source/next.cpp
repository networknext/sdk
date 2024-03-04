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

#include "next.h"
#include "next_crypto.h"
#include "next_platform.h"
#include "next_address.h"
#include "next_read_write.h"
#include "next_base64.h"
#include "next_bitpacker.h"
#include "next_serialize.h"
#include "next_stream.h"
#include "next_queue.h"
#include "next_hash.h"
#include "next_config.h"
#include "next_replay_protection.h"
#include "next_ping_history.h"
#include "next_upgrade_token.h"
#include "next_route_token.h"
#include "next_header.h"
#include "next_packet_filter.h"
#include "next_bandwidth_limiter.h"
#include "next_packet_loss_tracker.h"
#include "next_out_of_order_tracker.h"
#include "next_jitter_tracker.h"
#include "next_pending_session_manager.h"
#include "next_proxy_session_manager.h"
#include "next_session_manager.h"
#include "next_relay_manager.h"
#include "next_route_manager.h"
#include "next_autodetect.h"
#include "next_internal_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <inttypes.h>
#if defined( _MSC_VER )
#include <malloc.h>
#endif // #if defined( _MSC_VER )
#include <time.h>
#include <atomic>

#if defined( _MSC_VER )
#pragma warning(push)
#pragma warning(disable:4996)
#pragma warning(disable:4127)
#pragma warning(disable:4244)
#pragma warning(disable:4668)
#endif

// -------------------------------------------------

void next_printf( const char * format, ... );

static void default_assert_function( const char * condition, const char * function, const char * file, int line )
{
    next_printf( "assert failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
    fflush( stdout );
    #if defined(_MSC_VER)
        __debugbreak();
    #elif defined(__ORBIS__)
        __builtin_trap();
    #elif defined(__PROSPERO__)
        __builtin_trap();
    #elif defined(__clang__)
        __builtin_debugtrap();
    #elif defined(__GNUC__)
        __builtin_trap();
    #elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
        raise(SIGTRAP);
    #else
        #error "asserts not supported on this platform!"
    #endif
}

void (*next_assert_function_pointer)( const char * condition, const char * function, const char * file, int line ) = default_assert_function;

void next_assert_function( void (*function)( const char * condition, const char * function, const char * file, int line ) )
{
    next_assert_function_pointer = function;
}

// -------------------------------------------------------------

static int log_quiet = 0;

void next_quiet( bool flag )
{
    log_quiet = flag;
}

static int log_level = NEXT_LOG_LEVEL_INFO;

void next_log_level( int level )
{
    log_level = level;
}

const char * next_log_level_string( int level )
{
    if ( level == NEXT_LOG_LEVEL_SPAM )
        return "spam";
    else if ( level == NEXT_LOG_LEVEL_DEBUG )
        return "debug";
    else if ( level == NEXT_LOG_LEVEL_INFO )
        return "info";
    else if ( level == NEXT_LOG_LEVEL_ERROR )
        return "error";
    else if ( level == NEXT_LOG_LEVEL_WARN )
        return "warning";
    else
        return "???";
}

static void default_log_function( int level, const char * format, ... )
{
    va_list args;
    va_start( args, format );
    char buffer[1024];
    vsnprintf( buffer, sizeof( buffer ), format, args );
    if ( level != NEXT_LOG_LEVEL_NONE )
    {
        if ( !log_quiet )
        {
            const char * level_string = next_log_level_string( level );
            printf( "%.6f: %s: %s\n", next_platform_time(), level_string, buffer );
        }
    }
    else
    {
        printf( "%s\n", buffer );
    }
    va_end( args );
    fflush( stdout );
}

static void (*log_function)( int level, const char * format, ... ) = default_log_function;

void next_log_function( void (*function)( int level, const char * format, ... ) )
{
    log_function = function;
}

void next_printf( const char * format, ... )
{
    va_list args;
    va_start( args, format );
    char buffer[1024];
    vsnprintf( buffer, sizeof(buffer), format, args );
    log_function( NEXT_LOG_LEVEL_NONE, "%s", buffer );
    va_end( args );
}

void next_printf( int level, const char * format, ... )
{
    if ( level > log_level )
        return;
    va_list args;
    va_start( args, format );
    char buffer[1024];
    vsnprintf( buffer, sizeof( buffer ), format, args );
    log_function( level, "%s", buffer );
    va_end( args );
}

// ------------------------------------------------------------

void * next_default_malloc_function( void * context, size_t bytes )
{
    (void) context;
    return malloc( bytes );
}

void next_default_free_function( void * context, void * p )
{
    (void) context;
    free( p );
}

static void * (*next_malloc_function)( void * context, size_t bytes ) = next_default_malloc_function;
static void (*next_free_function)( void * context, void * p ) = next_default_free_function;

void next_allocator( void * (*malloc_function)( void * context, size_t bytes ), void (*free_function)( void * context, void * p ) )
{
    next_assert( malloc_function );
    next_assert( free_function );
    next_malloc_function = malloc_function;
    next_free_function = free_function;
}

void * next_malloc( void * context, size_t bytes )
{
    next_assert( next_malloc_function );
    return next_malloc_function( context, bytes );
}

void next_free( void * context, void * p )
{
    next_assert( next_free_function );
    return next_free_function( context, p );
}

void next_clear_and_free( void * context, void * p, size_t p_size )
{
    memset( p, 0, p_size );
    next_free( context, p );
}

// -------------------------------------------------------------

const char * next_user_id_string( uint64_t user_id, char * buffer, size_t buffer_size )
{
    snprintf( buffer, buffer_size, "%" PRIx64, user_id );
    return buffer;
}

uint64_t next_protocol_version()
{
#if !NEXT_DEVELOPMENT
    #define VERSION_STRING(major,minor) #major #minor
    return next_hash_string( VERSION_STRING(NEXT_VERSION_MAJOR_INT, NEXT_VERSION_MINOR_INT) );
#else // #if !NEXT_DEVELOPMENT
    return 0;
#endif // #if !NEXT_DEVELOPMENT
}

float next_random_float()
{
    uint32_t uint32_value;
    next_crypto_random_bytes( (uint8_t*)&uint32_value, sizeof(uint32_value) );
    uint64_t uint64_value = uint64_t(uint32_value);
    double double_value = double(uint64_value) / 0xFFFFFFFF;
    return float(double_value);
}

uint64_t next_random_uint64()
{
    uint64_t value;
    next_crypto_random_bytes( (uint8_t*)&value, sizeof(value) );
    return value;
}

// -------------------------------------------------------------

void next_copy_string( char * dest, const char * source, size_t dest_size )
{
    next_assert( dest );
    next_assert( source );
    next_assert( dest_size >= 1 );
    memset( dest, 0, dest_size );
    for ( size_t i = 0; i < dest_size - 1; i++ )
    {
        if ( source[i] == '\0' )
            break;
        dest[i] = source[i];
    }
}

// -------------------------------------------------------------

int next_signed_packets[256];

int next_encrypted_packets[256];

void * next_global_context = NULL;

next_internal_config_t next_global_config;

void next_default_config( next_config_t * config )
{
    next_assert( config );
    memset( config, 0, sizeof(next_config_t) );
    next_copy_string( config->server_backend_hostname, NEXT_SERVER_BACKEND_HOSTNAME, sizeof(config->server_backend_hostname) );
    config->server_backend_hostname[sizeof(config->server_backend_hostname)-1] = '\0';
    config->socket_send_buffer_size = NEXT_DEFAULT_SOCKET_SEND_BUFFER_SIZE;
    config->socket_receive_buffer_size = NEXT_DEFAULT_SOCKET_RECEIVE_BUFFER_SIZE;
}

const char * next_platform_string( int platform_id )
{
    switch ( platform_id )
    {
        case NEXT_PLATFORM_WINDOWS:       return "windows";
        case NEXT_PLATFORM_MAC:           return "mac";
        case NEXT_PLATFORM_LINUX:         return "linux";
        case NEXT_PLATFORM_SWITCH:        return "switch";
        case NEXT_PLATFORM_PS4:           return "ps4";
        case NEXT_PLATFORM_PS5:           return "ps5";
        case NEXT_PLATFORM_IOS:           return "ios";
        case NEXT_PLATFORM_XBOX_ONE:      return "xboxone";
        case NEXT_PLATFORM_XBOX_SERIES_X: return "seriesx";
        default:
            break;
    }
    return "unknown";
}

const char * next_connection_string( int connection_type )
{
    switch ( connection_type )
    {
        case NEXT_CONNECTION_TYPE_WIRED:    return "wired";
        case NEXT_CONNECTION_TYPE_WIFI:     return "wi-fi";
        case NEXT_CONNECTION_TYPE_CELLULAR: return "cellular";
        default:
            break;
    }
    return "unknown";
}

int next_init( void * context, next_config_t * config_in )
{
    next_assert( next_global_context == NULL );

    next_base64_decode_data( NEXT_SERVER_BACKEND_PUBLIC_KEY, next_server_backend_public_key, 32 );
    next_base64_decode_data( NEXT_RELAY_BACKEND_PUBLIC_KEY, next_relay_backend_public_key, 32 );

    next_global_context = context;

    if ( next_platform_init() != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to initialize platform" );
        return NEXT_ERROR;
    }

    const char * platform_string = next_platform_string( next_platform_id() );
    const char * connection_string = next_connection_string( next_platform_connection_type() );

    next_printf( NEXT_LOG_LEVEL_INFO, "platform is %s (%s)", platform_string, connection_string );

    if ( next_crypto_init() == -1 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to initialize sodium" );
        return NEXT_ERROR;
    }

    const char * log_level_override = next_platform_getenv( "NEXT_LOG_LEVEL" );
    if ( log_level_override )
    {
        log_level = atoi( log_level_override );
        next_printf( NEXT_LOG_LEVEL_INFO, "log level overridden to %d", log_level );
    }

    next_internal_config_t config;

    memset( &config, 0, sizeof(next_internal_config_t) );

    config.socket_send_buffer_size = NEXT_DEFAULT_SOCKET_SEND_BUFFER_SIZE;
    config.socket_receive_buffer_size = NEXT_DEFAULT_SOCKET_RECEIVE_BUFFER_SIZE;

    const char * buyer_public_key_env = next_platform_getenv( "NEXT_BUYER_PUBLIC_KEY" );
    if ( buyer_public_key_env )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "buyer public key override: '%s'", buyer_public_key_env );
    }

    const char * buyer_public_key = buyer_public_key_env ? buyer_public_key_env : ( config_in ? config_in->buyer_public_key : "" );
    if ( buyer_public_key )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "buyer public key is '%s'", buyer_public_key );
        uint8_t decode_buffer[8+NEXT_CRYPTO_SIGN_PUBLICKEYBYTES];
        if ( next_base64_decode_data( buyer_public_key, decode_buffer, sizeof(decode_buffer) ) == sizeof(decode_buffer) )
        {
            const uint8_t * p = decode_buffer;
            config.client_buyer_id = next_read_uint64( &p );
            memcpy( config.buyer_public_key, decode_buffer + 8, NEXT_CRYPTO_SIGN_PUBLICKEYBYTES );
            next_printf( NEXT_LOG_LEVEL_INFO, "found valid buyer public key: '%s'", buyer_public_key );
            config.valid_buyer_public_key = true;
        }
        else
        {
            if ( buyer_public_key[0] != '\0' )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "buyer public key is invalid: '%s'", buyer_public_key );
            }
        }
    }

    const char * buyer_private_key_env = next_platform_getenv( "NEXT_BUYER_PRIVATE_KEY" );
    if ( buyer_private_key_env )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "buyer private key override" );
    }

    const char * buyer_private_key = buyer_private_key_env ? buyer_private_key_env : ( config_in ? config_in->buyer_private_key : "" );
    if ( buyer_private_key )
    {
        uint8_t decode_buffer[8+NEXT_CRYPTO_SIGN_SECRETKEYBYTES];
        if ( buyer_private_key && next_base64_decode_data( buyer_private_key, decode_buffer, sizeof(decode_buffer) ) == sizeof(decode_buffer) )
        {
            const uint8_t * p = decode_buffer;
            config.server_buyer_id = next_read_uint64( &p );
            memcpy( config.buyer_private_key, decode_buffer + 8, NEXT_CRYPTO_SIGN_SECRETKEYBYTES );
            config.valid_buyer_private_key = true;
            next_printf( NEXT_LOG_LEVEL_INFO, "found valid buyer private key" );
        }
        else
        {
            if ( buyer_private_key[0] != '\0' )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "buyer private key is invalid" );
            }
        }
    }

    if ( config.valid_buyer_private_key && config.valid_buyer_public_key && config.client_buyer_id != config.server_buyer_id )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "mismatch between client and server buyer id. please check the private and public keys are part of the same keypair!" );
        config.valid_buyer_public_key = false;
        config.valid_buyer_private_key = false;
        memset( config.buyer_public_key, 0, sizeof(config.buyer_public_key) );
        memset( config.buyer_private_key, 0, sizeof(config.buyer_private_key) );
    }

    next_copy_string( config.server_backend_hostname, config_in ? config_in->server_backend_hostname : NEXT_SERVER_BACKEND_HOSTNAME, sizeof(config.server_backend_hostname) );

    if ( config_in )
    {
        config.socket_send_buffer_size = config_in->socket_send_buffer_size;
        config.socket_receive_buffer_size = config_in->socket_receive_buffer_size;
    }

    config.disable_network_next = config_in ? config_in->disable_network_next : false;

    const char * next_disable_override = next_platform_getenv( "NEXT_DISABLE_NETWORK_NEXT" );
    {
        if ( next_disable_override != NULL )
        {
            int value = atoi( next_disable_override );
            if ( value > 0 )
            {
                config.disable_network_next = true;
            }
        }
    }

    if ( config.disable_network_next )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "network next is disabled" );
    }

    config.disable_autodetect = config_in ? config_in->disable_autodetect : false;

    const char * next_disable_autodetect_override = next_platform_getenv( "NEXT_DISABLE_AUTODETECT" );
    {
        if ( next_disable_autodetect_override != NULL )
        {
            int value = atoi( next_disable_autodetect_override );
            if ( value > 0 )
            {
                config.disable_autodetect = true;
            }
        }
    }

    if ( config.disable_autodetect )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "autodetect is disabled" );
    }

    const char * socket_send_buffer_size_override = next_platform_getenv( "NEXT_SOCKET_SEND_BUFFER_SIZE" );
    if ( socket_send_buffer_size_override != NULL )
    {
        int value = atoi( socket_send_buffer_size_override );
        if ( value > 0 )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "override socket send buffer size: %d", value );
            config.socket_send_buffer_size = value;
        }
    }

    const char * socket_receive_buffer_size_override = next_platform_getenv( "NEXT_SOCKET_RECEIVE_BUFFER_SIZE" );
    if ( socket_receive_buffer_size_override != NULL )
    {
        int value = atoi( socket_receive_buffer_size_override );
        if ( value > 0 )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "override socket receive buffer size: %d", value );
            config.socket_receive_buffer_size = value;
        }
    }

    const char * next_server_backend_hostname_override = next_platform_getenv( "NEXT_SERVER_BACKEND_HOSTNAME" );

    if ( next_server_backend_hostname_override )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "override server backend hostname: '%s'", next_server_backend_hostname_override );
        next_copy_string( config.server_backend_hostname, next_server_backend_hostname_override, sizeof(config.server_backend_hostname) );
    }

    const char * server_backend_public_key_env = next_platform_getenv( "NEXT_SERVER_BACKEND_PUBLIC_KEY" );
    if ( server_backend_public_key_env )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server backend public key override: %s", server_backend_public_key_env );

        if ( next_base64_decode_data( server_backend_public_key_env, next_server_backend_public_key, NEXT_CRYPTO_SIGN_PUBLICKEYBYTES ) == NEXT_CRYPTO_SIGN_PUBLICKEYBYTES )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "valid server backend public key" );
        }
        else
        {
            if ( server_backend_public_key_env[0] != '\0' )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "server backend public key is invalid: \"%s\"", server_backend_public_key_env );
            }
        }
    }

    const char * relay_backend_public_key_env = next_platform_getenv( "NEXT_RELAY_BACKEND_PUBLIC_KEY" );
    if ( relay_backend_public_key_env )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "relay backend public key override: %s", relay_backend_public_key_env );

        if ( next_base64_decode_data( relay_backend_public_key_env, next_relay_backend_public_key, NEXT_CRYPTO_BOX_PUBLICKEYBYTES ) == NEXT_CRYPTO_BOX_PUBLICKEYBYTES )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "valid relay backend public key" );
        }
        else
        {
            if ( relay_backend_public_key_env[0] != '\0' )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "relay backend public key is invalid: \"%s\"", relay_backend_public_key_env );
            }
        }
    }

    next_global_config = config;

    next_signed_packets[NEXT_UPGRADE_REQUEST_PACKET] = 1;
    next_signed_packets[NEXT_UPGRADE_CONFIRM_PACKET] = 1;

    next_signed_packets[NEXT_BACKEND_SERVER_INIT_REQUEST_PACKET] = 1;
    next_signed_packets[NEXT_BACKEND_SERVER_INIT_RESPONSE_PACKET] = 1;
    next_signed_packets[NEXT_BACKEND_SERVER_UPDATE_REQUEST_PACKET] = 1;
    next_signed_packets[NEXT_BACKEND_SERVER_UPDATE_RESPONSE_PACKET] = 1;
    next_signed_packets[NEXT_BACKEND_SESSION_UPDATE_REQUEST_PACKET] = 1;
    next_signed_packets[NEXT_BACKEND_SESSION_UPDATE_RESPONSE_PACKET] = 1;

    next_encrypted_packets[NEXT_DIRECT_PING_PACKET] = 1;
    next_encrypted_packets[NEXT_DIRECT_PONG_PACKET] = 1;
    next_encrypted_packets[NEXT_CLIENT_STATS_PACKET] = 1;
    next_encrypted_packets[NEXT_ROUTE_UPDATE_PACKET] = 1;
    next_encrypted_packets[NEXT_ROUTE_UPDATE_ACK_PACKET] = 1;

    return NEXT_OK;
}

void next_term()
{
    next_platform_term();

    next_global_context = NULL;
}

#if NEXT_DEVELOPMENT
bool next_packet_loss;
#endif // #if NEXT_DEVELOPMENT

// ---------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(pop)
#endif

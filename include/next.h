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

#ifndef NEXT_H
#define NEXT_H

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if !defined( NEXT_DEVELOPMENT )
#define NEXT_DEVELOPMENT 0
#endif // #if !defined( NEXT_DEVELOPMENT )

#if !NEXT_DEVELOPMENT

    #define NEXT_VERSION_FULL                               "1.0.0"
    #define NEXT_VERSION_MAJOR_INT                                1
    #define NEXT_VERSION_MINOR_INT                                0
    #define NEXT_VERSION_PATCH_INT                                0

#else // !NEXT_DEVELOPMENT

    #define NEXT_VERSION_FULL                                 "dev"
    #define NEXT_VERSION_MAJOR_INT                              255
    #define NEXT_VERSION_MINOR_INT                              255
    #define NEXT_VERSION_PATCH_INT                              255

#endif // !NEXT_DEVELOPMENT

#define NEXT_MTU                                               1200

#define NEXT_MAX_PACKET_BYTES                                  1384

#define NEXT_OK                                                   0
#define NEXT_ERROR                                               -1

#define NEXT_LOG_LEVEL_NONE                                       0
#define NEXT_LOG_LEVEL_ERROR                                      1
#define NEXT_LOG_LEVEL_WARN                                       2
#define NEXT_LOG_LEVEL_INFO                                       3
#define NEXT_LOG_LEVEL_DEBUG                                      4
#define NEXT_LOG_LEVEL_SPAM                                       5

#define NEXT_ADDRESS_NONE                                         0
#define NEXT_ADDRESS_IPV4                                         1
#define NEXT_ADDRESS_IPV6                                         2

#define NEXT_MAX_ADDRESS_STRING_LENGTH                          256

#define NEXT_CONNECTION_TYPE_UNKNOWN                              0
#define NEXT_CONNECTION_TYPE_WIRED                                1
#define NEXT_CONNECTION_TYPE_WIFI                                 2
#define NEXT_CONNECTION_TYPE_CELLULAR                             3
#define NEXT_CONNECTION_TYPE_MAX                                  3

#define NEXT_PLATFORM_UNKNOWN                                     0
#define NEXT_PLATFORM_WINDOWS                                     1
#define NEXT_PLATFORM_MAC                                         2
#define NEXT_PLATFORM_LINUX                                       3
#define NEXT_PLATFORM_SWITCH                                      4
#define NEXT_PLATFORM_PS4                                         5
#define NEXT_PLATFORM_IOS                                         6
#define NEXT_PLATFORM_XBOX_ONE                                    7
#define NEXT_PLATFORM_XBOX_SERIES_X                               8
#define NEXT_PLATFORM_PS5                                         9
#define NEXT_PLATFORM_GDK                                        10
#define NEXT_PLATFORM_MAX                                        10

#define NEXT_MAX_TAGS                                             8

#if defined(_WIN32)
#define NOMINMAX
#endif

#if defined( NEXT_SHARED )
    #if defined( _WIN32 ) || defined( __ORBIS__ ) || defined( __PROSPERO__ )
        #ifdef NEXT_EXPORT
            #if __cplusplus
            #define NEXT_EXPORT_FUNC extern "C" __declspec(dllexport)
            #else
            #define NEXT_EXPORT_FUNC extern __declspec(dllexport)
            #endif
        #else
            #if __cplusplus
            #define NEXT_EXPORT_FUNC extern "C" __declspec(dllimport)
            #else
            #define NEXT_EXPORT_FUNC extern __declspec(dllimport)
            #endif
        #endif
    #else
        #if __cplusplus
        #define NEXT_EXPORT_FUNC extern "C"
        #else
        #define NEXT_EXPORT_FUNC extern
        #endif
    #endif
#else
    #if __cplusplus
    #define NEXT_EXPORT_FUNC extern "C"
    #else
    #define NEXT_EXPORT_FUNC extern
    #endif
#endif

#if defined(NN_NINTENDO_SDK)
    #define NEXT_PLATFORM NEXT_PLATFORM_SWITCH
#elif defined(__ORBIS__)
    #define NEXT_PLATFORM NEXT_PLATFORM_PS4
#elif defined(__PROSPERO__)
    #define NEXT_PLATFORM NEXT_PLATFORM_PS5
#elif defined(_XBOX_ONE)
    #define NEXT_PLATFORM NEXT_PLATFORM_XBOX_ONE
#elif defined(_GAMING_XBOX)
    #define NEXT_PLATFORM NEXT_PLATFORM_GDK
#elif defined(_WIN32)
    #define NEXT_PLATFORM NEXT_PLATFORM_WINDOWS
#elif defined(__APPLE__)
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE
        #define NEXT_PLATFORM NEXT_PLATFORM_IOS
    #else
        #define NEXT_PLATFORM NEXT_PLATFORM_MAC
    #endif
#else
    #define NEXT_PLATFORM NEXT_PLATFORM_LINUX
#endif

#if NEXT_PLATFORM != NEXT_PLATFORM_PS4 && NEXT_PLATFORM != NEXT_PLATFORM_PS5 && NEXT_PLATFORM != NEXT_PLATFORM_SWITCH
#define NEXT_PLATFORM_HAS_IPV6 1
#endif // #if NEXT_PLATFORM != NEXT_PLATFORM_PS4 && NEXT_PLATFORM != NEXT_PLATFORM_PS5 && NEXT_PLATFORM != NEXT_PLATFORM_SWITCH

#if NEXT_PLATFORM != NEXT_PLATFORM_XBOX_ONE && NEXT_PLATFORM != NEXT_PLATFORM_GDK
#define NEXT_PLATFORM_CAN_RUN_SERVER 1
#endif // #if NEXT_PLATFORM != NEXT_PLATFORM_XBOX_ONE && NEXT_PLATFORM != NEXT_PLATFORM_GDK

#if !defined(NEXT_UNREAL_ENGINE)
#define NEXT_UNREAL_ENGINE 0
#endif // #if !defined(NEXT_UNREAL_ENGINE)

#if NEXT_UNREAL_ENGINE && NEXT_PLATFORM == NEXT_PLATFORM_PS5 && !defined(PLATFORM_PS5)
#error Building unreal engine on PS5, but PLATFORM_PS5 is not defined! Please follow steps in README.md for PS5 platform setup!
#endif // #if NEXT_UNREAL_ENGINE && NEXT_PLATFORM == NEXT_PLATFORM_PS5 && !defined(PLATFORM_PS5)

#if !defined(NEXT_SPIKE_TRACKING)
#define NEXT_SPIKE_TRACKING 0
#endif // #if !defined(NEXT_SPIKE_TRACKING)

#if !defined(NEXT_ENABLE_MEMORY_CHECKS)
#define NEXT_ENABLE_MEMORY_CHECKS 0
#endif // #if !defined(NEXT_ENABLE_MEMORY_CHECKS)

#if !defined (NEXT_LITTLE_ENDIAN ) && !defined( NEXT_BIG_ENDIAN )

  #ifdef __BYTE_ORDER__
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      #define NEXT_LITTLE_ENDIAN 1
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
      #define NEXT_BIG_ENDIAN 1
    #else
      #error Unknown machine endianess detected. Please define NEXT_LITTLE_ENDIAN or NEXT_BIG_ENDIAN.
    #endif // __BYTE_ORDER__

  // Detect with GLIBC's endian.h
  #elif defined(__GLIBC__)
    #include <endian.h>
    #if (__BYTE_ORDER == __LITTLE_ENDIAN)
      #define NEXT_LITTLE_ENDIAN 1
    #elif (__BYTE_ORDER == __BIG_ENDIAN)
      #define NEXT_BIG_ENDIAN 1
    #else
      #error Unknown machine endianess detected. Please define NEXT_LITTLE_ENDIAN or NEXT_BIG_ENDIAN.
    #endif // __BYTE_ORDER

  // Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro
  #elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
    #define NEXT_LITTLE_ENDIAN 1
  #elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
    #define NEXT_BIG_ENDIAN 1

  // Detect with architecture macros
  #elif    defined(__sparc)     || defined(__sparc__)                           \
        || defined(_POWER)      || defined(__powerpc__)                         \
        || defined(__ppc__)     || defined(__hpux)      || defined(__hppa)      \
        || defined(_MIPSEB)     || defined(_POWER)      || defined(__s390__)
    #define NEXT_BIG_ENDIAN 1
  #elif    defined(__i386__)    || defined(__alpha__)   || defined(__ia64)      \
        || defined(__ia64__)    || defined(_M_IX86)     || defined(_M_IA64)     \
        || defined(_M_ALPHA)    || defined(__amd64)     || defined(__amd64__)   \
        || defined(_M_AMD64)    || defined(__x86_64)    || defined(__x86_64__)  \
        || defined(_M_X64)      || defined(__bfin__)
    #define NEXT_LITTLE_ENDIAN 1
  #elif defined(_MSC_VER) && defined(_M_ARM)
    #define NEXT_LITTLE_ENDIAN 1
  #else
    #error Unknown machine endianess detected. Please define NEXT_LITTLE_ENDIAN or NEXT_BIG_ENDIAN.
  #endif

#endif

#if !defined(NEXT_BIG_ENDIAN)
#define NEXT_BIG_ENDIAN 0
#endif // #if !defined(NEXT_BIG_ENDIAN)

#if !defined(NEXT_LITTLE_ENDIAN)
#define NEXT_LITTLE_ENDIAN 0
#endif // #if !defined(NEXT_LITTLE_ENDIAN)

#if defined( _MSC_VER ) && _MSC_VER < 1700
typedef __int32 int32_t;
typedef __int64 int64_t;
#define PRId64 "I64d"
#define SCNd64 "I64d"
#define PRIx64 "I64x"
#define SCNx64 "I64x"
#else
#include <inttypes.h>
#endif

#if defined( _MSC_VER ) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

__inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

__inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}

#endif

// -----------------------------------------

struct next_config_t
{
    char server_backend_hostname[256];
    char buyer_public_key[256];
    char buyer_private_key[256];
    int socket_send_buffer_size;
    int socket_receive_buffer_size;
    bool disable_network_next;
    bool disable_autodetect;
};

NEXT_EXPORT_FUNC void next_default_config( struct next_config_t * config );

NEXT_EXPORT_FUNC int next_init( void * context, struct next_config_t * config );

NEXT_EXPORT_FUNC void next_term();

// -----------------------------------------

extern void (*next_assert_function_pointer)( const char * condition, const char * function, const char * file, int line );

#ifndef NEXT_ASSERTS
    #ifdef NDEBUG
        #define NEXT_ASSERTS 0
    #else
        #define NEXT_ASSERTS 1
    #endif
#endif

#if NEXT_ASSERTS
#define next_assert( condition )                                                            \
do                                                                                          \
{                                                                                           \
    if ( !(condition) )                                                                     \
    {                                                                                       \
        next_assert_function_pointer( #condition, __FUNCTION__, __FILE__, __LINE__ );       \
    }                                                                                       \
} while(0)
#else
#define next_assert( ignore ) ((void)0)
#endif

NEXT_EXPORT_FUNC void next_assert_function( void (*function)( const char * condition, const char * function, const char * file, int line ) );

// ------------------------------------------

NEXT_EXPORT_FUNC void next_quiet( bool flag );

NEXT_EXPORT_FUNC void next_log_level( int level );

NEXT_EXPORT_FUNC void next_log_function( void (*function)( int level, const char * format, ... ) );

NEXT_EXPORT_FUNC void next_printf( int level, const char * format, ... );

// ------------------------------------------

NEXT_EXPORT_FUNC void next_allocator( void * (*malloc_function)( void * context, size_t bytes ), void (*free_function)( void * context, void * p ) );

NEXT_EXPORT_FUNC void * next_malloc( void * context, size_t bytes );

NEXT_EXPORT_FUNC void next_free( void * context, void * p );

NEXT_EXPORT_FUNC void next_clear_and_free( void * context, void * p, size_t p_size );

// -----------------------------------------

NEXT_EXPORT_FUNC const char * next_user_id_string( uint64_t user_id, char * buffer, size_t buffer_size );

NEXT_EXPORT_FUNC float next_random_float();

NEXT_EXPORT_FUNC uint64_t next_random_uint64();

NEXT_EXPORT_FUNC uint64_t next_protocol_version();

// -----------------------------------------

struct next_client_stats_t
{
    int platform_id;
    int connection_type;
    bool next;
    bool upgraded;
    bool multipath;
    bool reported;
    bool fallback_to_direct;
    float direct_rtt;
    float direct_jitter;
    float direct_packet_loss;
    float direct_max_packet_loss_seen;
    float direct_kbps_up;
    float direct_kbps_down;
    float next_rtt;
    float next_jitter;
    float next_packet_loss;
    float next_kbps_up;
    float next_kbps_down;
    uint64_t packets_sent_client_to_server;
    uint64_t packets_sent_server_to_client;
    uint64_t packets_lost_client_to_server;
    uint64_t packets_lost_server_to_client;
    uint64_t packets_out_of_order_client_to_server;
    uint64_t packets_out_of_order_server_to_client;
    float jitter_client_to_server;
    float jitter_server_to_client;
};

// -----------------------------------------

#define NEXT_CLIENT_STATE_CLOSED        0
#define NEXT_CLIENT_STATE_OPEN          1
#define NEXT_CLIENT_STATE_ERROR         2

struct next_client_t;
struct next_address_t;

NEXT_EXPORT_FUNC struct next_client_t * next_client_create( void * context, const char * bind_address, void (*packet_received_callback)( struct next_client_t * client, void * context, const struct next_address_t * from, const uint8_t * packet_data, int packet_bytes ) );

NEXT_EXPORT_FUNC void next_client_destroy( struct next_client_t * client );

NEXT_EXPORT_FUNC uint16_t next_client_port( struct next_client_t * client );

NEXT_EXPORT_FUNC void next_client_open_session( struct next_client_t * client, const char * server_address );

NEXT_EXPORT_FUNC void next_client_close_session( struct next_client_t * client );

NEXT_EXPORT_FUNC bool next_client_is_session_open( struct next_client_t * client );

NEXT_EXPORT_FUNC int next_client_state( struct next_client_t * client );

NEXT_EXPORT_FUNC void next_client_update( struct next_client_t * client );

NEXT_EXPORT_FUNC void next_client_send_packet( struct next_client_t * client, const uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC void next_client_send_packet_direct( struct next_client_t * client, const uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC void next_client_send_packet_raw( struct next_client_t * client, const struct next_address_t * address, const uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC void next_client_report_session( struct next_client_t * client );

NEXT_EXPORT_FUNC uint64_t next_client_session_id( struct next_client_t * client );

NEXT_EXPORT_FUNC const struct next_client_stats_t * next_client_stats( struct next_client_t * client );

NEXT_EXPORT_FUNC const struct next_address_t * next_client_server_address( struct next_client_t * client );

NEXT_EXPORT_FUNC bool next_client_ready( struct next_client_t * client );

NEXT_EXPORT_FUNC bool next_client_fallback_to_direct( struct next_client_t * client );

// -----------------------------------------

struct next_server_stats_t
{
    uint64_t session_id;
    uint64_t user_hash;
    int platform_id;
    int connection_type;
    bool next;
    bool multipath;
    bool reported;
    bool fallback_to_direct;
    float direct_rtt;
    float direct_jitter;
    float direct_packet_loss;
    float direct_max_packet_loss_seen;
    float direct_kbps_up;
    float direct_kbps_down;
    float next_rtt;
    float next_jitter;
    float next_packet_loss;
    float next_kbps_up;
    float next_kbps_down;
    uint64_t packets_sent_client_to_server;
    uint64_t packets_sent_server_to_client;
    uint64_t packets_lost_client_to_server;
    uint64_t packets_lost_server_to_client;
    uint64_t packets_out_of_order_client_to_server;
    uint64_t packets_out_of_order_server_to_client;
    float jitter_client_to_server;
    float jitter_server_to_client;
};

#define NEXT_SERVER_STATE_DIRECT_ONLY               0
#define NEXT_SERVER_STATE_INITIALIZING              1
#define NEXT_SERVER_STATE_INITIALIZED               2

struct next_server_t;
struct next_address_t;

NEXT_EXPORT_FUNC struct next_server_t * next_server_create( void * context, const char * server_address, const char * bind_address, const char * datacenter, void (*packet_received_callback)( struct next_server_t * server, void * context, const struct next_address_t * from, const uint8_t * packet_data, int packet_bytes ) );

NEXT_EXPORT_FUNC void next_server_destroy( struct next_server_t * server );

NEXT_EXPORT_FUNC uint16_t next_server_port( struct next_server_t * server );

NEXT_EXPORT_FUNC const struct next_address_t * next_server_address( struct next_server_t * server );

NEXT_EXPORT_FUNC int next_server_state( struct next_server_t * server );

NEXT_EXPORT_FUNC void next_server_update( struct next_server_t * server );

NEXT_EXPORT_FUNC uint64_t next_server_upgrade_session( struct next_server_t * server, const struct next_address_t * address, const char * user_id );

NEXT_EXPORT_FUNC bool next_server_session_upgraded( struct next_server_t * server, const struct next_address_t * address );

NEXT_EXPORT_FUNC void next_server_send_packet( struct next_server_t * server, const struct next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC void next_server_send_packet_direct( struct next_server_t * server, const struct next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC void next_server_send_packet_raw( struct next_server_t * server, const struct next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC bool next_server_stats( struct next_server_t * server, const struct next_address_t * address, struct next_server_stats_t * stats );

NEXT_EXPORT_FUNC bool next_server_ready( struct next_server_t * server );

NEXT_EXPORT_FUNC const char * next_server_datacenter( struct next_server_t * server );

NEXT_EXPORT_FUNC void next_server_session_event( struct next_server_t * server, const struct next_address_t * address, uint64_t server_events );

NEXT_EXPORT_FUNC void next_server_flush( struct next_server_t * server );

NEXT_EXPORT_FUNC void next_server_set_packet_receive_callback( struct next_server_t * server, void (*callback) ( void * data, struct next_address_t * from, uint8_t * packet_data, int * begin, int * end ), void * callback_data );

NEXT_EXPORT_FUNC void next_server_set_send_packet_to_address_callback( struct next_server_t * server, int (*callback) ( void * data, const struct next_address_t * address, const uint8_t * packet_data, int packet_bytes ), void * callback_data );

NEXT_EXPORT_FUNC void next_server_set_payload_receive_callback( struct next_server_t * server, int (*callback) ( void * data, const struct next_address_t * address, const uint8_t * payload_data, int payload_bytes ), void * callback_data );

NEXT_EXPORT_FUNC bool next_server_direct_only( struct next_server_t * server );

// -----------------------------------------

NEXT_EXPORT_FUNC void next_copy_string( char * dest, const char * source, size_t dest_size );

// -----------------------------------------

#endif // #ifndef NEXT_H

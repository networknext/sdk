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

#include "next_platform_mac.h"

#if NEXT_PLATFORM == NEXT_PLATFORM_MAC

#include "next_platform.h"
#include "next_address.h"

#define __APPLE_USE_RFC_3542

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <CoreFoundation/CoreFoundation.h>

extern void * next_malloc( void * context, size_t bytes );

extern void next_free( void * context, void * p );

// ---------------------------------------------------

static int connection_type = NEXT_CONNECTION_TYPE_UNKNOWN;

static mach_timebase_info_data_t timebase_info;

static uint64_t time_start;

int next_platform_init()
{
    mach_timebase_info( &timebase_info );

    time_start = mach_absolute_time();

    connection_type = NEXT_CONNECTION_TYPE_UNKNOWN;

    SCDynamicStoreRef dynamic_store = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR( "FindCurrentInterfaceIpMac" ), NULL, NULL );
    CFPropertyListRef global = SCDynamicStoreCopyValue( dynamic_store, CFSTR( "State:/Network/Global/IPv4" ) );
    if ( global )
    {
        CFStringRef primary_interface_name = (CFStringRef)( CFDictionaryGetValue( (CFDictionaryRef)( global ), CFSTR( "PrimaryInterface" ) ) );
        CFArrayRef interfaces = SCNetworkInterfaceCopyAll();
        CFIndex count = CFArrayGetCount( interfaces );
        for ( CFIndex i = 0; i < count; i++ )
        {   
            SCNetworkInterfaceRef interface = (SCNetworkInterfaceRef)( CFArrayGetValueAtIndex( interfaces, i ) );
            CFStringRef bsd_name = SCNetworkInterfaceGetBSDName( interface );
            if ( CFStringCompare( primary_interface_name, bsd_name, 0 ) == kCFCompareEqualTo )
            {   
                CFStringRef interface_type = SCNetworkInterfaceGetInterfaceType( interface );
                if ( CFStringCompare( interface_type, kSCNetworkInterfaceTypeEthernet, 0 ) == kCFCompareEqualTo )
                {
                    connection_type = NEXT_CONNECTION_TYPE_WIRED;
                }
                else if ( CFStringCompare( interface_type, kSCNetworkInterfaceTypeIEEE80211, 0 ) == kCFCompareEqualTo )
                {
                    connection_type = NEXT_CONNECTION_TYPE_WIFI;
                }
                else if ( CFStringCompare( interface_type, kSCNetworkInterfaceTypeWWAN, 0 ) == kCFCompareEqualTo )
                {
                    connection_type = NEXT_CONNECTION_TYPE_CELLULAR;
                }
                break;
            }
        }
        CFRelease( interfaces );
        CFRelease( global );
    }
    CFRelease( dynamic_store );

    return NEXT_OK;
}

void next_platform_term()
{
    // ...
}

const char * next_platform_getenv( const char * var )
{
    return getenv( var );
}

// ---------------------------------------------------

uint16_t next_platform_ntohs( uint16_t in )
{
    return (uint16_t)( ( ( in << 8 ) & 0xFF00 ) | ( ( in >> 8 ) & 0x00FF ) );
}

uint16_t next_platform_htons( uint16_t in )
{
    return (uint16_t)( ( ( in << 8 ) & 0xFF00 ) | ( ( in >> 8 ) & 0x00FF ) );
}

int next_platform_inet_pton4( const char * address_string, uint32_t * address_out )
{
    sockaddr_in sockaddr4;
    bool success = inet_pton( AF_INET, address_string, &sockaddr4.sin_addr ) == 1;
    *address_out = sockaddr4.sin_addr.s_addr;
    return success ? NEXT_OK : NEXT_ERROR;
}

int next_platform_inet_pton6( const char * address_string, uint16_t * address_out )
{
    return inet_pton( AF_INET6, address_string, address_out ) == 1 ? NEXT_OK : NEXT_ERROR;
}

int next_platform_inet_ntop6( const uint16_t * address, char * address_string, size_t address_string_size )
{
    return inet_ntop( AF_INET6, (void*)address, address_string, socklen_t( address_string_size ) ) == NULL ? NEXT_ERROR : NEXT_OK;
}

// ---------------------------------------------------

int next_platform_hostname_resolve( const char * hostname, const char * port, next_address_t * address )
{
    addrinfo hints;
    memset( &hints, 0, sizeof(hints) );
    addrinfo * result;
    if ( getaddrinfo( hostname, port, &hints, &result ) == 0 )
    {
        if ( result )
        {
            if ( result->ai_addr->sa_family == AF_INET6 )
            {
                sockaddr_in6 * addr_ipv6 = (sockaddr_in6 *)( result->ai_addr );
                address->type = NEXT_ADDRESS_IPV6;
                for ( int i = 0; i < 8; ++i )
                {
                    address->data.ipv6[i] = next_platform_ntohs( ( (uint16_t*) &addr_ipv6->sin6_addr ) [i] );
                }
                address->port = next_platform_ntohs( addr_ipv6->sin6_port );
                freeaddrinfo( result );
                return NEXT_OK;
            }
            else if ( result->ai_addr->sa_family == AF_INET )
            {
                sockaddr_in * addr_ipv4 = (sockaddr_in *)( result->ai_addr );
                address->type = NEXT_ADDRESS_IPV4;
                address->data.ipv4[0] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x000000FF ) );
                address->data.ipv4[1] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x0000FF00 ) >> 8 );
                address->data.ipv4[2] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x00FF0000 ) >> 16 );
                address->data.ipv4[3] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0xFF000000 ) >> 24 );
                address->port = next_platform_ntohs( addr_ipv4->sin_port );
                freeaddrinfo( result );
                return NEXT_OK;
            }
            else
            {
                next_assert( 0 );
                freeaddrinfo( result );
                return NEXT_ERROR;
            }
        }
    }

    return NEXT_ERROR;
}

uint16_t next_platform_preferred_client_port()
{
    return 0;
}

bool next_platform_client_dual_stack()
{
    return true;
}

// ---------------------------------------------------

int next_platform_id()
{
    return NEXT_PLATFORM_MAC;
}

int next_platform_connection_type()
{
    return connection_type;
}

double next_platform_time()
{
    uint64_t current = mach_absolute_time();
    return ( (double) ( current - time_start ) ) * ( (double) timebase_info.numer ) / ( (double) timebase_info.denom ) / 1000000000.0;
}

void next_platform_sleep( double time )
{
    usleep( (int) ( time * 1000000 ) );
}

// ---------------------------------------------------

void next_platform_socket_destroy( next_platform_socket_t * socket );

extern bool next_packet_tagging_enabled;

next_platform_socket_t * next_platform_socket_create( void * context, next_address_t * address, int socket_type, float timeout_seconds, int send_buffer_size, int receive_buffer_size )
{
    next_assert( address );
    next_assert( address->type != NEXT_ADDRESS_NONE );

    next_platform_socket_t * socket = (next_platform_socket_t*) next_malloc( context, sizeof( next_platform_socket_t ) );

    next_assert( socket );

    socket->context = context;

    socket->ipv6 = address->type == NEXT_ADDRESS_IPV6;

    // create socket

    socket->handle = ::socket( ( address->type == NEXT_ADDRESS_IPV6 ) ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if ( socket->handle < 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to create socket" );
        next_free( context, socket );
        return NULL;
    }

    // setup socket as dual stack ipv4/ipv6

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        int yes = 0;
        if ( setsockopt( socket->handle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)( &yes ), sizeof( yes ) ) != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to clear socket ipv6 only" );
            next_platform_socket_destroy( socket );
            return NULL;
        }
    }

    // increase socket send and receive buffer sizes

    if ( setsockopt( socket->handle, SOL_SOCKET, SO_SNDBUF, (char*)( &send_buffer_size ), sizeof( int ) ) != 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket send buffer size" );
        return NULL;
    }

    if ( setsockopt( socket->handle, SOL_SOCKET, SO_RCVBUF, (char*)( &receive_buffer_size ), sizeof( int ) ) != 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket receive buffer size" );
        next_platform_socket_destroy( socket );
        return NULL;
    }

    // bind to port

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        sockaddr_in6 socket_address;
        memset( &socket_address, 0, sizeof( sockaddr_in6 ) );
        socket_address.sin6_family = AF_INET6;
        for ( int i = 0; i < 8; ++i )
        {
            ( (uint16_t*) &socket_address.sin6_addr ) [i] = next_platform_htons( address->data.ipv6[i] );
        }
        socket_address.sin6_port = next_platform_htons( address->port );

        if ( bind( socket->handle, (sockaddr*) &socket_address, sizeof( socket_address ) ) < 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to bind socket (ipv6)" );
            next_platform_socket_destroy( socket );
            return NULL;
        }
    }
    else
    {
        sockaddr_in socket_address;
        memset( &socket_address, 0, sizeof( socket_address ) );
        socket_address.sin_family = AF_INET;
        socket_address.sin_addr.s_addr = ( ( (uint32_t) address->data.ipv4[0] ) )      | 
                                         ( ( (uint32_t) address->data.ipv4[1] ) << 8 )  | 
                                         ( ( (uint32_t) address->data.ipv4[2] ) << 16 ) | 
                                         ( ( (uint32_t) address->data.ipv4[3] ) << 24 );
        socket_address.sin_port = next_platform_htons( address->port );

        if ( bind( socket->handle, (sockaddr*) &socket_address, sizeof( socket_address ) ) < 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to bind socket (ipv4)" );
            next_platform_socket_destroy( socket );
            return NULL;
        }
    }

    // if bound to port 0 find the actual port we got

    if ( address->port == 0 )
    {
        if ( address->type == NEXT_ADDRESS_IPV6 )
        {
            sockaddr_in6 sin;
            socklen_t len = sizeof( sin );
            if ( getsockname( socket->handle, (sockaddr*)( &sin ), &len ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "failed to get socket port (ipv6)" );
                next_platform_socket_destroy( socket );
                return NULL;
            }
            address->port = next_platform_ntohs( sin.sin6_port );
        }
        else
        {
            sockaddr_in sin;
            socklen_t len = sizeof( sin );
            if ( getsockname( socket->handle, (sockaddr*)( &sin ), &len ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "failed to get socket port (ipv4)" );
                next_platform_socket_destroy( socket );
                return NULL;
            }
            address->port = next_platform_ntohs( sin.sin_port );
        }
    }

    // set non-blocking io and receive timeout

    if ( socket_type == NEXT_PLATFORM_SOCKET_NON_BLOCKING )
    {
        // non-blocking
        if ( fcntl( socket->handle, F_SETFL, O_NONBLOCK, 1 ) == -1 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket to non-blocking" );
            next_platform_socket_destroy( socket );
            return NULL;
        }
    }
    else if ( timeout_seconds > 0.0f )
    {
        // blocking with receive timeout
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = (int) ( timeout_seconds * 1000000.0 );
        if ( setsockopt( socket->handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) ) < 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket receive timeout" );
            next_platform_socket_destroy( socket );
            return NULL;
        }
    }
    else
    {
        // blocking with no timeout
    }

    // set don't fragment bit

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        int val = 1;
        setsockopt( socket->handle, IPPROTO_IPV6, IPV6_DONTFRAG, &val, sizeof(val) );
    }
    else
    {
        int val = 1;
        setsockopt( socket->handle, IPPROTO_IP, IP_DONTFRAG, &val, sizeof(val) );
    }

    // tag packet as low latency

    if ( next_packet_tagging_enabled )
    {
        if ( address->type == NEXT_ADDRESS_IPV6 )
        {
            int tos = 46;
            if ( setsockopt( socket->handle, IPPROTO_IPV6, IPV6_TCLASS, (const char *)&tos, sizeof(tos) ) != 0 )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to enable dscp packet tagging (ipv6)" );
            }
        }
        else
        {
            int tos = 46;
            if ( setsockopt( socket->handle, IPPROTO_IP, IP_TOS, (const char *)&tos, sizeof(tos) ) != 0 )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to enable dscp packet tagging (ipv4)" );
            }
        }
    }

    return socket;
}

void next_platform_socket_destroy( next_platform_socket_t * socket )
{
    next_assert( socket );

    if ( socket->handle != 0 )
    {
        close( socket->handle );
    }
    
    next_free( socket->context, socket );
}

void next_platform_socket_send_packet( next_platform_socket_t * socket, const next_address_t * input_to, const void * packet_data, int packet_bytes )
{
    next_assert( socket );
    next_assert( input_to );
    next_assert( input_to->type == NEXT_ADDRESS_IPV6 || input_to->type == NEXT_ADDRESS_IPV4 );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    next_address_t to = *input_to;

    if ( socket->ipv6 )
    {
        // socket is dual stack ipv4 and ipv6

        if ( to.type == NEXT_ADDRESS_IPV4 )
        {
            next_address_convert_ipv4_to_ipv6( &to );
        }

        sockaddr_in6 socket_address;
        memset( &socket_address, 0, sizeof( socket_address ) );
        socket_address.sin6_family = AF_INET6;
        for ( int i = 0; i < 8; ++i )
        {
            ( (uint16_t*) &socket_address.sin6_addr ) [i] = next_platform_htons( to.data.ipv6[i] );
        }
        socket_address.sin6_port = next_platform_htons( to.port );
        int result = int( sendto( socket->handle, (char*)( packet_data ), packet_bytes, 0, (sockaddr*)( &socket_address ), sizeof(sockaddr_in6) ) );
        if ( result < 0 )
        {
            char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_address_to_string( &to, address_string );
            next_printf( NEXT_LOG_LEVEL_DEBUG, "sendto (%s) failed: %s", address_string, strerror( errno ) );
        }
    }
    else
    {
        // socket is ipv4

        if ( to.type == NEXT_ADDRESS_IPV4 )
        {
            sockaddr_in socket_address;
            memset( &socket_address, 0, sizeof( socket_address ) );
            socket_address.sin_family = AF_INET;
            socket_address.sin_addr.s_addr = ( ( (uint32_t) to.data.ipv4[0] ) )        | 
                                             ( ( (uint32_t) to.data.ipv4[1] ) << 8 )   | 
                                             ( ( (uint32_t) to.data.ipv4[2] ) << 16 )  | 
                                             ( ( (uint32_t) to.data.ipv4[3] ) << 24 );
            socket_address.sin_port = next_platform_htons( to.port );
            int result = int( sendto( socket->handle, (const char*)( packet_data ), packet_bytes, 0, (sockaddr*)( &socket_address ), sizeof(sockaddr_in) ) );
            if ( result < 0 )
            {
                char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
                next_address_to_string( &to, address_string );
                next_printf( NEXT_LOG_LEVEL_DEBUG, "sendto (%s) failed: %s", address_string, strerror( errno ) );
            }
        }
        else
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "invalid address. could not send packet" );
        }
    }
}

int next_platform_socket_receive_packet( next_platform_socket_t * socket, next_address_t * from, void * packet_data, int max_packet_size )
{
    next_assert( socket );
    next_assert( from );
    next_assert( packet_data );
    next_assert( max_packet_size > 0 );

    sockaddr_storage sockaddr_from;
    socklen_t from_length = sizeof( sockaddr_from );

    int result = int( recvfrom( socket->handle, (char*) packet_data, max_packet_size, 0, (sockaddr*) &sockaddr_from, &from_length ) );

    if ( result <= 0 )
    {
        if ( errno == EAGAIN || errno == EINTR )
        {
            return 0;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "recvfrom failed with error %d", errno );
        
        return 0;
    }

    if ( sockaddr_from.ss_family == AF_INET6 )
    {
        sockaddr_in6 * addr_ipv6 = (sockaddr_in6*) &sockaddr_from;
        from->type = NEXT_ADDRESS_IPV6;
        for ( int i = 0; i < 8; ++i )
        {
            from->data.ipv6[i] = next_platform_ntohs( ( (uint16_t*) &addr_ipv6->sin6_addr ) [i] );
        }
        from->port = next_platform_ntohs( addr_ipv6->sin6_port );

        if ( socket->ipv6 && next_address_is_ipv4_in_ipv6( from ) )
        {
            next_address_convert_ipv6_to_ipv4( from );
        }
    }
    else if ( sockaddr_from.ss_family == AF_INET )
    {
        sockaddr_in * addr_ipv4 = (sockaddr_in*) &sockaddr_from;
        from->type = NEXT_ADDRESS_IPV4;
        from->data.ipv4[0] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x000000FF ) );
        from->data.ipv4[1] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x0000FF00 ) >> 8 );
        from->data.ipv4[2] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x00FF0000 ) >> 16 );
        from->data.ipv4[3] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0xFF000000 ) >> 24 );
        from->port = next_platform_ntohs( addr_ipv4->sin_port );
    }
    else
    {
        next_assert( 0 );
        return 0;
    }
  
    next_assert( result >= 0 );

    return result;
}

// ---------------------------------------------------

struct thread_shim_data_t
{
    void * context;
    void * real_thread_data;
    next_platform_thread_func_t real_thread_function;
};

static void * thread_function_shim( void * data )
{
    next_assert( data );
    thread_shim_data_t * shim_data = (thread_shim_data_t*) data;
    void * context = shim_data->context;
    void * real_thread_data = shim_data->real_thread_data;
    next_platform_thread_func_t real_thread_function = shim_data->real_thread_function;
    next_free( context, data );
    real_thread_function( real_thread_data );
    return NULL;
}

next_platform_thread_t * next_platform_thread_create( void * context, next_platform_thread_func_t thread_function, void * arg )
{
    next_platform_thread_t * thread = (next_platform_thread_t*) next_malloc( context, sizeof( next_platform_thread_t) );

    next_assert( thread );

    thread->context = context;

    thread_shim_data_t * shim_data = (thread_shim_data_t*) next_malloc( context, sizeof(thread_shim_data_t) );
    next_assert( shim_data );
    if ( !shim_data )
    {
        next_free( context, thread );
        return NULL;
    }
    shim_data->context = context;
    shim_data->real_thread_function = thread_function;
    shim_data->real_thread_data = arg;

    if ( pthread_create( &thread->handle, NULL, thread_function_shim, shim_data ) != 0 )
    {
        next_free( context, thread );
        next_free( context, shim_data );
        return NULL;
    }

    return thread;
}

void next_platform_thread_join( next_platform_thread_t * thread )
{
    next_assert( thread );
    pthread_join( thread->handle, NULL );
}

void next_platform_thread_destroy( next_platform_thread_t * thread )
{
    next_assert( thread );
    next_free( thread->context, thread );
}

void next_platform_client_thread_priority( next_platform_thread_t * thread )
{
    // IMPORTANT: If you are developing for mac or ios you can adjust the affinity and priority for the client thread here.
    struct sched_param param;
    param.sched_priority = sched_get_priority_min( SCHED_RR );
    pthread_setschedparam( thread->handle, SCHED_RR, &param );
}

void next_platform_server_thread_priority( next_platform_thread_t * thread )
{
    // IMPORTANT: If you are developing for mac or ios you can adjust the affinity and priority for the server thread here.
    struct sched_param param;
    param.sched_priority = sched_get_priority_min( SCHED_RR );
    pthread_setschedparam( thread->handle, SCHED_RR, &param );
}

// ---------------------------------------------------

int next_platform_mutex_create( next_platform_mutex_t * mutex )
{
    next_assert( mutex );

    memset( mutex, 0, sizeof(next_platform_mutex_t) );

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype( &attr, 0 );
    int result = pthread_mutex_init( &mutex->handle, &attr );
    pthread_mutexattr_destroy( &attr );

    if ( result != 0 )
    {
        return NEXT_ERROR;
    }

    mutex->ok = true;

    return NEXT_OK;
}

void next_platform_mutex_acquire( next_platform_mutex_t * mutex )
{
    next_assert( mutex );
    next_assert( mutex->ok );
    pthread_mutex_lock( &mutex->handle );
}

void next_platform_mutex_release( next_platform_mutex_t * mutex )
{
    next_assert( mutex );
    next_assert( mutex->ok );
    pthread_mutex_unlock( &mutex->handle );
}

void next_platform_mutex_destroy( next_platform_mutex_t * mutex )
{
    next_assert( mutex );
    if ( mutex->ok )
    {
        pthread_mutex_destroy( &mutex->handle );
        memset( mutex, 0, sizeof(next_platform_mutex_t) );
    }
}

// ---------------------------------------------------

bool next_platform_packet_tagging_can_be_enabled()
{
    return true;
}

// ---------------------------------------------------

#else // #if NEXT_PLATFORM == NEXT_PLATFORM_MAC

int next_platform_mac_dummy_symbol = 0;

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_MAC

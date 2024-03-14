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

#include "next_platform_linux.h"

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX

#include "next_platform.h"
#include "next_address.h"

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
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <alloca.h>

// ---------------------------------------------------

static int connection_type = NEXT_CONNECTION_TYPE_UNKNOWN;

static int get_connection_type();

static double time_start;

int next_platform_init()
{
    timespec ts;
    clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
    time_start = ts.tv_sec + ( (double) ( ts.tv_nsec ) ) / 1000000000.0;
    connection_type = get_connection_type();
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

int next_platform_id()
{
    return NEXT_PLATFORM_LINUX;
}

int next_platform_connection_type()
{
    return connection_type;
}

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

double next_platform_time()
{
    timespec ts;
    clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
    double current = ts.tv_sec + ( (double) ( ts.tv_nsec ) ) / 1000000000.0;
    return current - time_start;
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

    // create socket

    socket->type = socket_type;

    socket->ipv6 = address->type == NEXT_ADDRESS_IPV6;

    socket->handle = ::socket( ( address->type == NEXT_ADDRESS_IPV6 ) ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if ( socket->handle < 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to create socket" );
        next_free( context, socket );
        return NULL;
    }

    // enable dual stack for ipv6 sockets

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
        tv.tv_usec = (int) ( timeout_seconds * 1000000.0f );
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
        int val = IPV6_PMTUDISC_DO;
        setsockopt( socket->handle, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &val, sizeof(val) );
    }
    else
    {
        int val = IP_PMTUDISC_DO;
        setsockopt( socket->handle, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) );
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

void next_platform_socket_send_packet( next_platform_socket_t * socket, const next_address_t * to_input, const void * packet_data, int packet_bytes )
{
    next_assert( socket );
    next_assert( to_input );
    next_assert( to_input->type == NEXT_ADDRESS_IPV6 || to_input->type == NEXT_ADDRESS_IPV4 );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    next_address_t to = *to_input;

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

    int result = int( recvfrom( socket->handle, (char*) packet_data, max_packet_size, socket->type == NEXT_PLATFORM_SOCKET_NON_BLOCKING ? MSG_DONTWAIT : 0, (sockaddr*) &sockaddr_from, &from_length ) );

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

static void* thread_function_shim( void * data )
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
    // IMPORTANT: If you have linux clients, you can adjust the priority of the client thread here.
    struct sched_param param;
    param.sched_priority = sched_get_priority_min( SCHED_RR );
    pthread_setschedparam( thread->handle, SCHED_RR, &param );
}

void next_platform_server_thread_priority( next_platform_thread_t * thread )
{
    // IMPORTANT: If you have linux servers, you can adjust the priority of the server thread here.
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
    pthread_mutexattr_init( &attr );
    pthread_mutexattr_settype( &attr, 0 );
    int result = pthread_mutex_init( &mutex->handle, &attr );
    pthread_mutexattr_destroy( &attr );

    if ( result != 0 )
        return NEXT_ERROR;

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

extern void * next_global_context;

template <typename T> struct next_vector_t
{
    T * data;
    int length;
    int reserved;

    inline next_vector_t( int reserve_count = 0 )
    {
        next_assert( reserve_count >= 0 );
        data = 0;
        length = 0;
        reserved = 0;
        if ( reserve_count > 0 )
        {
            reserve( reserve_count );
        }
    }

    inline ~next_vector_t()
    {
        clear();
    }

    inline void clear() 
    {
        if ( data )
        {
            next_free( next_global_context, data );
        }
        data = NULL;
        length = 0;
        reserved = 0;
    }

    inline const T & operator [] ( int i ) const
    {
        next_assert( data );
        next_assert( i >= 0 && i < length );
        return *( data + i );
    }

    inline T & operator [] ( int i )
    {
        next_assert( data );
        next_assert( i >= 0 && i < length );
        return *( data + i );
    }

    void reserve( int size )
    {
        next_assert( size >= 0 );
        if ( size > reserved )
        {
            const double VECTOR_GROWTH_FACTOR = 1.5;
            const int VECTOR_INITIAL_RESERVATION = 1;
            unsigned int next_size = (unsigned int)( pow( VECTOR_GROWTH_FACTOR, int( log( double( size ) ) / log( double( VECTOR_GROWTH_FACTOR ) ) ) + 1 ) );
            if ( !reserved )
            {
                next_size = next_size > VECTOR_INITIAL_RESERVATION ? next_size : VECTOR_INITIAL_RESERVATION;
                data = (T*)( next_malloc( next_global_context, next_size * sizeof(T) ) );
                next_assert( data );
            }
            else
            {
                T * new_data = (T*)( next_malloc( next_global_context, next_size * sizeof(T) ) );
                next_assert( data );
                memcpy( new_data, data, reserved * sizeof(T) );
                next_free( next_global_context, data );
                data = new_data;
            }
            memset( (void*)( &data[reserved] ), 0, ( next_size - reserved ) * sizeof(T) );
            reserved = next_size;
        }
    }

    void resize( int i )
    {
        reserve( i );
        length = i;
    }

    void remove( int i )
    {
        next_assert( data );
        next_assert( i >= 0 && i < length );
        if ( i != length - 1 )
        {
            data[i] = data[length - 1];
        }
        length--;
    }

    void remove_ordered( int i )
    {
        next_assert( data );
        next_assert( i >= 0 && i < length );
        memmove( &data[i], &data[i + 1], sizeof( T ) * ( length - ( i + 1 ) ) );
        length--;
    }

    T * insert( int i )
    {
        next_assert( i >= 0 && i <= length );
        resize( length + 1 );
        memmove( &data[i + 1], &data[i], sizeof( T ) * ( length - 1 - i ) );
        return &data[i];
    }

    T * insert( int i, const T & t )
    {
        T * p = insert( i );
        *p = t;
        return p;
    }

    T * add()
    {
        reserve( ++length );
        return &data[length - 1];
    }

    T * add( const T & t )
    {
        T * p = add();
        *p = t;
        return p;
    }
};


// ---------------------------------------------------

struct next_iftable_t
{
    uint32_t if_index;
    int connection_type;
};

struct next_ifforward4_t
{
    uint32_t if_index;
    uint32_t forward_dest;
    uint32_t forward_mask;
    uint32_t forward_metric;
};

static int parse_kernel_route( struct nlmsghdr * msg_ptr, next_ifforward4_t * out )
{
    struct rtmsg * route_entry = (struct rtmsg *)( NLMSG_DATA( msg_ptr ) );

    if ( route_entry->rtm_table != RT_TABLE_MAIN )
        return NEXT_ERROR;

    out->forward_mask = ( 1 << route_entry->rtm_dst_len ) - 1;

    // read all attributes
    int route_attribute_len = RTM_PAYLOAD( msg_ptr );
    for ( struct rtattr * route_attribute = (struct rtattr *)( RTM_RTA( route_entry ) );
        RTA_OK( route_attribute, route_attribute_len );
        route_attribute = RTA_NEXT( route_attribute, route_attribute_len ) )
    {
        switch ( route_attribute->rta_type )
        {
            case RTA_DST:
            {
                memcpy( &out->forward_dest, RTA_DATA( route_attribute ), sizeof( out->forward_dest ) );
                break;
            }
            case RTA_OIF:
            {
                memcpy( &out->if_index, RTA_DATA( route_attribute ), sizeof( out->if_index ) );
                break;
            }
            case RTA_METRICS:
            {
                memcpy( &out->forward_metric, RTA_DATA( route_attribute ), sizeof( out->forward_metric ) );
                break;
            }
        }
    }

    return NEXT_OK;
}

#define CHECK_BIT( _num, _bit ) ( ( _num ) & ( uint32_t(1) << ( _bit ) ) )

static bool better_match( next_ifforward4_t * a, next_ifforward4_t * b, uint32_t ip )
{
    int matching_bits_a = 0;
    int matching_bits_b = 0;

    if ( a )
    {
        for ( int i = 0; i < 32; i++ )
        {
            if ( CHECK_BIT( a->forward_mask, i )
                && CHECK_BIT( a->forward_dest, i ) == CHECK_BIT( ip, i ) )
            {
                matching_bits_a++;
            }
        }
    }

    if ( b )
    {
        for ( int i = 0; i < 32; i++ )
        {
            if ( CHECK_BIT( b->forward_mask, i )
                && CHECK_BIT( b->forward_dest, i ) == CHECK_BIT( ip, i ) )
            {
                matching_bits_b++;
            }
        }
    }
    
    return matching_bits_a > matching_bits_b;
}

static int classify_connection_type( next_vector_t<next_ifforward4_t> * route_table, next_vector_t<next_iftable_t> * interface_list, const next_address_t * addr )
{
    uint32_t ip = ( ( (uint32_t) addr->data.ipv4[0] ) )        | 
                  ( ( (uint32_t) addr->data.ipv4[1] ) << 8 )   | 
                  ( ( (uint32_t) addr->data.ipv4[2] ) << 16 )  | 
                  ( ( (uint32_t) addr->data.ipv4[3] ) << 24 );
    next_ifforward4_t * best_match = NULL;
    for ( int i = 0; i < route_table->length; i++ )
    {
        next_ifforward4_t * route = &( ( *route_table )[i] );
        if ( better_match( route, best_match, ip ) )
        {
            best_match = route;
        }
    }

    next_iftable_t * outgoing_interface = NULL;

    if ( best_match )
    {
        for ( int i = 0; i < interface_list->length; i++ )
        {
            next_iftable_t * iface = &( (* interface_list )[i] );
            if ( iface->if_index == best_match->if_index )
            {
                outgoing_interface = iface;
                break;
            }
        }
    }

    if ( outgoing_interface )
    {
        return outgoing_interface->connection_type;
    }
    else
    {
        return NEXT_CONNECTION_TYPE_WIRED;
    }
}

static int get_connection_type()
{
    next_vector_t<next_iftable_t> interface_list;
    next_vector_t<next_ifforward4_t> route_table;

    next_printf( NEXT_LOG_LEVEL_DEBUG, "getting network info" );

    // get interfaces
    {
        struct ifaddrs * ifaddr;

        if ( getifaddrs( &ifaddr ) == -1 )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "failed to get network interface addresses" );
            return NEXT_CONNECTION_TYPE_UNKNOWN;
        }

        int sock = -1;

        if ( ( sock = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
        {
            freeifaddrs( ifaddr );
            next_printf( NEXT_LOG_LEVEL_WARN, "failed to get network interface addresses" );
            return NEXT_CONNECTION_TYPE_UNKNOWN;
        }

        for ( struct ifaddrs * i = ifaddr; i != NULL; i = i->ifa_next )
        {
            struct ifreq req;
            memset( &req, 0, sizeof( req ) );
            strncpy( req.ifr_name, i->ifa_name, IFNAMSIZ - 1 );

            if ( ioctl( sock, SIOCGIFINDEX, &req ) == -1 )
            {
                continue;
            }

            uint32_t if_index = req.ifr_ifindex;

            // check for duplicates
            bool duplicate = false;
            for ( int j = 0; j < interface_list.length; j++ )
            {
                if ( interface_list[j].if_index == if_index )
                {
                    duplicate = true;
                    break;
                }
            }
            if ( duplicate )
            {
                continue;
            }

            next_iftable_t * interface_entry = interface_list.add();
            interface_entry->if_index = if_index;

            struct iwreq pwrq;
            memset( &pwrq, 0, sizeof( pwrq ) );
            strncpy( pwrq.ifr_name, i->ifa_name, IFNAMSIZ - 1 );

            if ( ioctl( sock, SIOCGIWNAME, &pwrq ) == -1 )
            {
                interface_entry->connection_type = NEXT_CONNECTION_TYPE_WIRED;
            }
            else
            {
                interface_entry->connection_type = NEXT_CONNECTION_TYPE_WIFI;
            }
        }

        close( sock );

        freeifaddrs( ifaddr );
    }

    // get kernel route table
    {
        // open netlink socket
        
        pid_t pid = getpid();

        int sock = socket( AF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
        if ( sock == -1 )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "failed to open netlink socket" );
            return NEXT_CONNECTION_TYPE_UNKNOWN;
        }

        struct sockaddr_nl local;
        memset( &local, 0, sizeof( local ) );
        local.nl_family = AF_NETLINK;
        local.nl_pid = pid;
        local.nl_groups = 0;

        if ( bind( sock, (struct sockaddr *)( &local ), sizeof( local ) ) < 0 )
        {
            next_printf( NEXT_LOG_LEVEL_WARN, "failed to bind netlink socket" );
            return NEXT_CONNECTION_TYPE_UNKNOWN;
        }

        // prepare route table request
        typedef struct nl_req_s nl_req_t;  
        struct nl_req_s
        {
            struct nlmsghdr hdr;
            struct rtgenmsg gen;
        };

        nl_req_t req;
        memset( &req, 0, sizeof( req ) );
        req.hdr.nlmsg_len = NLMSG_LENGTH( sizeof( struct rtgenmsg ) );
        req.hdr.nlmsg_type = RTM_GETROUTE;
        req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP; 
        req.hdr.nlmsg_seq = 1;
        req.hdr.nlmsg_pid = pid;
        req.gen.rtgen_family = AF_INET; 

        struct iovec io;
        io.iov_base = &req;
        io.iov_len = req.hdr.nlmsg_len;

        struct sockaddr_nl kernel;
        memset( &kernel, 0, sizeof( kernel ) );
        kernel.nl_family = AF_NETLINK;
        kernel.nl_groups = 0;

        struct msghdr rtnl_msg;
        memset( &rtnl_msg, 0, sizeof( rtnl_msg ) );
        rtnl_msg.msg_iov = &io;
        rtnl_msg.msg_iovlen = 1;
        rtnl_msg.msg_name = &kernel;
        rtnl_msg.msg_namelen = sizeof( kernel );

        // send request
        sendmsg( sock, (struct msghdr *)( &rtnl_msg ), 0 );

        // read reply

        bool done = false;
        while ( !done )
        {
            struct msghdr rtnl_reply;
            struct iovec io_reply;

            memset( &io_reply, 0, sizeof( io_reply ) );
            memset( &rtnl_reply, 0, sizeof( rtnl_reply ) );

            const size_t IFLIST_REPLY_BUFFER = 8192;
            char reply[IFLIST_REPLY_BUFFER];
            io_reply.iov_base = reply;
            io_reply.iov_len = IFLIST_REPLY_BUFFER;
            rtnl_reply.msg_iov = &io_reply;
            rtnl_reply.msg_iovlen = 1;
            rtnl_reply.msg_name = &kernel;
            rtnl_reply.msg_namelen = sizeof( kernel );

            int len = recvmsg( sock, &rtnl_reply, 0 );
            if ( len )
            {
                for ( struct nlmsghdr * msg_ptr = (struct nlmsghdr *)( reply ); NLMSG_OK( msg_ptr, len ); msg_ptr = NLMSG_NEXT( msg_ptr, len ) )
                {
                    if ( msg_ptr->nlmsg_type == 24 )
                    {
                        next_ifforward4_t route;
                        if ( parse_kernel_route( msg_ptr, &route ) == NEXT_OK )
                        {
                            route_table.add( route );
                        }
                    }
                    else if ( msg_ptr->nlmsg_type == 3 )
                    {
                        done = true;
                    }
                }
            }
        }

        close( sock );
    }

    next_address_t address;
    address.port = 80;
    address.data.ipv4[0] = 8;
    address.data.ipv4[1] = 8;
    address.data.ipv4[2] = 8;
    address.data.ipv4[3] = 8;

    return classify_connection_type( &route_table, &interface_list, &address );
}

// ---------------------------------------------------

bool next_platform_packet_tagging_can_be_enabled()
{
    return true;
}

// ---------------------------------------------------

#else // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX

int next_linux_dummy_symbol = 0;

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX

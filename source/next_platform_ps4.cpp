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

#include "next_platform_ps4.h"

#if NEXT_PLATFORM == NEXT_PLATFORM_PS4

#include "next_platform.h"
#include "next_address.h"

#include <kernel.h>
#include <net.h>
#include <libnetctl.h>
#include <libsecure.h>
#include <sce_random.h>
#include <libsysmodule.h>
#include <string.h>
#include "sodium.h"

#define HEAP_SIZE_NET (16 * 1024)

static int handle_net;

static int connection_type = NEXT_CONNECTION_TYPE_UNKNOWN;

static const char * next_randombytes_implementation_name()
{
    return "ps4";
}

static void next_randombytes_stir()
{
}

static void next_randombytes_buf(void* const buf, const size_t size_const)
{
    // IMPORTANT: sceRandomGetRandomNumber can only do max of SCE_RANDOM_MAX_SIZE bytes at a time. why god why.
    uint8_t* start = (uint8_t*)buf;
    uint8_t* finish = start + size_const;
    uint8_t* p = start;
    while (p < finish)
    {
        size_t remaining = size_t( finish - p );
        size_t size = (remaining >= SCE_RANDOM_MAX_SIZE) ? SCE_RANDOM_MAX_SIZE : remaining;
        sceRandomGetRandomNumber(p, size);
        p += size;
    }
}

static uint32_t next_randombytes_random()
{
    uint32_t random;
    next_randombytes_buf(&random, sizeof(random));
    return random;
}

static uint32_t next_randombytes_uniform(const uint32_t upper_bound)
{
    uint32_t mask = upper_bound - 1;

    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8; // mask is smallest ((power of 2) - 1) > upper_bound

    uint32_t result;
    do
    {
        result = mask & next_randombytes_random();  // 16-bit random number
    } while (result >= upper_bound);
    return result;
}

static int next_randombytes_close()
{
    return 0;
}

static randombytes_implementation next_random_implementation =
{
    &next_randombytes_implementation_name,
    &next_randombytes_random,
    &next_randombytes_stir,
    &next_randombytes_uniform,
    &next_randombytes_buf,
    &next_randombytes_close,
};

int next_platform_init()
{
    if ( sceSysmoduleLoadModule( SCE_SYSMODULE_RANDOM ) != SCE_OK ) 
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to load random sysmodule" );
        return NEXT_ERROR;
    }

    if ( randombytes_set_implementation( &next_random_implementation ) != 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set random bytes implementation" );
        return NEXT_ERROR;
    }

    if ( ( handle_net = sceNetPoolCreate("net", HEAP_SIZE_NET, 0 ) ) < 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to init network pool" );
        return NEXT_ERROR;
    }

    connection_type = NEXT_CONNECTION_TYPE_UNKNOWN;

    if ( sceNetCtlInit() != SCE_OK )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "failed to init netctl library" );
        return NEXT_OK;
    }

    SceNetCtlInfo info;
    if ( sceNetCtlGetInfo( SCE_NET_CTL_INFO_DEVICE, &info ) == SCE_OK )
    {
        switch ( info.device )
        {
            case SCE_NET_CTL_DEVICE_WIRED:
                connection_type = NEXT_CONNECTION_TYPE_WIRED;
                break;
            case SCE_NET_CTL_DEVICE_WIRELESS:
                connection_type = NEXT_CONNECTION_TYPE_WIFI;
                break;
        }
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "failed to determine network connection type" );
        return NEXT_OK;
    }

    return NEXT_OK;
}

void next_platform_term()
{
    sceNetPoolDestroy( handle_net );
}

int next_platform_connection_type()
{
    return connection_type;
}

const char * next_platform_getenv( const char * )
{
    return NULL; // not supported
}

// threads

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
    next_platform_thread_t * thread = (next_platform_thread_t *) next_malloc( context, sizeof( next_platform_thread_t ) );
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

    if ( scePthreadCreate( &thread->handle, NULL, thread_function_shim, shim_data, "next" ) != 0 )
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
    scePthreadJoin( thread->handle, NULL );
}

void next_platform_thread_destroy( next_platform_thread_t * thread )
{
    next_assert( thread );
    next_free( thread->context, thread );
}

void next_platform_client_thread_priority( next_platform_thread_t * thread )
{
    // IMPORTANT: If you are developing for PS4 you can adjust the affinity and priority for the client thread here.
    next_assert( thread );
    scePthreadSetprio( thread->handle, SCE_KERNEL_PRIO_FIFO_HIGHEST );
}

void next_platform_server_thread_priority( next_platform_thread_t * thread )
{
    (void) thread;
}

int next_platform_mutex_create( next_platform_mutex_t * mutex )
{
    next_assert( mutex );

    memset( mutex, 0, sizeof( next_platform_mutex_t) );

    ScePthreadMutexattr attr;
    scePthreadMutexattrInit(&attr);
    scePthreadMutexattrSettype( &attr, SCE_PTHREAD_MUTEX_RECURSIVE );
    bool success = scePthreadMutexInit( &mutex->handle, &attr, "next" ) == SCE_OK;
    scePthreadMutexattrDestroy( &attr );
    
    if ( !success )
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
    scePthreadMutexLock( &mutex->handle );
}

void next_platform_mutex_release( next_platform_mutex_t * mutex )
{
    next_assert( mutex );
    next_assert( mutex->ok );
    scePthreadMutexUnlock( &mutex->handle );
}

void next_platform_mutex_destroy( next_platform_mutex_t * mutex )
{
    next_assert( mutex );
    if ( mutex->ok )
    {
        scePthreadMutexDestroy( &mutex->handle );
        memset( mutex, 0, sizeof(next_platform_mutex_t) );
    }
}

// time

void next_platform_sleep( double time )
{
    sceKernelUsleep( time * 1000000.0 );
}

double next_platform_time()
{
    return double( sceKernelGetProcessTime() ) / 1000000.0;
}

// sockets

uint16_t next_platform_ntohs( uint16_t in )
{
    return (uint16_t)( ( ( in << 8 ) & 0xFF00 ) | ( ( in >> 8 ) & 0x00FF ) );
}

uint16_t next_platform_htons( uint16_t in )
{
    return (uint16_t)( ( ( in << 8 ) & 0xFF00 ) | ( ( in >> 8 ) & 0x00FF ) );
}

int next_platform_hostname_resolve( const char * hostname, const char * port, next_address_t * address )
{
    SceNetId resolver = sceNetResolverCreate( "resolver", handle_net, 0 );
    if ( resolver < 0 )
        return NEXT_ERROR;

    SceNetSockaddrIn addr;
    memset( &addr, 0, sizeof(addr) );
    addr.sin_len = sizeof(addr);
    addr.sin_family = SCE_NET_AF_INET;
    int result = sceNetResolverStartNtoa( resolver, hostname, &addr.sin_addr, 0, 0, 0 );
    sceNetResolverDestroy( resolver );

    if ( result < 0 )
        return NEXT_ERROR;

    if ( addr.sin_family != SCE_NET_AF_INET )
        return NEXT_ERROR;

    address->type = NEXT_ADDRESS_IPV4;
    address->data.ipv4[0] = (uint8_t)((addr.sin_addr.s_addr & 0x000000FF));
    address->data.ipv4[1] = (uint8_t)((addr.sin_addr.s_addr & 0x0000FF00) >> 8);
    address->data.ipv4[2] = (uint8_t)((addr.sin_addr.s_addr & 0x00FF0000) >> 16);
    address->data.ipv4[3] = (uint8_t)((addr.sin_addr.s_addr & 0xFF000000) >> 24);
    address->port = uint16_t( atoi( port ) );
    return NEXT_OK;
}

uint16_t next_platform_preferred_client_port()
{
    return 0;
}

bool next_platform_client_dual_stack()
{
    return false;
}

int next_platform_inet_pton4( const char * address_string, uint32_t * address_out )
{
    SceNetInAddr sockaddr4;
    bool success = sceNetInetPton( SCE_NET_AF_INET, address_string, &sockaddr4.s_addr ) == 1;
    *address_out = sockaddr4.s_addr;
    return success ? NEXT_OK : NEXT_ERROR;
}

int next_platform_inet_pton6( const char * address_string, uint16_t * address_out )
{
    return NEXT_ERROR;
}

int next_platform_inet_ntop6( const uint16_t * address, char * address_string, size_t address_string_size )
{
    return NEXT_ERROR;
}

void next_platform_socket_destroy( next_platform_socket_t * socket );

next_platform_socket_t * next_platform_socket_create( void * context, next_address_t * address, int socket_type, float timeout_seconds, int send_buffer_size, int receive_buffer_size )
{
    next_platform_socket_t * s = (next_platform_socket_t *) next_malloc( context, sizeof( next_platform_socket_t ) );
    next_assert( s );
    s->context = context;

    next_assert( address );
    next_assert( address->type == NEXT_ADDRESS_IPV4 );

    // create socket

    s->handle = sceNetSocket( "next", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, SCE_NET_IPPROTO_UDP );

    if ( s->handle < 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to create socket" );
        next_platform_socket_destroy( s );
        return NULL;
    }

    // increase socket send and receive buffer sizes

    if ( sceNetSetsockopt( s->handle, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDBUF, (char*)( &send_buffer_size ), sizeof( int ) ) != 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket send buffer size" );
        next_platform_socket_destroy( s );
        return NULL;
    }

    if ( sceNetSetsockopt( s->handle, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVBUF, (char*)( &receive_buffer_size ), sizeof( int ) ) != 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket receive buffer size" );
        next_platform_socket_destroy( s );
        return NULL;
    }

    // bind to port

    {
        SceNetSockaddrIn socket_address;
        memset( &socket_address, 0, sizeof( socket_address ) );
        socket_address.sin_family = SCE_NET_AF_INET;
        socket_address.sin_addr.s_addr = ( ( (uint32_t) address->data.ipv4[0] ) )      | 
                                         ( ( (uint32_t) address->data.ipv4[1] ) << 8 )  | 
                                         ( ( (uint32_t) address->data.ipv4[2] ) << 16 ) | 
                                         ( ( (uint32_t) address->data.ipv4[3] ) << 24 );
        socket_address.sin_port = next_platform_htons( address->port );

        if ( sceNetBind( s->handle, (SceNetSockaddr*) &socket_address, sizeof( socket_address ) ) < 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to bind socket (ipv4)" );
            next_platform_socket_destroy( s );
            return NULL;
        }
    }

    // if bound to port 0 find the actual port we got

    if ( address->port == 0 )
    {
        SceNetSockaddrIn sin;
        SceNetSocklen_t len = sizeof( sin );
        if ( sceNetGetsockname( s->handle, (SceNetSockaddr*)( &sin ), &len ) == -1 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to get socket port (ipv4)" );
            next_platform_socket_destroy( s );
            return NULL;
        }
        address->port = next_platform_ntohs( sin.sin_port );
    }

    // set non-blocking io

    s->type = socket_type;
    s->timeout_seconds = timeout_seconds;
    if ( socket_type == NEXT_PLATFORM_SOCKET_NON_BLOCKING )
    {
        int value = SCE_KERNEL_O_NONBLOCK;
        if ( sceNetSetsockopt( s->handle, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &value, sizeof( value ) ) < 0 )
        {
            next_platform_socket_destroy( s );
            return NULL;
        }
    }
    else if ( timeout_seconds > 0.0f )
    {
        s->timeout_seconds = timeout_seconds;
        // set receive timeout
        int tv = int( timeout_seconds * 1000000.0f );
        if ( sceNetSetsockopt( s->handle, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &tv, sizeof( tv ) ) < 0 )
        {
            next_platform_socket_destroy( s );
            return NULL;
        }
    }
    else
    {
        // timeout <= 0, socket is blocking with no timeout
    }

    // IMPORTANT: packet tagging is not currently supported on ps4
    (void) enable_packet_tagging;

    return s;
}

void next_platform_socket_destroy( next_platform_socket_t * socket )
{
    next_assert( socket );

    if ( socket->handle != 0 )
    {
        sceNetSocketClose( socket->handle );
        socket->handle = 0;
    }

    next_free( socket->context, socket );
}

void next_platform_socket_send_packet( next_platform_socket_t * socket, const next_address_t * to, const void * packet_data, int packet_bytes )
{
    next_assert( socket );
    next_assert( to );
    next_assert( to->type == NEXT_ADDRESS_IPV4 );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    if ( to->type == NEXT_ADDRESS_IPV4 )
    {
        SceNetSockaddrIn socket_address;
        memset( &socket_address, 0, sizeof( socket_address ) );
        socket_address.sin_family = SCE_NET_AF_INET;
        socket_address.sin_addr.s_addr = ( ( (uint32_t) to->data.ipv4[0] ) )        | 
                                         ( ( (uint32_t) to->data.ipv4[1] ) << 8 )   | 
                                         ( ( (uint32_t) to->data.ipv4[2] ) << 16 )  | 
                                         ( ( (uint32_t) to->data.ipv4[3] ) << 24 );
        socket_address.sin_port = next_platform_htons( to->port );
        int result = sceNetSendto( socket->handle, (const char*)( packet_data ), packet_bytes, 0, (SceNetSockaddr*)( &socket_address ), sizeof( SceNetSockaddrIn ) );
        if ( result < 0 )
        {
            char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_address_to_string( to, address_string );
            next_printf( NEXT_LOG_LEVEL_DEBUG, "sendto (%s) failed: %s", address_string, strerror( sce_net_errno ) );
        }
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "invalid address type. could not send packet" );
    }
}

int next_platform_socket_receive_packet( next_platform_socket_t * socket, next_address_t * from, void * packet_data, int max_packet_size )
{
    next_assert( socket );
    next_assert( from );
    next_assert( packet_data );
    next_assert( max_packet_size > 0 );

    SceNetSockaddrIn sockaddr_from;
    SceNetSocklen_t from_length = sizeof( sockaddr_from );

    int result = sceNetRecvfrom( socket->handle, (char*) packet_data, max_packet_size, socket->timeout_seconds == 0.0f && socket->type == NEXT_PLATFORM_SOCKET_NON_BLOCKING ? SCE_NET_MSG_DONTWAIT : 0, (SceNetSockaddr*) &sockaddr_from, &from_length );

    if ( result <= 0 )
    {
        if ( sce_net_errno == SCE_NET_EAGAIN || sce_net_errno == SCE_NET_EINTR )
        {
            return 0;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "recvfrom failed with error %d", sce_net_errno );
        
        return 0;
    }

    if ( sockaddr_from.sin_family == SCE_NET_AF_INET )
    {
        from->type = NEXT_ADDRESS_IPV4;
        from->data.ipv4[0] = (uint8_t) ( ( sockaddr_from.sin_addr.s_addr & 0x000000FF ) );
        from->data.ipv4[1] = (uint8_t) ( ( sockaddr_from.sin_addr.s_addr & 0x0000FF00 ) >> 8 );
        from->data.ipv4[2] = (uint8_t) ( ( sockaddr_from.sin_addr.s_addr & 0x00FF0000 ) >> 16 );
        from->data.ipv4[3] = (uint8_t) ( ( sockaddr_from.sin_addr.s_addr & 0xFF000000 ) >> 24 );
        from->port = next_platform_ntohs( sockaddr_from.sin_port );
    }
    else
    {
        next_assert( 0 );
        return 0;
    }
  
    next_assert( result >= 0 );

    return result;

}

int next_platform_id()
{
    return NEXT_PLATFORM_PS4;
}

#else // #if NEXT_PLATFORM == NEXT_PLATFORM_PS4

int next_ps4_dummy_symbol = 0;

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_PS4

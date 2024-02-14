
Complex Client Example
----------------------

In this example we build the kitchen sink version of a client where we show off all the features :)

We demonstrate:

- Setting the network next log level
- Setting a custom log function
- Setting a custom assert handler
- Setting a custom allocator
- Querying the port the client socket is bound to
- Getting statistics from the client

This is going to be a huge example, so let's get started!

First, as with the upgraded client example, we start by defining our key configuration variables:

.. code-block:: c++

	const char * bind_address = "0.0.0.0:0";
	const char * server_address = "127.0.0.1:50000";
	const char * buyer_public_key = "leN7D7+9vr24uT4f1Ba8PEEvIQA/UkGZLlT+sdeLRHKsVqaZq723Zw==";

Next, we dive right in and define a custom allocator class that tracks all allocations made, and checks them for leaks when it shuts down:

.. code-block:: c++

	struct AllocatorEntry
	{
	    // ...
	};

	class Allocator
	{
	    int64_t num_allocations;
	    next_platform_mutex_t mutex;
	    std::map<void*, AllocatorEntry*> entries;

	public:

	    Allocator()
	    {
	        int result = next_platform_mutex_create( &mutex );
	        next_assert( result == NEXT_OK );
	        num_allocations = 0;
	    }

	    ~Allocator()
	    {
	        next_platform_mutex_destroy( &mutex );
	        next_assert( num_allocations == 0 );
	        next_assert( entries.size() == 0 );
	    }

	    void * Alloc( size_t size )
	    {
	        next_platform_mutex_guard( &mutex );
	        void * pointer = malloc( size );
	        next_assert( pointer );
	        next_assert( entries[pointer] == NULL );
	        AllocatorEntry * entry = new AllocatorEntry();
	        entries[pointer] = entry;
	        num_allocations++;
	        return pointer;
	    }

	    void Free( void * pointer )
	    {
	        next_platform_mutex_guard( &mutex );
	        next_assert( pointer );
	        next_assert( num_allocations > 0 );
	        std::map<void*, AllocatorEntry*>::iterator itor = entries.find( pointer );
	        next_assert( itor != entries.end() );
	        entries.erase( itor );
	        num_allocations--;
	        free( pointer );
	    }
	};

IMPORTANT: Since this allocator will be called from multiple threads, it must be thread safe. This is done by using the platform independent mutex supplied by the Network Next SDK.

There are three types of allocations done by the Network Next SDK:

1. Global allocations
2. Per-client allocations
3. Per-server allocations

Each of these situations corresponds to what is called a "context" in the Network Next SDK. 

A context is simply a void* to a type that you define which is passed in to malloc and free callbacks that we call to perform allocations on behalf of the SDK. The context passed is gives you the flexibility to have a specific memory pool for Network Next (most common), or even to have a completely different allocation pool used for each client and server instance. That's what we're going to do in this example.

Let's define a base context that will be used for global allocations:

.. code-block:: c++

	struct Context
	{
	    Allocator * allocator;
	};

And a per-client context that is binary compatible with the base context, to be used for per-client allocations:

.. code-block:: c++

	struct ClientContext
	{
	    Allocator * allocator;
	    uint32_t client_data;
	};

The client context can contain additional information aside from the allocator. The context is not *just* passed into allocator callbacks, but to all callbacks from the client and server, so you can use it to integrate with your own client and server objects in your game. 

Here we just put a dummy uint32_t in the client context and check its value to verify it's being passed through correctly. For example, in the received packet callback, we have access to the client context and check the dummy value is what we expect:

.. code-block:: c++

	void client_packet_received( next_client_t * client, void * _context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
	{
	    (void) client;
	    (void) from;

	    ClientContext * context = (ClientContext*) _context;

	    next_assert( context );
	    next_assert( context->allocator != NULL );
	    next_assert( context->client_data == 0x12345 );

	    next_printf( NEXT_LOG_LEVEL_INFO, "client received packet from server (%d bytes)", packet_bytes );

	    verify_packet( packet_data, packet_bytes );
	}

Next we define custom malloc and free functions to pass in to the SDK. These same functions are used for global, per-client and per-server allocations. The only difference is the context passed in to each.

.. code-block:: c++

	void * malloc_function( void * _context, size_t bytes )
	{
	    Context * context = (Context*) _context;
	    next_assert( context );
	    next_assert( context->allocator );
	    return context->allocator->Alloc( bytes );
	}

	void free_function( void * _context, void * p )
	{
	    Context * context = (Context*) _context;
	    next_assert( context );
	    next_assert( context->allocator );
	    return context->allocator->Free( p );
	}

Moving past allocations for the moment, we set up a callback for our own custom logging function:

.. code-block:: c++

	extern const char * log_level_string( int level )
	{
	    if ( level == NEXT_LOG_LEVEL_DEBUG )
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

	void log_function( int level, const char * format, ... ) 
	{
	    va_list args;
	    va_start( args, format );
	    char buffer[1024];
	    vsnprintf( buffer, sizeof( buffer ), format, args );
	    if ( level != NEXT_LOG_LEVEL_NONE )
	    {
	        const char * level_string = log_level_string( level );
	        printf( "%.2f: %s: %s\n", next_platform_time(), level_string, buffer );
	    }
	    else
	    {
	        printf( "%s\n", buffer );
	    }
	    va_end( args );
	    fflush( stdout );
	}

There are four different log levels in Network Next:

1. NEXT_LOG_LEVEL_NONE (0)
2. NEXT_LOG_LEVEL_ERROR (1)
3. NEXT_LOG_LEVEL_INFO (2)
4. NEXT_LOG_LEVEL_WARN (3)
5. NEXT_LOG_LEVEL_DEBUG (4)

The default log level is NEXT_LOG_LEVEL_INFO, which shows both info and error logs. This is a good default, as these messages are infrequent. Warnings can be more frequent, and aren't important enough to be errors, so are off by default. Debug logs are incredibly spammy and should only be turned on when debugging a specific issue in the Network Next SDK.

How you handle each of these log levels in the log function callback is up to you. We just pass them in, but depending on the log level we will not call the callback unless the level of the log is <= the current log level value set.

Finally, there is a small feature where a log with NEXT_LOG_LEVEL_NONE is used to indicate an unadorned regular printf. This is useful for console platforms like XBoxOne where hoops need to be jumped through just to get text printed to stdout. This is used by our unit tests and by the default assert handler function in the Network Next SDK.

Next we define a custom assert handler:

.. code-block:: c++

	void assert_function( const char * condition, const char * function, const char * file, int line )
	{
	    next_printf( NEXT_LOG_LEVEL_NONE, "assert failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
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

Here we print out the assert message and force a break. Again, typically you would override this to point to your own assert handler in your game. The code above actually corresponds exactly to our default handler, so you can see what we do if you choose to not override it.

Now instead of sending zero byte packets, let's send some packets with real intent.

.. code-block:: c++

	void generate_packet( uint8_t * packet_data, int & packet_bytes )
	{
	    packet_bytes = 1 + ( rand() % NEXT_MTU );
	    const int start = packet_bytes % 256;
	    for ( int i = 0; i < packet_bytes; ++i )
	        packet_data[i] = (uint8_t) ( start + i ) % 256;
	}

	void verify_packet( const uint8_t * packet_data, int packet_bytes )
	{
	    const int start = packet_bytes % 256;
	    for ( int i = 0; i < packet_bytes; ++i )
	    {
	        if ( packet_data[i] != (uint8_t) ( ( start + i ) % 256 ) )
	        {
	            printf( "%d: %d != %d (%d)\n", i, packet_data[i], ( start + i ) % 256, packet_bytes );
	        }
	        next_assert( packet_data[i] == (uint8_t) ( ( start + i ) % 256 ) );
	    }
	}

The functions above generate packets of random length from 1 to the maximum size packet that can be sent across Network Next -- NEXT_MTU (1300 bytes). These packets have contents that can be inferred by the size of the packet, making it possible for us to test a packet and with high probability, ensure that the packet has not been incorrectly truncated or padded, and that it contains the exact bytes sent.

Now we are ready to set a custom log level, set our custom log function, allocators and assert handler. 

Before initializing Network Next, do this:

.. code-block:: c++

    next_log_level( NEXT_LOG_LEVEL_INFO );

    next_log_function( log_function );

    next_assert_function( assert_function );

    next_allocator( malloc_function, free_function );

Next, create a global context and pass it in to *next_init* to be used for any global allocations made by the Network Next SDK:

.. code-block:: c++

    Context global_context;
    global_context.allocator = &global_allocator;

    next_config_t config;
    next_default_config( &config );
    strncpy( config.buyer_public_key, buyer_public_key, sizeof(config.buyer_public_key) - 1 );

Now when the Network Next SDK makes any global allocations, they will be made by calling the malloc_function and free_function callbacks, passing in the global context pointer as a void*.

Next, create a per-client context and pass it in as the context when creating the client:

.. code-block:: c++

    Allocator client_allocator;
    ClientContext client_context;
    client_context.allocator = &client_allocator;
    client_context.client_data = 0x12345;

    next_client_t * client = next_client_create( &client_context, bind_address, client_packet_received );
    if ( client == NULL )
    {
        printf( "error: failed to create client\n" );
        return 1;
    }

Now when the client makes any allocations, and when it calls callbacks like *packet_received* it will pass in the client context as a void*.

Since we are binding the client to port 0, the system will choose the actual port number. We can retrieve this port number as follows and print it out for posterity:

.. code-block:: c++

	uint16_t client_port = next_client_port( client );

	next_printf( NEXT_LOG_LEVEL_INFO, "client port is %d", client_port );

Finally, the client has been extended to print out all the useful stats you can retrieve from a network next client, once every ten seconds:

.. code-block:: c++

	accumulator += delta_time;

	if ( accumulator > 10.0 )
	{
	    accumulator = 0.0;

	    printf( "================================================================\n" );
	    
	    printf( "Client Stats:\n" );

	    const next_client_stats_t * stats = next_client_stats( client );

	    const char * platform = "unknown";

	    switch ( stats->platform_id )
	    {
	        case NEXT_PLATFORM_WINDOWS:
	            platform = "windows";
	            break;

	        case NEXT_PLATFORM_MAC:
	            platform = "mac";
	            break;

	        case NEXT_PLATFORM_LINUX:
	            platform = "linux";
	            break;

	        case NEXT_PLATFORM_SWITCH:
	            platform = "nintendo switch";
	            break;

	        case NEXT_PLATFORM_PS4:
	            platform = "ps4";
	            break;

	        case NEXT_PLATFORM_PS5:
	            platform = "ps5";
	            break;

	        case NEXT_PLATFORM_IOS:
	            platform = "ios";
	            break;

	        case NEXT_PLATFORM_XBOX_ONE:
	            platform = "xbox one";
	            break;

	        case NEXT_PLATFORM_XBOX_SERIES_X:
	            platform = "xbox series x";
	            break;

	        default:
	            break;
	    }

	    const char * state = "???";

	    const int client_state = next_client_state( client );
	    
	    switch ( client_state )
	    {
	        case NEXT_CLIENT_STATE_CLOSED:
	            state = "closed";
	            break;

	        case NEXT_CLIENT_STATE_OPEN:
	            state = "open";
	            break;

	        case NEXT_CLIENT_STATE_ERROR:
	            state = "error";
	            break;

	        default:
	            break;
	    }

	    printf( " + State = %s (%d)\n", state, client_state );

	    printf( " + Session Id = %" PRIx64 "\n", next_client_session_id( client ) );

	    printf( " + Platform = %s (%d)\n", platform, (int) stats->platform_id );

	    const char * connection = "unknown";
	    
	    switch ( stats->connection_type )
	    {
	        case NEXT_CONNECTION_TYPE_WIRED:
	            connection = "wired";
	            break;

	        case NEXT_CONNECTION_TYPE_WIFI:
	            connection = "wifi";
	            break;

	        case NEXT_CONNECTION_TYPE_CELLULAR:
	            connection = "cellular";
	            break;

	        default:
	            break;
	    }

	    printf( " + Connection = %s (%d)\n", connection, stats->connection_type );

	    printf( " + Multipath = %s\n", stats->multipath ? "yes" : "no" );

	    printf( " + Flagged = %s\n", stats->flagged ? "yes" : "no" );

	    printf( " + Direct RTT = %.2fms\n", stats->direct_min_rtt );
	    printf( " + Direct Jitter = %.2fms\n", stats->direct_jitter );
	    printf( " + Direct Packet Loss = %.1f%%\n", stats->direct_packet_loss );

	    if ( stats->next )
	    {
	        printf( " + Next RTT = %.2fms\n", stats->next_min_rtt );
	        printf( " + Next Jitter = %.2fms\n", stats->next_jitter );
	        printf( " + Next Packet Loss = %.1f%%\n", stats->next_packet_loss );
	        printf( " + Next Bandwidth Up = %.1fkbps\n", stats->next_kbps_up );
	        printf( " + Next Bandwidth Down = %.1fkbps\n", stats->next_kbps_down );
	    }

	    printf( "================================================================\n" );
	}

Thanks to Jacob Langworthy and the whole crew at Velan Studios for inspiring this example!

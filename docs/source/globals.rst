
globals
=======

next_config_t
-------------

Configuration struct for the Network Next SDK.

.. code-block:: c++

	struct next_config_t
	{
	    char hostname[256];
	    char buyer_public_key[256];
	    char buyer_private_key[256];
	    int socket_send_buffer_size;
	    int socket_receive_buffer_size;
	    bool disable_network_next;
	    bool disable_autodetect;
	};

**hostname** - The hostname for the backend the Network Next SDK is talking to. Set to "server.virtualgo.net" by default.

**buyer_public_key** - The buyer public key as a base64 encoded string.

**buyer_private_key** - The buyer private key as a base64 encoded string.

**socket_send_buffer_size** - The size of the socket send buffer in bytes.

**socket_receive_buffer_size** - The size of the socket receive buffer in bytes.

**disable_network_next** - Set this to true to disable Network Next entirely and always send packets across the public internet.

**disable_autodetect*** - Set this to true to disable autodetect datacenter from running. In this case the datacenter string passed in is always used as is.

next_default_config
-------------------

Sets default configuration values.

.. code-block:: c++

	void next_default_config( next_config_t * config );

Use this to set default values for config variables, then make only the changes you want on top.

- **hostname** -- "server.virtualgo.net"
- **buyer_public_key** -- ""
- **buyer_private_key** -- ""
- **socket_send_buffer_size** -- 1000000
- **socket_receive_buffer_size** -- 1000000
- **disable_network_next** -- false
- **disable_autodetect** -- false

**Example:**

.. code-block:: c++

	next_config_t config;
	next_default_config( &config );
	printf( "default hostname is %s\n", config.hostname );

next_init
---------

Initializes the Network Next SDK.

.. code-block:: c++

	int next_init( void * context, next_config_t * config );

Call this before creating a client or server.

**Parameters:**

	- **context** -- An optional pointer to context passed to overridden malloc and free for global allocations.
	- **config** -- An optional pointer to the network next configuration. NULL means to use the default configuration.

**Return value:**

	NEXT_OK if the Network Next SDK initialized successfully, NEXT_ERROR otherwise.

**Example:**

.. code-block:: c++

	next_init( NULL, NULL );

next_term
---------

Shuts down the Network Next SDK.

.. code-block:: c++

	void next_term();

Call this before you shut down your application.

**Example:**

.. code-block:: c++

	next_term();

next_platform_time
------------------

Gets the current time in seconds.

.. code-block:: c++

	double next_platform_time();

IMPORTANT: Only defined when called after *next_init*.

**Return value:**

	The time in seconds since *next_init* was called.

**Example:**

.. code-block:: c++

	next_init( NULL, NULL );

	// .. do stuff ...

	printf( "%.2f seconds since next_init\n", next_platform_time() );

next_platform_sleep
-------------------

Sleep for some amount of time.

.. code-block:: c++

	void next_platform_sleep( double time_seconds );

**Parameters:**

	- **time_seconds** -- The length of time to sleep in seconds.

**Example:**

.. code-block:: c++

	next_init( NULL, NULL );

	const double start_time = next_platform_time();

	next_platform_sleep( 10.0 );

	const double finish_time = next_platform_time();

	printf( "slept for %.2f seconds\n", finish_time - start_time );

next_printf
-----------

Log level aware printf.

.. code-block:: c++

	void next_printf( int level, const char * format, ... );

Log levels:

- NEXT_LOG_LEVEL_NONE (0)
- NEXT_LOG_LEVEL_ERROR (1)
- NEXT_LOG_LEVEL_INFO (2)
- NEXT_LOG_LEVEL_WARN (3)
- NEXT_LOG_LEVEL_DEBUG (4)

**Parameters:**

	- **level** -- Log level. Only logs <= the current log level are printed.

next_assert
-----------

Assert.

.. code-block:: c++

	void next_assert( bool condition );

**Example:**

.. code-block:: c++

	next_assert( true != false );

next_quiet
----------

Enable/disable network next logs entirely.

.. code-block:: c++

	void next_quiet( bool flag );

**Example:**

.. code-block:: c++

	// shut up network next!
	next_quiet( true );

next_log_level
--------------

Sets the Network Next log level.

.. code-block:: c++

	void next_log_level( int level );

Log levels:

- NEXT_LOG_LEVEL_NONE (0)
- NEXT_LOG_LEVEL_ERROR (1)
- NEXT_LOG_LEVEL_INFO (2)
- NEXT_LOG_LEVEL_WARN (3)
- NEXT_LOG_LEVEL_DEBUG (4)

The default log level is info. This includes both info messages and errors, which are both infrequent.

**Example:**

.. code-block:: c++

	// unleash the kraken!
	next_log_level( NEXT_LOG_LEVEL_DEBUG );

next_log_function
-----------------

Sets a custom log function.

.. code-block:: c++

	void next_log_function( void (*function)( int level, const char * format, ... ) );

**Example:**

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

	int main()
	{
	    next_log_function( log_function );

	    next_init( NULL, NULL );

	    next_printf( NEXT_LOG_LEVEL_INFO, "Hi, Mum!" );

	    next_term();

	    return 0;
	}

next_assert_function
--------------------

Set a custom assert handler.

.. code-block:: c++

	void next_assert_function( void (*function)( const char * condition, const char * function, const char * file, int line ) );

**Example:**

.. code-block:: c++

	void assert_function( const char * condition, const char * function, const char * file, int line )
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

next_allocator
--------------

Sets a custom allocator.

.. code-block:: c++

	void next_allocator( void * (*malloc_function)( void * context, size_t bytes ), void (*free_function)( void * context, void * p ) );

**Example:**

.. code-block:: c++

	void * malloc_function( void * context, size_t bytes )
	{
	    return malloc( bytes );
	}

	void free_function( void * context, void * p )
	{
	    return free( p );
	}

	int main()
	{
	    next_allocator( malloc_function, free_function );

	    next_init( NULL, NULL );

	    // ... do stuff ...

	    next_term();

	    return 0;
	}

next_user_id_string
-------------------

Converts a legacy uint64_t user id to a string.

.. code-block:: c++

	const char * next_user_id_string( uint64_t user_id, char * buffer );

Used to migrate from old uint64_t user ids to the new string based ids.

**Example:**

.. code-block:: c++

	char buffer[256];
	next_server_upgrade_session( server, client_address, next_user_id_string( user_id, buffer ) );

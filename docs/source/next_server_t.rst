
next_server_t
=============

The server side of a client/server connection.

To use a server, create it and it automatically starts accepting sessions from clients.

To upgrade a session for monitoring and *potential* acceleration, call *next_server_upgrade_session* on a client's address.

Packets received from clients are passed to you via a callback function, including the address of the client that sent the packet.

Make sure to pump the server update once per frame.

**Examples:**

-   :doc:`simple_server_example`
-   :doc:`upgraded_server_example`
-   :doc:`complex_server_example`

next_server_create
------------------

Creates an instance of a server, binding a socket to the specified address and port.

.. code-block:: c++

	next_server_t * next_server_create( void * context, 
	                                    const char * server_address, 
	                                    const char * bind_address, 
	                                    const char * datacenter, 
	                                    void (*packet_received_callback)( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes ) );

**Parameters:**

	- **context** -- An optional pointer to context passed to any callbacks made from the server.

	- **server_address** -- The public IP address and port that clients will connect to.

	- **bind_address** -- The address and port to bind to. Typically "0.0.0.0:[portnum]" is passed in, binding the server socket to any IPv4 interface on a specific port, for example: "0.0.0.0:50000".

	- **datacenter** -- The name of the datacenter that the game server is running in. Please pass in "local" until we work with you to determine the set of datacenters you host servers in.

	- **packet_received_callback** -- Called from the same thread that calls *next_server_update*, whenever a packet is received from a client. Required.

**Return value:** 

	The newly created server instance, or NULL, if the server could not be created. 

	Typically, NULL is only returned when another socket is already bound on the same port, or if an invalid server or bind address is passed in.

**Example:**

First define a callback for received packets:

.. code-block:: c++

	void server_packet_received( next_server_t * server, void * _context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
	{
	    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
	    next_printf( NEXT_LOG_LEVEL_INFO, "server received packet from client %s (%d bytes)", next_address_to_string( from, address_buffer ), packet_bytes );
	}

Then, create a server:

.. code-block:: c++

    next_server_t * server = next_server_create( NULL, "127.0.0.1", "0.0.0.0:50000", "local", server_packet_received );
    if ( server == NULL )
    {
        printf( "error: failed to create server\n" );
        return 1;
    }

next_server_destroy
-------------------

Destroys a server instance.

Make sure to call next_server_flush before destroying the server.

.. code-block:: c++

	void next_server_destroy( next_server_t * server );

**Parameters:**

	- **server** -- The server instance to destroy. Must be a valid server instance created by *next_server_create*. Do not pass in NULL.

**Example:**

.. code-block:: c++

	next_server_destroy( server );

next_server_port
----------------

Gets the port the server socket is bound to.

.. code-block:: c++

	uint16_t next_server_port( next_server_t * server );

**Parameters:**

	- **server** -- The server instance.

**Return value:** 

	The port number the server socket is bound to.

**Example:**

.. code-block:: c++

    next_server_t * server = next_server_create( NULL, "127.0.0.1", "0.0.0.0:50000", "local", server_packet_received );
    if ( server == NULL )
    {
        printf( "error: failed to create server\n" );
        return 1;
    }

    const uint16_t server_port = next_server_port( server );

    printf( "the server is bound to port %d\n", server_port );

next_server_address
-------------------

Gets the address of the server instance.

.. code-block:: c++

	next_address_t next_server_address( next_server_t * server )

**Parameters:**

	- **server** -- The server instance.

**Return value:** 

	The address of the server.

**Example:**

.. code-block:: c++

    next_server_t * server = next_server_create( NULL, "127.0.0.1:50000", "0.0.0.0:50000", "local", server_packet_received );
    if ( server == NULL )
    {
        printf( "error: failed to create server\n" );
        return 1;
    }

    next_address_t server_address = next_server_address( server );

    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
    printf( "the server address is %s\n", next_address_to_string( &server_address, address_buffer ) );

next_server_state
-----------------

Gets the state the server is in.

.. code-block:: c++

	int next_server_state( next_server_t * server );

**Parameters:**

	- **server** -- The server instance.

**Return value:** 

	The server state, which is one of the following:
	
		- NEXT_SERVER_STATE_DIRECT_ONLY
		- NEXT_SERVER_STATE_RESOLVING_HOSTNAME
		- NEXT_SERVER_STATE_INITIALIZING
		- NEXT_SERVER_STATE_INITIALIZED

	The server is initially in the direct only state. 

	If a valid buyer private key is setup, the server will first try to resolve the backend hostname.

	Once the backend hostname is resolved, the server initializes with the backend. When everything works, the server lands in the initialized state and is ready to accelerate players.

	If anything fails, the server falls back to the direct only state, and only serves up direct routes over the public internet.

**Example:**

.. code-block:: c++

    const char * state = "???";

    const int server_state = next_server_state( server );
    
    switch ( server_state )
    {
        case NEXT_SERVER_STATE_DIRECT_ONLY:
            state = "direct only";
            break;

        case NEXT_SERVER_STATE_RESOLVING_HOSTNAME:
            state = "resolving hostname";
            break;

        case NEXT_SERVER_STATE_INITIALIZING:
            state = "initializing";
            break;

        case NEXT_SERVER_STATE_INITIALIZED:
            state = "initialized";
            break;

        default:
            break;
    }

    printf( "server state = %s (%d)\n", state, server_state );

next_server_upgrade_session
---------------------------

Upgrades a session for monitoring and *potential* acceleration by Network Next.

.. code-block:: c++

	uint64_t next_server_upgrade_session( next_server_t * server, 
	                                      const next_address_t * address, 
	                                      const char * user_id );

IMPORTANT: Make sure you only call this function when you are 100% sure this is a real player in your game.

**Parameters:**

	- **server** -- The server instance.

	- **address** -- The address of the client to be upgraded.

	- **user_id** -- The user id for the session. Pass in any unique per-user identifier you have.

**Return value:**

	The session id assigned the session that was upgraded.

**Example:**

The address struct is defined as follows:

.. code-block:: c++

	struct next_address_t
	{
	    union { uint8_t ipv4[4]; uint16_t ipv6[8]; } data;
	    uint16_t port;
	    uint8_t type;
	};

You can parse an address from a string like this:

.. code-block:: c++

	next_address_t address;
	if ( next_address_parse( &address, "127.0.0.1:50000" ) != NEXT_OK )
	{
	    printf( "error: failed to parse address\n" );
	}

The address struct is passed in when you receive packet from a client:

.. code-block:: c++

	void server_packet_received( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
	{
	    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
	    next_printf( NEXT_LOG_LEVEL_INFO, "server received packet from client %s (%d bytes)", next_address_to_string( from, address_buffer ), packet_bytes );
	}

Once you have the address, upgrading a session is easy:

.. code-block:: c++

	next_server_upgrade_session( server, client_address, user_id );

next_server_session_upgraded
----------------------------

Checks if a session has been upgraded.

.. code-block:: c++

	bool next_server_session_upgraded( next_server_t * server, const next_address_t * address );

**Parameters:**

	- **server** -- The server instance.

	- **address** -- The address of the client to check.

**Return value:**

	True if the session has been upgraded, false otherwise.

**Example:**

.. code-block:: c++

	const bool upgraded = next_server_session_upgraded( server, client_address );

	printf( "session upgraded = %s\n", upgraded ? "true" : "false" );

next_server_send_packet
-----------------------

Send a packet to a client.

.. code-block:: c++

	void next_server_send_packet( next_server_t * server, const next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

Sends a packet to a client. If the client is upgraded and accelerated by network next, the packet will be sent across our private network of networks.

Otherwise, the packet will be sent across the public internet.

**Parameters:**

	- **server** -- The server instance.

	- **to_address** -- The address of the client to send the packet to.

	- **packet_data** -- The packet data to send.

	- **packet_bytes** -- The size of the packet. Must be in the range 1 to NEXT_MTU (1300).

**Example:**

.. code-block:: c++

	uint8_t packet_data[32];
	memset( packet_data, 0, sizeof(packet_data) );
	next_server_send_packet( server, client_address, packet_data, sizeof(packet_data) );

next_server_send_packet_direct
------------------------------

Send a packet to a client, forcing the packet to be sent over the public internet.

.. code-block:: c++

	void next_server_send_packet_direct( next_server_t * server, const next_address_t * to_address, const uint8_t * packet_data, int packet_bytes );

This function is useful when you need to send non-latency sensitive packets to the client, for example, during a load screen.

Packets sent via this function do not apply to your network next bandwidth envelope.

**Parameters:**

	- **server** -- The server instance.

	- **to_address** -- The address of the client to send the packet to.

	- **packet_data** -- The packet data to send.

	- **packet_bytes** -- The size of the packet. Must be in the range 1 to NEXT_MTU (1300).

**Example:**

.. code-block:: c++

	uint8_t packet_data[32];
	memset( packet_data, 0, sizeof(packet_data) );
	next_server_send_packet_direct( server, client_address, packet_data, sizeof(packet_data) );

next_server_stats
-----------------

Gets statistics for a specific client address.

.. code-block:: c++

	bool next_server_stats( struct next_server_t * server, const struct next_address_t * address, struct next_server_stats_t * stats );

**Parameters:**

	- **server** -- The server instance.

	- **next_address_t** -- The address of the client to get statistics for.

	- **next_server_stats_t** -- The pointer to the server stats struct to fill.

**Return value:**

	True if a session exists for the given IP address, false otherwise.

**Example**

The server stats struct is defined as follows:

.. code-block:: c++

	struct next_server_stats_t
	{
	    struct next_address_t address;
	    uint64_t session_id;
	    uint64_t user_hash;
	    int platform_id;
	    int connection_type;
	    bool next;
	    bool multipath;
	    bool reported;
	    bool fallback_to_direct;
	    float direct_min_rtt;
	    float direct_max_rtt;
	    float direct_prime_rtt;
	    float direct_jitter;
	    float direct_packet_loss;
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

Here is how to query it, and print out various interesting values:

.. code-block:: c++

	next_server_stats_t stats;
	if ( !next_server_stats( server, client_address, &stats ) )
	{
	    printf( "server does not contain a session for provided address" );
	    return;
	}
	
	char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
	printf( "address = %s\n", next_address_to_string( client_address, address_buffer ) );

	const char * platform = "unknown";

	switch ( stats.platform_id )
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

	printf( "session_id = %" PRIx64 "\n", stats.session_id );

	printf( "platform_id = %s (%d)\n", platform, (int) stats.platform_id );

	const char * connection = "unknown";
	
	switch ( stats.connection_type )
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

	printf( "connection_type = %s (%d)\n", connection, stats.connection_type );

	if ( !stats.fallback_to_direct )
	{
	    printf( "multipath = %s\n", stats.multipath ? "true" : "false" );
	    printf( "reported = %s\n", stats.reported ? "true" : "false" );
	}

	printf( "fallback_to_direct = %s\n", stats.fallback_to_direct ? "true" : "false" );

	printf( "direct_min_rtt = %.2fms\n", stats.direct_min_rtt );
	printf( "direct_max_rtt = %.2fms\n", stats.direct_max_rtt );
	printf( "direct_prime_rtt = %.2fms\n", stats.direct_prime_rtt );
	printf( "direct_jitter = %.2fms\n", stats.direct_jitter );
	printf( "direct_packet_loss = %.1f%%\n", stats.direct_packet_loss );

	if ( stats.next )
	{
	    printf( "next_rtt = %.2fms\n", stats.next_rtt );
	    printf( "next_jitter = %.2fms\n", stats.next_jitter );
	    printf( "next_packet_loss = %.1f%%\n", stats.next_packet_loss );
	    printf( "next_bandwidth_up = %.1fkbps\n", stats.next_kbps_up );
	    printf( "next_bandwidth_down = %.1fkbps\n", stats.next_kbps_down );
	}

	if ( !stats.fallback_to_direct )
	{
	    printf( "packets_sent_client_to_server = %" PRId64 "\n", stats.packets_sent_client_to_server );
	    printf( "packets_sent_server_to_client = %" PRId64 "\n", stats.packets_sent_server_to_client );
	    printf( "packets_lost_client_to_server = %" PRId64 "\n", stats.packets_lost_client_to_server );
	    printf( "packets_lost_server_to_client = %" PRId64 "\n", stats.packets_lost_server_to_client );
	    printf( "packets_out_of_order_client_to_server = %" PRId64 "\n", stats.packets_out_of_order_client_to_server );
	    printf( "packets_out_of_order_server_to_client = %" PRId64 "\n", stats.packets_out_of_order_server_to_client );
	    printf( "jitter_client_to_server = %f\n", stats.jitter_client_to_server );
	    printf( "jitter_server_to_client = %f\n", stats.jitter_server_to_client );
	}

next_server_ready
-----------------

Wait until this function returns true, before sending clients to connect to your server.

This function return true once server has finished DNS resolve of the Network Next backend IP address, and has completed autodetection of the datacenter when the server is hosted in Google Cloud or AWS, or managed by Multiplay.

.. code-block:: c++

	bool next_server_ready( struct next_server_t * server );

**Parameters:**

	- **server** -- The server instance.

**Return value:**

	True if the server is ready to receive client connections, false otherwise.

**Example:**

.. code-block:: c++

	const bool ready = next_server_ready( server );

	printf( "server is ready = %s\n", ready ? "true" : "false" );

next_server_datacenter
----------------------

Call this once next_server_ready returns true to get the autodetected datacenter name.

.. code-block:: c++

	const char * next_server_datacenter( struct next_server_t * server );

**Parameters:**

	- **server** -- The server instance.

**Return value:**

	The name of the autodetected datacenter.

**Example:**

.. code-block:: c++

	const bool ready = next_server_ready( server );

	if ( ready )
	{
		const char * datacenter = next_server_datacenter( server );
		printf( "server datacenter is %s\n", datacenter );
	}

next_server_session_event
-------------------------

Triggers a user-defined event on a session. This event is stored alongside network performance data once every 10 seconds.

You can define up to 64 event flags for your game, one event per bit in the *server_events* bitfield.

Use this function to input in-game events that may be relevant to analytics.

.. code-block:: c++

	void next_server_session_event( struct next_server_t * server, const struct next_address_t * address, uint64_t session_events );

**Parameters:**

	- **server** -- The server instance.

	- **address** -- The address of the client that triggered the event.

	- **session_events** -- Bitfield of events that just triggered for the session.

**Example:**

.. code-block:: c++

	enum GameEvents
	{
		GAME_EVENT_RESPAWNED = (1<<0),
		GAME_EVENT_CATCH = (1<<1),
		GAME_EVENT_THROW = (1<<2),
		GAME_EVENT_KNOCKED_OUT = (1<<3),
		GAME_EVENT_WON_MATCH = (1<<4),
		GAME_EVENT_LOST_MATCH = (1<<5),
		// ...
	};

	next_server_session_event( server, client_address, GAME_EVENT_KNOCKED_OUT | GAME_EVENT_LOST_MATCH );

next_server_flush
-----------------

Call this to flush all server data before shutting a server down.

.. code-block:: c++

	void next_server_flush( struct next_server_t * server );

This function blocks for up to 10 seconds to ensure that all session is recorded.

After calling this function, destroy the server via *next_server_destroy*.

**Parameters:**

	- **server** -- The server instance.

**Example:**

.. code-block:: c++

	next_server_flush( server );
	next_server_destroy( server );

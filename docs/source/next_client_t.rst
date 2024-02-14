
next_client_t
=============

The client side of a client/server connection.

To use a client, create it, open a session to the server address and start sending packets.

Packets received from the server are passed to you via a callback function.

Make sure you pump the client update once per frame.

**Examples:**

-   :doc:`simple_client_example`
-   :doc:`upgraded_client_example`
-   :doc:`complex_client_example`

next_client_create
------------------

Creates an instance of a client, binding a socket to the specified address and port.

.. code-block:: c++

	next_client_t * next_client_create( void * context, 
	                                    const char * bind_address, 
	                                    void (*packet_received_callback)( next_client_t * client, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes ) );

**Parameters:**

	- **context** -- An optional pointer to context passed to any callbacks made from the client.

	- **bind_address** -- An address string describing the bind address and port to bind to. Typically "0.0.0.0:0" is passed in, which binds to any IPv4 interface and lets the system pick a port. Alternatively, you can bind to a specific port, for example: "0.0.0.0:50000".

	- **packet_received_callback** -- Called from the same thread that calls *next_client_update*, whenever a packet is received from the server. Required.

**Return value:** 

	The newly created client instance, or NULL, if the client could not be created. 

	Typically, NULL is only returned when another socket is already bound on the same port, or if an invalid bind address is passed in.

**Example:**

First define a callback for received packets:

.. code-block:: c++

	void client_packet_received( next_client_t * client, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
	{
	    printf( "client received packet from server (%d bytes)\n", packet_bytes );
	}

Then, create a client:

.. code-block:: c++

    next_client_t * client = next_client_create( NULL, "0.0.0.0:0", client_packet_received );
    if ( client == NULL )
    {
        printf( "error: failed to create client\n" );
        return 1;
    }

next_client_destroy
-------------------

Destroys a client instance, and the socket it manages internally.

.. code-block:: c++

	void next_client_destroy( next_client_t * client );

**Parameters:**

	- **client** -- The client instance to destroy. Must be a valid client instance created by *next_client_create*. Do not pass in NULL.

**Example:**

.. code-block:: c++

	next_client_destroy( client );

next_client_port
----------------

Gets the port the client socket is bound to.

.. code-block:: c++

	uint16_t next_client_port( next_client_t * client );

**Return value:** 

	The port number the client socket is bound to.

	This makes it possible to look up what specific port the client is bound to when you bind to port zero and the system chooses a port.

**Example:**

.. code-block:: c++

    next_client_t * client = next_client_create( NULL, "0.0.0.0:0", client_packet_received );
    if ( client == NULL )
    {
        printf( "error: failed to create client\n" );
        return 1;
    }

    const uint16_t client_port = next_client_port( client );

    printf( "the client was bound to port %d\n", client_port );

next_client_open_session
------------------------

Opens a session between the client and a server.

.. code-block:: c++

	void next_client_open_session( next_client_t * client,
	                               const char * server_address )

**Parameters:**

	- **client** -- The client instance.

	- **server_address** -- The address of the server that the client wants to connect to.

**Example:**

.. code-block:: c++

	next_client_open_session( client, "127.0.0.1:50000" );

next_client_close_session
-------------------------

Closes the session between the client and server.

.. code-block:: c++

	void next_client_close_session( next_client_t * client )

**Parameters:**

	- **client** -- The client instance.

**Example:**

.. code-block:: c++

	next_client_close_session( client );

next_client_is_session_open
---------------------------

Check if the client has a session open.

.. code-block:: c++

	bool next_client_is_session_open( next_client_t * client );

**Parameters:**

	- **client** -- The client instance.

**Return value:** 

	True, if the client has an open session with a server, false otherwise.

**Example:**

.. code-block:: c++

    const bool session_open = next_client_session_open( client );

    printf( "session open = %s\n", session_open ? "yes" : "no" );

next_client_state
-----------------

Gets the state the client is in.

.. code-block:: c++

	int next_client_state( next_client_t * client );

**Parameters:**

	- **client** -- The client instance.

**Return value:** 

	The client state is either:

		- NEXT_CLIENT_STATE_CLOSED
		- NEXT_CLIENT_STATE_OPEN
		- NEXT_CLIENT_STATE_ERROR

	The client is initially in closed state. After *next_client_open_session* the client is immediately in open state on success, or error state if something went wrong while opening the session, for example, an invalid server address was passed in.

**Example:**

.. code-block:: c++

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

    printf( "client state = %s (%d)\n", state, client_state );

next_client_update
------------------

Updates the client.

.. code-block:: c++

	int next_client_update( next_client_t * client );

Please call this every frame as it drives the packet received callback.

**Parameters:**

	- **client** -- The client instance.

**Example:**

.. code-block:: c++

    while ( !quit )
    {
        next_client_update( client );

        // ... do stuff ...
        
        next_platform_sleep( 1.0 / 60.0 );
    }

next_client_send_packet
-----------------------

Sends a packet to the server.

.. code-block:: c++

	void next_client_send_packet( next_client_t * client, const uint8_t * packet_data, int packet_bytes );

Depending on whether this player is accelerated or not, this packet will be sent direct across the public internet, or through Network Next's network of private networks.

**Parameters:**

	- **client** -- The client instance.
	- **packet_data** -- The packet data to send to the server.
	- **packet_bytes** -- The size of the packet in bytes. Must be in range 1 to NEXT_MTU (1300).

**Example:**

.. code-block:: c++

    uint8_t packet_data[32];
    memset( packet_data, 0, sizeof( packet_data ) );

    while ( !quit )
    {
        next_client_update( client );

        next_client_send_packet( client, packet_data, sizeof(packet_data) );
        
        next_platform_sleep( 1.0 / 60.0 );
    }

next_client_send_packet_direct
------------------------------

Sends a packet to the server, forcing the packet to be sent across the public internet.

.. code-block:: c++

	void next_client_send_packet_direct( next_client_t * client, const uint8_t * packet_data, int packet_bytes );

The packet will be sent unaccelerated across the public internet and will not count towards your Network Next bandwidth envelope.

This can be very useful when you need to send a burst of non-latency sensitive packets, for example, in a load screen.

**Parameters:**

	- **client** -- The client instance.
	- **packet_data** -- The packet data to send to the server.
	- **packet_bytes** -- The size of the packet in bytes. Must be in range 1 to NEXT_MTU (1300).

Example:

.. code-block:: c++

    uint8_t packet_data[32];
    memset( packet_data, 0, sizeof( packet_data ) );

    while ( !quit )
    {
        next_client_update( client );

        next_client_send_packet_direct( client, packet_data, sizeof(packet_data) );
        
        next_platform_sleep( 1.0 / 60.0 );
    }

next_client_report_session
--------------------------

Report the session as problematic.

.. code-block:: c++

	void next_client_report_session( next_client_t * client );

This feature was added to support games who let players flag bad play sessions in their game UI.

Call this function when your players complain, and it's sent to our backend so we can help you track down why!

**Parameters:**

	- **client** -- The client instance.

**Example:**

.. code-block:: c++

    next_client_report_session( client );

next_client_session_id
----------------------

Gets the client session id.

.. code-block:: c++

	uint64_t next_client_session_id( next_client_t * client );

A session id uniquely identifies each session on Network Next.

Session ids are distinct from user ids. User ids are unique on a per-user basis, while session ids are unique for each call to *next_client_open_session*.

A session id is assigned when the server upgrades the session via *next_server_upgrade_session*. Until that point the session id is 0.

**Parameters:**

	- **client** -- The client instance.

**Return value:** 

	The session id, if the client has been upgraded, otherwise 0.

**Example:**

.. code-block:: c++

    const uint64_t session_id = next_client_session_id( client );

    printf( "session id = %" PRIx64 "\n", session_id );

next_client_stats
-----------------

Gets client statistics.

.. code-block:: c++

	const next_client_stats_t * next_client_stats( next_client_t * client );

**Parameters:**

	- **client** -- The client instance.

**Return value:** 

	A const pointer to the client stats struct.

**Example:**

The client stats struct is defined as follows:

.. code-block:: c++

	struct next_client_stats_t
	{
	    int platform_id;
	    int connection_type;
	    bool next;
	    bool upgraded;
	    bool multipath;
	    bool reported;
	    bool fallback_to_direct;
	    bool high_frequency_pings;
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

	const next_client_stats_t * stats = next_client_stats( client );

	printf( "Client Stats:\n" );

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

	if ( !stats->fallback_to_direct )
	{
	    printf( " + Upgraded = %s\n", stats->upgraded ? "true" : "false" );
	    printf( " + Multipath = %s\n", stats->multipath ? "true" : "false" );
	    printf( " + Reported = %s\n", stats->reported ? "true" : "false" );
	}

	printf( " + Fallback to Direct = %s\n", stats->fallback_to_direct ? "true" : "false" );

	printf( " + High Frequency Pings = %s\n", stats->high_frequency_pings ? "true" : "false" );

	printf( " + Direct Min RTT = %.2fms\n", stats->direct_min_rtt );
	printf( " + Direct Max RTT = %.2fms\n", stats->direct_max_rtt );
	printf( " + Direct Prime RTT = %.2fms\n", stats->direct_prime_rtt );
	printf( " + Direct Jitter = %.2fms\n", stats->direct_jitter );
	printf( " + Direct Packet Loss = %.1f%%\n", stats->direct_packet_loss );

	if ( stats->next )
	{
	    printf( " + Next RTT = %.2fms\n", stats->next_rtt );
	    printf( " + Next Jitter = %.2fms\n", stats->next_jitter );
	    printf( " + Next Packet Loss = %.1f%%\n", stats->next_packet_loss );
	    printf( " + Next Bandwidth Up = %.1fkbps\n", stats->next_kbps_up );
	    printf( " + Next Bandwidth Down = %.1fkbps\n", stats->next_kbps_down );
	}

	if ( stats->upgraded && !stats->fallback_to_direct )
	{
	    printf( " + Packets Sent Client to Server = %" PRId64 "\n", stats->packets_sent_client_to_server );
	    printf( " + Packets Sent Server to Client = %" PRId64 "\n", stats->packets_sent_server_to_client );
	    printf( " + Packets Lost Client to Server = %" PRId64 "\n", stats->packets_lost_client_to_server );
	    printf( " + Packets Lost Server to Client = %" PRId64 "\n", stats->packets_lost_server_to_client );
	    printf( " + Packets Out of Order Client to Server = %" PRId64 "\n", stats->packets_out_of_order_client_to_server );
	    printf( " + Packets Out of Order Server to Client = %" PRId64 "\n", stats->packets_out_of_order_server_to_client );
	    printf( " + Jitter Client to Server = %f\n", stats->jitter_client_to_server );
	    printf( " + Jitter Server to Client = %f\n", stats->jitter_server_to_client );
	}

next_client_server_address
--------------------------

Gets the address of the server that the client is communicating with.

.. code-block:: c++

	const next_address_t * next_client_server_address( next_client_t * client );

**Parameters:**

	- **client** -- The client instance.

**Return value:** 

	A const pointer to the client's server address.

**Example:**

.. code-block:: c++

	const next_address_t * server_address = next_client_server_address( client );

	char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
	printf( "the client is connected to %s\n", next_address_to_string( server_address, address_buffer ) );

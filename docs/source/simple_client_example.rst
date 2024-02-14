
Simple Client Example
---------------------

In this example we setup the simplest possible client using Network Next.

First, initialize the SDK:

.. code-block:: c++

	if ( next_init( NULL, NULL ) != NEXT_OK )
	{
	    printf( "error: could not initialize network next\n" );
	    return 1;
	}

Next, define a function to be called when packets are received:

.. code-block:: c++

	void client_packet_received( next_client_t * client, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
	{
	    next_printf( NEXT_LOG_LEVEL_INFO, "client received packet (%d bytes)", packet_bytes );
	}

Create the client.

.. code-block:: c++

    next_client_t * client = next_client_create( NULL, "0.0.0.0:0", client_packet_received );
    if ( client == NULL )
    {
        printf( "error: failed to create client\n" );
        return 1;
    }

In this case we bind the client to any IPv4 address and port zero, so the system selects a port to use.

Next, open a session between the client and the server:

.. code-block:: c++

	next_client_open_session( client, "127.0.0.1:50000" );

Now you can send packets to the server like this:

.. code-block:: c++

	uint8_t packet_data[32];
	memset( packet_data, 0, sizeof( packet_data ) );
	next_client_send_packet( client, packet_data, sizeof(packet_data) );

Make sure the client is updated once every frame:

.. code-block:: c++

	next_client_update( client );

When you have finished your session with the server, close it:

.. code-block:: c++

	next_client_close_session( client );

When you have finished using your client, destroy it:

.. code-block:: c++

	next_client_destroy( client );

Before your application terminates, please shut down the SDK:

.. code-block:: c++

	next_term();

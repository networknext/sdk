
Simple Server Example
---------------------

In this example we setup the simplest possible server using Network Next.

First, initialize the SDK:

.. code-block:: c++

	if ( next_init( NULL, NULL ) != NEXT_OK )
	{
	    printf( "error: could not initialize network next\n" );
	    return 1;
	}

Next, define a function to be called when packets are received from clients.

Here is one that reflects the packet back to the client that sent it:

.. code-block:: c++

	void server_packet_received( next_server_t * server, void * context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
	{
	    next_server_send_packet( server, from, packet_data, packet_bytes );
	}

Now create the server. 

In this example, we bind the server to port 50000 on 127.0.0.1 IPv4 address (localhost) and set the datacenter where your server is running to "local":

.. code-block:: c++

    next_server_t * server = next_server_create( NULL, "127.0.0.1:50000", "0.0.0.0:50000", "local", server_packet_received );
    if ( server == NULL )
    {
        printf( "error: failed to create server\n" );
        return 1;
    }

Make sure the server gets updated every frame:

.. code-block:: c++

	next_server_update( server );

When you have finished using your server, please flush and destroy it:

.. code-block:: c++

	next_server_flush( server);

	next_server_destroy( server );

Before your application terminates, please shut down the SDK:

.. code-block:: c++

	next_term();

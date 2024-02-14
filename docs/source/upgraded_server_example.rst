
Upgraded Server Example
-----------------------

In this example we setup a server for monitoring and acceleration by network next.

First define configuration values for the server:

.. code-block:: c++

	const char * bind_address = "0.0.0.0:50000";
	const char * server_address = "127.0.0.1:50000";
	const char * server_datacenter = "local";
	const char * backend_hostname = "server.virtualgo.net";
	const char * buyer_private_key = "leN7D7+9vr3TEZexVmvbYzdH1hbpwBvioc6y1c9Dhwr4ZaTkEWyX2Li5Ph/UFrw8QS8hAD9SQZkuVP6x14tEcqxWppmrvbdn";

This includes the test buyer private key we're using in this example. A buyer private key is required on the server to enable acceleration by Network Next.

Next, initialize a configuration struct to defaults, then copy the hostname and the buyer private key on top.

.. code-block:: c++

	next_config_t config;
	next_default_config( &config );
	strncpy( config.hostname, backend_hostname, sizeof(config.hostname) - 1 );
	strncpy( config.buyer_private_key, buyer_private_key, sizeof(config.buyer_private_key) - 1 );

	if ( next_init( NULL, &config ) != NEXT_OK )
	{
	    printf( "error: could not initialize network next\n" );
	    return 1;
	}

IMPORTANT: Generally speaking it's bad form to include a private key in your codebase like this, it's done here only to make this example easy to use. In production environments, we strongly recommend passing in "" for your buyer private key, and setting it via the environment variable: *NEXT_BUYER_PRIVATE_KEY* which overrides the value specified in code.

Next we initialize the SDK, this time passing in the configuration struct. 

.. code-block:: c++

	if ( next_init( NULL, &config ) != NEXT_OK )
	{
	    printf( "error: could not initialize network next\n" );
	    return 1;
	}

Now we define a function to be called when packets are received from clients.

Here is one that reflects the packet back to the client that sent it, and upgrades the client that sent the packet for monitoring and acceleration by Network Next.

.. code-block:: c++

	next_server_send_packet( server, from, packet_data, packet_bytes );

	next_printf( NEXT_LOG_LEVEL_INFO, "server received packet from client (%d bytes)", packet_bytes );

	if ( !next_server_session_upgraded( server, from ) )
	{
	    const char * user_id_string = "12345";
	    next_server_upgrade_session( server, from, user_id_string );
	}

Generally you would *not* want to upgrade every client session you receive a packet from. This is just done to make this example easy to implement.

Instead, you should only upgrade sessions that have passed whatever security and protocol level checks you have in your game so you are 100% confident this is a real player joining your game.

Also notice that you can pass in a user id as a string to the upgrade call.

.. code-block:: c++

	next_server_upgrade_session( server, from, user_id_string );

This user id is very important because it allows you to look up users by that id in our portal. Please make sure you set the user id to however you uniquely identify users in your game. For example, PSN ids, Steam Ids and so on. For privacy reasons, this user id is hashed before sending to our backend.

Now, create the server. 

.. code-block:: c++

    next_server_t * server = next_server_create( NULL, server_address, bind_address, server_datacenter, server_packet_received );
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

	next_server_flush( server );

	next_server_destroy( server );

Before your application terminates, please shut down the SDK:

.. code-block:: c++

	next_term();

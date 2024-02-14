
Complex Server Example
----------------------

In this example we build the kitchen sink version of a server where we show off all the features :)

We demonstrate:

- Setting the network next log level
- Setting a custom log function
- Setting a custom assert handler
- Setting a custom allocator

In this example, everything is as per the complex client example: setting up the allocator, a global context, override functions for malloc and free, custom log function and a custom assert function.

When creating a server, we create it with a server context as follows:

.. code-block:: c++

	Allocator server_allocator;
	ServerContext server_context;
	server_context.allocator = &server_allocator;
	server_context.server_data = 0x12345678;

	next_server_t * server = next_server_create( &server_context, server_address, bind_address, server_datacenter, server_packet_received );
	if ( server == NULL )
	{
	    printf( "error: failed to create server\n" );
	    return 1;
	}

The overridden malloc and free functions are now called with the server context including our custom allocator:

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

And the packet received callback with the server context, allowing you to get a pointer to your own internal server data structure passed in to the packet received callback:

.. code-block:: c++

	void server_packet_received( next_server_t * server, void * _context, const next_address_t * from, const uint8_t * packet_data, int packet_bytes )
	{
	    ServerContext * context = (ServerContext*) _context;

	    (void) context;

	    next_assert( context );
	    next_assert( context->allocator != NULL );
	    next_assert( context->server_data == 0x12345678 );

	    verify_packet( packet_data, packet_bytes );

	    next_server_send_packet( server, from, packet_data, packet_bytes );
	    
	    char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];

	    next_printf( NEXT_LOG_LEVEL_INFO, "server received packet from client %s (%d bytes)", next_address_to_string( from, address_buffer ), packet_bytes );

	    if ( !next_server_session_upgraded( server, from ) )
	    {
	        next_server_upgrade_session( server, from, NULL );
	    }
	}

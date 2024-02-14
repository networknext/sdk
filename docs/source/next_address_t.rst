
next_address_t
==============

This is a struct that can represent any IPv4 or IPv6 address and port.

.. code-block:: c++

	struct next_address_t
	{
	    union { uint8_t ipv4[4]; uint16_t ipv6[8]; } data;
	    uint16_t port;
	    uint8_t type;
	};

It's used when sending and receiving packets. For example, in the server packet received callback, the address of the client is passed to you via this structure.

next_address_parse
------------------

Parses an address from a string

.. code-block:: c++

	int next_address_parse( next_address_t * address, const char * address_string );

**Parameters:**

	- **address** -- The address struct to be initialized.

	- **address_string** -- An address string in IPv4 or IPv6 format.

**Return value:** 

	NEXT_OK if the address was parsed successfully, NEXT_ERROR otherwise.

**Example:**

.. code-block:: c++

	next_address_t address;

	next_address_parse( &address, "127.0.0.1" );

	next_address_parse( &address, "127.0.0.1:50000" );

	next_address_parse( &address, "::1" );

	next_address_parse( &address, "[::1]:50000" );

next_address_to_string
----------------------

Converts an address to a string

.. code-block:: c++

	const char * next_address_to_string( const next_address_t * address, char * buffer );

**Parameters:**

	- **address** -- The address to convert to a string
	- **buffer** -- The buffer used to store the string representation of the address. Must be at least NEXT_MAX_ADDRESS_STRING_LENGTH in size.

**Return value:** 

	A const char* to a string representation of the address. Makes it easy to printf.

**Example:**

.. code-block:: c++

	next_address_t address;
	next_address_parse( &address, "[::1]:50000" );
	
	char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
	printf( "address string = %s\n", next_address_string( &address, buffer ) );

next_address_equal
------------------

Checks if two addresses are equal

.. code-block:: c++

	bool next_address_equal( const next_address_t * a, const next_address_t * b );

**Parameters:**

	- **a** -- Pointer to the first address.
	- **b** -- Pointer to the second address.

**Return value:** 

	True if the addresses are equal, false otherwise.

**Example:**

.. code-block:: c++

	next_address_t a, b;
	
	next_address_parse( &a, "127.0.0.1" );
	
	next_address_parse( &b, "127.0.0.1:0" );

	const bool addresses_are_equal = next_address_equal( &a, &b );

	printf( "addresses are equal = %s\n", addresses_are_equal ? "yes" : "no" );

next_address_anonymize
----------------------

Anonymizes an address by zeroing the last tuple and port.

.. code-block:: c++

	void next_address_anonymize( next_address_t * address );

**Parameters:**

	- **address** -- Pointer to the address to anonymize.

**Example:**

.. code-block:: c++

	next_address_t address;
	
	next_address_parse( &address, "127.0.0.1:50000" );

	next_address_anonymize( &address );

	char address_buffer[NEXT_MAX_ADDRESS_STRING_LENGTH];
	printf( "the anonymized address is %s\n", next_address_to_string( address, address_buffer ) );

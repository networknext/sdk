
env
===

You can configure the Network Next SDK with environment variables.

NEXT_LOG_LEVEL
--------------

Overrides whatever log level is set at the point *next_init* is called.

Valid values:

 - **1** = NEXT_LOG_LEVEL_ERROR
 - **2** = NEXT_LOG_LEVEL_INFO (default)
 - **3** = NEXT_LOG_LEVEL_WARN
 - **4** = NEXT_LOG_LEVEL_DEBUG

**Example:**

.. code-block:: console

	$ export NEXT_LOG_LEVEL=4
	$ ./bin/simple_server &
	$ ./bin/simple_client
	(extreme spam follows...)

NEXT_BUYER_PUBLIC_KEY
---------------------

Overrides the public key set in *next_config_t*.

**Example:**

.. code-block:: console

	$ export NEXT_BUYER_PUBLIC_KEY=tTpIS31zg4tAdFHjPnMUFIhhsWjsR1u5Tj0Ygd7cC1OEk6gjBv6r4Q==

NEXT_BUYER_PRIVATE_KEY
----------------------

Overrides the private key set in *next_config_t*.

**Example:**

.. code-block:: console

	$ export NEXT_BUYER_PRIVATE_KEY=tTpIS31zg4tsrIKlNL6LAXVAQLZc0rzD5utQLcKgnpYuCC7PsoyPkEB0UeM+cxQUiGGxaOxHW7lOPRiB3twLU4STqCMG/qvh

NEXT_DISABLE_NETWORK_NEXT
-------------------------

Disables network next and send packets over the public internet.

**Example:**

.. code-block:: console

	$ export NEXT_DISABLE_NETWORK_NEXT=1

NEXT_DISABLE_AUTODETECT
-----------------------

Disables datacenter autodetection. The datacenter string passed in is always the datacenter used.

**Example:**

.. code-block:: console

	$ export NEXT_DISABLE_AUTODETECT=1

NEXT_SOCKET_SEND_BUFFER_SIZE
----------------------------

Overrides the socket send buffer size in *next_config_t*.

**Example:**

.. code-block:: console

	$ export NEXT_SEND_BUFFER_SIZE=500000

NEXT_SOCKET_RECEIVE_BUFFER_SIZE
-------------------------------

Overrides the socket receive buffer size in *next_config_t*.

**Example:**

.. code-block:: console

	$ export NEXT_RECEIVE_BUFFER_SIZE=500000

NEXT_SERVER_BACKEND_HOSTNAME
----------------------------

Overrides the server backend hostname in *next_config_t*.

**Example:**

.. code-block:: console

	$ export NEXT_SERVER_BACKEND_HOSTNAME=server.virtualgo.net

NEXT_SERVER_ADDRESS
-------------------

Overrides the server address passed in to *next_server_create*.

**Example:**

.. code-block:: console

	$ export NEXT_SERVER_ADDRESS=173.255.241.176:50000

NEXT_BIND_ADDRESS
-----------------

Overrides the bind address passed in to *next_server_create*.

**Example:**

.. code-block:: console

	$ export NEXT_BIND_ADDRESS=0.0.0.0:50000

NEXT_DATACENTER
---------------

Overrides the datacenter passed in to *next_server_create*.

**Example:**

.. code-block:: console

	$ export NEXT_DATACENTER=i3d.rotterdam

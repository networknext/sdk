
Reference
---------

To use the Network Next SDK, replace the socket on your client with *next_client_t* and the socket on your server with *next_server_t*.

The client and server objects provide an interface for sending and receiving packets, much like sendto and recvfrom, extend that they monitor each player's network connection, and steer packets across Network Next, when we find a route that meets your network optimization requirements.

If our system is down for any reason, the client and server simply fall back to sending packets over the public internet, without any disruption for your players.

.. toctree::
   :maxdepth: 1

   next_client_t
   next_server_t
   next_address_t
   globals
   env
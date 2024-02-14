
Building the SDK
================

To build the example programs, see the instructions for your platform below:

Windows
-------

Install *premake5* from https://premake.github.io/download.html

Generate a visual studio solution by running premake at the root directory of the SDK:

.. code-block:: console

    $ premake5 vs2019

Open the generated solution file under the "visualstudio" directory and build all.

Mac
---

Make sure the XCode command line tools are installed:

.. code-block:: console

	$ xcode-select --install

Install *premake5* from https://premake.github.io/download.html

Generate makefiles by running premake at the root directory of the SDK:

.. code-block:: console

    $ premake5 gmake

Build the SDK:

.. code-block:: console

	$ make -j

Run the unit tests:

.. code-block:: console

	$ ./bin/test

Linux
-----

Make sure the build essential package is installed:

.. code-block:: console

	$ sudo apt install build-essential

Install *premake5* from https://premake.github.io/download.html

Generate makefiles by running premake at the root directory of the SDK:

.. code-block:: console

    $ premake5 gmake

Build the SDK:

.. code-block:: console

	$ make -j

Run the unit tests:

.. code-block:: console

	$ ./bin/test

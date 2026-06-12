=============================
Zero Board Computer Reference
=============================

.. include:: ../../README.rst
   :start-after: Zero Board Computer
   :end-before: Quick Start

Documentation
-------------

.. toctree::
   :maxdepth: 2
   :caption: User Guides

   introduction
   client-library
   emulator-integration
   security
   specification
   building
   testing
   examples
   linux-extensions-proposal
   qemu-transports-proposal
   documentation-sources

.. toctree::
   :maxdepth: 2
   :caption: Architecture

   architecture

.. toctree::
   :maxdepth: 2
   :caption: C++ API

   api/cpp

.. toctree::
   :maxdepth: 2
   :caption: C API

   api/high-level
   api/client
   api/host
   api/backend
   api/protocol

.. include:: ../../README.rst
   :start-after: Quick Start
   :end-before: License

Testing
-------

Tests run on Linux, macOS, and Windows. CI includes AddressSanitizer,
UndefinedBehaviorSanitizer, and continuous fuzzing via ClusterFuzzLite.

Related Projects
----------------

- `MAME <https://github.com/mamedev/mame>`_ -- includes ZBC machine drivers
- `zeroboardcomputer.com <https://www.zeroboardcomputer.com>`_ -- ZBC specification

.. include:: ../../README.rst
   :start-after: License

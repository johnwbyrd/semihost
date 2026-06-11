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
   :caption: API Reference

   api/index

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

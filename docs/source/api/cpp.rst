C++ Host Library
================

.. default-domain:: cpp

The C++ host library (``zbc/Semihost.h``, namespace ``zbc``) is a C++17
alternative to the C host API, intended for emulators and other
host-side embedders that prefer object lifetimes, value types, and
composition over C vtables and callbacks. It implements the same RIFF
wire protocol as the C host -- in fact it reuses the C library's
endianness helpers -- so a guest cannot tell which host it is talking
to.

For the high-level structure (collaborators, Policy-vs-Backend
separation, integration sketch, error reporting model), see
:doc:`/architecture`. This page is the per-class reference.

Value types
-----------

The library depends only on the C++17 standard library. ``zbc::Result<T>``
carries a value or an error message and ``zbc::Status`` a success/failure;
``zbc::ByteSpan`` is a minimal ``std::span``-like view; owned data is a
``std::vector<uint8_t>`` (``zbc::Bytes``). Strings cross the API as
``std::string_view``.

Reference
---------

The following classes and structs are extracted from the headers in
``include/cpp/zbc/`` by Doxygen and rendered here through Breathe.

Device
^^^^^^

.. doxygenclass:: zbc::Device

Backend hierarchy
^^^^^^^^^^^^^^^^^

.. doxygenclass:: zbc::Backend

.. doxygenclass:: zbc::ConsoleBackend

.. doxygenclass:: zbc::FileBackend

.. doxygenstruct:: zbc::OpResult

Policy hierarchy
^^^^^^^^^^^^^^^^

.. doxygenclass:: zbc::Policy

.. doxygenclass:: zbc::ConsoleOnlyPolicy

.. doxygenclass:: zbc::SandboxedPolicy

.. doxygenclass:: zbc::UnrestrictedPolicy

Guest memory and support types
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: zbc::GuestMemory

.. doxygenstruct:: zbc::PlatformConfig

.. doxygenstruct:: zbc::ParsedRequest

.. doxygenclass:: zbc::FileDescTable

.. doxygenclass:: zbc::PathValidator

.. doxygenstruct:: zbc::PathValidatorConfig

.. doxygenclass:: zbc::Status

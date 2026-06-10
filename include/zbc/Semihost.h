//===-- zbc/Semihost.h - Umbrella header ------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Convenience header pulling in the whole C++ host library. Most embedders
/// need only this. See docs/source/api/cpp.rst for an integration walkthrough.
///
/// Quick start:
/// \code
///   class MyMem : public zbc::GuestMemory { ... };
///   MyMem Mem;
///   zbc::PlatformConfig Cfg(2, 2, zbc::Endian::Little);   // e.g. 6502
///   zbc::Device Dev(Mem, Cfg,
///                   std::make_unique<zbc::FileBackend>(onExit, onTimer),
///                   std::make_unique<zbc::SandboxedPolicy>("/srv/sandbox"));
///   // map Dev.read()/Dev.write() over the 32-byte register window
/// \endcode
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_SEMIHOST_HPP
#define ZBC_SEMIHOST_HPP

#include "zbc/Backend.h"
#include "zbc/Common.h"
#include "zbc/Device.h"
#include "zbc/GuestMemory.h"
#include "zbc/Policy.h"
#include "zbc/Protocol.h"
#include "zbc/RiffCodec.h"

#endif // ZBC_SEMIHOST_HPP

//===-- zbc/GuestMemory.h - Guest RAM access --------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Abstract access to guest memory. The Device reads RIFF requests from,
/// and writes responses to, guest RAM through this interface. Embedders
/// implement it over their own memory model (MAME's address_space, an
/// emulator's flat array, an FPGA bridge's DMA, etc.).
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_GUEST_MEMORY_HPP
#define ZBC_GUEST_MEMORY_HPP

#include <cstddef>
#include <cstdint>

namespace zbc {

class GuestMemory {
public:
  virtual ~GuestMemory() = default;

  virtual uint8_t readByte(uint64_t Addr) = 0;
  virtual void writeByte(uint64_t Addr, uint8_t Value) = 0;

  /// Block helpers default to byte-at-a-time; override for efficiency.
  virtual void readBlock(uint8_t *Dest, uint64_t Addr, std::size_t Size) {
    for (std::size_t I = 0; I < Size; ++I)
      Dest[I] = readByte(Addr + I);
  }
  virtual void writeBlock(uint64_t Addr, const uint8_t *Src, std::size_t Size) {
    for (std::size_t I = 0; I < Size; ++I)
      writeByte(Addr + I, Src[I]);
  }
};

} // namespace zbc

#endif // ZBC_GUEST_MEMORY_HPP

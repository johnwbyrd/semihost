//===-- zbc/Device.h - Memory-mapped semihosting device ---------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// The 32-byte memory-mapped semihosting device. The embedder maps
/// read()/write() over the register window, implements GuestMemory, and
/// supplies a Backend and a Policy. On a DOORBELL write the device reads
/// the RIFF request from guest RAM, checks the Policy, dispatches to the
/// Backend, and writes the response back -- touching only the guest's
/// pre-allocated RETN/ERRO payloads.
///
/// Conforms to spec v0.2.0: 16-byte guest-native RIFF_PTR, STATUS bitmask
/// (TIMER / RESPONSE_READY / PROTO_ERROR), and the ERROR_CODE register as
/// the diagnostic channel when no ERRO chunk can be written.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_DEVICE_HPP
#define ZBC_DEVICE_HPP

#include "zbc/Backend.h"
#include "zbc/GuestMemory.h"
#include "zbc/Policy.h"
#include "zbc/Protocol.h"
#include "zbc/RiffCodec.h"

#include <functional>
#include <memory>
#include <vector>

namespace zbc {

class Device {
public:
  /// Asserts (true) or deasserts (false) the CPU interrupt line.
  using IrqCallback = std::function<void(bool Assert)>;

  /// Default work buffer size; bounds the largest acceptable request.
  static constexpr std::size_t DefaultWorkBufferSize = 4096;

  /// @param Mem         Guest memory accessor (must outlive the device).
  /// @param Config      Platform defaults (CNFG, if sent, overrides them).
  /// @param Backend     Performs the actual I/O.
  /// @param Pol         Authorizes each operation.
  /// @param WorkBufSize Work buffer size. Requests whose RIFF size exceeds
  ///                    this are rejected as malformed (the size field is
  ///                    guest-controlled and must not drive allocation).
  Device(GuestMemory &Mem, PlatformConfig Config,
         std::unique_ptr<Backend> Backend, std::unique_ptr<Policy> Pol,
         std::size_t WorkBufSize = DefaultWorkBufferSize);

  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;

  /// Register-window access (offsets 0x00-0x1F).
  uint8_t read(uint64_t Offset);
  void write(uint64_t Offset, uint8_t Value);

  /// Set the IRQ line callback (for the periodic timer).
  void setIrqCallback(IrqCallback CB) { OnIrq_ = std::move(CB); }

  /// Called by the embedder when the periodic timer fires: latches the
  /// TIMER status bit and asserts the IRQ line.
  void timerTick();

  uint8_t statusRegister() const { return StatusReg_; }
  bool responseReady() const { return (StatusReg_ & status::ResponseReady) != 0; }

  /// Direct access for embedders that wire their own backend factories.
  Backend &backend() { return *Backend_; }
  Policy &policy() { return *Policy_; }

private:
  void processRequest();
  void dispatch(ParsedRequest &Req);
  /// Report a protocol error: prefer the ERRO chunk, else the register
  /// channel. Pass Parsed == nullptr when no chunk locations are trusted.
  void reportProtoError(const ParsedRequest *Parsed, uint16_t Code);
  /// Flush a response payload region of the work buffer back to guest RAM.
  void flushPayload(std::size_t Offset, std::size_t Size);

  uint64_t decodeRiffPtr() const;

  GuestMemory &Mem_;
  PlatformConfig Config_;
  std::unique_ptr<Backend> Backend_;
  std::unique_ptr<Policy> Policy_;
  IrqCallback OnIrq_;

  std::vector<uint8_t> WorkBuffer_;
  uint64_t RiffAddr_ = 0; ///< guest address of the request currently in WorkBuffer_

  // Registers.
  uint8_t RiffPtr_[16] = {0};
  uint8_t StatusReg_ = status::None;
  uint16_t ErrorCodeReg_ = 0;

  static constexpr char Signature_[] = "SEMIHOST";
};

} // namespace zbc

#endif // ZBC_DEVICE_HPP

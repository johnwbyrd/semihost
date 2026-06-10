//===-- Device.cpp - Memory-mapped semihosting device -----------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//

#include "zbc/Device.h"

#include <cerrno>

namespace zbc {

namespace {
constexpr std::size_t WorkBufferSize = 4096;
} // namespace

constexpr char Device::Signature_[];

Device::Device(GuestMemory &Mem, PlatformConfig Config,
               std::unique_ptr<Backend> Backend, std::unique_ptr<Policy> Pol)
    : Mem_(Mem), Config_(Config), Backend_(std::move(Backend)),
      Policy_(std::move(Pol)), WorkBuffer_(WorkBufferSize) {}

//===----------------------------------------------------------------------===//
// Register window
//===----------------------------------------------------------------------===//

uint64_t Device::decodeRiffPtr() const {
  // RIFF_PTR is 16 raw bytes; interpret PtrSize of them in guest byte order.
  return (uint64_t)zbc_read_native_uint(RiffPtr_, (int)Config_.PtrSize,
                                        Config_.endianCode());
}

uint8_t Device::read(uint64_t Offset) {
  if (Offset >= reg::Size)
    return 0xFF;
  if (Offset < ZBC_SIGNATURE_SIZE)
    return (uint8_t)Signature_[Offset];
  if (Offset >= reg::RiffPtr && Offset < reg::RiffPtr + reg::RiffPtrWidth)
    return RiffPtr_[Offset - reg::RiffPtr];
  if (Offset == reg::Doorbell)
    return 0;
  if (Offset == reg::Status)
    return StatusReg_;
  if (Offset == reg::ErrorCode)
    return (uint8_t)(ErrorCodeReg_ & 0xFF);
  if (Offset == reg::ErrorCode + 1)
    return (uint8_t)((ErrorCodeReg_ >> 8) & 0xFF);
  return 0x00;
}

void Device::write(uint64_t Offset, uint8_t Value) {
  if (Offset >= reg::Size)
    return;
  if (Offset >= reg::RiffPtr && Offset < reg::RiffPtr + reg::RiffPtrWidth) {
    RiffPtr_[Offset - reg::RiffPtr] = Value;
    return;
  }
  if (Offset == reg::Doorbell) {
    processRequest();
    return;
  }
  if (Offset == reg::Status) {
    if (Value == 0) {
      // Acknowledge: clear all latched bits, deassert IRQ.
      StatusReg_ = status::None;
      ErrorCodeReg_ = 0;
      if (OnIrq_)
        OnIrq_(false);
    }
    return;
  }
  // SIGNATURE and ERROR_CODE are read-only; ignore writes.
}

void Device::timerTick() {
  StatusReg_ |= status::Timer;
  if (OnIrq_)
    OnIrq_(true);
}

//===----------------------------------------------------------------------===//
// Request processing
//===----------------------------------------------------------------------===//

void Device::flushPayload(std::size_t Offset, std::size_t Size) {
  if (Size == 0)
    return;
  Mem_.writeBlock(RiffAddr_ + Offset, WorkBuffer_.data() + Offset, Size);
}

void Device::reportProtoError(const ParsedRequest *Parsed, uint16_t Code) {
  // Prefer the guest's pre-allocated ERRO chunk (richest diagnostic).
  if (Parsed && Parsed->hasErro() &&
      Parsed->ErroPayloadCapacity >= ErroPayloadSize) {
    auto Written = writeError(MutableByteSpan(WorkBuffer_), *Parsed, Code);
    if (Written) {
      flushPayload(Parsed->ErroPayloadOffset, *Written);
      return;
    }
  }
  // Fall back to the register channel; never touch guest memory.
  ErrorCodeReg_ = Code;
  StatusReg_ |= status::ProtoError;
}

void Device::processRequest() {
  // A fresh request supersedes any previous completion/error flags.
  StatusReg_ &= (uint8_t)~(status::ResponseReady | status::ProtoError);
  ErrorCodeReg_ = 0;

  RiffAddr_ = decodeRiffPtr();

  // Read the RIFF header to learn the total size.
  Mem_.readBlock(WorkBuffer_.data(), RiffAddr_, RiffHdrSize);
  if (ZBC_READ_U32_LE(WorkBuffer_.data()) != fourcc::Riff) {
    reportProtoError(nullptr, proto_err::MalformedRiff);
    StatusReg_ |= status::ResponseReady;
    return;
  }

  std::size_t Total = 8 + (std::size_t)ZBC_READ_U32_LE(WorkBuffer_.data() + 4);
  if (Total > WorkBuffer_.size())
    WorkBuffer_.resize(Total);
  Mem_.readBlock(WorkBuffer_.data(), RiffAddr_, Total);

  auto Parsed = parseRequest(ByteSpan(WorkBuffer_.data(), Total), Config_);
  if (!Parsed) {
    reportProtoError(nullptr, proto_err::MalformedRiff);
    StatusReg_ |= status::ResponseReady;
    return;
  }

  // CNFG, if present, overrides the platform defaults for the session.
  if (Parsed->HasCnfg)
    Config_ = Parsed->Config;

  if (Parsed->HasCall)
    dispatch(*Parsed);
  else
    reportProtoError(&*Parsed, proto_err::InvalidChunk);

  StatusReg_ |= status::ResponseReady;
}

//===----------------------------------------------------------------------===//
// Opcode dispatch
//===----------------------------------------------------------------------===//

void Device::dispatch(ParsedRequest &Req) {
  OpResult Result;
  Backend &B = *Backend_;
  Policy &P = *Policy_;

  // Emit a result into the RETN chunk, or a register/ERRO error if RETN
  // cannot hold it.
  auto emit = [&](const OpResult &R) {
    auto Written = writeReturn(MutableByteSpan(WorkBuffer_), Req, R.Value,
                               R.Errno, ByteSpan(R.Data));
    if (Written) {
      flushPayload(Req.RetnPayloadOffset, *Written);
    } else if (!Req.hasRetn()) {
      reportProtoError(&Req, proto_err::MissingRetn);
    } else {
      reportProtoError(&Req, proto_err::RetnTooSmall);
    }
  };
  // Report a malformed-arguments error (wrong PARM/DATA count).
  auto badParams = [&]() { reportProtoError(&Req, proto_err::InvalidParams); };

  switch (Req.Op) {
  case Opcode::Open: {
    if (Req.DataChunks.empty() || Req.Parms.empty())
      return badParams();
    std::string_view Path = Req.dataAsString(0);
    OpenMode Mode = static_cast<OpenMode>(Req.Parms[0]);
    if (!P.allowOpen(Path, Mode)) {
      Result = OpResult::error(EACCES);
      break;
    }
    auto Resolved = P.resolvePath(Path, openModeIsWrite(Mode));
    if (!Resolved) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.open(*Resolved, Mode);
    break;
  }
  case Opcode::Close:
    if (Req.Parms.empty())
      return badParams();
    if (!P.allowClose((int)Req.Parms[0])) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.close((int)Req.Parms[0]);
    break;
  case Opcode::Read: {
    if (Req.Parms.size() < 2)
      return badParams();
    int FD = (int)Req.Parms[0];
    std::size_t Count = (std::size_t)Req.Parms[1];
    if (!P.allowRead(FD, Count)) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.read(FD, Count);
    break;
  }
  case Opcode::Write: {
    if (Req.Parms.empty() || Req.DataChunks.empty())
      return badParams();
    int FD = (int)Req.Parms[0];
    ByteSpan Data = Req.DataChunks[0];
    if (!P.allowWrite(FD, Data)) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.write(FD, Data);
    break;
  }
  case Opcode::WriteC: {
    char C;
    if (!Req.Parms.empty())
      C = (char)Req.Parms[0];
    else if (!Req.DataChunks.empty() && !Req.DataChunks[0].empty())
      C = (char)Req.DataChunks[0][0];
    else
      return badParams();
    if (!P.allowWriteChar(C)) {
      Result = OpResult::error(EACCES);
      break;
    }
    B.writeChar(C);
    Result = OpResult::success();
    break;
  }
  case Opcode::Write0: {
    if (Req.DataChunks.empty())
      return badParams();
    std::string_view Str = Req.dataAsString(0);
    if (!P.allowWriteString(Str)) {
      Result = OpResult::error(EACCES);
      break;
    }
    B.writeString(Str);
    Result = OpResult::success();
    break;
  }
  case Opcode::ReadC:
    if (!P.allowReadChar()) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = OpResult::success(B.readChar());
    break;
  case Opcode::Seek: {
    if (Req.Parms.size() < 2)
      return badParams();
    int FD = (int)Req.Parms[0];
    int64_t Off = (int64_t)Req.Parms[1];
    if (!P.allowSeek(FD, Off)) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.seek(FD, Off);
    break;
  }
  case Opcode::FLen:
    if (Req.Parms.empty())
      return badParams();
    if (!P.allowFileLength((int)Req.Parms[0])) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.fileLength((int)Req.Parms[0]);
    break;
  case Opcode::Remove: {
    if (Req.DataChunks.empty())
      return badParams();
    std::string_view Path = Req.dataAsString(0);
    if (!P.allowRemove(Path)) {
      Result = OpResult::error(EACCES);
      break;
    }
    auto Resolved = P.resolvePath(Path, true);
    if (!Resolved) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.remove(*Resolved);
    break;
  }
  case Opcode::Rename: {
    if (Req.DataChunks.size() < 2)
      return badParams();
    std::string_view Old = Req.dataAsString(0);
    std::string_view New = Req.dataAsString(1);
    if (!P.allowRename(Old, New)) {
      Result = OpResult::error(EACCES);
      break;
    }
    auto RO = P.resolvePath(Old, true);
    auto RN = P.resolvePath(New, true);
    if (!RO || !RN) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.rename(*RO, *RN);
    break;
  }
  case Opcode::TmpNam: {
    int Id = Req.Parms.empty() ? 0 : (int)Req.Parms[0];
    if (!P.allowTmpnam(Id)) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.tmpnam(Id);
    break;
  }
  case Opcode::IsError:
    if (Req.Parms.empty())
      return badParams();
    Result = OpResult::success(B.isError((int)Req.Parms[0]) ? 1 : 0);
    break;
  case Opcode::IsTTY:
    if (Req.Parms.empty())
      return badParams();
    Result = OpResult::success(B.isTTY((int)Req.Parms[0]) ? 1 : 0);
    break;
  case Opcode::Clock:
    Result = B.clock();
    break;
  case Opcode::Time:
    Result = B.time();
    break;
  case Opcode::Elapsed:
    Result = B.elapsed();
    break;
  case Opcode::TickFreq:
    Result = B.tickFreq();
    break;
  case Opcode::System: {
    if (Req.DataChunks.empty())
      return badParams();
    std::string_view Cmd = Req.dataAsString(0);
    if (!P.allowSystem(Cmd)) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.system(Cmd);
    break;
  }
  case Opcode::GetCmdLine:
    if (!P.allowGetCmdLine()) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.getCmdLine();
    break;
  case Opcode::HeapInfo:
    if (!P.allowHeapInfo()) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.heapInfo();
    break;
  case Opcode::Errno:
    Result = OpResult::success(B.getErrno());
    break;
  case Opcode::Exit:
  case Opcode::ExitExtended: {
    unsigned Reason = Req.Parms.empty() ? 0 : (unsigned)Req.Parms[0];
    unsigned Subcode = Req.Parms.size() > 1 ? (unsigned)Req.Parms[1] : 0;
    B.exit(Reason, Subcode);
    Result = OpResult::success();
    break;
  }
  case Opcode::TimerConfig: {
    if (Req.Parms.empty())
      return badParams();
    unsigned Rate = (unsigned)Req.Parms[0];
    if (!P.allowTimerConfig(Rate)) {
      Result = OpResult::error(EACCES);
      break;
    }
    Result = B.timerConfig(Rate);
    break;
  }
  default:
    reportProtoError(&Req, proto_err::UnsupportedOp);
    return;
  }

  emit(Result);
}

} // namespace zbc

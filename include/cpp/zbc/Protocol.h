//===-- zbc/Protocol.h - Typed wire protocol --------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Typed C++ view of the ZBC wire protocol. This is deliberately a thin
/// layer over the C header <zbc_protocol.h>: every enum value and constant
/// is defined in terms of the C macros, so the C and C++ implementations
/// can never disagree about opcodes, FourCC codes, register offsets, or
/// error codes. Add new protocol constants in zbc_protocol.h, not here.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_PROTOCOL_HPP
#define ZBC_PROTOCOL_HPP

#include "zbc/Common.h"

extern "C" {
#include "zbc_protocol.h"
}

namespace zbc {

/// Guest byte order (mirrors the CNFG endianness field).
enum class Endian : uint8_t {
  Little = ZBC_ENDIAN_LITTLE,
  Big = ZBC_ENDIAN_BIG,
};

/// ARM-compatible semihosting opcodes.
enum class Opcode : uint8_t {
  Open = SH_SYS_OPEN,
  Close = SH_SYS_CLOSE,
  WriteC = SH_SYS_WRITEC,
  Write0 = SH_SYS_WRITE0,
  Write = SH_SYS_WRITE,
  Read = SH_SYS_READ,
  ReadC = SH_SYS_READC,
  IsError = SH_SYS_ISERROR,
  IsTTY = SH_SYS_ISTTY,
  Seek = SH_SYS_SEEK,
  FLen = SH_SYS_FLEN,
  TmpNam = SH_SYS_TMPNAM,
  Remove = SH_SYS_REMOVE,
  Rename = SH_SYS_RENAME,
  Clock = SH_SYS_CLOCK,
  Time = SH_SYS_TIME,
  System = SH_SYS_SYSTEM,
  Errno = SH_SYS_ERRNO,
  GetCmdLine = SH_SYS_GET_CMDLINE,
  HeapInfo = SH_SYS_HEAPINFO,
  Exit = SH_SYS_EXIT,
  ExitExtended = SH_SYS_EXIT_EXTENDED,
  Elapsed = SH_SYS_ELAPSED,
  TickFreq = SH_SYS_TICKFREQ,
  TimerConfig = SH_SYS_TIMER_CONFIG,

  // Linux extensions.
  OpenDir = SH_SYS_OPENDIR,
  ReadDir = SH_SYS_READDIR,
  CloseDir = SH_SYS_CLOSEDIR,
  Stat = SH_SYS_STAT,
  FStat = SH_SYS_FSTAT,
  MkDir = SH_SYS_MKDIR,
  RmDir = SH_SYS_RMDIR,
  FTruncate = SH_SYS_FTRUNCATE,
  FSync = SH_SYS_FSYNC,
  ReadCPoll = SH_SYS_READC_POLL,
  Link = SH_SYS_LINK,
  Symlink = SH_SYS_SYMLINK,
  ReadLink = SH_SYS_READLINK,
  LStat = SH_SYS_LSTAT,
};

/// Linux-extension wire layout sizes (shared with the C header).
constexpr std::size_t StatBufSize = SH_STAT_BUF_SIZE;     // 48
constexpr std::size_t DirentHdrSize = SH_DIRENT_HDR_SIZE; // 10

/// readdir d_type values (matching POSIX DT_*).
namespace dt {
constexpr uint8_t Unknown = SH_DT_UNKNOWN;
constexpr uint8_t Fifo = SH_DT_FIFO;
constexpr uint8_t Chr = SH_DT_CHR;
constexpr uint8_t Dir = SH_DT_DIR;
constexpr uint8_t Blk = SH_DT_BLK;
constexpr uint8_t Reg = SH_DT_REG;
constexpr uint8_t Lnk = SH_DT_LNK;
constexpr uint8_t Sock = SH_DT_SOCK;
} // namespace dt

/// SYS_OPEN mode flags.
enum class OpenMode : uint8_t {
  R = SH_OPEN_R,
  RB = SH_OPEN_RB,
  RPlus = SH_OPEN_R_PLUS,
  RPlusB = SH_OPEN_R_PLUS_B,
  W = SH_OPEN_W,
  WB = SH_OPEN_WB,
  WPlus = SH_OPEN_W_PLUS,
  WPlusB = SH_OPEN_W_PLUS_B,
  A = SH_OPEN_A,
  AB = SH_OPEN_AB,
  APlus = SH_OPEN_A_PLUS,
  APlusB = SH_OPEN_A_PLUS_B,
};

/// Convert an OpenMode to an fopen() mode string, or nullptr if invalid.
inline const char *openModeToString(OpenMode Mode) {
  switch (Mode) {
  case OpenMode::R:      return "r";
  case OpenMode::RB:     return "rb";
  case OpenMode::RPlus:  return "r+";
  case OpenMode::RPlusB: return "r+b";
  case OpenMode::W:      return "w";
  case OpenMode::WB:     return "wb";
  case OpenMode::WPlus:  return "w+";
  case OpenMode::WPlusB: return "w+b";
  case OpenMode::A:      return "a";
  case OpenMode::AB:     return "ab";
  case OpenMode::APlus:  return "a+";
  case OpenMode::APlusB: return "a+b";
  }
  return nullptr;
}

/// True if the open mode can modify the file.
inline bool openModeIsWrite(OpenMode Mode) {
  return Mode != OpenMode::R && Mode != OpenMode::RB;
}

/// RIFF FourCC codes.
namespace fourcc {
constexpr uint32_t Riff = ZBC_ID_RIFF;
constexpr uint32_t Semi = ZBC_ID_SEMI;
constexpr uint32_t Cnfg = ZBC_ID_CNFG;
constexpr uint32_t Call = ZBC_ID_CALL;
constexpr uint32_t Parm = ZBC_ID_PARM;
constexpr uint32_t Data = ZBC_ID_DATA;
constexpr uint32_t Retn = ZBC_ID_RETN;
constexpr uint32_t Erro = ZBC_ID_ERRO;
} // namespace fourcc

/// Device register offsets.
namespace reg {
constexpr uint64_t Signature = ZBC_REG_SIGNATURE;
constexpr uint64_t RiffPtr = ZBC_REG_RIFF_PTR;
constexpr uint64_t Doorbell = ZBC_REG_DOORBELL;
constexpr uint64_t Status = ZBC_REG_STATUS;
constexpr uint64_t ErrorCode = ZBC_REG_ERROR_CODE;
constexpr uint64_t Size = ZBC_REG_SIZE;
constexpr uint64_t RiffPtrWidth = 16; ///< RIFF_PTR occupies 0x08-0x17
} // namespace reg

/// STATUS register bits.
namespace status {
constexpr uint8_t None = ZBC_STATUS_NONE;
constexpr uint8_t Timer = ZBC_STATUS_TIMER;
constexpr uint8_t ResponseReady = ZBC_STATUS_RESPONSE_READY;
constexpr uint8_t ProtoError = ZBC_STATUS_PROTO_ERROR;
} // namespace status

/// PARM / DATA chunk type codes.
namespace parmtype {
constexpr uint8_t Int = ZBC_PARM_TYPE_INT;
constexpr uint8_t Ptr = ZBC_PARM_TYPE_PTR;
} // namespace parmtype
namespace datatype {
constexpr uint8_t Binary = ZBC_DATA_TYPE_BINARY;
constexpr uint8_t String = ZBC_DATA_TYPE_STRING;
} // namespace datatype

/// Protocol error codes (ERRO chunk / ERROR_CODE register).
namespace proto_err {
constexpr uint16_t InvalidChunk = ZBC_PROTO_ERR_INVALID_CHUNK;
constexpr uint16_t MalformedRiff = ZBC_PROTO_ERR_MALFORMED_RIFF;
constexpr uint16_t MissingCnfg = ZBC_PROTO_ERR_MISSING_CNFG;
constexpr uint16_t UnsupportedOp = ZBC_PROTO_ERR_UNSUPPORTED_OP;
constexpr uint16_t InvalidParams = ZBC_PROTO_ERR_INVALID_PARAMS;
constexpr uint16_t MissingRetn = ZBC_PROTO_ERR_MISSING_RETN;
constexpr uint16_t MissingErro = ZBC_PROTO_ERR_MISSING_ERRO;
constexpr uint16_t RetnTooSmall = ZBC_PROTO_ERR_RETN_TOO_SMALL;
} // namespace proto_err

/// Wire-format sizes (shared with the C header).
constexpr std::size_t RiffHdrSize = ZBC_RIFF_HDR_SIZE;     // 12
constexpr std::size_t ChunkHdrSize = ZBC_CHUNK_HDR_SIZE;   // 8
constexpr std::size_t CallHdrPayloadSize = ZBC_CALL_HDR_PAYLOAD_SIZE; // 4
constexpr std::size_t ParmHdrSize = ZBC_PARM_HDR_SIZE;     // 4
constexpr std::size_t DataHdrSize = ZBC_DATA_HDR_SIZE;     // 4
constexpr std::size_t CnfgPayloadSize = ZBC_CNFG_PAYLOAD_SIZE; // 4
constexpr std::size_t ErroPayloadSize = ZBC_ERRO_PAYLOAD_SIZE; // 4
constexpr std::size_t RetnErrnoSize = ZBC_RETN_ERRNO_SIZE; // 4

/// Round a size up to an even (word) boundary, per RIFF.
constexpr std::size_t padSize(std::size_t S) { return (S + 1) & ~std::size_t(1); }

//===----------------------------------------------------------------------===//
// PlatformConfig
//===----------------------------------------------------------------------===//

/// Guest architecture parameters.
///
/// Provided by the embedder at construction (an emulator knows these from
/// the CPU it emulates / the target triple). A CNFG chunk, if present in a
/// request, overrides these for the session -- see the spec's
/// "Platform-provided defaults".
struct PlatformConfig {
  uint8_t IntSize;
  uint8_t PtrSize;
  Endian ByteOrder;

  PlatformConfig(uint8_t Int, uint8_t Ptr, Endian E)
      : IntSize(Int), PtrSize(Ptr), ByteOrder(E) {}

  bool isValid() const {
    auto OkSize = [](uint8_t S) {
      return S == 1 || S == 2 || S == 4 || S == 8;
    };
    return OkSize(IntSize) && OkSize(PtrSize);
  }

  /// Endianness as the C library's int code (ZBC_ENDIAN_*).
  int endianCode() const { return static_cast<int>(ByteOrder); }
};

} // namespace zbc

#endif // ZBC_PROTOCOL_HPP

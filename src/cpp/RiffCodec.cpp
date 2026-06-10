//===-- RiffCodec.cpp - RIFF parse/build ------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//

#include "zbc/RiffCodec.h"

#include <cstring>

namespace zbc {

namespace {

/// Read a guest integer of Size bytes using the shared C endian helper.
intmax_t readGuestInt(const uint8_t *Buf, uint8_t Size, Endian E) {
  return (intmax_t)zbc_read_native_int(Buf, (int)Size, (int)E);
}

/// Parse the PARM/DATA sub-chunks inside a CALL container.
void parseSubchunks(const uint8_t *Start, const uint8_t *End,
                    const PlatformConfig &Config, ParsedRequest &Out) {
  const uint8_t *Pos = Start;

  while (Pos + ChunkHdrSize <= End) {
    uint32_t Id = ZBC_READ_U32_LE(Pos);
    uint32_t Size = ZBC_READ_U32_LE(Pos + 4);
    const uint8_t *Data = Pos + ChunkHdrSize;

    if (Data + Size > End)
      break; // truncated sub-chunk

    if (Id == fourcc::Parm && Out.Parms.size() < MaxParms) {
      if (Size >= ParmHdrSize) {
        uint8_t Type = Data[0];
        uint8_t ValueSize =
            (Type == parmtype::Ptr) ? Config.PtrSize : Config.IntSize;
        if (Size >= ParmHdrSize + ValueSize) {
          Out.Parms.push_back(
              readGuestInt(Data + ParmHdrSize, ValueSize, Config.ByteOrder));
        }
      }
    } else if (Id == fourcc::Data && Out.DataChunks.size() < MaxData) {
      if (Size >= DataHdrSize) {
        Out.DataChunks.push_back(
            ByteSpan(Data + DataHdrSize, Size - DataHdrSize));
      }
    }

    Pos += ChunkHdrSize + padSize(Size);
  }
}

} // namespace

Result<ParsedRequest> parseRequest(ByteSpan Buf, PlatformConfig Default) {
  ParsedRequest Out(Default);

  if (Buf.size() < RiffHdrSize)
    return Result<ParsedRequest>::error("buffer too small for RIFF header");

  const uint8_t *Base = Buf.data();
  if (ZBC_READ_U32_LE(Base) != fourcc::Riff)
    return Result<ParsedRequest>::error("bad RIFF magic");
  if (ZBC_READ_U32_LE(Base + 8) != fourcc::Semi)
    return Result<ParsedRequest>::error("wrong form type (expected SEMI)");

  uint32_t RiffSize = ZBC_READ_U32_LE(Base + 4);
  std::size_t TotalSize = 8 + (std::size_t)RiffSize;
  if (TotalSize > Buf.size())
    return Result<ParsedRequest>::error("RIFF size exceeds buffer");

  const uint8_t *End = Base + TotalSize;
  const uint8_t *Pos = Base + RiffHdrSize;

  while (Pos + ChunkHdrSize <= End) {
    uint32_t Id = ZBC_READ_U32_LE(Pos);
    uint32_t Size = ZBC_READ_U32_LE(Pos + 4);
    const uint8_t *Data = Pos + ChunkHdrSize;

    if (Data + Size > End)
      return Result<ParsedRequest>::error("chunk data exceeds container");

    if (Id == fourcc::Cnfg) {
      if (Size >= CnfgPayloadSize) {
        Out.Config.IntSize = Data[0];
        Out.Config.PtrSize = Data[1];
        Out.Config.ByteOrder =
            (Data[2] == (uint8_t)Endian::Little) ? Endian::Little : Endian::Big;
        Out.HasCnfg = true;
        if (!Out.Config.isValid())
          return Result<ParsedRequest>::error("invalid int/ptr size in CNFG");
      }
    } else if (Id == fourcc::Call) {
      if (Size >= CallHdrPayloadSize) {
        Out.Op = static_cast<Opcode>(Data[0]);
        Out.HasCall = true;
        parseSubchunks(Data + CallHdrPayloadSize, Data + Size, Out.Config, Out);
      }
    } else if (Id == fourcc::Retn) {
      Out.RetnPayloadOffset = (std::size_t)(Data - Base);
      Out.RetnPayloadCapacity = Size;
    } else if (Id == fourcc::Erro) {
      Out.ErroPayloadOffset = (std::size_t)(Data - Base);
      Out.ErroPayloadCapacity = Size;
    }
    // Unknown chunks are skipped (forward compatibility).

    Pos += ChunkHdrSize + padSize(Size);
  }

  return Out;
}

Result<std::size_t> writeReturn(MutableByteSpan Buf, const ParsedRequest &Req,
                                intmax_t ResultValue, int Errno,
                                ByteSpan OutData) {
  if (!Req.hasRetn())
    return Result<std::size_t>::error("no RETN chunk");

  const uint8_t IntSize = Req.Config.IntSize;
  std::size_t Needed = (std::size_t)IntSize + RetnErrnoSize;
  if (!OutData.empty())
    Needed += ChunkHdrSize + padSize(DataHdrSize + OutData.size());

  if (Req.RetnPayloadCapacity < Needed)
    return Result<std::size_t>::error("RETN chunk too small for response");
  if (Req.RetnPayloadOffset + Needed > Buf.size())
    return Result<std::size_t>::error("RETN payload outside buffer");

  uint8_t *Dst = Buf.data() + Req.RetnPayloadOffset;
  std::size_t Pos = 0;

  zbc_write_native_uint(Dst + Pos, (uintptr_t)ResultValue, (int)IntSize,
                        Req.Config.endianCode());
  Pos += IntSize;
  ZBC_WRITE_U32_LE(Dst + Pos, (uint32_t)Errno);
  Pos += RetnErrnoSize;

  if (!OutData.empty()) {
    std::size_t DataPayload = DataHdrSize + OutData.size();
    ZBC_WRITE_U32_LE(Dst + Pos, fourcc::Data);
    Pos += 4;
    ZBC_WRITE_U32_LE(Dst + Pos, (uint32_t)DataPayload);
    Pos += 4;
    Dst[Pos + 0] = datatype::Binary;
    Dst[Pos + 1] = 0;
    Dst[Pos + 2] = 0;
    Dst[Pos + 3] = 0;
    Pos += DataHdrSize;
    std::memcpy(Dst + Pos, OutData.data(), OutData.size());
    Pos += OutData.size();
    if (DataPayload & 1)
      Dst[Pos++] = 0; // pad to even boundary
  }

  return Pos;
}

Result<std::size_t> writeError(MutableByteSpan Buf, const ParsedRequest &Req,
                               uint16_t ErrorCode) {
  if (!Req.hasErro())
    return Result<std::size_t>::error("no ERRO chunk");
  if (Req.ErroPayloadCapacity < ErroPayloadSize)
    return Result<std::size_t>::error("ERRO chunk too small");
  if (Req.ErroPayloadOffset + ErroPayloadSize > Buf.size())
    return Result<std::size_t>::error("ERRO payload outside buffer");

  uint8_t *Dst = Buf.data() + Req.ErroPayloadOffset;
  ZBC_WRITE_U16_LE(Dst, ErrorCode);
  Dst[2] = 0;
  Dst[3] = 0;
  return (std::size_t)ErroPayloadSize;
}

} // namespace zbc

//===-- zbc/RiffCodec.h - RIFF parse/build ----------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// RIFF SEMI request parsing and response writing for the host side.
/// Integer (de)serialization is delegated to the C library's
/// zbc_read_native_int / zbc_write_native_uint so the two implementations
/// share one definition of endianness handling.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_RIFF_CODEC_HPP
#define ZBC_RIFF_CODEC_HPP

#include "zbc/Common.h"
#include "zbc/Protocol.h"

namespace zbc {

/// Maximum PARM / DATA chunks captured from a single request.
constexpr std::size_t MaxParms = 8;
constexpr std::size_t MaxData = 4;

/// A parsed RIFF SEMI request.
///
/// Offsets are byte offsets from the start of the RIFF buffer to the
/// payload of the pre-allocated response chunks, so the host can write
/// responses in place without modifying the RIFF structure.
struct ParsedRequest {
  PlatformConfig Config;     ///< Effective config (platform default or CNFG)
  bool HasCnfg = false;      ///< A CNFG chunk was present (overrode the default)

  Opcode Op = Opcode::Open;
  bool HasCall = false;

  std::vector<intmax_t> Parms;     ///< Decoded PARM values, in order
  std::vector<ByteSpan> DataChunks; ///< DATA payloads (views into the buffer)

  std::size_t RetnPayloadOffset = 0;   ///< 0 means "no RETN chunk"
  std::size_t RetnPayloadCapacity = 0;
  std::size_t ErroPayloadOffset = 0;   ///< 0 means "no ERRO chunk"
  std::size_t ErroPayloadCapacity = 0;

  explicit ParsedRequest(PlatformConfig Cfg) : Config(Cfg) {}

  bool hasRetn() const { return RetnPayloadOffset != 0; }
  bool hasErro() const { return ErroPayloadOffset != 0; }

  /// Interpret DATA chunk Index as a string view (empty if out of range).
  ///
  /// String-typed DATA payloads carry a trailing NUL terminator on the wire
  /// (the client sends strlen()+1 bytes). It is dropped here so the view is
  /// the logical string; any interior NUL is preserved so callers that
  /// security-check the path still reject it.
  std::string_view dataAsString(std::size_t Index) const {
    if (Index >= DataChunks.size())
      return {};
    const char *Ptr = reinterpret_cast<const char *>(DataChunks[Index].data());
    std::size_t Len = DataChunks[Index].size();
    if (Len > 0 && Ptr[Len - 1] == '\0')
      --Len;
    return std::string_view(Ptr, Len);
  }
};

/// Parse a RIFF SEMI request.
/// @param Buf     The full request buffer (copied from guest memory).
/// @param Default Platform config used when the request omits CNFG.
Result<ParsedRequest> parseRequest(ByteSpan Buf, PlatformConfig Default);

/// Write a RETN response into the pre-allocated RETN payload region of Buf.
/// Returns the number of payload bytes written (so the caller can flush
/// just that region back to guest memory), or an error if RETN is missing
/// or too small.
Result<std::size_t> writeReturn(MutableByteSpan Buf, const ParsedRequest &Req,
                                intmax_t ResultValue, int Errno,
                                ByteSpan OutData = {});

/// Write an ERRO response into the pre-allocated ERRO payload region of Buf.
/// Returns the number of payload bytes written, or an error if ERRO is
/// missing or too small.
Result<std::size_t> writeError(MutableByteSpan Buf, const ParsedRequest &Req,
                               uint16_t ErrorCode);

} // namespace zbc

#endif // ZBC_RIFF_CODEC_HPP

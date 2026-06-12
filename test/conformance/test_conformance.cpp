//===-- test_conformance.cpp - C host vs C++ host conformance ---*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// Runs a corpus of protocol-level requests through BOTH host
/// implementations (the C90 library and the C++17 library) and asserts
/// that their observable behavior is identical:
///
///   - the entire guest RAM image after processing (RETN/ERRO payload
///     writes included, everything else untouched),
///   - the protocol error reported through the register channel,
///   - the sequence of backend operations invoked (including decoded
///     argument values).
///
/// Both sides use an equivalent deterministic "scripted" backend, so any
/// divergence in parsing, dispatch, argument decoding, endianness, or
/// response encoding shows up as a byte or trace mismatch.
///
/// A differential loop then feeds pseudo-random buffers (seeded, so runs
/// are reproducible) through both hosts to catch divergence on malformed
/// input -- a lightweight, always-on cousin of the RIFF fuzzer.
///
//===----------------------------------------------------------------------===//

#include "zbc/Semihost.h"

extern "C" {
#include "zbc_backend.h"
#include "zbc_host.h"
}

#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

//===----------------------------------------------------------------------===//
// Harness plumbing
//===----------------------------------------------------------------------===//

static int g_failures = 0;
static int g_checks = 0;

#define CHECK_MSG(cond, msg)                                                   \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      std::printf("  [FAIL] %s:%d: %s -- %s\n", __FILE__, __LINE__, #cond,     \
                  msg);                                                        \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)
#define CHECK(cond) CHECK_MSG(cond, "")

constexpr std::size_t GuestRamSize = 0x10000;
constexpr uint64_t RiffBase = 0x1000;
constexpr std::size_t WorkBufSize = 4096;

// Guest config used by the corpus (a typical 32-bit little-endian guest).
constexpr uint8_t GuestInt = 4;
constexpr uint8_t GuestPtr = 4;

//===----------------------------------------------------------------------===//
// Scripted backends: identical deterministic behavior on both sides.
// Every call is appended to a trace string for comparison.
//===----------------------------------------------------------------------===//

static std::string g_trace; // shared; reset per run

static void tracef(const char *Fmt, ...) {
  char Buf[256];
  va_list Ap;
  va_start(Ap, Fmt);
  std::vsnprintf(Buf, sizeof(Buf), Fmt, Ap);
  va_end(Ap);
  g_trace += Buf;
  g_trace += '\n';
}

// Strip one trailing NUL so both hosts record the same logical string.
static std::string canon(const char *S, size_t Len) {
  if (Len > 0 && S[Len - 1] == '\0')
    --Len;
  return std::string(S, Len);
}

// --- C side ---------------------------------------------------------------

extern "C" {
static int c_open(void *, const char *P, size_t L, int M) {
  tracef("open(%s,%d)", canon(P, L).c_str(), M);
  return 7;
}
static int c_close(void *, int FD) {
  tracef("close(%d)", FD);
  return 0;
}
static int c_read(void *, int FD, void *Buf, size_t N) {
  tracef("read(%d,%zu)", FD, N);
  for (size_t I = 0; I < N; ++I)
    ((uint8_t *)Buf)[I] = (uint8_t)(0xC3 + I);
  return 0; // all bytes read
}
static int c_write(void *, int FD, const void *Buf, size_t N) {
  tracef("write(%d,%s)", FD, canon((const char *)Buf, N).c_str());
  return 0; // all bytes written
}
static int c_seek(void *, int FD, int Pos) {
  tracef("seek(%d,%d)", FD, Pos);
  return 0;
}
static intmax_t c_flen(void *, int FD) {
  tracef("flen(%d)", FD);
  return 0x1234;
}
static void c_writec(void *, char C) { tracef("writec(%c)", C); }
static void c_write0(void *, const char *S) { tracef("write0(%s)", S); }
static int c_readc(void *) {
  tracef("readc()");
  return 0x41;
}
static int c_istty(void *, int FD) {
  tracef("istty(%d)", FD);
  return FD <= 2 ? 1 : 0;
}
static int c_clock(void *) {
  tracef("clock()");
  return 0x77;
}
static int c_time(void *) {
  tracef("time()");
  return 0x5F00D;
}
static int c_elapsed(void *, unsigned *Lo, unsigned *Hi) {
  tracef("elapsed()");
  *Lo = 0x11223344u;
  *Hi = 0x55667788u;
  return 0;
}
static int c_tickfreq(void *) {
  tracef("tickfreq()");
  return 1000;
}
static int c_get_errno(void *) { return 0; }
static void c_exit(void *, unsigned R, unsigned S) { tracef("exit(%u,%u)", R, S); }
static int c_timer(void *, unsigned Hz) {
  tracef("timer(%u)", Hz);
  return 0;
}
static int c_stat(void *, const char *P, size_t L, void *Buf) {
  tracef("stat(%s)", canon(P, L).c_str());
  /* Fill 48 bytes with a recognisable pattern so the cross-language
   * round-trip on this opcode would show up if it ever ran. */
  for (size_t I = 0; I < SH_STAT_BUF_SIZE; ++I)
    ((uint8_t *)Buf)[I] = (uint8_t)(0xA0 + I);
  return 0;
}
static int c_opendir(void *, const char *P, size_t L) {
  tracef("opendir(%s)", canon(P, L).c_str());
  return 42;
}
static int c_readdir(void *, int H, void *Buf, size_t N) {
  tracef("readdir(%d,%zu)", H, N);
  (void)Buf;
  return 0; /* end of directory */
}
static int c_closedir(void *, int H) {
  tracef("closedir(%d)", H);
  return 0;
}
static int c_readc_poll(void *) {
  tracef("readc_poll()");
  return -1; /* no character available */
}
static int c_fstat(void *, int FD, void *Buf) {
  tracef("fstat(%d)", FD);
  for (size_t I = 0; I < SH_STAT_BUF_SIZE; ++I)
    ((uint8_t *)Buf)[I] = (uint8_t)(0xB0 + I);
  return 0;
}
static int c_mkdir(void *, const char *P, size_t L, int M) {
  tracef("mkdir(%s,%d)", canon(P, L).c_str(), M);
  return 0;
}
static int c_rmdir(void *, const char *P, size_t L) {
  tracef("rmdir(%s)", canon(P, L).c_str());
  return 0;
}
static int c_ftruncate(void *, int FD, uint64_t N) {
  tracef("ftruncate(%d,%llu)", FD, (unsigned long long)N);
  return 0;
}
static int c_fsync(void *, int FD) {
  tracef("fsync(%d)", FD);
  return 0;
}
static int c_link(void *, const char *O, size_t OL, const char *N, size_t NL) {
  tracef("link(%s,%s)", canon(O, OL).c_str(), canon(N, NL).c_str());
  return 0;
}
static int c_symlink(void *, const char *T, size_t TL, const char *L,
                     size_t LL) {
  tracef("symlink(%s,%s)", canon(T, TL).c_str(), canon(L, LL).c_str());
  return 0;
}
static int c_readlink(void *, const char *P, size_t L, void *Buf, size_t N) {
  static const char fake[] = "target";
  size_t n = sizeof(fake) - 1;
  tracef("readlink(%s,%zu)", canon(P, L).c_str(), N);
  if (n > N) n = N;
  memcpy(Buf, fake, n);
  return (int)n;
}
static int c_lstat(void *, const char *P, size_t L, void *Buf) {
  tracef("lstat(%s)", canon(P, L).c_str());
  for (size_t I = 0; I < SH_STAT_BUF_SIZE; ++I)
    ((uint8_t *)Buf)[I] = (uint8_t)(0xC0 + I);
  return 0;
}
}

static const zbc_backend_t ScriptedC = {
    c_open,   c_close,  c_read,    c_write,    c_seek,     c_flen,
    nullptr,  nullptr,  nullptr, /* remove, rename, tmpnam */
    c_writec, c_write0, c_readc,   nullptr,    c_istty, /* iserror unused */
    c_clock,  c_time,   c_elapsed, c_tickfreq, nullptr,    nullptr, /* system, cmdline */
    nullptr, /* heapinfo */
    c_exit,   c_get_errno, c_timer, c_stat,
    c_opendir, c_readdir, c_closedir, c_readc_poll,
    c_fstat,  c_mkdir,    c_rmdir,    c_ftruncate, c_fsync,
    c_link,   c_symlink,  c_readlink, c_lstat};

// --- C++ side ---------------------------------------------------------------

struct ScriptedCpp : zbc::Backend {
  zbc::OpResult open(std::string_view P, zbc::OpenMode M) override {
    tracef("open(%.*s,%d)", (int)P.size(), P.data(), (int)M);
    return zbc::OpResult::success(7);
  }
  zbc::OpResult close(int FD) override {
    tracef("close(%d)", FD);
    return zbc::OpResult::success(0);
  }
  zbc::OpResult read(int FD, std::size_t N) override {
    tracef("read(%d,%zu)", FD, N);
    zbc::OpResult R;
    R.Data.resize(N);
    for (std::size_t I = 0; I < N; ++I)
      R.Data[I] = (uint8_t)(0xC3 + I);
    R.Value = 0;
    return R;
  }
  zbc::OpResult write(int FD, zbc::ByteSpan D) override {
    tracef("write(%d,%s)", FD,
           canon((const char *)D.data(), D.size()).c_str());
    return zbc::OpResult::success(0);
  }
  zbc::OpResult seek(int FD, int64_t Pos) override {
    tracef("seek(%d,%d)", FD, (int)Pos);
    return zbc::OpResult::success(0);
  }
  zbc::OpResult fileLength(int FD) override {
    tracef("flen(%d)", FD);
    return zbc::OpResult::success(0x1234);
  }
  void writeChar(char C) override { tracef("writec(%c)", C); }
  void writeString(std::string_view S) override {
    tracef("write0(%.*s)", (int)S.size(), S.data());
  }
  int readChar() override {
    tracef("readc()");
    return 0x41;
  }
  bool isTTY(int FD) override {
    tracef("istty(%d)", FD);
    return FD <= 2;
  }
  zbc::OpResult clock() override {
    tracef("clock()");
    return zbc::OpResult::success(0x77);
  }
  zbc::OpResult time() override {
    tracef("time()");
    return zbc::OpResult::success(0x5F00D);
  }
  zbc::OpResult elapsed() override {
    tracef("elapsed()");
    zbc::OpResult R;
    R.Value = 0;
    R.Data.resize(8);
    ZBC_WRITE_U32_LE(R.Data.data(), 0x11223344u);
    ZBC_WRITE_U32_LE(R.Data.data() + 4, 0x55667788u);
    return R;
  }
  zbc::OpResult tickFreq() override {
    tracef("tickfreq()");
    return zbc::OpResult::success(1000);
  }
  void exit(unsigned R, unsigned S) override { tracef("exit(%u,%u)", R, S); }
  zbc::OpResult timerConfig(unsigned Hz) override {
    tracef("timer(%u)", Hz);
    return zbc::OpResult::success(0);
  }
};

//===----------------------------------------------------------------------===//
// Host runners: process one request image, return RAM + error code + trace
//===----------------------------------------------------------------------===//

struct Outcome {
  std::vector<uint8_t> Ram;
  uint16_t ProtoError = 0; // register-channel code (0 = none)
  std::string Trace;
};

// --- C host -----------------------------------------------------------------

static uint8_t *g_c_ram;
extern "C" {
static uint8_t c_rd(uintptr_t A, void *) { return g_c_ram[A % GuestRamSize]; }
static void c_wr(uintptr_t A, uint8_t V, void *) {
  g_c_ram[A % GuestRamSize] = V;
}
static void c_rdb(void *D, uintptr_t A, size_t N, void *) {
  for (size_t I = 0; I < N; ++I)
    ((uint8_t *)D)[I] = g_c_ram[(A + I) % GuestRamSize];
}
static void c_wrb(uintptr_t A, const void *S, size_t N, void *) {
  for (size_t I = 0; I < N; ++I)
    g_c_ram[(A + I) % GuestRamSize] = ((const uint8_t *)S)[I];
}
static uint16_t g_c_proto_error;
static void c_proto_cb(void *, int Code) { g_c_proto_error = (uint16_t)Code; }
}

static Outcome runCHost(const std::vector<uint8_t> &Image) {
  Outcome O;
  O.Ram = Image;
  g_c_ram = O.Ram.data();
  g_c_proto_error = 0;
  g_trace.clear();

  static uint8_t WorkBuf[WorkBufSize];
  zbc_host_state_t Host;
  zbc_host_mem_ops_t Ops = {c_rd, c_wr, c_rdb, c_wrb};
  zbc_host_init(&Host, &Ops, nullptr, &ScriptedC, nullptr, WorkBuf,
                sizeof(WorkBuf));
  zbc_host_set_platform_config(&Host, GuestInt, GuestPtr, ZBC_ENDIAN_LITTLE);
  zbc_host_set_proto_error_cb(&Host, c_proto_cb, nullptr);

  zbc_host_process(&Host, (uintptr_t)RiffBase);

  O.ProtoError = g_c_proto_error;
  O.Trace = g_trace;
  return O;
}

// --- C++ host ----------------------------------------------------------------

struct RamMem : zbc::GuestMemory {
  std::vector<uint8_t> &Ram;
  explicit RamMem(std::vector<uint8_t> &R) : Ram(R) {}
  uint8_t readByte(uint64_t A) override { return Ram[A % Ram.size()]; }
  void writeByte(uint64_t A, uint8_t V) override { Ram[A % Ram.size()] = V; }
};

static Outcome runCppHost(const std::vector<uint8_t> &Image) {
  Outcome O;
  O.Ram = Image;
  g_trace.clear();

  RamMem Mem(O.Ram);
  zbc::Device Dev(Mem,
                  zbc::PlatformConfig(GuestInt, GuestPtr, zbc::Endian::Little),
                  std::make_unique<ScriptedCpp>(),
                  std::make_unique<zbc::UnrestrictedPolicy>(), WorkBufSize);

  // Write RIFF_PTR (guest-native LE) and ring the doorbell.
  uint64_t Addr = RiffBase;
  for (unsigned I = 0; I < GuestPtr; ++I)
    Dev.write(ZBC_REG_RIFF_PTR + I, (uint8_t)((Addr >> (I * 8)) & 0xFF));
  Dev.write(ZBC_REG_DOORBELL, 1);

  O.ProtoError = (uint16_t)(Dev.read(ZBC_REG_ERROR_CODE) |
                            (Dev.read(ZBC_REG_ERROR_CODE + 1) << 8));
  O.Trace = g_trace;
  return O;
}

// --- Comparison ---------------------------------------------------------------

static void compareHosts(const char *Name, const std::vector<uint8_t> &Image) {
  Outcome C = runCHost(Image);
  Outcome Cpp = runCppHost(Image);

  if (C.Ram != Cpp.Ram) {
    for (std::size_t I = 0; I < C.Ram.size(); ++I) {
      if (C.Ram[I] != Cpp.Ram[I]) {
        std::printf("    first RAM divergence at 0x%zx: C=%02x C++=%02x\n", I,
                    C.Ram[I], Cpp.Ram[I]);
        break;
      }
    }
  }
  CHECK_MSG(C.Ram == Cpp.Ram, Name);
  CHECK_MSG(C.ProtoError == Cpp.ProtoError, Name);
  if (C.ProtoError != Cpp.ProtoError)
    std::printf("    proto error: C=0x%04x C++=0x%04x\n", C.ProtoError,
                Cpp.ProtoError);
  CHECK_MSG(C.Trace == Cpp.Trace, Name);
  if (C.Trace != Cpp.Trace)
    std::printf("    trace C:\n%s    trace C++:\n%s", C.Trace.c_str(),
                Cpp.Trace.c_str());
}

//===----------------------------------------------------------------------===//
// Raw request builder
//===----------------------------------------------------------------------===//

class Builder {
public:
  Builder &riffHeader() {
    put32(ZBC_ID_RIFF);
    SizeAt_ = Buf_.size();
    put32(0); // patched by finish()
    put32(ZBC_ID_SEMI);
    return *this;
  }
  Builder &cnfg(uint8_t IntSz = GuestInt, uint8_t PtrSz = GuestPtr,
                uint8_t E = ZBC_ENDIAN_LITTLE) {
    put32(ZBC_ID_CNFG);
    put32(4);
    put8(IntSz);
    put8(PtrSz);
    put8(E);
    put8(0);
    return *this;
  }
  Builder &beginCall(uint8_t Op) {
    put32(ZBC_ID_CALL);
    CallSizeAt_ = Buf_.size();
    put32(0);
    put8(Op);
    put8(0);
    put8(0);
    put8(0);
    return *this;
  }
  Builder &parm(uint64_t V, uint8_t Size = GuestInt,
                uint8_t E = ZBC_ENDIAN_LITTLE) {
    put32(ZBC_ID_PARM);
    put32(4 + Size);
    put8(ZBC_PARM_TYPE_INT);
    put8(0);
    put8(0);
    put8(0);
    uint8_t Tmp[8];
    zbc_write_native_uint(Tmp, (uintptr_t)V, Size, E);
    for (unsigned I = 0; I < Size; ++I)
      put8(Tmp[I]);
    pad();
    return *this;
  }
  Builder &data(const void *P, std::size_t N,
                uint8_t Type = ZBC_DATA_TYPE_STRING) {
    put32(ZBC_ID_DATA);
    put32((uint32_t)(4 + N));
    put8(Type);
    put8(0);
    put8(0);
    put8(0);
    const uint8_t *S = (const uint8_t *)P;
    for (std::size_t I = 0; I < N; ++I)
      put8(S[I]);
    pad();
    return *this;
  }
  Builder &str(const char *S) { return data(S, std::strlen(S) + 1); }
  Builder &endCall() {
    patch32(CallSizeAt_, (uint32_t)(Buf_.size() - CallSizeAt_ - 4));
    return *this;
  }
  Builder &retn(uint32_t Capacity) {
    put32(ZBC_ID_RETN);
    put32(Capacity);
    for (uint32_t I = 0; I < ((Capacity + 1u) & ~1u); ++I)
      put8(0);
    return *this;
  }
  Builder &erro(uint32_t Capacity = ZBC_ERRO_PREALLOC_SIZE) {
    put32(ZBC_ID_ERRO);
    put32(Capacity);
    for (uint32_t I = 0; I < ((Capacity + 1u) & ~1u); ++I)
      put8(0);
    return *this;
  }

  /// Patch the RIFF size and return a full guest-RAM image with the
  /// request at RiffBase.
  std::vector<uint8_t> finish() {
    patch32(SizeAt_, (uint32_t)(Buf_.size() - SizeAt_ - 4));
    std::vector<uint8_t> Ram(GuestRamSize, 0x00);
    std::memcpy(Ram.data() + RiffBase, Buf_.data(), Buf_.size());
    return Ram;
  }

  /// As finish(), but without patching the RIFF size (for malformed cases).
  std::vector<uint8_t> finishRaw() {
    std::vector<uint8_t> Ram(GuestRamSize, 0x00);
    std::memcpy(Ram.data() + RiffBase, Buf_.data(), Buf_.size());
    return Ram;
  }

  Builder &raw32(uint32_t V) {
    put32(V);
    return *this;
  }

private:
  void put8(uint8_t B) { Buf_.push_back(B); }
  void put32(uint32_t V) {
    uint8_t T[4];
    ZBC_WRITE_U32_LE(T, V);
    Buf_.insert(Buf_.end(), T, T + 4);
  }
  void patch32(std::size_t At, uint32_t V) { ZBC_WRITE_U32_LE(&Buf_[At], V); }
  void pad() {
    if (Buf_.size() & 1)
      put8(0);
  }
  std::vector<uint8_t> Buf_;
  std::size_t SizeAt_ = 0, CallSizeAt_ = 0;
};

static uint32_t retnCap(std::size_t DataLen = 0) {
  std::size_t Cap = GuestInt + ZBC_RETN_ERRNO_SIZE;
  if (DataLen)
    Cap += ZBC_CHUNK_HDR_SIZE + ZBC_DATA_HDR_SIZE + ((DataLen + 1) & ~1u);
  return (uint32_t)Cap;
}

//===----------------------------------------------------------------------===//
// Corpus
//===----------------------------------------------------------------------===//

static void corpus() {
  std::printf("corpus\n");

  compareHosts("close",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_CLOSE)
                   .parm(5)
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());

  compareHosts("close_no_cnfg (platform defaults)",
               Builder()
                   .riffHeader()
                   .beginCall(SH_SYS_CLOSE)
                   .parm(9)
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());

  {
    const char *Payload = "hello, host";
    compareHosts("write",
                 Builder()
                     .riffHeader()
                     .cnfg()
                     .beginCall(SH_SYS_WRITE)
                     .parm(1)
                     .data(Payload, std::strlen(Payload), ZBC_DATA_TYPE_BINARY)
                     .parm(std::strlen(Payload))
                     .endCall()
                     .retn(retnCap())
                     .erro()
                     .finish());
  }

  compareHosts("read",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_READ)
                   .parm(3)
                   .parm(16)
                   .endCall()
                   .retn(retnCap(16))
                   .erro()
                   .finish());

  compareHosts("open",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_OPEN)
                   .str("subdir/file.bin")
                   .parm(SH_OPEN_RB)
                   .parm(15)
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());

  {
    uint8_t C = 'Z';
    compareHosts("writec",
                 Builder()
                     .riffHeader()
                     .cnfg()
                     .beginCall(SH_SYS_WRITEC)
                     .data(&C, 1, ZBC_DATA_TYPE_BINARY)
                     .endCall()
                     .retn(retnCap())
                     .erro()
                     .finish());
  }

  compareHosts("write0",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_WRITE0)
                   .str("console line")
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());

  compareHosts("time",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_TIME)
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());

  compareHosts("elapsed (8-byte DATA)",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_ELAPSED)
                   .endCall()
                   .retn(retnCap(8))
                   .erro()
                   .finish());

  compareHosts("iserror (pure logic)",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_ISERROR)
                   .parm((uint64_t)(int64_t)-3)
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());

  compareHosts("unknown opcode -> ERRO",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(0x7F)
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());

  compareHosts("missing RETN -> ERRO(MISSING_RETN)",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_CLOSE)
                   .parm(5)
                   .endCall()
                   .erro()
                   .finish());

  compareHosts("RETN too small -> ERRO(RETN_TOO_SMALL)",
               Builder()
                   .riffHeader()
                   .cnfg()
                   .beginCall(SH_SYS_CLOSE)
                   .parm(5)
                   .endCall()
                   .retn(2)
                   .erro()
                   .finish());

  // Malformed: bad magic. Neither host may write guest memory.
  {
    Builder B;
    B.raw32(0xDEADBEEF).raw32(32).raw32(ZBC_ID_SEMI);
    compareHosts("bad magic -> register channel", B.finishRaw());
  }

  // Malformed: chunk size overflows the container.
  {
    Builder B;
    B.riffHeader().cnfg();
    // Hand-write a CALL chunk whose size lies.
    B.raw32(ZBC_ID_CALL).raw32(0x7FFFFFFF);
    compareHosts("overflowing chunk -> register channel", B.finishRaw());
  }

  // Big-endian guest: values encoded BE; both hosts must decode/encode alike.
  compareHosts("big-endian guest",
               Builder()
                   .riffHeader()
                   .cnfg(GuestInt, GuestPtr, ZBC_ENDIAN_BIG)
                   .beginCall(SH_SYS_CLOSE)
                   .parm(0x1234, GuestInt, ZBC_ENDIAN_BIG)
                   .endCall()
                   .retn(retnCap())
                   .erro()
                   .finish());
}

//===----------------------------------------------------------------------===//
// Differential loop: reproducible pseudo-random buffers through both hosts
//===----------------------------------------------------------------------===//

static uint32_t g_rng = 0xC0FFEE42;
static uint32_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return g_rng;
}

static void differential() {
  std::printf("differential (2000 random buffers)\n");
  int Before = g_failures;

  for (int Iter = 0; Iter < 2000; ++Iter) {
    std::vector<uint8_t> Ram(GuestRamSize, 0x00);
    std::size_t Len = 16 + rnd() % 160;
    for (std::size_t I = 0; I < Len; ++I)
      Ram[RiffBase + I] = (uint8_t)rnd();

    // Half the time, seed a plausible RIFF header so parsing goes deeper.
    if (Iter & 1) {
      ZBC_WRITE_U32_LE(&Ram[RiffBase], ZBC_ID_RIFF);
      ZBC_WRITE_U32_LE(&Ram[RiffBase + 4], rnd() % 256);
      ZBC_WRITE_U32_LE(&Ram[RiffBase + 8], ZBC_ID_SEMI);
    }

    Outcome C = runCHost(Ram);
    Outcome Cpp = runCppHost(Ram);

    if (C.Ram != Cpp.Ram || C.ProtoError != Cpp.ProtoError ||
        C.Trace != Cpp.Trace) {
      ++g_failures;
      std::printf("  [FAIL] differential iteration %d (seed path)\n", Iter);
      if (C.ProtoError != Cpp.ProtoError)
        std::printf("    proto error: C=0x%04x C++=0x%04x\n", C.ProtoError,
                    Cpp.ProtoError);
      if (C.Trace != Cpp.Trace)
        std::printf("    trace C:\n%s    trace C++:\n%s", C.Trace.c_str(),
                    Cpp.Trace.c_str());
      for (std::size_t I = 0; I < C.Ram.size(); ++I)
        if (C.Ram[I] != Cpp.Ram[I]) {
          std::printf("    first RAM divergence at 0x%zx: C=%02x C++=%02x\n",
                      I, C.Ram[I], Cpp.Ram[I]);
          break;
        }
      break; // first divergence is enough; the iteration number reproduces it
    }
    ++g_checks;
  }
  if (g_failures == Before)
    std::printf("  no divergence\n");
}

int main() {
  std::printf("=== C/C++ host conformance ===\n");
  corpus();
  differential();
  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures == 0)
    std::printf("Hosts are conformant.\n");
  return g_failures == 0 ? 0 : 1;
}

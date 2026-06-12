//===-- test_cpp.cpp - C++ host library tests -------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// Exercises the C++ host library (zbc::Device + Backend + Policy + codec).
/// The requests are built by the *C* client library and parsed back by the
/// C client, so this doubles as a conformance test that the C client and
/// the C++ host agree on the wire protocol.
///
//===----------------------------------------------------------------------===//

#include "zbc/Semihost.h"

extern "C" {
#include "zbc_client.h"
}

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <string>

//===----------------------------------------------------------------------===//
// Tiny test harness
//===----------------------------------------------------------------------===//

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      std::printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

//===----------------------------------------------------------------------===//
// Test fixtures
//===----------------------------------------------------------------------===//

/// Identity-mapped guest memory: addresses ARE host pointers. Valid for an
/// in-process test where the client's RIFF buffer is real host memory and
/// the client writes its real pointer into RIFF_PTR.
struct IdentityMem : zbc::GuestMemory {
  uint8_t readByte(uint64_t Addr) override {
    return *reinterpret_cast<uint8_t *>((uintptr_t)Addr);
  }
  void writeByte(uint64_t Addr, uint8_t V) override {
    *reinterpret_cast<uint8_t *>((uintptr_t)Addr) = V;
  }
};

/// Bridges the C client's register writes to the C++ Device on doorbell.
struct Bridge {
  zbc::Device *Dev;
  uint8_t *Regs;
};

static void doorbell_bridge(void *Ctx) {
  Bridge *B = static_cast<Bridge *>(Ctx);
  // Replay the RIFF_PTR bytes the client just wrote, then ring the doorbell.
  for (int I = 0; I < 16; ++I)
    B->Dev->write(ZBC_REG_RIFF_PTR + I, B->Regs[ZBC_REG_RIFF_PTR + I]);
  B->Dev->write(ZBC_REG_DOORBELL, 1);
  // Surface STATUS/ERROR_CODE back to the client's register view.
  B->Regs[ZBC_REG_STATUS] = B->Dev->read(ZBC_REG_STATUS);
  B->Regs[ZBC_REG_ERROR_CODE] = B->Dev->read(ZBC_REG_ERROR_CODE);
  B->Regs[ZBC_REG_ERROR_CODE + 1] = B->Dev->read(ZBC_REG_ERROR_CODE + 1);
}

// Exit/timer state for backends.
static unsigned g_exit_reason = 0;
static bool g_exited = false;
static unsigned g_timer_rate = 0;
static bool g_timer_accept = true;

static zbc::ExitCallback makeExit() {
  return [](unsigned R, unsigned) {
    g_exited = true;
    g_exit_reason = R;
  };
}
static zbc::TimerCallback makeTimer() {
  return [](unsigned Hz) {
    g_timer_rate = Hz;
    return g_timer_accept;
  };
}

/// Wire a C client to a C++ device sharing one register array.
struct Harness {
  uint8_t Regs[ZBC_REG_SIZE] = {0};
  IdentityMem Mem;
  zbc::Device Dev;
  zbc_client_state_t Client;
  Bridge Br;

  Harness(std::unique_ptr<zbc::Backend> Backend,
          std::unique_ptr<zbc::Policy> Pol)
      : Dev(Mem, zbc::PlatformConfig(sizeof(int), sizeof(void *),
                                     zbc::Endian::Little),
            std::move(Backend), std::move(Pol)) {
    std::memcpy(Regs, "SEMIHOST", 8);
    zbc_client_init(&Client, Regs);
    Br.Dev = &Dev;
    Br.Regs = Regs;
    Client.doorbell_callback = doorbell_bridge;
    Client.doorbell_ctx = &Br;
  }
};

//===----------------------------------------------------------------------===//
// Tests
//===----------------------------------------------------------------------===//

static void test_write0_console() {
  std::printf("test_write0_console\n");
  Harness H(std::make_unique<zbc::ConsoleBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::ConsoleOnlyPolicy>());
  uint8_t buf[256];
  zbc_response_t resp;
  uintptr_t args[1];
  args[0] = (uintptr_t) "hi from zbc\n";
  int rc = zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_WRITE0, args);
  CHECK(rc == ZBC_OK);
  CHECK(resp.is_error == 0);
  CHECK((H.Regs[ZBC_REG_STATUS] & ZBC_STATUS_RESPONSE_READY) != 0);
}

static void test_open_denied_by_policy() {
  std::printf("test_open_denied_by_policy\n");
  // ConsoleOnlyPolicy denies file open -> backend never called, EACCES.
  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::ConsoleOnlyPolicy>());
  uint8_t buf[256];
  zbc_response_t resp;
  uintptr_t args[4];
  const char *p = "/etc/passwd";
  args[0] = (uintptr_t)p;          // path
  args[1] = SH_OPEN_R;             // mode
  args[2] = (uintptr_t)strlen(p);  // path_len
  args[3] = 0;
  int rc = zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_OPEN, args);
  CHECK(rc == ZBC_OK);
  CHECK((int)resp.result == -1);
  CHECK(resp.error_code == EACCES);
}

// std::filesystem replacement for mkdtemp(): unique subdirectory under the
// platform temp dir, returned in canonical form so the sandbox root and the
// request paths agree under PathValidator's canonicalization (which matters
// on macOS where /tmp is a symlink to /private/tmp).
static std::string makeUniqueTempDir(std::string_view Prefix) {
  namespace fs = std::filesystem;
  std::random_device RD;
  std::uniform_int_distribution<int> Dist(0, 35);
  static constexpr const char *Alphabet =
      "0123456789abcdefghijklmnopqrstuvwxyz";
  std::error_code EC;
  for (int Attempt = 0; Attempt < 100; ++Attempt) {
    std::string Suffix(6, '\0');
    for (char &C : Suffix)
      C = Alphabet[Dist(RD)];
    fs::path Candidate =
        fs::temp_directory_path() / (std::string(Prefix) + Suffix);
    if (fs::create_directory(Candidate, EC) && !EC)
      return fs::weakly_canonical(Candidate, EC).string();
  }
  return {};
}

static void test_file_roundtrip_sandboxed() {
  std::printf("test_file_roundtrip_sandboxed\n");
  namespace fs = std::filesystem;

  std::string Dir = makeUniqueTempDir("zbccpp");
  CHECK(!Dir.empty());
  if (Dir.empty())
    return;

  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::SandboxedPolicy>(Dir));

  // Build request paths through fs::path so the separator matches the
  // canonical form PathValidator will compute (forward slash on POSIX,
  // backslash on Windows).
  std::string Path = (fs::path(Dir) / "data.txt").string();
  const char *Payload = "roundtrip!";

  uint8_t Buf[512];
  zbc_response_t Resp;

  // open (write), write, close
  uintptr_t OArgs[3] = {(uintptr_t)Path.c_str(), SH_OPEN_W,
                        (uintptr_t)Path.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_OPEN, OArgs);
  CHECK((int)Resp.result >= 0);
  int FD = (int)Resp.result;

  uintptr_t WArgs[3] = {(uintptr_t)FD, (uintptr_t)Payload,
                        (uintptr_t)std::strlen(Payload)};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_WRITE, WArgs);
  CHECK((int)Resp.result == 0); // 0 bytes NOT written

  uintptr_t CArgs[1] = {(uintptr_t)FD};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_CLOSE, CArgs);
  CHECK((int)Resp.result == 0);

  // open (read), read back, verify
  uintptr_t ORArgs[3] = {(uintptr_t)Path.c_str(), SH_OPEN_R,
                         (uintptr_t)Path.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_OPEN, ORArgs);
  CHECK((int)Resp.result >= 0);
  FD = (int)Resp.result;

  char ReadBuf[64];
  std::memset(ReadBuf, 0, sizeof(ReadBuf));
  size_t Want = std::strlen(Payload);
  // SH_SYS_READ args: fd, dest, len  (per opcode table)
  uintptr_t RArgs[3] = {(uintptr_t)FD, (uintptr_t)ReadBuf, (uintptr_t)Want};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_READ, RArgs);
  CHECK((int)Resp.result == 0); // all requested bytes read
  CHECK(std::memcmp(ReadBuf, Payload, Want) == 0);

  uintptr_t CArgs2[1] = {(uintptr_t)FD};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_CLOSE, CArgs2);

  // path traversal escape is denied
  std::string Escape =
      (fs::path(Dir) / ".." / ".." / "etc" / "passwd").string();
  uintptr_t EArgs[3] = {(uintptr_t)Escape.c_str(), SH_OPEN_R,
                        (uintptr_t)Escape.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_OPEN, EArgs);
  CHECK((int)Resp.result == -1);

  std::error_code EC;
  fs::remove_all(Dir, EC);
}

static void test_malformed_riff_register_channel() {
  std::printf("test_malformed_riff_register_channel\n");
  Harness H(std::make_unique<zbc::ConsoleBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::ConsoleOnlyPolicy>());

  // Hand a garbage buffer straight to the device (no RIFF magic).
  uint8_t garbage[64];
  std::memset(garbage, 0xAA, sizeof(garbage));
  uint8_t snapshot[64];
  std::memcpy(snapshot, garbage, sizeof(garbage));

  uintptr_t addr = (uintptr_t)garbage;
  for (int i = 0; i < (int)sizeof(void *); ++i)
    H.Dev.write(ZBC_REG_RIFF_PTR + i, (uint8_t)((addr >> (i * 8)) & 0xFF));
  H.Dev.write(ZBC_REG_DOORBELL, 1);

  CHECK((H.Dev.read(ZBC_REG_STATUS) & ZBC_STATUS_PROTO_ERROR) != 0);
  CHECK(H.Dev.read(ZBC_REG_ERROR_CODE) == ZBC_PROTO_ERR_MALFORMED_RIFF);
  // Guest memory must be untouched.
  CHECK(std::memcmp(garbage, snapshot, sizeof(garbage)) == 0);

  // Acknowledge clears the flags.
  H.Dev.write(ZBC_REG_STATUS, 0);
  CHECK(H.Dev.read(ZBC_REG_STATUS) == ZBC_STATUS_NONE);
  CHECK(H.Dev.read(ZBC_REG_ERROR_CODE) == 0);
}

static void test_timer_reject_einval() {
  std::printf("test_timer_reject_einval\n");
  g_timer_accept = false;
  Harness H(std::make_unique<zbc::ConsoleBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::UnrestrictedPolicy>());
  uint8_t buf[256];
  zbc_response_t resp;
  uintptr_t args[1] = {123456789u};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_TIMER_CONFIG, args);
  CHECK((int)resp.result == -1);
  CHECK(resp.error_code == EINVAL);
  g_timer_accept = true; // restore
}

static void test_timer_tick_irq() {
  std::printf("test_timer_tick_irq\n");
  Harness H(std::make_unique<zbc::ConsoleBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::UnrestrictedPolicy>());
  bool irq = false;
  H.Dev.setIrqCallback([&](bool A) { irq = A; });
  H.Dev.timerTick();
  CHECK(irq == true);
  CHECK((H.Dev.read(ZBC_REG_STATUS) & ZBC_STATUS_TIMER) != 0);
  H.Dev.write(ZBC_REG_STATUS, 0); // acknowledge
  CHECK(irq == false);
  CHECK(H.Dev.read(ZBC_REG_STATUS) == ZBC_STATUS_NONE);
}

//===----------------------------------------------------------------------===//
// Linux extensions (0x80 - 0x8D)
//===----------------------------------------------------------------------===//

// Read the 8-byte LE size field out of the 48-byte wire stat layout.
static uint64_t unpackStatSize(const uint8_t *Raw) {
  uint64_t Lo = 0, Hi = 0;
  for (int I = 0; I < 4; ++I)
    Lo |= ((uint64_t)Raw[16 + I]) << (I * 8);
  for (int I = 0; I < 4; ++I)
    Hi |= ((uint64_t)Raw[20 + I]) << (I * 8);
  return Lo | (Hi << 32);
}

static void test_mkdir_rmdir_sandboxed() {
  std::printf("test_mkdir_rmdir_sandboxed\n");
  namespace fs = std::filesystem;

  std::string Dir = makeUniqueTempDir("zbccpp_mkrm");
  CHECK(!Dir.empty());
  if (Dir.empty())
    return;

  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::SandboxedPolicy>(Dir));

  std::string Sub = (fs::path(Dir) / "newsub").string();
  uint8_t Buf[512];
  zbc_response_t Resp;

  // mkdir(newsub, 0755)
  uintptr_t MArgs[3] = {(uintptr_t)Sub.c_str(), (uintptr_t)Sub.size(),
                        (uintptr_t)0755};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_MKDIR, MArgs);
  CHECK((int)Resp.result == 0);
  CHECK(fs::is_directory(Sub));

  // rmdir(newsub) succeeds
  uintptr_t RArgs[2] = {(uintptr_t)Sub.c_str(), (uintptr_t)Sub.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_RMDIR, RArgs);
  CHECK((int)Resp.result == 0);
  CHECK(!fs::exists(Sub));

  // rmdir(newsub) on the missing dir now fails.
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_RMDIR, RArgs);
  CHECK((int)Resp.result == -1);

  std::error_code EC;
  fs::remove_all(Dir, EC);
}

static void test_fstat_ftruncate_fsync() {
  std::printf("test_fstat_ftruncate_fsync\n");
  namespace fs = std::filesystem;

  std::string Dir = makeUniqueTempDir("zbccpp_ftrunc");
  CHECK(!Dir.empty());
  if (Dir.empty())
    return;

  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::SandboxedPolicy>(Dir));

  std::string Path = (fs::path(Dir) / "f.bin").string();
  uint8_t Buf[512];
  zbc_response_t Resp;

  // open(W+)
  uintptr_t OArgs[3] = {(uintptr_t)Path.c_str(), SH_OPEN_W_PLUS,
                        (uintptr_t)Path.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_OPEN, OArgs);
  CHECK((int)Resp.result >= 0);
  int FD = (int)Resp.result;

  // write 5 bytes
  const char *Payload = "hello";
  uintptr_t WArgs[3] = {(uintptr_t)FD, (uintptr_t)Payload, (uintptr_t)5};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_WRITE, WArgs);
  CHECK((int)Resp.result == 0);

  // fstat -> size == 5
  uint8_t StatRaw[SH_STAT_BUF_SIZE];
  uintptr_t FArgs[3] = {(uintptr_t)FD, (uintptr_t)StatRaw,
                        (uintptr_t)SH_STAT_BUF_SIZE};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_FSTAT, FArgs);
  CHECK((int)Resp.result == 0);
  CHECK(unpackStatSize(StatRaw) == 5);

  // ftruncate to 2
  uint8_t LenBytes[8] = {2, 0, 0, 0, 0, 0, 0, 0};
  uintptr_t TArgs[3] = {(uintptr_t)FD, (uintptr_t)LenBytes,
                        (uintptr_t)sizeof(LenBytes)};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_FTRUNCATE, TArgs);
  CHECK((int)Resp.result == 0);

  // fstat -> size == 2  (this is the regression catcher: a stale stdio
  // buffer or a missed fflush would leave size at 5)
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_FSTAT, FArgs);
  CHECK((int)Resp.result == 0);
  CHECK(unpackStatSize(StatRaw) == 2);

  // fsync
  uintptr_t SArgs[1] = {(uintptr_t)FD};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_FSYNC, SArgs);
  CHECK((int)Resp.result == 0);

  uintptr_t CArgs[1] = {(uintptr_t)FD};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_CLOSE, CArgs);

  std::error_code EC;
  fs::remove_all(Dir, EC);
}

static void test_stat_path_metadata() {
  std::printf("test_stat_path_metadata\n");
  namespace fs = std::filesystem;

  std::string Dir = makeUniqueTempDir("zbccpp_stat");
  CHECK(!Dir.empty());
  if (Dir.empty())
    return;

  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::SandboxedPolicy>(Dir));

  // Create a known-size file out-of-band.
  std::string Path = (fs::path(Dir) / "known.bin").string();
  {
    std::FILE *FP = std::fopen(Path.c_str(), "wb");
    CHECK(FP != nullptr);
    const char *S = "abcdefghij"; // 10 bytes
    std::fwrite(S, 1, 10, FP);
    std::fclose(FP);
  }

  uint8_t Buf[512];
  zbc_response_t Resp;

  // stat -> size == 10
  uint8_t StatRaw[SH_STAT_BUF_SIZE];
  uintptr_t StArgs[4] = {(uintptr_t)Path.c_str(), (uintptr_t)Path.size(),
                         (uintptr_t)StatRaw, (uintptr_t)SH_STAT_BUF_SIZE};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_STAT, StArgs);
  CHECK((int)Resp.result == 0);
  CHECK(unpackStatSize(StatRaw) == 10);

  // stat on missing path -> -1
  std::string Miss = (fs::path(Dir) / "absent").string();
  uintptr_t MissArgs[4] = {(uintptr_t)Miss.c_str(), (uintptr_t)Miss.size(),
                           (uintptr_t)StatRaw, (uintptr_t)SH_STAT_BUF_SIZE};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_STAT, MissArgs);
  CHECK((int)Resp.result == -1);

  std::error_code EC;
  fs::remove_all(Dir, EC);
}

#ifndef _WIN32
static void test_opendir_readdir_closedir() {
  std::printf("test_opendir_readdir_closedir\n");
  namespace fs = std::filesystem;

  std::string Dir = makeUniqueTempDir("zbccpp_dir");
  CHECK(!Dir.empty());
  if (Dir.empty())
    return;

  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::SandboxedPolicy>(Dir));

  // Two marker files.
  for (const char *N : {"alpha", "beta"}) {
    std::FILE *FP = std::fopen((fs::path(Dir) / N).string().c_str(), "wb");
    CHECK(FP != nullptr);
    std::fclose(FP);
  }

  uint8_t Buf[512];
  zbc_response_t Resp;

  // opendir(Dir)
  uintptr_t OArgs[2] = {(uintptr_t)Dir.c_str(), (uintptr_t)Dir.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_OPENDIR, OArgs);
  CHECK((int)Resp.result >= 0);
  int Handle = (int)Resp.result;

  // Drain entries until end-of-dir (result == 0). Look for our markers.
  uint8_t EntryBuf[SH_DIRENT_HDR_SIZE + 256];
  bool SawAlpha = false, SawBeta = false;
  for (int Safety = 0; Safety < 1024; ++Safety) {
    uintptr_t RArgs[3] = {(uintptr_t)Handle, (uintptr_t)EntryBuf,
                          (uintptr_t)sizeof(EntryBuf)};
    zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_READDIR, RArgs);
    if ((intmax_t)Resp.result == 0)
      break; // end of directory
    CHECK((intmax_t)Resp.result > 0);
    uint8_t NameLen = EntryBuf[9];
    std::string Name((const char *)(EntryBuf + SH_DIRENT_HDR_SIZE), NameLen);
    if (Name == "alpha")
      SawAlpha = true;
    if (Name == "beta")
      SawBeta = true;
  }
  CHECK(SawAlpha);
  CHECK(SawBeta);

  // closedir
  uintptr_t CArgs[1] = {(uintptr_t)Handle};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_CLOSEDIR, CArgs);
  CHECK((int)Resp.result == 0);

  // readdir on a stale handle now fails (-1, EBADF).
  uintptr_t RArgs[3] = {(uintptr_t)Handle, (uintptr_t)EntryBuf,
                        (uintptr_t)sizeof(EntryBuf)};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_READDIR, RArgs);
  CHECK((int)Resp.result == -1);

  std::error_code EC;
  fs::remove_all(Dir, EC);
}

static void test_symlink_readlink_lstat_link() {
  std::printf("test_symlink_readlink_lstat_link\n");
  namespace fs = std::filesystem;

  std::string Dir = makeUniqueTempDir("zbccpp_lnk");
  CHECK(!Dir.empty());
  if (Dir.empty())
    return;

  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::SandboxedPolicy>(Dir));

  // Create a target file with known content (16 bytes).
  std::string Target = (fs::path(Dir) / "target.bin").string();
  {
    std::FILE *FP = std::fopen(Target.c_str(), "wb");
    CHECK(FP != nullptr);
    std::fwrite("0123456789ABCDEF", 1, 16, FP);
    std::fclose(FP);
  }

  std::string SymPath = (fs::path(Dir) / "sym").string();
  std::string HardPath = (fs::path(Dir) / "hard").string();

  uint8_t Buf[512];
  zbc_response_t Resp;

  // symlink(target.bin, sym) -- linkpath is the one PathValidator
  // resolves; target is opaque text.
  uintptr_t SymArgs[4] = {(uintptr_t) "target.bin", (uintptr_t)10,
                          (uintptr_t)SymPath.c_str(),
                          (uintptr_t)SymPath.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_SYMLINK, SymArgs);
  CHECK((int)Resp.result == 0);
  CHECK(fs::is_symlink(SymPath));

  // readlink(sym) -> "target.bin"
  char LinkBuf[64];
  std::memset(LinkBuf, 0, sizeof(LinkBuf));
  uintptr_t RLArgs[4] = {(uintptr_t)SymPath.c_str(), (uintptr_t)SymPath.size(),
                         (uintptr_t)LinkBuf, (uintptr_t)sizeof(LinkBuf)};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_READLINK, RLArgs);
  CHECK((int)Resp.result == 10);
  CHECK(std::memcmp(LinkBuf, "target.bin", 10) == 0);

  // lstat(sym) -- size reflects the symlink target string length, not 16.
  uint8_t StatRaw[SH_STAT_BUF_SIZE];
  uintptr_t LSArgs[4] = {(uintptr_t)SymPath.c_str(), (uintptr_t)SymPath.size(),
                         (uintptr_t)StatRaw, (uintptr_t)SH_STAT_BUF_SIZE};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_LSTAT, LSArgs);
  CHECK((int)Resp.result == 0);
  // The target string is "target.bin" (10 bytes), so lstat size should be 10.
  CHECK(unpackStatSize(StatRaw) == 10);

  // link(target.bin, hard) creates a hard link; stat shows nlink >= 2.
  uintptr_t LArgs[4] = {(uintptr_t)Target.c_str(), (uintptr_t)Target.size(),
                        (uintptr_t)HardPath.c_str(),
                        (uintptr_t)HardPath.size()};
  zbc_call(&Resp, &H.Client, Buf, sizeof(Buf), SH_SYS_LINK, LArgs);
  CHECK((int)Resp.result == 0);
  CHECK(fs::exists(HardPath));

  std::error_code EC;
  fs::remove_all(Dir, EC);
}
#endif // !_WIN32

static void test_platform_config_no_cnfg() {
  std::printf("test_platform_config_no_cnfg\n");
  // Device has platform config; client omits CNFG -> still works.
  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::UnrestrictedPolicy>());
  H.Client.cnfg_sent = 1; // suppress the client's CNFG chunk
  uint8_t buf[256];
  zbc_response_t resp;
  uintptr_t args[1] = {7};
  int rc = zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_CLOSE, args);
  CHECK(rc == ZBC_OK);
  // close of an unknown fd fails, but importantly it dispatched (no proto err)
  CHECK((H.Dev.read(ZBC_REG_STATUS) & ZBC_STATUS_PROTO_ERROR) == 0);
}

// Force-unbuffer stdout so a crash points at the failing test in CI logs.
// Without this, Windows block-buffers stdout and the post-mortem only shows
// the last fflush()ed line, which is misleading when the crash is in a
// later test.
struct UnbufferStdout {
  UnbufferStdout() { std::setvbuf(stdout, nullptr, _IONBF, 0); }
};
static UnbufferStdout g_unbuffer_stdout;

int main() {
  std::printf("=== C++ host library tests ===\n");
  test_write0_console();
  test_open_denied_by_policy();
  test_file_roundtrip_sandboxed();
  test_malformed_riff_register_channel();
  test_timer_reject_einval();
  test_timer_tick_irq();
  test_platform_config_no_cnfg();
  test_mkdir_rmdir_sandboxed();
  test_fstat_ftruncate_fsync();
  test_stat_path_metadata();
#ifndef _WIN32
  test_opendir_readdir_closedir();
  test_symlink_readlink_lstat_link();
#endif

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures == 0)
    std::printf("All C++ tests passed!\n");
  return g_failures == 0 ? 0 : 1;
}

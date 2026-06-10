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

int main() {
  std::printf("=== C++ host library tests ===\n");
  test_write0_console();
  test_open_denied_by_policy();
  test_file_roundtrip_sandboxed();
  test_malformed_riff_register_channel();
  test_timer_reject_einval();
  test_timer_tick_irq();
  test_platform_config_no_cnfg();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures == 0)
    std::printf("All C++ tests passed!\n");
  return g_failures == 0 ? 0 : 1;
}

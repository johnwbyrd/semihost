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
#include <memory>
#include <string>
#include <unistd.h>

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

static void test_file_roundtrip_sandboxed() {
  std::printf("test_file_roundtrip_sandboxed\n");
  char tmpl[] = "/tmp/zbccppXXXXXX";
  char *dir = mkdtemp(tmpl);
  CHECK(dir != nullptr);
  if (!dir)
    return;

  Harness H(std::make_unique<zbc::FileBackend>(makeExit(), makeTimer()),
            std::make_unique<zbc::SandboxedPolicy>(dir));

  std::string path = std::string(dir) + "/data.txt";
  const char *payload = "roundtrip!";

  uint8_t buf[512];
  zbc_response_t resp;

  // open (write), write, close
  uintptr_t oargs[3] = {(uintptr_t)path.c_str(), SH_OPEN_W,
                        (uintptr_t)path.size()};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_OPEN, oargs);
  CHECK((int)resp.result >= 0);
  int fd = (int)resp.result;

  uintptr_t wargs[3] = {(uintptr_t)fd, (uintptr_t)payload,
                        (uintptr_t)std::strlen(payload)};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_WRITE, wargs);
  CHECK((int)resp.result == 0); // 0 bytes NOT written

  uintptr_t cargs[1] = {(uintptr_t)fd};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_CLOSE, cargs);
  CHECK((int)resp.result == 0);

  // open (read), read back, verify
  uintptr_t orargs[3] = {(uintptr_t)path.c_str(), SH_OPEN_R,
                         (uintptr_t)path.size()};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_OPEN, orargs);
  CHECK((int)resp.result >= 0);
  fd = (int)resp.result;

  char readbuf[64];
  std::memset(readbuf, 0, sizeof(readbuf));
  size_t want = std::strlen(payload);
  // SH_SYS_READ args: fd, dest, len  (per opcode table)
  uintptr_t rargs[3] = {(uintptr_t)fd, (uintptr_t)readbuf, (uintptr_t)want};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_READ, rargs);
  CHECK((int)resp.result == 0); // all requested bytes read
  CHECK(std::memcmp(readbuf, payload, want) == 0);

  uintptr_t cargs2[1] = {(uintptr_t)fd};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_CLOSE, cargs2);

  // path traversal escape is denied
  std::string escape = std::string(dir) + "/../../etc/passwd";
  uintptr_t eargs[3] = {(uintptr_t)escape.c_str(), SH_OPEN_R,
                        (uintptr_t)escape.size()};
  zbc_call(&resp, &H.Client, buf, sizeof(buf), SH_SYS_OPEN, eargs);
  CHECK((int)resp.result == -1);

  std::remove(path.c_str());
  rmdir(dir);
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

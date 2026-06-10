//===-- zbc/Policy.h - Security policy ---------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Security policy: per-operation allow/deny hooks checked by the Device
/// before each backend call (LSM-style: decision points, not wrappers).
/// The base class is secure by default -- everything is denied. Presets
/// cover the common cases; subclass for anything bespoke.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_POLICY_HPP
#define ZBC_POLICY_HPP

#include "zbc/Common.h"
#include "zbc/PathValidator.h"
#include "zbc/Protocol.h"

#include <memory>
#include <string>
#include <string_view>

namespace zbc {

/// Per-operation security decisions. Default: deny everything.
class Policy {
public:
  virtual ~Policy() = default;

  // File operations.
  virtual bool allowOpen(std::string_view, OpenMode) { return false; }
  virtual bool allowClose(int) { return true; } // denying could leak FDs
  virtual bool allowRead(int, std::size_t) { return false; }
  virtual bool allowWrite(int, ByteSpan) { return false; }
  virtual bool allowSeek(int, int64_t) { return false; }
  virtual bool allowFileLength(int) { return false; }
  virtual bool allowRemove(std::string_view) { return false; }
  virtual bool allowRename(std::string_view, std::string_view) { return false; }
  virtual bool allowTmpnam(int) { return false; }

  // Console operations.
  virtual bool allowReadChar() { return false; }
  virtual bool allowWriteChar(char) { return false; }
  virtual bool allowWriteString(std::string_view) { return false; }

  // System operations.
  virtual bool allowSystem(std::string_view) { return false; }
  virtual bool allowGetCmdLine() { return false; }
  virtual bool allowHeapInfo() { return false; }
  virtual bool allowTimerConfig(unsigned) { return false; }

  /// Resolve/authorize a path for path-based operations (after the allow*
  /// check). May rewrite the path (e.g. into the sandbox) or reject it.
  virtual Result<std::string> resolvePath(std::string_view Path, bool /*ForWrite*/) {
    return std::string(Path);
  }

  /// Add an allowed path prefix (only meaningful for SandboxedPolicy).
  virtual void addAllowedPath(std::string_view, bool) {}
};

/// Console I/O only: stdin/stdout/stderr, nothing else.
class ConsoleOnlyPolicy : public Policy {
public:
  bool allowReadChar() override { return true; }
  bool allowWriteChar(char) override { return true; }
  bool allowWriteString(std::string_view) override { return true; }
  bool allowRead(int FD, std::size_t) override { return FD >= 0 && FD <= 2; }
  bool allowWrite(int FD, ByteSpan) override { return FD >= 1 && FD <= 2; }
};

/// Everything allowed. DANGEROUS -- trusted guests only.
class UnrestrictedPolicy : public Policy {
public:
  bool allowOpen(std::string_view, OpenMode) override { return true; }
  bool allowRead(int, std::size_t) override { return true; }
  bool allowWrite(int, ByteSpan) override { return true; }
  bool allowSeek(int, int64_t) override { return true; }
  bool allowFileLength(int) override { return true; }
  bool allowRemove(std::string_view) override { return true; }
  bool allowRename(std::string_view, std::string_view) override { return true; }
  bool allowTmpnam(int) override { return true; }
  bool allowReadChar() override { return true; }
  bool allowWriteChar(char) override { return true; }
  bool allowWriteString(std::string_view) override { return true; }
  bool allowSystem(std::string_view) override { return true; }
  bool allowGetCmdLine() override { return true; }
  bool allowHeapInfo() override { return true; }
  bool allowTimerConfig(unsigned) override { return true; }
};

/// Filesystem access sandboxed to a directory, plus console and timer.
class SandboxedPolicy : public Policy {
public:
  explicit SandboxedPolicy(std::string_view SandboxDir);

  bool allowOpen(std::string_view, OpenMode) override { return true; }
  bool allowRead(int, std::size_t) override { return true; }
  bool allowWrite(int, ByteSpan) override { return true; }
  bool allowSeek(int, int64_t) override { return true; }
  bool allowFileLength(int) override { return true; }
  bool allowRemove(std::string_view) override { return true; }
  bool allowRename(std::string_view, std::string_view) override { return true; }
  bool allowTmpnam(int) override { return true; }
  bool allowReadChar() override { return true; }
  bool allowWriteChar(char) override { return true; }
  bool allowWriteString(std::string_view) override { return true; }
  bool allowTimerConfig(unsigned) override { return true; }

  Result<std::string> resolvePath(std::string_view Path, bool ForWrite) override;
  void addAllowedPath(std::string_view Prefix, bool AllowWrite) override;

private:
  std::unique_ptr<PathValidator> Validator_;
};

} // namespace zbc

#endif // ZBC_POLICY_HPP

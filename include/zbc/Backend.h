//===-- zbc/Backend.h - Semihosting backend ---------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// The Backend performs the actual host I/O for a semihosting operation.
/// The base class is inert (every operation returns an error); subclasses
/// add capability:
///
///   Backend (base)
///     └── ConsoleBackend  (stdin/stdout/stderr, time, exit, timer)
///             └── FileBackend  (file operations via FileDescTable)
///
/// Security ("is this allowed?") lives in the Policy layer, not here.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_BACKEND_H
#define ZBC_BACKEND_H

#include "zbc/Common.h"
#include "zbc/FileDescTable.h"
#include "zbc/Protocol.h"

#include <cerrno>
#include <chrono>
#include <functional>
#include <string_view>

namespace zbc {

/// Result of a backend operation: return value, errno, and optional data.
struct OpResult {
  intmax_t Value = 0;
  int Errno = 0;
  Bytes Data;

  static OpResult success(intmax_t V = 0) { return {V, 0, {}}; }
  static OpResult error(int E) { return {-1, E, {}}; }
  bool isError() const { return Value < 0 && Errno != 0; }
};

/// Called when the guest exits.
using ExitCallback = std::function<void(unsigned Reason, unsigned Subcode)>;
/// Called to (re)configure the periodic timer. Return false if the rate is
/// not achievable (reported to the guest as -1 / EINVAL).
using TimerCallback = std::function<bool(unsigned RateHz)>;

/// Abstract backend. Every operation fails by default.
class Backend {
public:
  virtual ~Backend() = default;

  // File operations.
  virtual OpResult open(std::string_view, OpenMode) { return OpResult::error(ENOSYS); }
  virtual OpResult close(int) { return OpResult::error(EBADF); }
  virtual OpResult read(int, std::size_t) { return OpResult::error(EBADF); }
  virtual OpResult write(int, ByteSpan) { return OpResult::error(EBADF); }
  virtual OpResult seek(int, int64_t) { return OpResult::error(EBADF); }
  virtual OpResult fileLength(int) { return OpResult::error(EBADF); }
  virtual OpResult remove(std::string_view) { return OpResult::error(ENOSYS); }
  virtual OpResult rename(std::string_view, std::string_view) {
    return OpResult::error(ENOSYS);
  }
  virtual OpResult tmpnam(int) { return OpResult::error(ENOSYS); }

  // Console operations.
  virtual void writeChar(char) {}
  virtual void writeString(std::string_view) {}
  virtual int readChar() { return -1; }

  // Queries.
  virtual bool isError(int Status) { return Status < 0; }
  virtual bool isTTY(int) { return false; }

  // Time.
  virtual OpResult clock() { return OpResult::error(ENOSYS); }
  virtual OpResult time() { return OpResult::error(ENOSYS); }
  virtual OpResult elapsed() { return OpResult::error(ENOSYS); }
  virtual OpResult tickFreq() { return OpResult::error(ENOSYS); }

  // System.
  virtual OpResult system(std::string_view) { return OpResult::error(ENOSYS); }
  virtual OpResult getCmdLine() { return OpResult::error(ENOSYS); }
  virtual OpResult heapInfo() { return OpResult::error(ENOSYS); }
  virtual int getErrno() { return LastErrno; }
  virtual void exit(unsigned Reason, unsigned Subcode) = 0;
  virtual OpResult timerConfig(unsigned) { return OpResult::error(ENOSYS); }

protected:
  int LastErrno = 0;
};

/// Backend with console I/O, time, exit, and timer support.
class ConsoleBackend : public Backend {
public:
  ConsoleBackend(ExitCallback OnExit, TimerCallback OnTimer = nullptr)
      : OnExit_(std::move(OnExit)), OnTimer_(std::move(OnTimer)) {}

  void writeChar(char C) override;
  void writeString(std::string_view Str) override;
  int readChar() override;
  OpResult read(int FD, std::size_t Count) override;
  OpResult write(int FD, ByteSpan Data) override;
  bool isTTY(int FD) override { return FD >= 0 && FD <= 2; }
  OpResult clock() override;
  OpResult time() override;
  void exit(unsigned Reason, unsigned Subcode) override;
  OpResult timerConfig(unsigned RateHz) override;

private:
  ExitCallback OnExit_;
  TimerCallback OnTimer_;
  std::chrono::steady_clock::time_point StartTime_ =
      std::chrono::steady_clock::now();
};

/// Backend that adds file operations. Paths arrive already resolved by the
/// Policy layer; this class performs no security checks of its own.
class FileBackend : public ConsoleBackend {
public:
  FileBackend(ExitCallback OnExit, TimerCallback OnTimer = nullptr)
      : ConsoleBackend(std::move(OnExit), std::move(OnTimer)) {}

  OpResult open(std::string_view Path, OpenMode Mode) override;
  OpResult close(int FD) override;
  OpResult read(int FD, std::size_t Count) override;
  OpResult write(int FD, ByteSpan Data) override;
  OpResult seek(int FD, int64_t Pos) override;
  OpResult fileLength(int FD) override;
  OpResult remove(std::string_view Path) override;
  OpResult rename(std::string_view OldPath, std::string_view NewPath) override;
  OpResult tmpnam(int Id) override;
  bool isTTY(int FD) override;

private:
  FileDescTable FDTable_;
  int TmpNameCounter_ = 0;
};

} // namespace zbc

#endif // ZBC_BACKEND_H

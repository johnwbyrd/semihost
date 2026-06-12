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

#ifndef ZBC_BACKEND_HPP
#define ZBC_BACKEND_HPP

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
  virtual int readCharPoll() { return -1; }

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

  // Linux extensions: file/dir/symlink.
  virtual OpResult stat(std::string_view) { return OpResult::error(ENOSYS); }
  virtual OpResult fstat(int) { return OpResult::error(EBADF); }
  virtual OpResult openDir(std::string_view) { return OpResult::error(ENOSYS); }
  virtual OpResult readDir(int) { return OpResult::error(EBADF); }
  virtual OpResult closeDir(int) { return OpResult::error(EBADF); }
  virtual OpResult mkdir(std::string_view, int /*Mode*/) {
    return OpResult::error(ENOSYS);
  }
  virtual OpResult rmdir(std::string_view) { return OpResult::error(ENOSYS); }
  virtual OpResult ftruncate(int, uint64_t /*Length*/) {
    return OpResult::error(EBADF);
  }
  virtual OpResult fsync(int) { return OpResult::error(EBADF); }
  virtual OpResult link(std::string_view, std::string_view) {
    return OpResult::error(ENOSYS);
  }
  virtual OpResult symlink(std::string_view, std::string_view) {
    return OpResult::error(ENOSYS);
  }
  virtual OpResult readLink(std::string_view, std::size_t /*MaxLen*/) {
    return OpResult::error(ENOSYS);
  }
  virtual OpResult lstat(std::string_view) { return OpResult::error(ENOSYS); }

protected:
  int LastErrno = 0;
};

/// Backend with console I/O, time, exit, and timer support.
class ConsoleBackend : public Backend {
public:
  ConsoleBackend(ExitCallback OnExit, TimerCallback OnTimer = nullptr)
      : OnExit(std::move(OnExit)), OnTimer(std::move(OnTimer)) {}

  void writeChar(char C) override;
  void writeString(std::string_view Str) override;
  int readChar() override;
  int readCharPoll() override;
  OpResult read(int FD, std::size_t Count) override;
  OpResult write(int FD, ByteSpan Data) override;
  bool isTTY(int FD) override { return FD >= 0 && FD <= 2; }
  OpResult clock() override;
  OpResult time() override;
  void exit(unsigned Reason, unsigned Subcode) override;
  OpResult timerConfig(unsigned RateHz) override;

private:
  ExitCallback OnExit;
  TimerCallback OnTimer;
  std::chrono::steady_clock::time_point StartTime =
      std::chrono::steady_clock::now();
};

/// Backend that adds file operations. Paths arrive already resolved by the
/// Policy layer; this class performs no security checks of its own.
class FileBackend : public ConsoleBackend {
public:
  /// Maximum simultaneously open directories.
  static constexpr int MaxDirs = 8;
  /// Directory handles start here, well above any FILE* fd.
  static constexpr int FirstDirHandle = 256;

  FileBackend(ExitCallback OnExit, TimerCallback OnTimer = nullptr)
      : ConsoleBackend(std::move(OnExit), std::move(OnTimer)) {
    for (int I = 0; I < MaxDirs; ++I)
      Dirs[I] = nullptr;
  }

  ~FileBackend() override;

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

  // Linux extensions.
  OpResult stat(std::string_view Path) override;
  OpResult fstat(int FD) override;
  OpResult openDir(std::string_view Path) override;
  OpResult readDir(int Handle) override;
  OpResult closeDir(int Handle) override;
  OpResult mkdir(std::string_view Path, int Mode) override;
  OpResult rmdir(std::string_view Path) override;
  OpResult ftruncate(int FD, uint64_t Length) override;
  OpResult fsync(int FD) override;
  OpResult link(std::string_view OldPath, std::string_view NewPath) override;
  OpResult symlink(std::string_view Target, std::string_view LinkPath) override;
  OpResult readLink(std::string_view Path, std::size_t MaxLen) override;
  OpResult lstat(std::string_view Path) override;

private:
  FileDescTable FDTable;
  int TmpNameCounter = 0;
  /// Opaque DIR* slots (stored as void* so the header avoids <dirent.h>,
  /// which is POSIX-only).
  void *Dirs[MaxDirs];
};

} // namespace zbc

#endif // ZBC_BACKEND_HPP

//===-- zbc/FileDescTable.h - FD pool ---------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// RAII table mapping integer file descriptors to FILE* handles. FDs 0-2
/// are stdin/stdout/stderr; user files start at 3.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_FILE_DESC_TABLE_HPP
#define ZBC_FILE_DESC_TABLE_HPP

#include <array>
#include <cstdio>

namespace zbc {

class FileDescTable {
public:
  static constexpr int MaxFiles = 64;
  static constexpr int FirstUserFD = 3;

  FileDescTable();
  ~FileDescTable();

  FileDescTable(const FileDescTable &) = delete;
  FileDescTable &operator=(const FileDescTable &) = delete;
  FileDescTable(FileDescTable &&Other) noexcept;
  FileDescTable &operator=(FileDescTable &&Other) noexcept;

  /// Allocate an FD for FP, or -1 if full.
  int allocate(std::FILE *FP);
  /// Close and release an FD (no-op for stdio). Returns true if released.
  bool release(int FD);
  /// FILE* for FD, or nullptr if invalid.
  std::FILE *get(int FD) const;
  bool isValid(int FD) const;
  bool isStdio(int FD) const { return FD >= 0 && FD < FirstUserFD; }
  void closeAll();

private:
  std::array<std::FILE *, MaxFiles> Files_;
};

} // namespace zbc

#endif // ZBC_FILE_DESC_TABLE_HPP

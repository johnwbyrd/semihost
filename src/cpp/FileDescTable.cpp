//===-- FileDescTable.cpp - FD pool -----------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//

#include "zbc/FileDescTable.h"

namespace zbc {

FileDescTable::FileDescTable() {
  Files.fill(nullptr);
  Files[0] = stdin;
  Files[1] = stdout;
  Files[2] = stderr;
}

FileDescTable::~FileDescTable() { closeAll(); }

FileDescTable::FileDescTable(FileDescTable &&Other) noexcept
    : Files(Other.Files) {
  Other.Files.fill(nullptr);
}

FileDescTable &FileDescTable::operator=(FileDescTable &&Other) noexcept {
  if (this != &Other) {
    closeAll();
    Files = Other.Files;
    Other.Files.fill(nullptr);
  }
  return *this;
}

int FileDescTable::allocate(std::FILE *FP) {
  if (!FP)
    return -1;
  for (int I = FirstUserFD; I < MaxFiles; ++I) {
    if (Files[I] == nullptr) {
      Files[I] = FP;
      return I;
    }
  }
  return -1;
}

bool FileDescTable::release(int FD) {
  if (FD < FirstUserFD || FD >= MaxFiles)
    return false;
  if (Files[FD] == nullptr)
    return false;
  std::fclose(Files[FD]);
  Files[FD] = nullptr;
  return true;
}

std::FILE *FileDescTable::get(int FD) const {
  if (FD < 0 || FD >= MaxFiles)
    return nullptr;
  return Files[FD];
}

bool FileDescTable::isValid(int FD) const {
  if (FD < 0 || FD >= MaxFiles)
    return false;
  return Files[FD] != nullptr;
}

void FileDescTable::closeAll() {
  for (int I = FirstUserFD; I < MaxFiles; ++I) {
    if (Files[I] != nullptr) {
      std::fclose(Files[I]);
      Files[I] = nullptr;
    }
  }
}

} // namespace zbc

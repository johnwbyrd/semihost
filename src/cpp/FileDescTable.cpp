//===-- FileDescTable.cpp - FD pool -----------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//

#include "zbc/FileDescTable.h"

namespace zbc {

FileDescTable::FileDescTable() {
  Files_.fill(nullptr);
  Files_[0] = stdin;
  Files_[1] = stdout;
  Files_[2] = stderr;
}

FileDescTable::~FileDescTable() { closeAll(); }

FileDescTable::FileDescTable(FileDescTable &&Other) noexcept
    : Files_(Other.Files_) {
  Other.Files_.fill(nullptr);
}

FileDescTable &FileDescTable::operator=(FileDescTable &&Other) noexcept {
  if (this != &Other) {
    closeAll();
    Files_ = Other.Files_;
    Other.Files_.fill(nullptr);
  }
  return *this;
}

int FileDescTable::allocate(std::FILE *FP) {
  if (!FP)
    return -1;
  for (int I = FirstUserFD; I < MaxFiles; ++I) {
    if (Files_[I] == nullptr) {
      Files_[I] = FP;
      return I;
    }
  }
  return -1;
}

bool FileDescTable::release(int FD) {
  if (FD < FirstUserFD || FD >= MaxFiles)
    return false;
  if (Files_[FD] == nullptr)
    return false;
  std::fclose(Files_[FD]);
  Files_[FD] = nullptr;
  return true;
}

std::FILE *FileDescTable::get(int FD) const {
  if (FD < 0 || FD >= MaxFiles)
    return nullptr;
  return Files_[FD];
}

bool FileDescTable::isValid(int FD) const {
  if (FD < 0 || FD >= MaxFiles)
    return false;
  return Files_[FD] != nullptr;
}

void FileDescTable::closeAll() {
  for (int I = FirstUserFD; I < MaxFiles; ++I) {
    if (Files_[I] != nullptr) {
      std::fclose(Files_[I]);
      Files_[I] = nullptr;
    }
  }
}

} // namespace zbc

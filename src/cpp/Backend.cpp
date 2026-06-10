//===-- Backend.cpp - Console + File backends -------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//

#include "zbc/Backend.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace zbc {

//===----------------------------------------------------------------------===//
// ConsoleBackend
//===----------------------------------------------------------------------===//

void ConsoleBackend::writeChar(char C) {
  std::putchar(C);
  std::fflush(stdout);
}

void ConsoleBackend::writeString(std::string_view Str) {
  std::fwrite(Str.data(), 1, Str.size(), stdout);
  std::fflush(stdout);
}

int ConsoleBackend::readChar() { return std::getchar(); }

OpResult ConsoleBackend::read(int FD, std::size_t Count) {
  if (FD != 0)
    return OpResult::error(EBADF);
  OpResult R;
  R.Data.resize(Count);
  std::size_t Got = std::fread(R.Data.data(), 1, Count, stdin);
  R.Data.resize(Got);
  R.Value = (intmax_t)(Count - Got); // bytes NOT read
  R.Errno = 0;
  return R;
}

OpResult ConsoleBackend::write(int FD, ByteSpan Data) {
  std::FILE *Stream = (FD == 1) ? stdout : (FD == 2) ? stderr : nullptr;
  if (!Stream)
    return OpResult::error(EBADF);
  std::size_t Wrote = std::fwrite(Data.data(), 1, Data.size(), Stream);
  std::fflush(Stream);
  return OpResult::success((intmax_t)(Data.size() - Wrote)); // bytes NOT written
}

OpResult ConsoleBackend::clock() {
  auto Now = std::chrono::steady_clock::now();
  auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(Now - StartTime_);
  return OpResult::success((intmax_t)(Ms.count() / 10)); // centiseconds
}

OpResult ConsoleBackend::time() {
  return OpResult::success((intmax_t)std::time(nullptr));
}

void ConsoleBackend::exit(unsigned Reason, unsigned Subcode) {
  if (OnExit_)
    OnExit_(Reason, Subcode);
}

OpResult ConsoleBackend::timerConfig(unsigned RateHz) {
  if (!OnTimer_)
    return OpResult::error(ENOSYS);
  if (!OnTimer_(RateHz))
    return OpResult::error(EINVAL); // rate not achievable
  return OpResult::success();
}

//===----------------------------------------------------------------------===//
// FileBackend
//===----------------------------------------------------------------------===//

OpResult FileBackend::open(std::string_view Path, OpenMode Mode) {
  const char *ModeStr = openModeToString(Mode);
  if (!ModeStr) {
    LastErrno = EINVAL;
    return OpResult::error(EINVAL);
  }
  std::string PathStr(Path);
  std::FILE *FP = std::fopen(PathStr.c_str(), ModeStr);
  if (!FP) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  int FD = FDTable_.allocate(FP);
  if (FD < 0) {
    std::fclose(FP);
    LastErrno = EMFILE;
    return OpResult::error(EMFILE);
  }
  return OpResult::success(FD);
}

OpResult FileBackend::close(int FD) {
  if (FDTable_.isStdio(FD))
    return OpResult::success();
  if (!FDTable_.isValid(FD)) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  FDTable_.release(FD);
  return OpResult::success();
}

OpResult FileBackend::read(int FD, std::size_t Count) {
  if (FD == 0)
    return ConsoleBackend::read(FD, Count);
  std::FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  OpResult R;
  R.Data.resize(Count);
  std::size_t Got = std::fread(R.Data.data(), 1, Count, FP);
  R.Data.resize(Got);
  if (Got < Count && std::ferror(FP)) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  R.Value = (intmax_t)(Count - Got);
  R.Errno = 0;
  return R;
}

OpResult FileBackend::write(int FD, ByteSpan Data) {
  if (FD == 1 || FD == 2)
    return ConsoleBackend::write(FD, Data);
  std::FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  std::size_t Wrote = std::fwrite(Data.data(), 1, Data.size(), FP);
  if (Wrote < Data.size()) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success((intmax_t)(Data.size() - Wrote));
}

OpResult FileBackend::seek(int FD, int64_t Pos) {
  std::FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  if (std::fseek(FP, (long)Pos, SEEK_SET) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success();
}

OpResult FileBackend::fileLength(int FD) {
  std::FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  long Cur = std::ftell(FP);
  if (Cur < 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  if (std::fseek(FP, 0, SEEK_END) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  long Len = std::ftell(FP);
  if (Len < 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  std::fseek(FP, Cur, SEEK_SET);
  return OpResult::success(Len);
}

OpResult FileBackend::remove(std::string_view Path) {
  std::string PathStr(Path);
  if (std::remove(PathStr.c_str()) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success();
}

OpResult FileBackend::rename(std::string_view OldPath, std::string_view NewPath) {
  std::string Old(OldPath), New(NewPath);
  if (std::rename(Old.c_str(), New.c_str()) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success();
}

OpResult FileBackend::tmpnam(int Id) {
  char Buf[64];
  std::snprintf(Buf, sizeof(Buf), "tmp%05d_%03d", TmpNameCounter_++, Id);
  OpResult R;
  R.Value = 0;
  R.Errno = 0;
  std::size_t Len = std::strlen(Buf);
  R.Data.assign(Buf, Buf + Len + 1); // include null terminator
  return R;
}

bool FileBackend::isTTY(int FD) { return FDTable_.isStdio(FD); }

} // namespace zbc

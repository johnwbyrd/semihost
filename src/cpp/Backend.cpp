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
#include <sys/stat.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // GetStdHandle, PeekConsoleInputW, PeekNamedPipe, etc.
#include <direct.h>  // _mkdir, _rmdir
#include <io.h>      // _chsize_s, _commit, _fileno
#else
#include <dirent.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace zbc {

namespace {

// Little-endian pack helpers for the 48-byte stat and dirent layouts.
void lePackU32(uint8_t *P, uint32_t V) {
  P[0] = (uint8_t)V;
  P[1] = (uint8_t)(V >> 8);
  P[2] = (uint8_t)(V >> 16);
  P[3] = (uint8_t)(V >> 24);
}

void lePackU64(uint8_t *P, uint64_t V) {
  lePackU32(P, (uint32_t)V);
  lePackU32(P + 4, (uint32_t)((V >> 16) >> 16));
}

// Pack a host struct stat into the 48-byte wire layout
//   ino[8] mode[4] nlink[4] size[8] mtime[8] atime[8] ctime[8]
Bytes packStat(const struct stat &St) {
  Bytes Out(StatBufSize);
  uint8_t *P = Out.data();
  lePackU64(P + 0, (uint64_t)St.st_ino);
  lePackU32(P + 8, (uint32_t)St.st_mode);
  lePackU32(P + 12, (uint32_t)St.st_nlink);
  lePackU64(P + 16, (uint64_t)St.st_size);
  lePackU64(P + 24, (uint64_t)St.st_mtime);
  lePackU64(P + 32, (uint64_t)St.st_atime);
  lePackU64(P + 40, (uint64_t)St.st_ctime);
  return Out;
}

} // namespace

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

int ConsoleBackend::readCharPoll() {
#ifdef _WIN32
  // Windows has no select() over arbitrary handles, so branch on the
  // concrete kind of stdin (see the matching C implementation in
  // zbc_ansi_common.c for the rationale):
  //   FILE_TYPE_CHAR  -> PeekConsoleInputW / ReadConsoleInputW
  //   FILE_TYPE_PIPE  -> PeekNamedPipe + ReadFile when data is queued
  //   FILE_TYPE_DISK  -> ReadFile directly (returns 0 at EOF)
  // All three bypass stdio so the polled byte does not race a
  // line-buffered getchar() buffer.
  HANDLE H = GetStdHandle(STD_INPUT_HANDLE);
  if (H == INVALID_HANDLE_VALUE || H == nullptr)
    return -1;
  DWORD FT = GetFileType(H);

  if (FT == FILE_TYPE_CHAR) {
    INPUT_RECORD IR;
    DWORD Avail = 0;
    DWORD N = 0;
    while (PeekConsoleInputW(H, &IR, 1, &Avail) && Avail > 0) {
      if (IR.EventType == KEY_EVENT && IR.Event.KeyEvent.bKeyDown &&
          IR.Event.KeyEvent.uChar.AsciiChar != 0) {
        if (!ReadConsoleInputW(H, &IR, 1, &N) || N != 1)
          return -1;
        return (int)(unsigned char)IR.Event.KeyEvent.uChar.AsciiChar;
      }
      // Drain one uninteresting event (mouse / focus / resize) and loop.
      if (!ReadConsoleInputW(H, &IR, 1, &N) || N != 1)
        return -1;
    }
    return -1;
  }

  if (FT == FILE_TYPE_PIPE) {
    DWORD Avail = 0;
    if (!PeekNamedPipe(H, nullptr, 0, nullptr, &Avail, nullptr) || Avail == 0)
      return -1;
    // Fall through to ReadFile: avail > 0 means it returns immediately.
  } else if (FT != FILE_TYPE_DISK) {
    // Socket / unknown -- be conservative.
    return -1;
  }

  unsigned char C;
  DWORD Got = 0;
  if (!ReadFile(H, &C, 1, &Got, nullptr) || Got != 1)
    return -1;
  return (int)C;
#else
  fd_set RFDs;
  struct timeval TV;
  unsigned char C;

  FD_ZERO(&RFDs);
  FD_SET(0, &RFDs);
  TV.tv_sec = 0;
  TV.tv_usec = 0;
  if (::select(1, &RFDs, nullptr, nullptr, &TV) <= 0)
    return -1;
  if (::read(0, &C, 1) != 1)
    return -1;
  return (int)C;
#endif
}

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
  auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(Now - StartTime);
  return OpResult::success((intmax_t)(Ms.count() / 10)); // centiseconds
}

OpResult ConsoleBackend::time() {
  return OpResult::success((intmax_t)std::time(nullptr));
}

void ConsoleBackend::exit(unsigned Reason, unsigned Subcode) {
  if (OnExit)
    OnExit(Reason, Subcode);
}

OpResult ConsoleBackend::timerConfig(unsigned RateHz) {
  if (!OnTimer)
    return OpResult::error(ENOSYS);
  if (!OnTimer(RateHz))
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
  int FD = FDTable.allocate(FP);
  if (FD < 0) {
    std::fclose(FP);
    LastErrno = EMFILE;
    return OpResult::error(EMFILE);
  }
  return OpResult::success(FD);
}

OpResult FileBackend::close(int FD) {
  if (FDTable.isStdio(FD))
    return OpResult::success();
  if (!FDTable.isValid(FD)) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  FDTable.release(FD);
  return OpResult::success();
}

OpResult FileBackend::read(int FD, std::size_t Count) {
  if (FD == 0)
    return ConsoleBackend::read(FD, Count);
  std::FILE *FP = FDTable.get(FD);
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
  std::FILE *FP = FDTable.get(FD);
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
  std::FILE *FP = FDTable.get(FD);
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
  std::FILE *FP = FDTable.get(FD);
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
  std::snprintf(Buf, sizeof(Buf), "tmp%05d_%03d", TmpNameCounter++, Id);
  OpResult R;
  R.Value = 0;
  R.Errno = 0;
  std::size_t Len = std::strlen(Buf);
  R.Data.assign(Buf, Buf + Len + 1); // include null terminator
  return R;
}

bool FileBackend::isTTY(int FD) { return FDTable.isStdio(FD); }

FileBackend::~FileBackend() {
#ifndef _WIN32
  for (int I = 0; I < MaxDirs; ++I) {
    if (Dirs[I]) {
      ::closedir((DIR *)Dirs[I]);
      Dirs[I] = nullptr;
    }
  }
#endif
}

//===----------------------------------------------------------------------===//
// FileBackend - Linux extensions
//===----------------------------------------------------------------------===//

OpResult FileBackend::stat(std::string_view Path) {
  std::string P(Path);
  struct stat St;
  if (::stat(P.c_str(), &St) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  OpResult R;
  R.Value = 0;
  R.Errno = 0;
  R.Data = packStat(St);
  return R;
}

OpResult FileBackend::fstat(int FD) {
  std::FILE *FP = FDTable.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  // Flush stdio so st_size reflects everything the caller has written.
  std::fflush(FP);
  // Use plain fstat / struct stat on both platforms. MSVC aliases fstat
  // to the size-matched _fstat32i64 / _fstat64i32 (matching struct stat),
  // so we don't have to pick the variant ourselves; passing a struct
  // stat to _fstat64 would write past the buffer and trip 0xC0000409.
  struct stat St;
  if (::fstat(::fileno(FP), &St) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  OpResult R;
  R.Value = 0;
  R.Errno = 0;
  R.Data = packStat(St);
  return R;
}

OpResult FileBackend::openDir(std::string_view Path) {
#ifdef _WIN32
  (void)Path;
  LastErrno = ENOSYS;
  return OpResult::error(ENOSYS);
#else
  int Slot;
  for (Slot = 0; Slot < MaxDirs; ++Slot) {
    if (Dirs[Slot] == nullptr)
      break;
  }
  if (Slot == MaxDirs) {
    LastErrno = EMFILE;
    return OpResult::error(EMFILE);
  }
  std::string P(Path);
  DIR *D = ::opendir(P.c_str());
  if (!D) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  Dirs[Slot] = D;
  return OpResult::success(FirstDirHandle + Slot);
#endif
}

OpResult FileBackend::readDir(int Handle) {
#ifdef _WIN32
  (void)Handle;
  LastErrno = ENOSYS;
  return OpResult::error(ENOSYS);
#else
  int Slot = Handle - FirstDirHandle;
  if (Slot < 0 || Slot >= MaxDirs || Dirs[Slot] == nullptr) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  errno = 0;
  struct dirent *DE = ::readdir((DIR *)Dirs[Slot]);
  if (!DE) {
    if (errno != 0) {
      LastErrno = errno;
      return OpResult::error(errno);
    }
    return OpResult::success(0); // clean end-of-directory
  }
  std::size_t NameLen = std::strlen(DE->d_name);
  if (NameLen > 255) {
    LastErrno = EINVAL;
    return OpResult::error(EINVAL);
  }
  std::size_t Need = DirentHdrSize + NameLen + 1;
  OpResult R;
  R.Value = (intmax_t)Need;
  R.Errno = 0;
  R.Data.resize(Need);
  uint8_t *Out = R.Data.data();
  lePackU64(Out + 0, (uint64_t)DE->d_ino);
  Out[8] = (uint8_t)DE->d_type;
  Out[9] = (uint8_t)NameLen;
  std::memcpy(Out + 10, DE->d_name, NameLen + 1);
  return R;
#endif
}

OpResult FileBackend::closeDir(int Handle) {
#ifdef _WIN32
  (void)Handle;
  LastErrno = ENOSYS;
  return OpResult::error(ENOSYS);
#else
  int Slot = Handle - FirstDirHandle;
  if (Slot < 0 || Slot >= MaxDirs || Dirs[Slot] == nullptr) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  int Rc = ::closedir((DIR *)Dirs[Slot]);
  Dirs[Slot] = nullptr;
  if (Rc != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success();
#endif
}

OpResult FileBackend::mkdir(std::string_view Path, int Mode) {
  std::string P(Path);
#ifdef _WIN32
  (void)Mode;
  if (::_mkdir(P.c_str()) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
#else
  if (::mkdir(P.c_str(), (mode_t)Mode) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
#endif
  return OpResult::success();
}

OpResult FileBackend::rmdir(std::string_view Path) {
  std::string P(Path);
#ifdef _WIN32
  if (::_rmdir(P.c_str()) != 0) {
#else
  if (::rmdir(P.c_str()) != 0) {
#endif
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success();
}

OpResult FileBackend::ftruncate(int FD, uint64_t Length) {
  std::FILE *FP = FDTable.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  std::fflush(FP);
#ifdef _WIN32
  if (::_chsize_s(::_fileno(FP), (__int64)Length) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
#else
  if (::ftruncate(::fileno(FP), (off_t)Length) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
#endif
  return OpResult::success();
}

OpResult FileBackend::fsync(int FD) {
  std::FILE *FP = FDTable.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }
  std::fflush(FP);
#ifdef _WIN32
  if (::_commit(::_fileno(FP)) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
#else
  if (::fsync(::fileno(FP)) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
#endif
  return OpResult::success();
}

OpResult FileBackend::link(std::string_view OldPath, std::string_view NewPath) {
#ifdef _WIN32
  (void)OldPath;
  (void)NewPath;
  LastErrno = ENOSYS;
  return OpResult::error(ENOSYS);
#else
  std::string Old(OldPath), New(NewPath);
  if (::link(Old.c_str(), New.c_str()) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success();
#endif
}

OpResult FileBackend::symlink(std::string_view Target,
                              std::string_view LinkPath) {
#ifdef _WIN32
  (void)Target;
  (void)LinkPath;
  LastErrno = ENOSYS;
  return OpResult::error(ENOSYS);
#else
  std::string T(Target), L(LinkPath);
  if (::symlink(T.c_str(), L.c_str()) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  return OpResult::success();
#endif
}

OpResult FileBackend::readLink(std::string_view Path, std::size_t MaxLen) {
#ifdef _WIN32
  (void)Path;
  (void)MaxLen;
  LastErrno = ENOSYS;
  return OpResult::error(ENOSYS);
#else
  std::string P(Path);
  OpResult R;
  R.Data.resize(MaxLen);
  ssize_t N = ::readlink(P.c_str(), (char *)R.Data.data(), MaxLen);
  if (N < 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }
  R.Data.resize((std::size_t)N);
  R.Value = (intmax_t)N;
  R.Errno = 0;
  return R;
#endif
}

OpResult FileBackend::lstat(std::string_view Path) {
  std::string P(Path);
  struct stat St;
#ifdef _WIN32
  // Windows has no lstat(); regular stat() is the closest approximation
  // and is correct for files that aren't NTFS reparse points.
  if (::stat(P.c_str(), &St) != 0) {
#else
  if (::lstat(P.c_str(), &St) != 0) {
#endif
    LastErrno = errno;
    return OpResult::error(errno);
  }
  OpResult R;
  R.Value = 0;
  R.Errno = 0;
  R.Data = packStat(St);
  return R;
}

} // namespace zbc

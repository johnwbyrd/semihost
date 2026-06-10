//===-- zbc/Common.h - Common C++ types -------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Lightweight, dependency-free building blocks for the C++ host library:
/// a byte span, an owned byte buffer, and Result/Status error types. These
/// pull in nothing beyond the standard library, so the library embeds
/// cleanly in any C++17 codebase.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_COMMON_HPP
#define ZBC_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zbc {

/// Owned, growable byte buffer.
using Bytes = std::vector<uint8_t>;

/// Non-owning view over a contiguous range of T.
///
/// A minimal stand-in for std::span (which requires C++20). Const and
/// mutable variants are the two instantiations used by the library.
template <typename T> class Span {
public:
  constexpr Span() = default;
  constexpr Span(T *Data, std::size_t Size) : Data_(Data), Size_(Size) {}

  /// Construct from a std::vector (const view).
  template <typename U = T,
            typename = std::enable_if_t<std::is_const<U>::value>>
  Span(const std::vector<std::remove_const_t<T>> &V)
      : Data_(V.data()), Size_(V.size()) {}

  /// Construct a mutable view from a std::vector.
  template <typename U = T,
            typename = std::enable_if_t<!std::is_const<U>::value>>
  Span(std::vector<T> &V) : Data_(V.data()), Size_(V.size()) {}

  constexpr T *data() const { return Data_; }
  constexpr std::size_t size() const { return Size_; }
  constexpr bool empty() const { return Size_ == 0; }
  constexpr T *begin() const { return Data_; }
  constexpr T *end() const { return Data_ + Size_; }
  constexpr T &operator[](std::size_t I) const { return Data_[I]; }

  /// Sub-span starting at Off for Count elements (Count clamped to bounds).
  Span subspan(std::size_t Off, std::size_t Count) const {
    if (Off > Size_)
      Off = Size_;
    if (Count > Size_ - Off)
      Count = Size_ - Off;
    return Span(Data_ + Off, Count);
  }

private:
  T *Data_ = nullptr;
  std::size_t Size_ = 0;
};

/// Read-only view of bytes (syscall input data).
using ByteSpan = Span<const uint8_t>;
/// Mutable view of bytes (buffers being filled).
using MutableByteSpan = Span<uint8_t>;

/// Make a ByteSpan from a std::string's characters (no null terminator).
inline ByteSpan asBytes(const std::string &S) {
  return ByteSpan(reinterpret_cast<const uint8_t *>(S.data()), S.size());
}

//===----------------------------------------------------------------------===//
// Status / Result
//===----------------------------------------------------------------------===//

/// Success-or-error with an optional message.
class Status {
public:
  static Status success() { return Status(true, {}); }
  static Status error(std::string Msg) { return Status(false, std::move(Msg)); }

  bool ok() const { return Ok_; }
  explicit operator bool() const { return Ok_; }
  const std::string &message() const { return Msg_; }

private:
  Status(bool Ok, std::string Msg) : Ok_(Ok), Msg_(std::move(Msg)) {}
  bool Ok_;
  std::string Msg_;
};

/// A value or an error.
///
/// T need not be default-constructible; the error state stores no T.
template <typename T> class Result {
public:
  Result(T Value) : Value_(std::move(Value)) {}
  static Result error(std::string Msg) {
    Result R;
    R.Msg_ = std::move(Msg);
    return R;
  }

  bool ok() const { return Value_.has_value(); }
  explicit operator bool() const { return ok(); }
  const std::string &message() const { return Msg_; }

  T &operator*() { return *Value_; }
  const T &operator*() const { return *Value_; }
  T *operator->() { return &*Value_; }
  const T *operator->() const { return &*Value_; }

private:
  Result() = default;
  std::optional<T> Value_;
  std::string Msg_;
};

} // namespace zbc

#endif // ZBC_COMMON_HPP

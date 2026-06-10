//===-- zbc/PathValidator.h - Path sandboxing -------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Path validation and sandboxing for the SandboxedPolicy. Resolves paths
/// (including "..", ".", and symlinks) and confirms the result stays inside
/// the sandbox directory or an explicitly allowed prefix.
///
//===----------------------------------------------------------------------===//

#ifndef ZBC_PATHVALIDATOR_H
#define ZBC_PATHVALIDATOR_H

#include "zbc/Common.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zbc {

/// Reasons a path may be rejected.
enum class ViolationType {
  SandboxEscape,  ///< Escapes the sandbox via "..", symlink, or unresolved path
  NullByte,       ///< Path contains a null byte
  NotAllowed,     ///< Path not within sandbox or allowed list
  WriteProtected, ///< Write attempted on a read-only path / read-only mode
};

using ViolationCallback =
    std::function<void(ViolationType Type, std::string_view Path)>;

struct PathValidatorConfig {
  std::string SandboxDir;
  bool ReadOnly = false;
  std::vector<std::pair<std::string, bool>> AllowedPaths; // (prefix, allowWrite)
  ViolationCallback OnViolation;
};

class PathValidator {
public:
  explicit PathValidator(PathValidatorConfig Config);

  /// Resolve and authorize Path. Returns the resolved absolute path or an
  /// error describing the denial.
  Result<std::string> validate(std::string_view Path, bool ForWrite) const;

  void addAllowedPath(std::string_view Prefix, bool AllowWrite);

private:
  bool isAllowed(const std::string &ResolvedPath, bool ForWrite) const;
  void reportViolation(ViolationType Type, std::string_view Path) const;

  PathValidatorConfig Config_;
};

} // namespace zbc

#endif // ZBC_PATHVALIDATOR_H

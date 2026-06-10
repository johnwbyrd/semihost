//===-- PathValidator.cpp - Path sandboxing ---------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//

#include "zbc/PathValidator.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace zbc {

namespace {
/// Resolve "..", ".", and symlinks without requiring the path to exist
/// (so newly-created files validate). Returns empty on failure.
std::string resolveReal(const fs::path &P) {
  std::error_code EC;
  fs::path R = fs::weakly_canonical(P, EC);
  if (EC)
    return {};
  return R.string();
}
} // namespace

PathValidator::PathValidator(PathValidatorConfig Config)
    : Config_(std::move(Config)) {
  if (!Config_.SandboxDir.empty()) {
    std::string Resolved = resolveReal(Config_.SandboxDir);
    if (Resolved.empty()) {
      // SECURITY: cannot resolve sandbox root -> deny everything.
      reportViolation(ViolationType::SandboxEscape, Config_.SandboxDir);
      Config_.SandboxDir.clear();
      return;
    }
    // Store with a trailing separator for prefix matching.
    if (Resolved.back() != static_cast<char>(fs::path::preferred_separator))
      Resolved.push_back(static_cast<char>(fs::path::preferred_separator));
    Config_.SandboxDir = std::move(Resolved);
  }
}

Result<std::string> PathValidator::validate(std::string_view Path,
                                            bool ForWrite) const {
  if (Path.find('\0') != std::string_view::npos) {
    reportViolation(ViolationType::NullByte, Path);
    return Result<std::string>::error("path contains null byte");
  }
  if (ForWrite && Config_.ReadOnly) {
    reportViolation(ViolationType::WriteProtected, Path);
    return Result<std::string>::error("write denied (read-only mode)");
  }

  // Make relative paths relative to the sandbox root.
  fs::path Requested(Path);
  fs::path Combined;
  if (Requested.is_relative() && !Config_.SandboxDir.empty())
    Combined = fs::path(Config_.SandboxDir) / Requested;
  else
    Combined = Requested;

  std::string Resolved = resolveReal(Combined);
  if (Resolved.empty()) {
    // SECURITY: an unresolvable path could hide a traversal/symlink escape.
    reportViolation(ViolationType::SandboxEscape, Path);
    return Result<std::string>::error("cannot verify path within sandbox");
  }

  if (!isAllowed(Resolved, ForWrite)) {
    reportViolation(ForWrite ? ViolationType::WriteProtected
                             : ViolationType::NotAllowed,
                    Path);
    return Result<std::string>::error("access denied");
  }

  return Resolved;
}

bool PathValidator::isAllowed(const std::string &ResolvedPath,
                              bool ForWrite) const {
  if (!Config_.SandboxDir.empty()) {
    // SandboxDir has a trailing separator; allow the dir itself or anything
    // beneath it.
    std::string NoSep = Config_.SandboxDir;
    NoSep.pop_back();
    if (ResolvedPath == NoSep ||
        ResolvedPath.compare(0, Config_.SandboxDir.size(),
                             Config_.SandboxDir) == 0)
      return true;
  }
  for (const auto &Rule : Config_.AllowedPaths) {
    const std::string &Prefix = Rule.first;
    bool AllowWrite = Rule.second;
    if (ResolvedPath.compare(0, Prefix.size(), Prefix) == 0) {
      if (!ForWrite || AllowWrite)
        return true;
    }
  }
  return false;
}

void PathValidator::reportViolation(ViolationType Type,
                                    std::string_view Path) const {
  if (Config_.OnViolation)
    Config_.OnViolation(Type, Path);
}

void PathValidator::addAllowedPath(std::string_view Prefix, bool AllowWrite) {
  std::string Resolved = resolveReal(fs::path(Prefix));
  if (Resolved.empty())
    Resolved = std::string(Prefix);
  Config_.AllowedPaths.emplace_back(std::move(Resolved), AllowWrite);
}

} // namespace zbc

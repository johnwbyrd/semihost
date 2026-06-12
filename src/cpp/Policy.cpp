//===-- Policy.cpp - Security policy -----------------------------*- C++ -*-===//
//
// Part of the ZBC semihosting monorepo. MIT licensed (see LICENSE).
//
//===----------------------------------------------------------------------===//

#include "zbc/Policy.h"

namespace zbc {

SandboxedPolicy::SandboxedPolicy(std::string_view SandboxDir) {
  PathValidatorConfig Config;
  Config.SandboxDir = std::string(SandboxDir);
  Validator = std::make_unique<PathValidator>(std::move(Config));
}

Result<std::string> SandboxedPolicy::resolvePath(std::string_view Path,
                                                 bool ForWrite,
                                                 bool FollowLeafSymlink) {
  return Validator->validate(Path, ForWrite, FollowLeafSymlink);
}

void SandboxedPolicy::addAllowedPath(std::string_view Prefix, bool AllowWrite) {
  Validator->addAllowedPath(Prefix, AllowWrite);
}

} // namespace zbc

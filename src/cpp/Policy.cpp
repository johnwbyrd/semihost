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
  Validator_ = std::make_unique<PathValidator>(std::move(Config));
}

Result<std::string> SandboxedPolicy::resolvePath(std::string_view Path,
                                                 bool ForWrite) {
  return Validator_->validate(Path, ForWrite);
}

void SandboxedPolicy::addAllowedPath(std::string_view Prefix, bool AllowWrite) {
  Validator_->addAllowedPath(Prefix, AllowWrite);
}

} // namespace zbc

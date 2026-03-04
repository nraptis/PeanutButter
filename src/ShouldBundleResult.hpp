#pragma once

#include <filesystem>
#include <string>

enum class ShouldBundleDecision {
  Yes,
  No,
  Prompt
};

struct ShouldBundleResult {
  ShouldBundleDecision mDecision = ShouldBundleDecision::No;
  std::string mMessage;
  std::filesystem::path mResolvedDestination;
};

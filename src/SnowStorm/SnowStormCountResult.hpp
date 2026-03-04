#pragma once

#include <cstdint>
#include <filesystem>

struct SnowStormCountResult {
  std::uint64_t mFileCount = 0;
  std::uint64_t mFolderCount = 0;
  std::uint64_t mPayloadBytes = 0;
};

SnowStormCountResult countDirectory(const std::filesystem::path& pDirectory);

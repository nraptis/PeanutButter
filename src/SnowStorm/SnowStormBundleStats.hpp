#pragma once

#include <cstdint>

struct SnowStormBundleStats {
  std::uint64_t mFileCount = 0;
  std::uint64_t mFolderCount = 0;
  std::uint64_t mPayloadBytes = 0;
  std::uint64_t mArchiveCount = 0;
};

struct SnowStormUnbundleStats {
  std::uint64_t mFilesUnbundled = 0;
  std::uint64_t mArchivesTotal = 0;
  std::uint64_t mArchivesTouched = 0;
};

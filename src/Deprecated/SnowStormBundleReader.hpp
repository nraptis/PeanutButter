#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Deprecated/SnowStorm.hpp"

class SnowStormBundleReader {
public:
  SnowStormBundleReader(SnowStorm& pSnowStorm,
                        const std::filesystem::path& pInputDirectory,
                        SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod);
  ~SnowStormBundleReader();

  std::vector<char> readExact(std::uint64_t pSize);
  void setFilesDone(std::uint64_t pFilesDone);

  std::uint64_t archiveTotal() const;
  std::uint64_t archivesTouched() const;

  void close();

private:
  void loadBundle(std::size_t pIndex);
  bool loadNextBlock();

  SnowStorm& mSnowStorm;
  SnowStorm::SnowStormProgressMethod mSnowStormProgressMethod;
  std::vector<std::filesystem::path> mBundlePaths;
  std::ifstream mInputFile;
  std::size_t mBundleIndex = 0;
  std::uint64_t mBlockOffset = 0;
  std::uint64_t mBlockValidBytes = 0;
  std::uint64_t mFilesDone = 0;
  std::uint64_t mArchivesTouched = 0;
  bool mClosed = false;
};

std::string readEntryName(SnowStormBundleReader& pReader);

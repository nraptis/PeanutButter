#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "Deprecated/SnowStorm.hpp"
#include "SnowStorm/SnowStormBundlePackerBackward.hpp"
#include "SnowStorm/SnowStormBundleSealerBackward.hpp"

class SnowStormBundleWriterBackward {
public:
  SnowStormBundleWriterBackward(SnowStorm& pSnowStorm,
                                const std::filesystem::path& pInputDirectory,
                                SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod);
  ~SnowStormBundleWriterBackward();

  std::vector<char> readExact(std::uint64_t pSize);
  void setFilesDone(std::uint64_t pFilesDone);
  std::uint64_t archiveTotal() const;
  std::uint64_t archivesTouched() const;
  void close();

private:
  bool loadNextPlainPage();

  SnowStorm& mSnowStorm;
  SnowStormBundlePackerBackward mPacker;
  SnowStormBundleSealerBackward mSealer;
  std::vector<unsigned char> mEncryptedPage;
  std::vector<unsigned char> mPlainPage;
  std::uint64_t mPageOffset = 0;
  std::uint64_t mPageValidBytes = 0;
  bool mClosed = false;
};

std::string readEntryName(SnowStormBundleWriterBackward& pReader);

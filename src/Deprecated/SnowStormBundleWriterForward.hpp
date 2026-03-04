#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "Deprecated/SnowStorm.hpp"
#include "SnowStorm/SnowStormBundlePackerForward.hpp"
#include "SnowStorm/SnowStormBundleSealerForward.hpp"

class SnowStormBundleWriterForward {
public:
  SnowStormBundleWriterForward(SnowStorm& pSnowStorm,
                               const std::filesystem::path& pOutputDirectory,
                               std::uint64_t pArchiveTotal,
                               SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod,
                               std::uint64_t pFilesTotal);
  ~SnowStormBundleWriterForward();

  void setFilesDone(std::uint64_t pFilesDone);
  void writeBytes(const char* pData, std::uint64_t pSize);
  void writeBytes(const std::vector<char>& pData);
  void close();

  std::uint64_t archiveCount() const;

private:
  void flushLayer1Input();
  void emitReadyPages();

  SnowStorm& mSnowStorm;
  SnowStormBundleSealerForward mSealer;
  SnowStormBundlePackerForward mPacker;
  std::uint64_t mArchiveTotal = 0;
  SnowStorm::SnowStormProgressMethod mSnowStormProgressMethod;
  std::uint64_t mFilesTotal = 0;
  std::uint64_t mFilesDone = 0;
  std::vector<unsigned char> mLayer1Input;
  std::uint64_t mPositionInLayer1 = 0;
  bool mClosed = false;
};

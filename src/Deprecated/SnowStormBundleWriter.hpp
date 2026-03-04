#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Deprecated/SnowStorm.hpp"

class SnowStormBundleWriter {
public:
  SnowStormBundleWriter(SnowStorm& pSnowStorm,
                        const std::filesystem::path& pOutputDirectory,
                        std::uint64_t pArchiveTotal,
                        SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod,
                        std::uint64_t pFilesTotal);
  ~SnowStormBundleWriter();

  void setFilesDone(std::uint64_t pFilesDone);
  void writeBytes(const char* pData, std::uint64_t pSize);
  void writeBytes(const std::vector<char>& pData);
  void close();

  std::uint64_t archiveCount() const;

private:
  void flushBlockIfFull();
  void writeBlock();
  void openNextArchive();
  void sealCurrentArchive();

  SnowStorm& mSnowStorm;
  std::filesystem::path mOutputDirectory;
  std::uint64_t mArchiveTotal = 0;
  SnowStorm::SnowStormProgressMethod mSnowStormProgressMethod;
  std::uint64_t mFilesTotal = 0;
  std::ofstream mArchiveFile;
  std::int64_t mCurrentArchiveIndex = -1;
  std::uint64_t mPositionInArchive = 0;
  std::uint64_t mPositionInBlock = 0;
  std::uint64_t mFilesDone = 0;
  std::uint64_t mArchivesSealed = 0;
  bool mClosed = false;
};

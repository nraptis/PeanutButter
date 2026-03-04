#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

class SnowStormBundlePackerBackward {
public:
  using SnowStormProgressMethod = std::function<void(std::uint64_t archive_index_1,
                                                     std::uint64_t archive_total,
                                                     std::uint64_t files_done,
                                                     std::uint64_t files_total)>;

  SnowStormBundlePackerBackward(const std::filesystem::path& pInputDirectory,
                                SnowStormProgressMethod pSnowStormProgressMethod);
  ~SnowStormBundlePackerBackward();

  bool readPage(unsigned char* pDestination,
                std::uint64_t* pBytesRead,
                std::string* pError);
  void setFilesDone(std::uint64_t pFilesDone);
  std::uint64_t archiveTotal() const;
  std::uint64_t archivesTouched() const;
  void close();

private:
  bool loadBundle(std::size_t pIndex, std::string* pError);
  std::vector<std::filesystem::path> discoverBundlePaths(const std::filesystem::path& pInputDirectory);

  SnowStormProgressMethod mSnowStormProgressMethod;
  std::vector<std::filesystem::path> mBundlePaths;
  std::ifstream mInputFile;
  std::size_t mBundleIndex = 0;
  std::uint64_t mFilesDone = 0;
  std::uint64_t mArchivesTouched = 0;
  bool mClosed = false;
};

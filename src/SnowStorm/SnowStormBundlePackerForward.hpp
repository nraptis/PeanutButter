#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

class SnowStormBundlePackerForward {
public:
  SnowStormBundlePackerForward(const std::filesystem::path& pOutputDirectory,
                               std::uint64_t pStorageFileSize);
  ~SnowStormBundlePackerForward();

  bool writePage(const unsigned char* pPage, std::string* pError);
  bool finalize(std::string* pError);
  bool sealCurrentArchiveWithZeroPages(const unsigned char* pEncryptedZeroPage, std::string* pError);
  std::uint64_t archiveCount() const;
  std::uint64_t positionInArchive() const;

private:
  bool openNextArchive(std::string* pError);
  bool sealCurrentArchive(std::string* pError);

  std::filesystem::path mOutputDirectory;
  std::uint64_t mStorageFileSize = 0;
  std::ofstream mArchiveFile;
  std::int64_t mCurrentArchiveIndex = -1;
  std::uint64_t mPositionInArchive = 0;
  std::uint64_t mArchivesSealed = 0;
};

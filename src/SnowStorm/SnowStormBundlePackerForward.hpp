#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

class FileWriterDelegate;

class SnowStormBundlePackerForward {
public:
  SnowStormBundlePackerForward(const std::filesystem::path& pOutputDirectory,
                               std::uint64_t pStorageFileSize,
                               FileWriterDelegate& pWriter);
  ~SnowStormBundlePackerForward();

  bool writePage(const unsigned char* pPage, std::string* pError);
  bool finalize(std::string* pError);
  bool sealCurrentArchiveWithZeroPages(const unsigned char* pEncryptedZeroPage, std::string* pError);
  bool setCurrentArchiveFirstFileOffset(std::uint64_t pFirstFileOffset,
                                        bool pHasRecoveryStart,
                                        std::string* pError);
  std::uint64_t archiveCount() const;
  std::uint64_t positionInArchive() const;
  std::int64_t currentArchiveIndex() const;

private:
  bool openNextArchive(std::string* pError);
  bool sealCurrentArchive(std::string* pError);

  FileWriterDelegate& mWriter;
  std::filesystem::path mOutputDirectory;
  std::filesystem::path mCurrentArchivePath;
  std::uint64_t mStorageFileSize = 0;
  std::int64_t mCurrentArchiveIndex = -1;
  std::uint64_t mPositionInArchive = 0;
  std::uint64_t mArchivesSealed = 0;
  std::uint64_t mCurrentArchiveFirstFileOffset = 0;
  bool mCurrentArchiveHasRecoveryStart = false;
  bool mCurrentArchiveHeaderWritten = false;
  std::uint64_t mPendingArchiveFirstFileOffset = 0;
  bool mPendingArchiveHasRecoveryStart = false;
  bool mHasPendingArchiveFirstFileOffset = false;
};

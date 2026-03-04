#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Encryptable.hpp"
#include "ShouldBundleResult.hpp"

struct BundleStats {
  std::uint64_t mFileCount = 0;
  std::uint64_t mFolderCount = 0;
  std::uint64_t mPayloadBytes = 0;
  std::uint64_t mArchiveCount = 0;
};

struct UnbundleStats {
  std::uint64_t mFilesUnbundled = 0;
  std::uint64_t mArchivesTotal = 0;
  std::uint64_t mArchivesTouched = 0;
};

class SnowStorm {
 public:
  using SnowStormProgressMethod = std::function<void(std::uint64_t archive_index_1,
                                                     std::uint64_t archive_total,
                                                     std::uint64_t files_done,
                                                     std::uint64_t files_total)>;

  SnowStorm(std::uint64_t pBlockSize,
            std::uint64_t pStorageFileSize,
            Encryptable* pCrypt);

  ShouldBundleResult shouldBundle(const std::filesystem::path& pSource,
                                  const std::filesystem::path& pDestination) const;

  BundleStats bundle(const std::filesystem::path& pSource,
                     const std::filesystem::path& pDestination,
                     SnowStormProgressMethod pSnowStormProgressMethod);

  ShouldBundleResult shouldUnbundle(const std::filesystem::path& pSource,
                                    const std::filesystem::path& pDestination) const;

  UnbundleStats unbundle(const std::filesystem::path& pSource,
                         const std::filesystem::path& pDestination,
                         SnowStormProgressMethod pSnowStormProgressMethod);

  const unsigned char* mReadBuffer = nullptr;
  unsigned int mReadBufferIndex = 0;

  const unsigned char* mWriteBuffer = nullptr;
  unsigned int mWriteBufferIndex = 0;

  const unsigned char* mCryptBuffer = nullptr;
  unsigned int mCryptBufferIndex = 0;

  Encryptable* mCrypt = nullptr;

  std::uint64_t blockSize() const;
  std::uint64_t storageFileSize() const;

 private:
  std::uint64_t mBlockSize = 0;
  std::uint64_t mStorageFileSize = 0;
  std::vector<unsigned char> mReadBufferStorage;
  std::vector<unsigned char> mWriteBufferStorage;
  std::vector<unsigned char> mCryptBufferStorage;
};

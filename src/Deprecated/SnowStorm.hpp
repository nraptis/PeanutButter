#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "SandStorm/EncryptionLayer.hpp"
#include "SnowStorm/SnowStormBundleStats.hpp"
#include "ShouldBundleResult.hpp"

class SnowStorm {
public:
  using SnowStormProgressMethod = std::function<void(std::uint64_t archive_index_1,
                                                     std::uint64_t archive_total,
                                                     std::uint64_t files_done,
                                                     std::uint64_t files_total)>;

  SnowStorm(std::uint64_t pBlockSize,
            std::uint64_t pStorageFileSize,
            EncryptionLayer* pCrypt1,
            EncryptionLayer* pCrypt2,
            EncryptionLayer* pCrypt3);

  ShouldBundleResult shouldBundle(const std::filesystem::path& pSource,
                                  const std::filesystem::path& pDestination) const;

  SnowStormBundleStats bundle(const std::filesystem::path& pSource,
                              const std::filesystem::path& pDestination,
                              SnowStormProgressMethod pSnowStormProgressMethod);

  ShouldBundleResult shouldUnbundle(const std::filesystem::path& pSource,
                                    const std::filesystem::path& pDestination) const;

  SnowStormUnbundleStats unbundle(const std::filesystem::path& pSource,
                                  const std::filesystem::path& pDestination,
                                  SnowStormProgressMethod pSnowStormProgressMethod);

  std::uint64_t storageFileSize() const;

private:
  void writeFileEntry(const std::filesystem::path& pFilePath,
                      const std::function<void(const char*, std::uint64_t)>& pWriteBytes) const;
  void bundleDirectory(const std::filesystem::path& pDirectory,
                       const std::function<void(const char*, std::uint64_t)>& pWriteBytes,
                       const std::function<void(std::uint64_t)>& pSetFilesDone,
                       std::uint64_t& pFilesDone) const;
  void unbundleDirectory(const std::filesystem::path& pDestination,
                         const std::function<std::vector<char>(std::uint64_t)>& pReadExact,
                         const std::function<void(std::uint64_t)>& pSetFilesDone,
                         std::vector<std::filesystem::path>& pCreatedFiles,
                         std::uint64_t& pFilesDone) const;
  std::string readEntryName(const std::function<std::vector<char>(std::uint64_t)>& pReadExact) const;

  const unsigned char* mReadBuffer = nullptr;
  unsigned int mReadBufferIndex = 0;
  const unsigned char* mWriteBuffer = nullptr;
  unsigned int mWriteBufferIndex = 0;
  const unsigned char* mCryptBuffer = nullptr;
  unsigned int mCryptBufferIndex = 0;
  EncryptionLayer* mEncryptionLayer1 = nullptr;
  EncryptionLayer* mEncryptionLayer2 = nullptr;
  EncryptionLayer* mEncryptionLayer3 = nullptr;

  std::uint64_t mStorageFileSize = 0;
  std::vector<unsigned char> mReadBufferStorage;
  std::vector<unsigned char> mWriteBufferStorage;
  std::vector<unsigned char> mCryptBufferStorage;
};

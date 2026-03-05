#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ShouldBundleResult.hpp"
#include "LayeredEncryption/LayeredEncryptionDelegate.hpp"
#include "SnowStorm/SnowStormBundleStats.hpp"

class FileReaderDelegate;
class FileWriterDelegate;

class SnowStormEngine {
public:
  using SnowStormProgressMethod = std::function<void(std::uint64_t archive_index_1,
                                                     std::uint64_t archive_total,
                                                     std::uint64_t files_done,
                                                     std::uint64_t files_total)>;

  explicit SnowStormEngine(std::uint64_t pStorageFileSize);
  SnowStormEngine(std::uint64_t pStorageFileSize,
                  FileReaderDelegate& pReader,
                  FileWriterDelegate& pWriter);
  SnowStormEngine(std::uint64_t pStorageFileSize,
                  FileReaderDelegate& pReader,
                  FileWriterDelegate& pWriter,
                  LayeredEncryptionDelegate& pLayeredEncryption);

  ShouldBundleResult shouldBundle(const std::filesystem::path& pSource,
                                  const std::filesystem::path& pDestination) const;

  ShouldBundleResult shouldUnbundle(const std::filesystem::path& pSource,
                                    const std::filesystem::path& pDestination) const;

  SnowStormBundleStats bundle(const std::filesystem::path& pSourceDirectory,
                              const std::filesystem::path& pDestinationDirectory,
                              SnowStormProgressMethod pProgress);

  SnowStormUnbundleStats unbundle(const std::filesystem::path& pSourceDirectory,
                                  const std::filesystem::path& pDestinationDirectory,
                                  SnowStormProgressMethod pProgress,
                                  std::uint64_t pStartArchiveIndex = 0,
                                  std::uint64_t pStartByteOffsetInArchive = 0);

  bool ExecuteBundle(const std::filesystem::path& pSourceDirectory,
                     const std::filesystem::path& pDestinationDirectory,
                     SnowStormProgressMethod pProgress,
                     SnowStormBundleStats& pStats,
                     std::string* pError);

  bool ExecuteUnbundle(const std::filesystem::path& pSourceDirectory,
                       const std::filesystem::path& pDestinationDirectory,
                       SnowStormProgressMethod pProgress,
                       SnowStormUnbundleStats& pStats,
                       std::string* pError,
                       std::uint64_t pStartArchiveIndex = 0,
                       std::uint64_t pStartByteOffsetInArchive = 0);

private:
  std::uint64_t mStorageFileSize = 0;
  FileReaderDelegate* mReader = nullptr;
  FileWriterDelegate* mWriter = nullptr;
  LayeredEncryptionDelegate* mLayeredEncryption = nullptr;
  std::unique_ptr<LayeredEncryptionDelegate> mOwnedLayeredEncryption;
};

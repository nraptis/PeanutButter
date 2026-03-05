#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "FileCompare.hpp"
#include "IO/FileReaderDelegate.hpp"
#include "IO/FileWriterDelegate.hpp"
#include "PeanutButterDelegate.hpp"
#include "ShouldBundleResult.hpp"
#include "SnowStorm/SnowStormBundleStats.hpp"

class PeanutButterImplementation {
public:
  struct RecoveryStart {
    std::filesystem::path mArchiveDirectory;
    std::uint64_t mArchiveIndex = 0;
    std::uint64_t mByteOffsetInArchive = 0;
  };

  PeanutButterImplementation(PeanutButterDelegate& pDelegate, std::uint64_t pArchiveSize);
  PeanutButterImplementation(PeanutButterDelegate& pDelegate,
                             std::uint64_t pArchiveSize,
                             FileReaderDelegate& pReader,
                             FileWriterDelegate& pWriter);

  static RecoveryStart resolveRecoveryStartFile(const std::filesystem::path& pRecoveryStartFile,
                                                const std::string& pFilePrefix,
                                                const std::string& pFileSuffix,
                                                const FileReaderDelegate& pReader);

  SnowStormBundleStats pack(const std::filesystem::path& pSource, const std::filesystem::path& pArchive);
  SnowStormUnbundleStats unpack(const std::filesystem::path& pArchive, const std::filesystem::path& pUnarchive);
  SnowStormUnbundleStats recover(const RecoveryStart& pRecoveryStart, const std::filesystem::path& pUnarchive);
  bool sanity(const std::filesystem::path& pSource, const std::filesystem::path& pDestination, std::string* pError);

private:
  SnowStormUnbundleStats runUnbundle(const std::filesystem::path& pArchive,
                                     const std::filesystem::path& pUnarchive,
                                     std::uint64_t pStartArchiveIndex,
                                     std::uint64_t pStartByteOffsetInArchive,
                                     const char* pVerbProgress,
                                     const char* pVerbSummary);
  void clearDestinationIfNeeded(const std::filesystem::path& pInput,
                                const ShouldBundleResult& pShould,
                                const char* pPromptMessage);

  PeanutButterDelegate& mDelegate;
  std::uint64_t mArchiveSize = 0;
  FileReaderDelegate* mReader = nullptr;
  FileWriterDelegate* mWriter = nullptr;
};

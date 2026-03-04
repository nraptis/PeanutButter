#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ShouldBundleResult.hpp"
#include "SnowStorm/SnowStormBundleStats.hpp"

class SnowStormEngine {
public:
  using SnowStormProgressMethod = std::function<void(std::uint64_t archive_index_1,
                                                     std::uint64_t archive_total,
                                                     std::uint64_t files_done,
                                                     std::uint64_t files_total)>;

  explicit SnowStormEngine(std::uint64_t pStorageFileSize);

  ShouldBundleResult shouldBundle(const std::filesystem::path& pSource,
                                  const std::filesystem::path& pDestination) const;

  ShouldBundleResult shouldUnbundle(const std::filesystem::path& pSource,
                                    const std::filesystem::path& pDestination) const;

  SnowStormBundleStats bundle(const std::filesystem::path& pSourceDirectory,
                              const std::filesystem::path& pDestinationDirectory,
                              SnowStormProgressMethod pProgress);

  SnowStormUnbundleStats unbundle(const std::filesystem::path& pSourceDirectory,
                                  const std::filesystem::path& pDestinationDirectory,
                                  SnowStormProgressMethod pProgress);

  bool ExecuteBundle(const std::filesystem::path& pSourceDirectory,
                     const std::filesystem::path& pDestinationDirectory,
                     SnowStormProgressMethod pProgress,
                     SnowStormBundleStats& pStats,
                     std::string* pError);

  bool ExecuteUnbundle(const std::filesystem::path& pSourceDirectory,
                       const std::filesystem::path& pDestinationDirectory,
                       SnowStormProgressMethod pProgress,
                       SnowStormUnbundleStats& pStats,
                       std::string* pError);

private:
  void writeFileEntry(const std::filesystem::path& pFilePath,
                      const std::function<void(const char*, std::uint64_t)>& pWriteBytes,
                      std::uint64_t& pFilesDone) const;
  void writeFolderEntry(const std::filesystem::path& pDirectory,
                        const std::function<void(const char*, std::uint64_t)>& pWriteBytes,
                        const std::function<void(std::uint64_t)>& pSetFilesDone,
                        std::uint64_t& pFilesDone) const;
  std::string readEntryName(const std::function<std::vector<char>(std::uint64_t)>& pReadExact) const;
  void unbundleDirectory(const std::filesystem::path& pDestination,
                         const std::function<std::vector<char>(std::uint64_t)>& pReadExact,
                         const std::function<void(std::uint64_t)>& pSetFilesDone,
                         std::vector<std::filesystem::path>& pCreatedFiles,
                         std::uint64_t& pFilesDone) const;
  std::uint64_t mStorageFileSize = 0;
};

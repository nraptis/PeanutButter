#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "SnowStorm/SnowStormBundleStats.hpp"

class SnowStormFileAndFolderProcessor {
public:
  bool prepareBundle(const std::filesystem::path& pSource,
                     const std::filesystem::path& pDestination,
                     std::filesystem::path& pResolvedDestination,
                     SnowStormBundleStats& pStats,
                     std::string* pError);

  bool step(std::string* pError);
  bool empty() const;
  void floodRemainingSpaceWithZeros();
  const char* blockData() const;
  std::uint64_t blockSize() const;

  bool prepareUnbundle(const std::filesystem::path& pDestination,
                       std::filesystem::path& pResolvedDestination,
                       std::string* pError);

  bool unbundleFromStream(const std::function<std::vector<char>(std::uint64_t)>& pReadExact,
                          SnowStormUnbundleStats& pStats,
                          std::string* pError);

private:
  bool serializeDirectory(const std::filesystem::path& pDirectory,
                          std::vector<char>& pOut,
                          std::uint64_t& pFilesDone,
                          SnowStormBundleStats& pStats,
                          std::string* pError);
  bool deserializeDirectory(const std::filesystem::path& pDestination,
                            const std::function<std::vector<char>(std::uint64_t)>& pReadExact,
                            std::vector<std::filesystem::path>& pCreatedFiles,
                            std::uint64_t& pFilesDone,
                            std::string* pError);
  std::string readEntryName(const std::function<std::vector<char>(std::uint64_t)>& pReadExact) const;
  struct DirectoryState {
    std::filesystem::path mDirectory;
    std::vector<std::filesystem::path> mFiles;
    std::vector<std::filesystem::path> mDirectories;
    std::size_t mFileIndex = 0;
    std::size_t mDirectoryIndex = 0;
    int mPhase = 0;
  };
  bool appendBytes(const char* pData, std::size_t pSize);
  bool fillBlockFromBundleStream(std::string* pError);
  bool pushDirectory(const std::filesystem::path& pDirectory, std::string* pError);
  void queueUInt16(std::uint16_t pValue);
  void queueUInt64(std::uint64_t pValue);
  void queueName(const std::string& pName);

  std::filesystem::path mDestinationDirectory;
  std::filesystem::path mSourceDirectory;
  std::vector<DirectoryState> mDirectoryStack;
  std::ifstream mCurrentFile;
  std::uint64_t mCurrentFileRemaining = 0;
  std::vector<char> mPendingBytes;
  std::size_t mPendingOffset = 0;
  std::vector<char> mBlockBuffer;
  std::uint64_t mBlockBufferUsed = 0;
  bool mBundleFinished = false;
};

#include "SnowStorm/SnowStormEngine.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include "Globals.hpp"
#include "SnowStorm/SnowStormBundlePackerBackward.hpp"
#include "SnowStorm/SnowStormBundlePackerForward.hpp"
#include "SnowStorm/SnowStormBundleSealerBackward.hpp"
#include "SnowStorm/SnowStormBundleSealerForward.hpp"
#include "SnowStorm/SnowStormCountResult.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

SnowStormEngine::SnowStormEngine(std::uint64_t pStorageFileSize)
    : mStorageFileSize(pStorageFileSize) {}

ShouldBundleResult SnowStormEngine::shouldBundle(const fs::path& pSource,
                                                 const fs::path& pDestination) const {
  ShouldBundleResult aResult;
  if (!fs::exists(pSource) || !fs::is_directory(pSource)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Source path must be an existing directory";
    return aResult;
  }
  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSource, pDestination);
  aResult.mResolvedDestination = aOutputDirectory;
  if (fs::exists(aOutputDirectory) && !fs::is_directory(aOutputDirectory)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Destination exists and is not a directory: " + aOutputDirectory.string();
    return aResult;
  }
  if (fs::exists(aOutputDirectory) && SnowStormUtils::hasMeaningfulEntries(aOutputDirectory)) {
    aResult.mDecision = ShouldBundleDecision::Prompt;
    aResult.mMessage = "Pack destination is not empty: " + aOutputDirectory.string();
    return aResult;
  }
  std::error_code aErrorCode;
  fs::create_directories(aOutputDirectory, aErrorCode);
  if (aErrorCode) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Failed to create output directory: " + aOutputDirectory.string();
    return aResult;
  }
  aResult.mDecision = ShouldBundleDecision::Yes;
  aResult.mMessage = "OK";
  return aResult;
}

ShouldBundleResult SnowStormEngine::shouldUnbundle(const fs::path& pSource,
                                                   const fs::path& pDestination) const {
  ShouldBundleResult aResult;
  if (!fs::exists(pSource) || !fs::is_directory(pSource)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Archive source path must be an existing directory";
    return aResult;
  }
  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSource, pDestination);
  aResult.mResolvedDestination = aOutputDirectory;
  if (fs::exists(aOutputDirectory) && !fs::is_directory(aOutputDirectory)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Destination exists and is not a directory: " + aOutputDirectory.string();
    return aResult;
  }
  if (fs::exists(aOutputDirectory) && SnowStormUtils::hasMeaningfulEntries(aOutputDirectory)) {
    aResult.mDecision = ShouldBundleDecision::Prompt;
    aResult.mMessage = "Unpack destination is not empty: " + aOutputDirectory.string();
    return aResult;
  }
  std::error_code aErrorCode;
  fs::create_directories(aOutputDirectory, aErrorCode);
  if (aErrorCode) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Failed to create unbundle destination: " + aOutputDirectory.string();
    return aResult;
  }
  aResult.mDecision = ShouldBundleDecision::Yes;
  aResult.mMessage = "OK";
  return aResult;
}

SnowStormBundleStats SnowStormEngine::bundle(const fs::path& pSourceDirectory,
                                             const fs::path& pDestinationDirectory,
                                             SnowStormProgressMethod pProgress) {
  const ShouldBundleResult aGate = shouldBundle(pSourceDirectory, pDestinationDirectory);
  if (aGate.mDecision != ShouldBundleDecision::Yes) {
    throw std::runtime_error(aGate.mMessage);
  }
  SnowStormBundleStats aStats;
  std::string aError;
  if (!ExecuteBundle(pSourceDirectory, pDestinationDirectory, std::move(pProgress), aStats, &aError)) {
    throw std::runtime_error(aError);
  }
  return aStats;
}

SnowStormUnbundleStats SnowStormEngine::unbundle(const fs::path& pSourceDirectory,
                                                 const fs::path& pDestinationDirectory,
                                                 SnowStormProgressMethod pProgress) {
  const ShouldBundleResult aGate = shouldUnbundle(pSourceDirectory, pDestinationDirectory);
  if (aGate.mDecision != ShouldBundleDecision::Yes) {
    throw std::runtime_error(aGate.mMessage);
  }
  SnowStormUnbundleStats aStats;
  std::string aError;
  if (!ExecuteUnbundle(pSourceDirectory, pDestinationDirectory, std::move(pProgress), aStats, &aError)) {
    throw std::runtime_error(aError);
  }
  return aStats;
}

void SnowStormEngine::writeFileEntry(const fs::path& pFilePath,
                                     const std::function<void(const char*, std::uint64_t)>& pWriteBytes,
                                     std::uint64_t& pFilesDone) const {
  std::vector<char> aHeader;
  const std::string aName = pFilePath.filename().string();
  SnowStormUtils::appendName(aHeader, aName);
  SnowStormUtils::writeUInt64(aHeader, fs::file_size(pFilePath));
  pWriteBytes(aHeader.data(), static_cast<std::uint64_t>(aHeader.size()));

  std::ifstream aInput(pFilePath, std::ios::binary);
  if (!aInput) {
    throw std::runtime_error("Failed to open file for read: " + pFilePath.string());
  }
  std::vector<char> aBlock(static_cast<std::size_t>(BLOCK_SIZE_LAYER_3), 0);
  while (aInput) {
    aInput.read(aBlock.data(), static_cast<std::streamsize>(aBlock.size()));
    const std::streamsize aBytesRead = aInput.gcount();
    if (aBytesRead <= 0) {
      break;
    }
    pWriteBytes(aBlock.data(), static_cast<std::uint64_t>(aBytesRead));
  }
  ++pFilesDone;
}

void SnowStormEngine::writeFolderEntry(const fs::path& pDirectory,
                                       const std::function<void(const char*, std::uint64_t)>& pWriteBytes,
                                       const std::function<void(std::uint64_t)>& pSetFilesDone,
                                       std::uint64_t& pFilesDone) const {
  std::vector<fs::path> aFiles;
  std::vector<fs::path> aDirectories;
  SnowStormUtils::collectEntries(pDirectory, aFiles, aDirectories);

  std::vector<char> aHeader;
  SnowStormUtils::writeUInt16(aHeader, static_cast<std::uint16_t>(aFiles.size()));
  pWriteBytes(aHeader.data(), static_cast<std::uint64_t>(aHeader.size()));

  for (const auto& aFilePath : aFiles) {
    writeFileEntry(aFilePath, pWriteBytes, pFilesDone);
    pSetFilesDone(pFilesDone);
  }

  std::vector<char> aDirectoryHeader;
  SnowStormUtils::writeUInt16(aDirectoryHeader, static_cast<std::uint16_t>(aDirectories.size()));
  pWriteBytes(aDirectoryHeader.data(), static_cast<std::uint64_t>(aDirectoryHeader.size()));
  for (const auto& aDirectoryPath : aDirectories) {
    std::vector<char> aName;
    SnowStormUtils::appendName(aName, aDirectoryPath.filename().string());
    pWriteBytes(aName.data(), static_cast<std::uint64_t>(aName.size()));
    writeFolderEntry(aDirectoryPath, pWriteBytes, pSetFilesDone, pFilesDone);
  }
}

std::string SnowStormEngine::readEntryName(const std::function<std::vector<char>(std::uint64_t)>& pReadExact) const {
  const std::vector<char> aLengthRaw = pReadExact(2);
  const std::uint16_t aLength = SnowStormUtils::readUInt16(aLengthRaw);
  const std::vector<char> aNameRaw = pReadExact(aLength);
  const std::string aName(aNameRaw.begin(), aNameRaw.end());

  if (aName.empty() || aName == "." || aName == ".." ||
      aName.find('/') != std::string::npos || aName.find('\\') != std::string::npos) {
    throw std::runtime_error(
        "Illegal entry name (len=" + std::to_string(aName.size()) +
        ", preview=\"" + SnowStormUtils::sanitizeNamePreview(aName) + "\")");
  }
  return aName;
}

void SnowStormEngine::unbundleDirectory(const fs::path& pDestination,
                                        const std::function<std::vector<char>(std::uint64_t)>& pReadExact,
                                        const std::function<void(std::uint64_t)>& pSetFilesDone,
                                        std::vector<fs::path>& pCreatedFiles,
                                        std::uint64_t& pFilesDone) const {
  const std::uint16_t aFileCount = SnowStormUtils::readUInt16(pReadExact(2));

  for (std::uint16_t aFileIndex = 0; aFileIndex < aFileCount; ++aFileIndex) {
    const std::string aName = readEntryName(pReadExact);
    const std::uint64_t aFileSize = SnowStormUtils::readUInt64(pReadExact(8));
    const fs::path aTargetPath = pDestination / aName;
    fs::create_directories(aTargetPath.parent_path());

    try {
      std::ofstream aOutput(aTargetPath, std::ios::binary | std::ios::trunc);
      if (!aOutput) {
        throw std::runtime_error("Failed creating output file: " + aTargetPath.string());
      }

      std::uint64_t aRemaining = aFileSize;
      while (aRemaining > 0) {
        const std::uint64_t aTake = std::min<std::uint64_t>(aRemaining, BLOCK_SIZE_LAYER_3);
        const std::vector<char> aBlock = pReadExact(aTake);
        aOutput.write(aBlock.data(), static_cast<std::streamsize>(aBlock.size()));
        if (!aOutput) {
          throw std::runtime_error("Failed writing output file: " + aTargetPath.string());
        }
        aRemaining -= aTake;
      }
    } catch (...) {
      std::error_code aErrorCode;
      fs::remove(aTargetPath, aErrorCode);
      throw;
    }

    pCreatedFiles.push_back(aTargetPath);
    ++pFilesDone;
    pSetFilesDone(pFilesDone);
  }

  const std::uint16_t aDirectoryCount = SnowStormUtils::readUInt16(pReadExact(2));
  for (std::uint16_t aDirectoryIndex = 0; aDirectoryIndex < aDirectoryCount; ++aDirectoryIndex) {
    const std::string aDirectoryName = readEntryName(pReadExact);
    const fs::path aDirectoryPath = pDestination / aDirectoryName;
    fs::create_directories(aDirectoryPath);
    unbundleDirectory(aDirectoryPath, pReadExact, pSetFilesDone, pCreatedFiles, pFilesDone);
  }
}

bool SnowStormEngine::ExecuteBundle(const fs::path& pSourceDirectory,
                                    const fs::path& pDestinationDirectory,
                                    SnowStormProgressMethod pProgress,
                                    SnowStormBundleStats& pStats,
                                    std::string* pError) {
  fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSourceDirectory, pDestinationDirectory);
  const SnowStormCountResult aCounts = countDirectory(pSourceDirectory);
  pStats.mFileCount = aCounts.mFileCount;
  pStats.mFolderCount = aCounts.mFolderCount;
  pStats.mPayloadBytes = aCounts.mPayloadBytes;

  fs::create_directories(aOutputDirectory);
  const std::uint64_t aArchiveCount = std::max<std::uint64_t>(1, (aCounts.mPayloadBytes + mStorageFileSize - 1) / mStorageFileSize);
  std::error_code aErrorCode;
  const fs::space_info aSpace = fs::space(aOutputDirectory, aErrorCode);
  const std::uint64_t aRequiredBytes = aArchiveCount * mStorageFileSize + mStorageFileSize;
  if (!aErrorCode && aSpace.available < aRequiredBytes) {
    if (pError != nullptr) {
      *pError = "Insufficient disk space for bundle";
    }
    return false;
  }

  SnowStormBundleSealerForward aSealer;
  SnowStormBundlePackerForward aPacker(aOutputDirectory, mStorageFileSize);
  std::vector<unsigned char> aPageBuffer(BLOCK_SIZE_LAYER_3, 0);
  std::uint64_t aPositionInPage = 0;
  std::uint64_t aFilesDone = 0;

  auto aWriteBytes = [&](const char* aBlockData, std::uint64_t aBlockSize) {
    std::uint64_t aOffset = 0;
    while (aOffset < aBlockSize) {
      const std::uint64_t aRemaining = BLOCK_SIZE_LAYER_3 - aPositionInPage;
      const std::uint64_t aChunk = std::min<std::uint64_t>(aRemaining, aBlockSize - aOffset);
      std::memcpy(aPageBuffer.data() + static_cast<std::size_t>(aPositionInPage),
                  aBlockData + static_cast<std::ptrdiff_t>(aOffset),
                  static_cast<std::size_t>(aChunk));
      aPositionInPage += aChunk;
      aOffset += aChunk;
      if (aPositionInPage == BLOCK_SIZE_LAYER_3) {
        std::string aSealerError;
        if (!aSealer.sealPage(aPageBuffer.data(), &aSealerError)) {
          if (pError != nullptr) {
            *pError = "Sealer sealPage failed: " + aSealerError;
          }
          throw std::runtime_error("Sealer sealPage failed: " + aSealerError);
        }
        std::string aWriteError;
        if (!aPacker.writePage(aSealer.mBufferSealed.data(), &aWriteError)) {
          if (pError != nullptr) {
            *pError = "Packer writePage failed: " + aWriteError;
          }
          throw std::runtime_error("Packer writePage failed: " + aWriteError);
        }
        aPositionInPage = 0;
      }
    }
  };
  auto aSetFilesDone = [&](std::uint64_t) {};
  try {
    writeFolderEntry(pSourceDirectory, aWriteBytes, aSetFilesDone, aFilesDone);
  } catch (const std::exception& aException) {
    if (pError != nullptr) {
      *pError = aException.what();
    }
    return false;
  }

  if (aPositionInPage > 0) {
    std::memset(aPageBuffer.data() + static_cast<std::size_t>(aPositionInPage),
                0,
                static_cast<std::size_t>(BLOCK_SIZE_LAYER_3 - aPositionInPage));
    std::string aSealerError;
    if (!aSealer.sealPage(aPageBuffer.data(), &aSealerError)) {
      if (pError != nullptr) {
        *pError = "Sealer sealPage failed: " + aSealerError;
      }
      return false;
    }
    std::string aWriteError;
    if (!aPacker.writePage(aSealer.mBufferSealed.data(), &aWriteError)) {
      if (pError != nullptr) {
        *pError = "Packer writePage failed: " + aWriteError;
      }
      return false;
    }
  }

  if (!aPacker.finalize(pError)) {
    return false;
  }

  pStats.mArchiveCount = aPacker.archiveCount();
  if (pProgress && pStats.mArchiveCount > 0) {
    pProgress(pStats.mArchiveCount, aArchiveCount, aFilesDone, aCounts.mFileCount);
  }
  return true;
}

bool SnowStormEngine::ExecuteUnbundle(const fs::path& pSourceDirectory,
                                      const fs::path& pDestinationDirectory,
                                      SnowStormProgressMethod pProgress,
                                      SnowStormUnbundleStats& pStats,
                                      std::string* pError) {
  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSourceDirectory, pDestinationDirectory);
  fs::create_directories(aOutputDirectory);

  SnowStormBundlePackerBackward aPacker(pSourceDirectory, std::move(pProgress));
  SnowStormBundleSealerBackward aSealer;
  std::vector<unsigned char> aEncryptedPage(BLOCK_SIZE_LAYER_3, 0);
  std::vector<unsigned char> aPlainPage(BLOCK_SIZE_LAYER_3, 0);
  std::uint64_t aPageOffset = 0;
  std::uint64_t aPageValidBytes = 0;
  std::uint64_t aFilesDone = 0;
  std::vector<fs::path> aCreatedFiles;

  auto aLoadNextPlainPage = [&]() -> bool {
    std::uint64_t aBytesRead = 0;
    std::string aReadError;
    if (!aPacker.readPage(aEncryptedPage.data(), &aBytesRead, &aReadError)) {
      if (!aReadError.empty()) {
        if (pError != nullptr) {
          *pError = aReadError;
        }
      }
      aPageOffset = 0;
      aPageValidBytes = 0;
      return false;
    }
    if (!aSealer.decryptLayer3Page(aEncryptedPage.data(), aPlainPage.data(), &aReadError)) {
      if (pError != nullptr) {
        *pError = "Decrypt block failed: " + aReadError;
      }
      return false;
    }
    aPageOffset = 0;
    aPageValidBytes = aBytesRead;
    return true;
  };

  auto aReadExact = [&](std::uint64_t pSize) -> std::vector<char> {
    std::vector<char> aOutput;
    aOutput.reserve(static_cast<std::size_t>(pSize));
    std::uint64_t aRemaining = pSize;
    while (aRemaining > 0) {
      if (aPageOffset >= aPageValidBytes) {
        if (!aLoadNextPlainPage()) {
          throw std::runtime_error("Reached end of bundle set");
        }
      }
      const std::uint64_t aAvailable = aPageValidBytes - aPageOffset;
      const std::uint64_t aTake = std::min<std::uint64_t>(aRemaining, aAvailable);
      const char* aBegin = reinterpret_cast<const char*>(aPlainPage.data() + aPageOffset);
      aOutput.insert(aOutput.end(), aBegin, aBegin + static_cast<std::ptrdiff_t>(aTake));
      aPageOffset += aTake;
      aRemaining -= aTake;
    }
    return aOutput;
  };

  auto aSetFilesDone = [&](std::uint64_t pFilesDone) {
    aPacker.setFilesDone(pFilesDone);
  };

  try {
    unbundleDirectory(aOutputDirectory, aReadExact, aSetFilesDone, aCreatedFiles, aFilesDone);
    aPacker.close();
  } catch (const std::exception& aException) {
    for (const auto& aCreatedFile : aCreatedFiles) {
      std::error_code aRemoveError;
      fs::remove(aCreatedFile, aRemoveError);
    }
    if (pError != nullptr) {
      *pError = aException.what();
    }
    aPacker.close();
    return false;
  }

  pStats.mArchivesTotal = aPacker.archiveTotal();
  pStats.mArchivesTouched = aPacker.archivesTouched();
  pStats.mFilesUnbundled = aFilesDone;
  return true;
}

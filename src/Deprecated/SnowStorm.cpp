#include "Deprecated/SnowStorm.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "Globals.hpp"
#include "SnowStorm/SnowStormBundlePackerBackward.hpp"
#include "SnowStorm/SnowStormBundlePackerForward.hpp"
#include "SnowStorm/SnowStormBundleSealerBackward.hpp"
#include "SnowStorm/SnowStormBundleSealerForward.hpp"
#include "SnowStorm/SnowStormCountResult.hpp"
#include "SnowStorm/SnowStormEngine.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

void SnowStorm::writeFileEntry(const fs::path& pFilePath,
                               const std::function<void(const char*, std::uint64_t)>& pWriteBytes) const {
  std::vector<char> aHeader;
  const std::string aName = pFilePath.filename().string();
  SnowStormUtils::appendName(aHeader, aName);
  SnowStormUtils::writeUInt64(aHeader, fs::file_size(pFilePath));
  pWriteBytes(aHeader.data(), static_cast<std::uint64_t>(aHeader.size()));

  std::ifstream aInput(pFilePath, std::ios::binary);
  if (!aInput) {
    throw std::runtime_error("Failed to open file for read: " + pFilePath.string());
  }

  std::vector<char> aBlock(static_cast<std::size_t>(gBlockSizeLayer3));
  while (aInput) {
    aInput.read(aBlock.data(), static_cast<std::streamsize>(aBlock.size()));
    const std::streamsize aBytesRead = aInput.gcount();
    if (aBytesRead <= 0) {
      break;
    }
    pWriteBytes(aBlock.data(), static_cast<std::uint64_t>(aBytesRead));
  }
}

void SnowStorm::bundleDirectory(const fs::path& pDirectory,
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
    writeFileEntry(aFilePath, pWriteBytes);
    ++pFilesDone;
    pSetFilesDone(pFilesDone);
  }

  std::vector<char> aDirectoryHeader;
  SnowStormUtils::writeUInt16(aDirectoryHeader, static_cast<std::uint16_t>(aDirectories.size()));
  pWriteBytes(aDirectoryHeader.data(), static_cast<std::uint64_t>(aDirectoryHeader.size()));

  for (const auto& aDirectoryPath : aDirectories) {
    std::vector<char> aName;
    SnowStormUtils::appendName(aName, aDirectoryPath.filename().string());
    pWriteBytes(aName.data(), static_cast<std::uint64_t>(aName.size()));
    bundleDirectory(aDirectoryPath, pWriteBytes, pSetFilesDone, pFilesDone);
  }
}

void SnowStorm::unbundleDirectory(const fs::path& pDestination,
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
        const std::uint64_t aTake = std::min<std::uint64_t>(aRemaining, gBlockSizeLayer3);
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

std::string SnowStorm::readEntryName(const std::function<std::vector<char>(std::uint64_t)>& pReadExact) const {
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

SnowStorm::SnowStorm(std::uint64_t pBlockSize,
                     std::uint64_t pStorageFileSize,
                     EncryptionLayer* pCrypt1,
                     EncryptionLayer* pCrypt2,
                     EncryptionLayer* pCrypt3)
    : mEncryptionLayer1(pCrypt1),
      mEncryptionLayer2(pCrypt2),
      mEncryptionLayer3(pCrypt3),
      mStorageFileSize(pStorageFileSize),
      mReadBufferStorage(static_cast<std::size_t>(pBlockSize), 0),
      mWriteBufferStorage(static_cast<std::size_t>(pBlockSize), 0),
      mCryptBufferStorage(static_cast<std::size_t>(pBlockSize), 0) {
  if (pBlockSize != gBlockSizeLayer3) {
    throw std::runtime_error("FATAL: block size must match gBlockSizeLayer3.");
  }
  if ((pStorageFileSize % gBlockSizeLayer3) != 0) {
    throw std::runtime_error("FATAL: archive size must be an exact multiple of gBlockSizeLayer3.");
  }
  if (gBlockSizeLayer3 == 0 || mStorageFileSize == 0 || (mStorageFileSize % gBlockSizeLayer3) != 0) {
    throw std::runtime_error("Invalid block/archive sizing. archive must be divisible by block size");
  }

  mReadBuffer = mReadBufferStorage.data();
  mWriteBuffer = mWriteBufferStorage.data();
  mCryptBuffer = mCryptBufferStorage.data();
}

ShouldBundleResult SnowStorm::shouldBundle(const fs::path& pSource,
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

SnowStormBundleStats SnowStorm::bundle(const fs::path& pSource,
                                       const fs::path& pDestination,
                                       SnowStormProgressMethod pSnowStormProgressMethod) {
  const ShouldBundleResult aGate = shouldBundle(pSource, pDestination);
  if (aGate.mDecision != ShouldBundleDecision::Yes) {
    throw std::runtime_error(aGate.mMessage);
  }

  SnowStormEngine aEngine(mStorageFileSize);
  SnowStormBundleStats aStats;
  std::string aError;
  if (!aEngine.ExecuteBundle(pSource, pDestination, std::move(pSnowStormProgressMethod), aStats, &aError)) {
    throw std::runtime_error(aError);
  }
  return aStats;
}

ShouldBundleResult SnowStorm::shouldUnbundle(const fs::path& pSource,
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

SnowStormUnbundleStats SnowStorm::unbundle(const fs::path& pSource,
                                           const fs::path& pDestination,
                                           SnowStormProgressMethod pSnowStormProgressMethod) {
  const ShouldBundleResult aGate = shouldUnbundle(pSource, pDestination);
  if (aGate.mDecision != ShouldBundleDecision::Yes) {
    throw std::runtime_error(aGate.mMessage);
  }

  SnowStormEngine aEngine(mStorageFileSize);
  SnowStormUnbundleStats aStats;
  std::string aError;
  if (!aEngine.ExecuteUnbundle(pSource, pDestination, std::move(pSnowStormProgressMethod), aStats, &aError)) {
    throw std::runtime_error(aError);
  }
  return aStats;
}

std::uint64_t SnowStorm::storageFileSize() const {
  return mStorageFileSize;
}

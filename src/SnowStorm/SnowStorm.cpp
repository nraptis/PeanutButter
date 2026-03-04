#include "SnowStorm/SnowStorm.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "Globals.hpp"
#include "SnowStorm/SnowStormBundleReader.hpp"
#include "SnowStorm/SnowStormBundleWriter.hpp"
#include "SnowStorm/SnowStormCountResult.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

namespace {

bool isIgnorableMetadata(const fs::path& pPath) {
  const std::string aName = pPath.filename().string();
  return aName == ".DS_Store" ||
         aName == ".localized" ||
         aName == "Icon\r" ||
         aName == "Thumbs.db" ||
         aName == "thumbs.db" ||
         aName == "ehthumbs.db" ||
         aName == "Ehthumbs.db" ||
         aName == "Thumbs.db:encryptable";
}

bool hasMeaningfulEntries(const fs::path& pDirectory) {
  for (const auto& aEntry : fs::directory_iterator(pDirectory)) {
    if (!isIgnorableMetadata(aEntry.path())) {
      return true;
    }
  }
  return false;
}

void writeFileEntry(const fs::path& pFilePath,
                    SnowStormBundleWriter& pWriter) {
  std::vector<char> aHeader;
  const std::string aName = pFilePath.filename().string();
  SnowStormUtils::appendName(aHeader, aName);
  SnowStormUtils::writeUInt64(aHeader, fs::file_size(pFilePath));
  pWriter.writeBytes(aHeader);

  std::ifstream aInput(pFilePath, std::ios::binary);
  if (!aInput) {
    throw std::runtime_error("Failed to open file for read: " + pFilePath.string());
  }

  std::vector<char> aBlock(static_cast<std::size_t>(gBlockSize));
  while (aInput) {
    aInput.read(aBlock.data(), static_cast<std::streamsize>(aBlock.size()));
    const std::streamsize aBytesRead = aInput.gcount();
    if (aBytesRead <= 0) {
      break;
    }
    pWriter.writeBytes(aBlock.data(), static_cast<std::uint64_t>(aBytesRead));
  }
}

void bundleDirectory(const fs::path& pDirectory,
                     SnowStormBundleWriter& pWriter,
                     std::uint64_t& pFilesDone) {
  std::vector<fs::path> aFiles;
  std::vector<fs::path> aDirectories;
  SnowStormUtils::collectEntries(pDirectory, aFiles, aDirectories);

  std::vector<char> aHeader;
  SnowStormUtils::writeUInt16(aHeader, static_cast<std::uint16_t>(aFiles.size()));
  pWriter.writeBytes(aHeader);

  for (const auto& aFilePath : aFiles) {
    writeFileEntry(aFilePath, pWriter);
    ++pFilesDone;
    pWriter.setFilesDone(pFilesDone);
  }

  std::vector<char> aDirectoryHeader;
  SnowStormUtils::writeUInt16(aDirectoryHeader, static_cast<std::uint16_t>(aDirectories.size()));
  pWriter.writeBytes(aDirectoryHeader);

  for (const auto& aDirectoryPath : aDirectories) {
    std::vector<char> aName;
    SnowStormUtils::appendName(aName, aDirectoryPath.filename().string());
    pWriter.writeBytes(aName);
    bundleDirectory(aDirectoryPath, pWriter, pFilesDone);
  }
}

void unbundleDirectory(const fs::path& pDestination,
                       SnowStormBundleReader& pReader,
                       std::vector<fs::path>& pCreatedFiles,
                       std::uint64_t& pFilesDone) {
  const std::uint16_t aFileCount = SnowStormUtils::readUInt16(pReader.readExact(2));

  for (std::uint16_t aFileIndex = 0; aFileIndex < aFileCount; ++aFileIndex) {
    const std::string aName = readEntryName(pReader);
    const std::uint64_t aFileSize = SnowStormUtils::readUInt64(pReader.readExact(8));
    const fs::path aTargetPath = pDestination / aName;
    fs::create_directories(aTargetPath.parent_path());

    try {
      std::ofstream aOutput(aTargetPath, std::ios::binary | std::ios::trunc);
      if (!aOutput) {
        throw std::runtime_error("Failed creating output file: " + aTargetPath.string());
      }

      std::uint64_t aRemaining = aFileSize;
      while (aRemaining > 0) {
        const std::uint64_t aTake = std::min<std::uint64_t>(aRemaining, gBlockSize);
        const std::vector<char> aBlock = pReader.readExact(aTake);
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
    pReader.setFilesDone(pFilesDone);
  }

  const std::uint16_t aDirectoryCount = SnowStormUtils::readUInt16(pReader.readExact(2));
  for (std::uint16_t aDirectoryIndex = 0; aDirectoryIndex < aDirectoryCount; ++aDirectoryIndex) {
    const std::string aDirectoryName = readEntryName(pReader);
    const fs::path aDirectoryPath = pDestination / aDirectoryName;
    fs::create_directories(aDirectoryPath);
    unbundleDirectory(aDirectoryPath, pReader, pCreatedFiles, pFilesDone);
  }
}

}  // namespace

SnowStorm::SnowStorm(std::uint64_t pBlockSize,
                     std::uint64_t pStorageFileSize,
                     Encryptable* pCrypt)
    : mCrypt(pCrypt),
      mBlockSize(pBlockSize),
      mStorageFileSize(pStorageFileSize),
      mReadBufferStorage(static_cast<std::size_t>(pBlockSize), 0),
      mWriteBufferStorage(static_cast<std::size_t>(pBlockSize), 0),
      mCryptBufferStorage(static_cast<std::size_t>(pBlockSize), 0) {
  if (mCrypt == nullptr) {
    throw std::runtime_error("SnowStorm requires a non-null Encryptable dependency");
  }
  if (mBlockSize == 0 || mStorageFileSize == 0 || (mStorageFileSize % mBlockSize) != 0) {
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
  if (fs::exists(aOutputDirectory) && hasMeaningfulEntries(aOutputDirectory)) {
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

  const SnowStormCountResult aCounts = countDirectory(pSource);
  const std::uint64_t aArchiveCount = std::max<std::uint64_t>(1, (aCounts.mPayloadBytes + mStorageFileSize - 1) / mStorageFileSize);
  const std::uint64_t aRequiredBytes = aArchiveCount * mStorageFileSize + mStorageFileSize;
  const fs::space_info aSpace = fs::space(aOutputDirectory, aErrorCode);
  if (!aErrorCode && aSpace.available < aRequiredBytes) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Insufficient disk space for bundle";
    return aResult;
  }

  aResult.mDecision = ShouldBundleDecision::Yes;
  aResult.mMessage = "OK";
  return aResult;
}

BundleStats SnowStorm::bundle(const fs::path& pSource,
                              const fs::path& pDestination,
                              SnowStormProgressMethod pSnowStormProgressMethod) {
  const ShouldBundleResult aGate = shouldBundle(pSource, pDestination);
  if (aGate.mDecision != ShouldBundleDecision::Yes) {
    throw std::runtime_error(aGate.mMessage);
  }

  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSource, pDestination);
  fs::create_directories(aOutputDirectory);

  const SnowStormCountResult aCounts = countDirectory(pSource);
  const std::uint64_t aArchiveCount = std::max<std::uint64_t>(1, (aCounts.mPayloadBytes + mStorageFileSize - 1) / mStorageFileSize);

  std::uint64_t aFilesDone = 0;
  SnowStormBundleWriter aWriter(*this, aOutputDirectory, aArchiveCount, std::move(pSnowStormProgressMethod), aCounts.mFileCount);
  bundleDirectory(pSource, aWriter, aFilesDone);
  aWriter.close();

  BundleStats aStats;
  aStats.mFileCount = aCounts.mFileCount;
  aStats.mFolderCount = aCounts.mFolderCount;
  aStats.mPayloadBytes = aCounts.mPayloadBytes;
  aStats.mArchiveCount = aWriter.archiveCount();
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
  if (fs::exists(aOutputDirectory) && hasMeaningfulEntries(aOutputDirectory)) {
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

UnbundleStats SnowStorm::unbundle(const fs::path& pSource,
                                  const fs::path& pDestination,
                                  SnowStormProgressMethod pSnowStormProgressMethod) {
  const ShouldBundleResult aGate = shouldUnbundle(pSource, pDestination);
  if (aGate.mDecision != ShouldBundleDecision::Yes) {
    throw std::runtime_error(aGate.mMessage);
  }

  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSource, pDestination);
  fs::create_directories(aOutputDirectory);

  SnowStormBundleReader aReader(*this, pSource, std::move(pSnowStormProgressMethod));
  std::vector<fs::path> aCreatedFiles;
  std::uint64_t aFilesDone = 0;

  try {
    unbundleDirectory(aOutputDirectory, aReader, aCreatedFiles, aFilesDone);
    aReader.close();
  } catch (...) {
    for (const auto& aPath : aCreatedFiles) {
      std::error_code aErrorCode;
      fs::remove(aPath, aErrorCode);
    }
    aReader.close();
    throw;
  }

  UnbundleStats aStats;
  aStats.mFilesUnbundled = aFilesDone;
  aStats.mArchivesTotal = aReader.archiveTotal();
  aStats.mArchivesTouched = aReader.archivesTouched();
  return aStats;
}

std::uint64_t SnowStorm::blockSize() const {
  return mBlockSize;
}

std::uint64_t SnowStorm::storageFileSize() const {
  return mStorageFileSize;
}

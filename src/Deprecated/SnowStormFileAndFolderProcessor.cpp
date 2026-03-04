#include "Deprecated/SnowStormFileAndFolderProcessor.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include "Globals.hpp"
#include "SnowStorm/SnowStormCountResult.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

bool SnowStormFileAndFolderProcessor::prepareBundle(const fs::path& pSource,
                                                    const fs::path& pDestination,
                                                    fs::path& pResolvedDestination,
                                                    SnowStormBundleStats& pStats,
                                                    std::string* pError) {
  mDestinationDirectory.clear();
  mSourceDirectory = pSource;
  mDirectoryStack.clear();
  mCurrentFile.close();
  mCurrentFileRemaining = 0;
  mPendingBytes.clear();
  mPendingOffset = 0;
  mBlockBuffer.assign(static_cast<std::size_t>(gBlockSizeLayer3), 0);
  mBlockBufferUsed = 0;
  mBundleFinished = false;

  pResolvedDestination = SnowStormUtils::resolveOutputPath(pSource, pDestination);
  const SnowStormCountResult aCounts = countDirectory(pSource);
  pStats.mFileCount = aCounts.mFileCount;
  pStats.mFolderCount = aCounts.mFolderCount;
  pStats.mPayloadBytes = aCounts.mPayloadBytes;

  return pushDirectory(pSource, pError);
}

bool SnowStormFileAndFolderProcessor::pushDirectory(const fs::path& pDirectory, std::string* pError) {
  DirectoryState aState;
  aState.mDirectory = pDirectory;
  SnowStormUtils::collectEntries(pDirectory, aState.mFiles, aState.mDirectories);
  aState.mPhase = 0;
  mDirectoryStack.push_back(std::move(aState));
  (void)pError;
  return true;
}

void SnowStormFileAndFolderProcessor::queueUInt16(std::uint16_t pValue) {
  mPendingBytes.clear();
  mPendingOffset = 0;
  SnowStormUtils::writeUInt16(mPendingBytes, pValue);
}

void SnowStormFileAndFolderProcessor::queueUInt64(std::uint64_t pValue) {
  mPendingBytes.clear();
  mPendingOffset = 0;
  SnowStormUtils::writeUInt64(mPendingBytes, pValue);
}

void SnowStormFileAndFolderProcessor::queueName(const std::string& pName) {
  mPendingBytes.clear();
  mPendingOffset = 0;
  SnowStormUtils::appendName(mPendingBytes, pName);
}

bool SnowStormFileAndFolderProcessor::appendBytes(const char* pData, std::size_t pSize) {
  if (mBlockBufferUsed >= gBlockSizeLayer3) {
    return false;
  }
  const std::size_t aSpace = static_cast<std::size_t>(gBlockSizeLayer3 - mBlockBufferUsed);
  const std::size_t aTake = std::min<std::size_t>(aSpace, pSize);
  std::copy(pData, pData + static_cast<std::ptrdiff_t>(aTake), mBlockBuffer.begin() + static_cast<std::ptrdiff_t>(mBlockBufferUsed));
  mBlockBufferUsed += static_cast<std::uint64_t>(aTake);
  return aTake == pSize;
}

bool SnowStormFileAndFolderProcessor::fillBlockFromBundleStream(std::string* pError) {
  while (mBlockBufferUsed < gBlockSizeLayer3 && !mBundleFinished) {
    if (mPendingOffset < mPendingBytes.size()) {
      const std::size_t aRemain = mPendingBytes.size() - mPendingOffset;
      const std::size_t aSpace = static_cast<std::size_t>(gBlockSizeLayer3 - mBlockBufferUsed);
      const std::size_t aTake = std::min<std::size_t>(aRemain, aSpace);
      std::copy(mPendingBytes.begin() + static_cast<std::ptrdiff_t>(mPendingOffset),
                mPendingBytes.begin() + static_cast<std::ptrdiff_t>(mPendingOffset + aTake),
                mBlockBuffer.begin() + static_cast<std::ptrdiff_t>(mBlockBufferUsed));
      mPendingOffset += aTake;
      mBlockBufferUsed += static_cast<std::uint64_t>(aTake);
      continue;
    }

    if (mCurrentFileRemaining > 0) {
      const std::size_t aSpace = static_cast<std::size_t>(gBlockSizeLayer3 - mBlockBufferUsed);
      const std::size_t aTake = static_cast<std::size_t>(std::min<std::uint64_t>(mCurrentFileRemaining, aSpace));
      mCurrentFile.read(mBlockBuffer.data() + static_cast<std::ptrdiff_t>(mBlockBufferUsed),
                        static_cast<std::streamsize>(aTake));
      const std::streamsize aRead = mCurrentFile.gcount();
      if (aRead <= 0) {
        if (pError != nullptr) {
          *pError = "Failed reading file payload stream";
        }
        return false;
      }
      mCurrentFileRemaining -= static_cast<std::uint64_t>(aRead);
      mBlockBufferUsed += static_cast<std::uint64_t>(aRead);
      if (mCurrentFileRemaining == 0) {
        mCurrentFile.close();
      }
      continue;
    }

    if (mDirectoryStack.empty()) {
      mBundleFinished = true;
      break;
    }

    DirectoryState& aState = mDirectoryStack.back();
    if (aState.mPhase == 0) {
      queueUInt16(static_cast<std::uint16_t>(aState.mFiles.size()));
      aState.mPhase = 1;
      continue;
    }
    if (aState.mPhase == 1) {
      if (aState.mFileIndex >= aState.mFiles.size()) {
        aState.mPhase = 2;
        continue;
      }
      const fs::path aFilePath = aState.mFiles[aState.mFileIndex];
      queueName(aFilePath.filename().string());
      aState.mPhase = 10;
      continue;
    }
    if (aState.mPhase == 10) {
      const fs::path aFilePath = aState.mFiles[aState.mFileIndex];
      queueUInt64(fs::file_size(aFilePath));
      aState.mPhase = 11;
      continue;
    }
    if (aState.mPhase == 11) {
      const fs::path aFilePath = aState.mFiles[aState.mFileIndex];
      mCurrentFile.open(aFilePath, std::ios::binary);
      if (!mCurrentFile) {
        if (pError != nullptr) {
          *pError = "Failed to open file for read: " + aFilePath.string();
        }
        return false;
      }
      mCurrentFileRemaining = fs::file_size(aFilePath);
      ++aState.mFileIndex;
      aState.mPhase = 1;
      continue;
    }
    if (aState.mPhase == 2) {
      queueUInt16(static_cast<std::uint16_t>(aState.mDirectories.size()));
      aState.mPhase = 3;
      continue;
    }
    if (aState.mPhase == 3) {
      if (aState.mDirectoryIndex >= aState.mDirectories.size()) {
        mDirectoryStack.pop_back();
        continue;
      }
      const fs::path aChild = aState.mDirectories[aState.mDirectoryIndex];
      ++aState.mDirectoryIndex;
      queueName(aChild.filename().string());
      aState.mPhase = 30;
      continue;
    }
    if (aState.mPhase == 30) {
      const fs::path aChild = aState.mDirectories[aState.mDirectoryIndex - 1];
      if (!pushDirectory(aChild, pError)) {
        return false;
      }
      aState.mPhase = 3;
      continue;
    }
  }

  return true;
}

bool SnowStormFileAndFolderProcessor::step(std::string* pError) {
  mBlockBufferUsed = 0;
  if (!fillBlockFromBundleStream(pError)) {
    return false;
  }
  return mBlockBufferUsed > 0;
}

bool SnowStormFileAndFolderProcessor::empty() const {
  return mBundleFinished && mBlockBufferUsed == 0;
}

void SnowStormFileAndFolderProcessor::floodRemainingSpaceWithZeros() {
  // Sealer performs final padding.
}

const char* SnowStormFileAndFolderProcessor::blockData() const {
  return mBlockBuffer.empty() ? nullptr : mBlockBuffer.data();
}

std::uint64_t SnowStormFileAndFolderProcessor::blockSize() const {
  return mBlockBufferUsed;
}

bool SnowStormFileAndFolderProcessor::prepareUnbundle(const fs::path& pDestination,
                                                      fs::path& pResolvedDestination,
                                                      std::string* pError) {
  (void)pError;
  mDestinationDirectory = pDestination;
  pResolvedDestination = pDestination;
  return true;
}

std::string SnowStormFileAndFolderProcessor::readEntryName(
    const std::function<std::vector<char>(std::uint64_t)>& pReadExact) const {
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

bool SnowStormFileAndFolderProcessor::deserializeDirectory(
    const fs::path& pDestination,
    const std::function<std::vector<char>(std::uint64_t)>& pReadExact,
    std::vector<fs::path>& pCreatedFiles,
    std::uint64_t& pFilesDone,
    std::string* pError) {
  const std::uint16_t aFileCount = SnowStormUtils::readUInt16(pReadExact(2));
  for (std::uint16_t aFileIndex = 0; aFileIndex < aFileCount; ++aFileIndex) {
    const std::string aName = readEntryName(pReadExact);
    const std::uint64_t aFileSize = SnowStormUtils::readUInt64(pReadExact(8));
    const fs::path aTargetPath = pDestination / aName;
    fs::create_directories(aTargetPath.parent_path());

    try {
      std::ofstream aOutput(aTargetPath, std::ios::binary | std::ios::trunc);
      if (!aOutput) {
        if (pError != nullptr) {
          *pError = "Failed creating output file: " + aTargetPath.string();
        }
        return false;
      }

      std::uint64_t aRemaining = aFileSize;
      while (aRemaining > 0) {
        const std::uint64_t aTake = std::min<std::uint64_t>(aRemaining, gBlockSizeLayer3);
        const std::vector<char> aBlock = pReadExact(aTake);
        aOutput.write(aBlock.data(), static_cast<std::streamsize>(aBlock.size()));
        if (!aOutput) {
          if (pError != nullptr) {
            *pError = "Failed writing output file: " + aTargetPath.string();
          }
          return false;
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
  }

  const std::uint16_t aDirectoryCount = SnowStormUtils::readUInt16(pReadExact(2));
  for (std::uint16_t aDirectoryIndex = 0; aDirectoryIndex < aDirectoryCount; ++aDirectoryIndex) {
    const std::string aDirectoryName = readEntryName(pReadExact);
    const fs::path aDirectoryPath = pDestination / aDirectoryName;
    fs::create_directories(aDirectoryPath);
    if (!deserializeDirectory(aDirectoryPath, pReadExact, pCreatedFiles, pFilesDone, pError)) {
      return false;
    }
  }

  return true;
}

bool SnowStormFileAndFolderProcessor::unbundleFromStream(
    const std::function<std::vector<char>(std::uint64_t)>& pReadExact,
    SnowStormUnbundleStats& pStats,
    std::string* pError) {
  std::vector<fs::path> aCreatedFiles;
  std::uint64_t aFilesDone = 0;
  if (!deserializeDirectory(mDestinationDirectory, pReadExact, aCreatedFiles, aFilesDone, pError)) {
    return false;
  }
  pStats.mFilesUnbundled = aFilesDone;
  return true;
}

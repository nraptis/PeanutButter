#include "SnowStorm/SnowStormEngine.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include "Globals.hpp"
#include "IO/FileReaderDelegate.hpp"
#include "IO/FileReaderImplementation.hpp"
#include "IO/FileWriterDelegate.hpp"
#include "IO/FileWriterImplementation.hpp"
#include "LayeredEncryption/LayeredEncryptionDelegate.hpp"
#include "LayeredEncryption/LayeredEncryptionImplementation.hpp"
#include "SnowStorm/SnowStormBundlePackerBackward.hpp"
#include "SnowStorm/SnowStormBundlePackerForward.hpp"
#include "SnowStorm/SnowStormBundleSealerBackward.hpp"
#include "SnowStorm/SnowStormBundleSealerForward.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kL1RecoveryHeaderBytes = static_cast<std::size_t>(RECOVERY_CHUNK_SIZE);

std::vector<fs::path> collectFilesRecursive(const fs::path& pRoot, const FileReaderDelegate& pReader) {
  std::vector<fs::path> aFiles;
  std::string aError;
  if (!pReader.listFilesRecursive(pRoot, aFiles, &aError)) {
    throw std::runtime_error(aError.empty() ? "Failed to list files recursively" : aError);
  }
  aFiles.erase(
      std::remove_if(aFiles.begin(), aFiles.end(), [](const fs::path& pPath) { return SnowStormUtils::shouldIgnoreEntry(pPath); }),
      aFiles.end());
  std::sort(aFiles.begin(), aFiles.end(), [](const fs::path& pA, const fs::path& pB) {
    return pA.generic_string() < pB.generic_string();
  });
  return aFiles;
}

bool hasOversizedPathComponent(const std::string& pPath) {
  std::size_t aStart = 0;
  while (aStart <= pPath.size()) {
    const std::size_t aSep = pPath.find('/', aStart);
    const std::size_t aLen = (aSep == std::string::npos) ? (pPath.size() - aStart) : (aSep - aStart);
    if (aLen == 0 || aLen > MAX_STORABLE_PATH_LENGTH) {
      return true;
    }
    if (aSep == std::string::npos) {
      break;
    }
    aStart = aSep + 1;
  }
  return false;
}

bool hasMeaningfulEntries(const fs::path& pDirectory, const FileReaderDelegate& pReader) {
  std::vector<fs::path> aFiles;
  std::string aError;
  if (!pReader.listFilesRecursive(pDirectory, aFiles, &aError)) {
    return false;
  }
  for (const auto& aFile : aFiles) {
    if (!SnowStormUtils::shouldIgnoreEntry(aFile)) {
      return true;
    }
  }
  return false;
}

}  // namespace

SnowStormEngine::SnowStormEngine(std::uint64_t pStorageFileSize)
    : mStorageFileSize(pStorageFileSize) {
  static FileReaderImplementation sReader;
  static FileWriterImplementation sWriter;
  mReader = &sReader;
  mWriter = &sWriter;
  mOwnedLayeredEncryption = std::make_unique<LayeredEncryptionImplementation>();
  mLayeredEncryption = mOwnedLayeredEncryption.get();
}

SnowStormEngine::SnowStormEngine(std::uint64_t pStorageFileSize,
                                 FileReaderDelegate& pReader,
                                 FileWriterDelegate& pWriter)
    : mStorageFileSize(pStorageFileSize),
      mReader(&pReader),
      mWriter(&pWriter) {
  mOwnedLayeredEncryption = std::make_unique<LayeredEncryptionImplementation>();
  mLayeredEncryption = mOwnedLayeredEncryption.get();
}

SnowStormEngine::SnowStormEngine(std::uint64_t pStorageFileSize,
                                 FileReaderDelegate& pReader,
                                 FileWriterDelegate& pWriter,
                                 LayeredEncryptionDelegate& pLayeredEncryption)
    : mStorageFileSize(pStorageFileSize),
      mReader(&pReader),
      mWriter(&pWriter),
      mLayeredEncryption(&pLayeredEncryption) {}

ShouldBundleResult SnowStormEngine::shouldBundle(const fs::path& pSource,
                                                 const fs::path& pDestination) const {
  ShouldBundleResult aResult;
  if (!mReader->exists(pSource) || !mReader->isDirectory(pSource)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Source path must be an existing directory";
    return aResult;
  }
  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSource, pDestination);
  aResult.mResolvedDestination = aOutputDirectory;
  if (mReader->exists(aOutputDirectory) && !mReader->isDirectory(aOutputDirectory)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Destination exists and is not a directory: " + aOutputDirectory.string();
    return aResult;
  }
  if (mReader->exists(aOutputDirectory) && hasMeaningfulEntries(aOutputDirectory, *mReader)) {
    aResult.mDecision = ShouldBundleDecision::Prompt;
    aResult.mMessage = "Pack destination is not empty: " + aOutputDirectory.string();
    return aResult;
  }
  std::string aCreateError;
  if (!mWriter->createDirectories(aOutputDirectory, &aCreateError)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = aCreateError.empty() ? ("Failed to create output directory: " + aOutputDirectory.string())
                                            : aCreateError;
    return aResult;
  }
  aResult.mDecision = ShouldBundleDecision::Yes;
  aResult.mMessage = "OK";
  return aResult;
}

ShouldBundleResult SnowStormEngine::shouldUnbundle(const fs::path& pSource,
                                                   const fs::path& pDestination) const {
  ShouldBundleResult aResult;
  if (!mReader->exists(pSource) || !mReader->isDirectory(pSource)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Archive source path must be an existing directory";
    return aResult;
  }
  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSource, pDestination);
  aResult.mResolvedDestination = aOutputDirectory;
  if (mReader->exists(aOutputDirectory) && !mReader->isDirectory(aOutputDirectory)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = "Destination exists and is not a directory: " + aOutputDirectory.string();
    return aResult;
  }
  if (mReader->exists(aOutputDirectory) && hasMeaningfulEntries(aOutputDirectory, *mReader)) {
    aResult.mDecision = ShouldBundleDecision::Prompt;
    aResult.mMessage = "Unpack destination is not empty: " + aOutputDirectory.string();
    return aResult;
  }
  std::string aCreateError;
  if (!mWriter->createDirectories(aOutputDirectory, &aCreateError)) {
    aResult.mDecision = ShouldBundleDecision::No;
    aResult.mMessage = aCreateError.empty() ? ("Failed to create unbundle destination: " + aOutputDirectory.string())
                                            : aCreateError;
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
                                                 SnowStormProgressMethod pProgress,
                                                 std::uint64_t pStartArchiveIndex,
                                                 std::uint64_t pStartByteOffsetInArchive) {
  const ShouldBundleResult aGate = shouldUnbundle(pSourceDirectory, pDestinationDirectory);
  if (aGate.mDecision != ShouldBundleDecision::Yes) {
    throw std::runtime_error(aGate.mMessage);
  }
  SnowStormUnbundleStats aStats;
  std::string aError;
  if (!ExecuteUnbundle(pSourceDirectory,
                       pDestinationDirectory,
                       std::move(pProgress),
                       aStats,
                       &aError,
                       pStartArchiveIndex,
                       pStartByteOffsetInArchive)) {
    throw std::runtime_error(aError);
  }
  return aStats;
}

bool SnowStormEngine::ExecuteBundle(const fs::path& pSourceDirectory,
                                    const fs::path& pDestinationDirectory,
                                    SnowStormProgressMethod pProgress,
                                    SnowStormBundleStats& pStats,
                                    std::string* pError) {
  fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSourceDirectory, pDestinationDirectory);
  if (!mWriter->createDirectories(aOutputDirectory, pError)) {
    return false;
  }

  std::vector<fs::path> aFiles = collectFilesRecursive(pSourceDirectory, *mReader);
  pStats.mFileCount = static_cast<std::uint64_t>(aFiles.size());
  pStats.mFolderCount = 0;
  pStats.mPayloadBytes = 0;
  for (const fs::path& aFile : aFiles) {
    std::uint64_t aSize = 0;
    if (!mReader->fileSize(aFile, &aSize, pError)) {
      return false;
    }
    pStats.mPayloadBytes += aSize;
  }

  const std::uint64_t aArchiveCount =
      std::max<std::uint64_t>(1, (pStats.mPayloadBytes + mStorageFileSize - 1) / mStorageFileSize);

  EncryptionLayer* aLayer1 = mLayeredEncryption != nullptr ? mLayeredEncryption->getEncryptionLayer1() : nullptr;
  EncryptionLayer* aLayer2 = mLayeredEncryption != nullptr ? mLayeredEncryption->getEncryptionLayer2() : nullptr;
  EncryptionLayer* aLayer3 = mLayeredEncryption != nullptr ? mLayeredEncryption->getEncryptionLayer3() : nullptr;
  if (aLayer1 == nullptr || aLayer2 == nullptr || aLayer3 == nullptr) {
    if (pError != nullptr) {
      *pError = "SnowStormEngine requires non-null encryption layers.";
    }
    return false;
  }

  SnowStormBundlePackerForward aPacker(aOutputDirectory, mStorageFileSize, *mWriter);
  SnowStormBundleSealerForward aSealer(aLayer1, aLayer2, aLayer3);
  std::vector<unsigned char> aPageBuffer(BLOCK_SIZE_LAYER_3, 0);
  std::uint64_t aPositionInPage = 0;
  std::uint64_t aFilesDone = 0;
  std::uint64_t aGlobalOffset = 0;
  std::int64_t aTrackedArchiveIndex = -1;
  bool aArchiveHasFileStart = false;
  if (BLOCK_SIZE_LAYER_1 <= kL1RecoveryHeaderBytes) {
    if (pError != nullptr) {
      *pError = "BLOCK_SIZE_LAYER_1 must be larger than kL1RecoveryHeaderBytes.";
    }
    return false;
  }

  auto aFormatBytesHex = [](const char* pBytes, std::size_t pCount) {
    static constexpr char cHex[] = "0123456789ABCDEF";
    std::string aText;
    aText.reserve(pCount * 3);
    for (std::size_t aIndex = 0; aIndex < pCount; ++aIndex) {
      const unsigned char aByte = static_cast<unsigned char>(pBytes[aIndex]);
      if (aIndex != 0) {
        aText.push_back(' ');
      }
      aText.push_back(cHex[(aByte >> 4) & 0x0F]);
      aText.push_back(cHex[aByte & 0x0F]);
    }
    return aText;
  };

  auto aWritePlainBytes = [&](const char* aBlockData,
                              std::uint64_t aBlockSize,
                              std::uint64_t pNextFileOffset) {
    auto aSyncArchiveTracking = [&]() {
      const std::int64_t aIndex = aPacker.currentArchiveIndex();
      if (aIndex != aTrackedArchiveIndex) {
        aTrackedArchiveIndex = aIndex;
        aArchiveHasFileStart = false;
      }
    };

    std::uint64_t aOffset = 0;
    while (aOffset < aBlockSize) {
      if (aPositionInPage == BLOCK_SIZE_LAYER_3) {
        std::string aSealerError;
        if (!aSealer.sealPage(aPageBuffer.data(), &aSealerError)) {
          if (pError != nullptr) {
            *pError = "Sealer sealPage failed: " + aSealerError;
          }
          throw std::runtime_error("Sealer sealPage failed: " + aSealerError);
        }
        std::memcpy(gTempL3BufferA, aSealer.mBufferSealed.data(), static_cast<std::size_t>(BLOCK_SIZE_LAYER_3));
        std::string aWriteError;
        if (!aPacker.writePage(gTempL3BufferA, &aWriteError)) {
          if (pError != nullptr) {
            *pError = "Packer writePage failed: " + aWriteError;
          }
          throw std::runtime_error("Packer writePage failed: " + aWriteError);
        }
        aPositionInPage = 0;
        aSyncArchiveTracking();
      }

      const std::uint64_t aL1Offset = aPositionInPage % BLOCK_SIZE_LAYER_1;
      if (aL1Offset == 0) {
        std::vector<char> aL1Header;
        aL1Header.reserve(kL1RecoveryHeaderBytes);
        SnowStormUtils::writeUInt48(aL1Header, pNextFileOffset);
        const std::uint64_t aDecoded = SnowStormUtils::readUInt48(aL1Header);
        std::cerr << "pack l1hdr bytes=[" << aFormatBytesHex(aL1Header.data(), kL1RecoveryHeaderBytes)
                  << "] next_file_offset=" << aDecoded << "\n";
        std::memcpy(aPageBuffer.data() + static_cast<std::size_t>(aPositionInPage),
                    aL1Header.data(),
                    static_cast<std::size_t>(kL1RecoveryHeaderBytes));
        aPositionInPage += kL1RecoveryHeaderBytes;
        aGlobalOffset += kL1RecoveryHeaderBytes;
      }

      const std::uint64_t aInBlock = aPositionInPage % BLOCK_SIZE_LAYER_1;
      const std::uint64_t aRemainingInL1Payload = BLOCK_SIZE_LAYER_1 - aInBlock;
      const std::uint64_t aChunk = std::min<std::uint64_t>(aRemainingInL1Payload, aBlockSize - aOffset);
      std::memcpy(aPageBuffer.data() + static_cast<std::size_t>(aPositionInPage),
                  aBlockData + static_cast<std::ptrdiff_t>(aOffset),
                  static_cast<std::size_t>(aChunk));
      aPositionInPage += aChunk;
      aOffset += aChunk;
      aGlobalOffset += aChunk;

      if (aPositionInPage == BLOCK_SIZE_LAYER_3) {
        std::string aSealerError;
        if (!aSealer.sealPage(aPageBuffer.data(), &aSealerError)) {
          if (pError != nullptr) {
            *pError = "Sealer sealPage failed: " + aSealerError;
          }
          throw std::runtime_error("Sealer sealPage failed: " + aSealerError);
        }
        std::memcpy(gTempL3BufferA, aSealer.mBufferSealed.data(), static_cast<std::size_t>(BLOCK_SIZE_LAYER_3));
        std::string aWriteError;
        if (!aPacker.writePage(gTempL3BufferA, &aWriteError)) {
          if (pError != nullptr) {
            *pError = "Packer writePage failed: " + aWriteError;
          }
          throw std::runtime_error("Packer writePage failed: " + aWriteError);
        }
        aPositionInPage = 0;
        aSyncArchiveTracking();
      }
    }
  };

  try {
    std::random_device aRd;
    std::mt19937_64 aRng(aRd());
    std::shuffle(aFiles.begin(), aFiles.end(), aRng);
    struct FilePlan {
      fs::path mAbsPath;
      std::string mRelativeName;
      std::uint64_t mSize = 0;
      std::uint64_t mNextFileOffset = 0;
    };
    std::vector<FilePlan> aPlan;
    aPlan.reserve(aFiles.size());
    std::uint64_t aLogicalOffset = 0;
    for (const fs::path& aPath : aFiles) {
      FilePlan aItem;
      aItem.mAbsPath = aPath;
      aItem.mRelativeName = fs::relative(aPath, pSourceDirectory).generic_string();
      if (aItem.mRelativeName.empty()) {
        continue;
      }
      if (aItem.mRelativeName.size() > 0xFFFFu) {
        throw std::runtime_error("Path too long: " + aItem.mRelativeName);
      }
      std::uint64_t aFileSize = 0;
      if (!mReader->fileSize(aPath, &aFileSize, pError)) {
        throw std::runtime_error((pError != nullptr && !pError->empty()) ? *pError : "Failed to get file size");
      }
      aItem.mSize = aFileSize;
      aPlan.push_back(aItem);
    }
    for (std::size_t aIndex = 0; aIndex < aPlan.size(); ++aIndex) {
      const std::uint64_t aRecordBytes =
          2ULL + static_cast<std::uint64_t>(aPlan[aIndex].mRelativeName.size()) + 8ULL + aPlan[aIndex].mSize;
      aLogicalOffset += aRecordBytes;
      aPlan[aIndex].mNextFileOffset = (aIndex + 1 < aPlan.size()) ? aLogicalOffset : 0ULL;
    }

    pStats.mFileCount = aPlan.size();
    std::uniform_int_distribution<std::uint64_t> aDist(1ULL, 0x0000FFFFFFFFFFFFULL);
    const std::uint64_t aFirstFileMarker = aDist(aRng);
    const std::uint64_t aFirstArchiveInvalidOffset =
        (mStorageFileSize < std::numeric_limits<std::uint64_t>::max()) ?
            std::uniform_int_distribution<std::uint64_t>(mStorageFileSize + 1ULL,
                                                         std::numeric_limits<std::uint64_t>::max())(aRng) :
            std::numeric_limits<std::uint64_t>::max();

    for (std::size_t aIndex = 0; aIndex < aPlan.size(); ++aIndex) {
      const FilePlan& aItem = aPlan[aIndex];
      const std::uint64_t aNextFileOffset = (aIndex == 0) ? aFirstFileMarker : aItem.mNextFileOffset;
      const bool aStartsAtNextArchive =
          (aPacker.positionInArchive() == mStorageFileSize && aPositionInPage == 0);
      if (aStartsAtNextArchive) {
        aArchiveHasFileStart = false;
      }
      const std::uint64_t aFileStartOffsetInArchive =
          aStartsAtNextArchive ? 0 : (aPacker.positionInArchive() + aPositionInPage);
      if (!aArchiveHasFileStart) {
        const bool aIsFirstArchiveHeader = (aIndex == 0 && aPacker.currentArchiveIndex() <= 0);
        const std::uint64_t aHeaderOffset = aIsFirstArchiveHeader ? aFirstArchiveInvalidOffset : aFileStartOffsetInArchive;
        const bool aHasRecoveryStart = !aIsFirstArchiveHeader;
        std::string aHeaderError;
        if (!aPacker.setCurrentArchiveFirstFileOffset(aHeaderOffset, aHasRecoveryStart, &aHeaderError)) {
          throw std::runtime_error("Failed setting archive first-file offset: " + aHeaderError);
        }
        aArchiveHasFileStart = true;
      }
      std::vector<char> aRecordHeader;
      SnowStormUtils::writeUInt16(aRecordHeader, static_cast<std::uint16_t>(aItem.mRelativeName.size()));
      aRecordHeader.insert(aRecordHeader.end(), aItem.mRelativeName.begin(), aItem.mRelativeName.end());
      SnowStormUtils::writeUInt64(aRecordHeader, aItem.mSize);
      aWritePlainBytes(aRecordHeader.data(), static_cast<std::uint64_t>(aRecordHeader.size()), aNextFileOffset);

      std::vector<unsigned char> aChunkBytes;
      std::uint64_t aReadOffset = 0;
      while (aReadOffset < aItem.mSize) {
        const std::uint64_t aWant = std::min<std::uint64_t>(BLOCK_SIZE_LAYER_2, aItem.mSize - aReadOffset);
        std::string aReadError;
        if (!mReader->readRange(aItem.mAbsPath, aReadOffset, aWant, aChunkBytes, &aReadError)) {
          throw std::runtime_error(aReadError.empty() ? ("Failed to open file for read: " + aItem.mAbsPath.string())
                                                      : aReadError);
        }
        if (aChunkBytes.empty()) {
          break;
        }
        aWritePlainBytes(reinterpret_cast<const char*>(aChunkBytes.data()),
                         static_cast<std::uint64_t>(aChunkBytes.size()),
                         aNextFileOffset);
        aReadOffset += static_cast<std::uint64_t>(aChunkBytes.size());
      }
      ++aFilesDone;
      if (pProgress) {
        const std::uint64_t aArchiveIndex = (aGlobalOffset / mStorageFileSize) + 1;
        pProgress(aArchiveIndex, aArchiveCount, aFilesDone, pStats.mFileCount);
      }
    }

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
    std::memcpy(gTempL3BufferA, aSealer.mBufferSealed.data(), static_cast<std::size_t>(BLOCK_SIZE_LAYER_3));
    std::string aWriteError;
    if (!aPacker.writePage(gTempL3BufferA, &aWriteError)) {
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
    pProgress(pStats.mArchiveCount, aArchiveCount, aFilesDone, pStats.mFileCount);
  }
  return true;
}

bool SnowStormEngine::ExecuteUnbundle(const fs::path& pSourceDirectory,
                                      const fs::path& pDestinationDirectory,
                                      SnowStormProgressMethod pProgress,
                                      SnowStormUnbundleStats& pStats,
                                      std::string* pError,
                                      std::uint64_t pStartArchiveIndex,
                                      std::uint64_t pStartByteOffsetInArchive) {
  const fs::path aOutputDirectory = SnowStormUtils::resolveOutputPath(pSourceDirectory, pDestinationDirectory);
  if (!mWriter->createDirectories(aOutputDirectory, pError)) {
    return false;
  }

  EncryptionLayer* aLayer1 = mLayeredEncryption != nullptr ? mLayeredEncryption->getEncryptionLayer1() : nullptr;
  EncryptionLayer* aLayer2 = mLayeredEncryption != nullptr ? mLayeredEncryption->getEncryptionLayer2() : nullptr;
  EncryptionLayer* aLayer3 = mLayeredEncryption != nullptr ? mLayeredEncryption->getEncryptionLayer3() : nullptr;
  if (aLayer1 == nullptr || aLayer2 == nullptr || aLayer3 == nullptr) {
    if (pError != nullptr) {
      *pError = "SnowStormEngine requires non-null encryption layers.";
    }
    return false;
  }

  SnowStormBundlePackerBackward aPacker(pSourceDirectory, std::move(pProgress), *mReader, pStartArchiveIndex);
  SnowStormBundleSealerBackward aSealer(aLayer1, aLayer2, aLayer3);
  std::vector<unsigned char> aEncryptedPage(BLOCK_SIZE_LAYER_3, 0);
  std::vector<unsigned char> aPlainPage(BLOCK_SIZE_LAYER_3, 0);
  std::vector<char> aStreamBuffer;
  std::uint64_t aFilesDone = 0;
  std::string aCurrentPath;
  std::uint64_t aCurrentRemaining = 0;
  std::uint64_t aCurrentWritten = 0;

  auto aParseBuffer = [&](bool pFinalPass) {
    while (true) {
      if (aCurrentRemaining > 0) {
        if (aStreamBuffer.empty()) {
          break;
        }
        const std::size_t aTake = static_cast<std::size_t>(
            std::min<std::uint64_t>(aCurrentRemaining, static_cast<std::uint64_t>(aStreamBuffer.size())));
        std::string aWriteError;
        if (!mWriter->writeAt(fs::path(aCurrentPath),
                              aCurrentWritten,
                              reinterpret_cast<const unsigned char*>(aStreamBuffer.data()),
                              static_cast<std::uint64_t>(aTake),
                              &aWriteError)) {
          throw std::runtime_error("Failed writing output file: " + aCurrentPath);
        }
        aCurrentWritten += static_cast<std::uint64_t>(aTake);
        aCurrentRemaining -= static_cast<std::uint64_t>(aTake);
        aStreamBuffer.erase(aStreamBuffer.begin(), aStreamBuffer.begin() + static_cast<std::ptrdiff_t>(aTake));
        if (aCurrentRemaining == 0) {
          ++aFilesDone;
          aPacker.setFilesDone(aFilesDone);
        }
        continue;
      }

      if (aStreamBuffer.size() < 2) {
        break;
      }
      const std::vector<char> aLenRaw = {aStreamBuffer[0], aStreamBuffer[1]};
      const std::uint16_t aPathLen = SnowStormUtils::readUInt16(aLenRaw);
      if (aPathLen == 0 || aPathLen > MAX_STORABLE_PATH_LENGTH * 8) {
        aStreamBuffer.erase(aStreamBuffer.begin());
        continue;
      }
      const std::size_t aHeaderSize = 2 + static_cast<std::size_t>(aPathLen) + 8;
      if (aStreamBuffer.size() < aHeaderSize) {
        break;
      }
      const std::string aPath(aStreamBuffer.begin() + 2, aStreamBuffer.begin() + 2 + aPathLen);
      if (aPath.find("..") != std::string::npos || aPath.empty() ||
          aPath.find('\\') != std::string::npos || aPath[0] == '/' || hasOversizedPathComponent(aPath)) {
        aStreamBuffer.erase(aStreamBuffer.begin());
        continue;
      }
      const std::vector<char> aSizeRaw(aStreamBuffer.begin() + 2 + aPathLen, aStreamBuffer.begin() + aHeaderSize);
      const std::uint64_t aFileSize = SnowStormUtils::readUInt64(aSizeRaw);

      const fs::path aOutputFile = aOutputDirectory / fs::path(aPath);
      std::string aCreateError;
      if (!mWriter->createDirectories(aOutputFile.parent_path(), &aCreateError)) {
        throw std::runtime_error("Failed creating output file: " + aOutputFile.string());
      }
      std::string aTruncateError;
      if (!mWriter->truncate(aOutputFile, 0, &aTruncateError)) {
        throw std::runtime_error("Failed creating output file: " + aOutputFile.string());
      }
      aCurrentPath = aOutputFile.string();
      aCurrentRemaining = aFileSize;
      aCurrentWritten = 0;
      aStreamBuffer.erase(aStreamBuffer.begin(), aStreamBuffer.begin() + static_cast<std::ptrdiff_t>(aHeaderSize));
      if (aCurrentRemaining == 0) {
        ++aFilesDone;
        aPacker.setFilesDone(aFilesDone);
      }
    }
    if (pFinalPass && aCurrentRemaining != 0) {
      throw std::runtime_error("Unexpected EOF while reading file payload");
    }
  };

  auto aFormatBytesHex = [](const unsigned char* pBytes, std::size_t pCount) {
    static constexpr char cHex[] = "0123456789ABCDEF";
    std::string aText;
    aText.reserve(pCount * 3);
    for (std::size_t aIndex = 0; aIndex < pCount; ++aIndex) {
      const unsigned char aByte = pBytes[aIndex];
      if (aIndex != 0) {
        aText.push_back(' ');
      }
      aText.push_back(cHex[(aByte >> 4) & 0x0F]);
      aText.push_back(cHex[aByte & 0x0F]);
    }
    return aText;
  };

  auto aReadU48Raw = [](const unsigned char* pBytes) {
    std::vector<char> aRaw(6, 0);
    for (std::size_t aIndex = 0; aIndex < 6; ++aIndex) {
      aRaw[aIndex] = static_cast<char>(pBytes[aIndex]);
    }
    return SnowStormUtils::readUInt48(aRaw);
  };

  auto aExtractPayloadFromPage = [&](std::uint64_t pBytesRead, std::size_t pRawStartOffset) {
    std::vector<char> aPayload;
    std::size_t aPos = std::min<std::size_t>(pRawStartOffset, static_cast<std::size_t>(pBytesRead));
    while (aPos < static_cast<std::size_t>(pBytesRead)) {
      const std::size_t aInBlock = aPos % static_cast<std::size_t>(BLOCK_SIZE_LAYER_1);
      if (aInBlock == 0) {
        if (aPos + kL1RecoveryHeaderBytes > static_cast<std::size_t>(pBytesRead)) {
          break;
        }
        const unsigned char* aHeader = aPlainPage.data() + aPos;
        const std::uint64_t aNextOffset = aReadU48Raw(aHeader);
        std::cerr << "l1hdr bytes=[" << aFormatBytesHex(aHeader, kL1RecoveryHeaderBytes)
                  << "] next_file_offset=" << aNextOffset << "\n";
        aPos += kL1RecoveryHeaderBytes;
      }
      if (aPos >= static_cast<std::size_t>(pBytesRead)) {
        break;
      }
      const std::size_t aNowInBlock = aPos % static_cast<std::size_t>(BLOCK_SIZE_LAYER_1);
      const std::size_t aTake = std::min<std::size_t>(static_cast<std::size_t>(BLOCK_SIZE_LAYER_1) - aNowInBlock,
                                                       static_cast<std::size_t>(pBytesRead) - aPos);
      aPayload.insert(aPayload.end(),
                      reinterpret_cast<const char*>(aPlainPage.data() + aPos),
                      reinterpret_cast<const char*>(aPlainPage.data() + aPos + aTake));
      aPos += aTake;
    }
    return aPayload;
  };

  try {
    std::uint64_t aRawSkipRemaining = pStartByteOffsetInArchive;
    while (true) {
      std::uint64_t aBytesRead = 0;
      std::string aReadError;
      if (!aPacker.readPage(aEncryptedPage.data(), &aBytesRead, &aReadError)) {
        break;
      }

      if (!aReadError.empty()) {
        continue;
      }

      std::string aDecryptError;
      if (!aSealer.unsealPage(aEncryptedPage.data(), aPlainPage.data(), &aDecryptError)) {
        continue;
      }

      std::size_t aPageRawStart = 0;
      if (aRawSkipRemaining > 0) {
        const std::uint64_t aDrop = std::min<std::uint64_t>(aRawSkipRemaining, aBytesRead);
        aRawSkipRemaining -= aDrop;
        aPageRawStart = static_cast<std::size_t>(aDrop);
      }

      const std::vector<char> aPayloadPage = aExtractPayloadFromPage(aBytesRead, aPageRawStart);
      if (!aPayloadPage.empty()) {
        const char* aBegin = aPayloadPage.data();
        aStreamBuffer.insert(aStreamBuffer.end(),
                             aBegin,
                             aBegin + static_cast<std::ptrdiff_t>(aPayloadPage.size()));
        aParseBuffer(false);
      }
    }

    aParseBuffer(true);
    aPacker.close();
  } catch (const std::exception& aException) {
    if (pError != nullptr) {
      *pError = aException.what();
    }
    aPacker.close();
    return false;
  }

  pStats.mArchivesTotal = aPacker.archiveTotal();
  pStats.mArchivesTouched = aPacker.archivesTouched();
  pStats.mFilesUnbundled = aFilesDone;
  if (aFilesDone == 0 && pError != nullptr && pError->empty()) {
    *pError = "No recoverable files found in archive stream.";
  }
  return aFilesDone > 0;
}

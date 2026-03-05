#include "SnowStorm/SnowStormBundlePackerForward.hpp"

#include <array>
#include <string>

#include "Globals.hpp"
#include "IO/FileWriterDelegate.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

namespace {
constexpr std::uint64_t kPlainHeaderSize = PLAIN_TEXT_HEADER_LENGTH;
constexpr std::uint16_t kPlainHeaderFlagHasRecoveryStart = 0x0001U;

void appendU16(std::array<unsigned char, 16>& pHeader, std::size_t pOffset, std::uint16_t pValue) {
  pHeader[pOffset + 0] = static_cast<unsigned char>(pValue & 0xFFU);
  pHeader[pOffset + 1] = static_cast<unsigned char>((pValue >> 8) & 0xFFU);
}

void appendU32(std::array<unsigned char, 16>& pHeader, std::size_t pOffset, std::uint32_t pValue) {
  pHeader[pOffset + 0] = static_cast<unsigned char>(pValue & 0xFFU);
  pHeader[pOffset + 1] = static_cast<unsigned char>((pValue >> 8) & 0xFFU);
  pHeader[pOffset + 2] = static_cast<unsigned char>((pValue >> 16) & 0xFFU);
  pHeader[pOffset + 3] = static_cast<unsigned char>((pValue >> 24) & 0xFFU);
}

void appendU64(std::array<unsigned char, 16>& pHeader, std::size_t pOffset, std::uint64_t pValue) {
  for (int aShift = 0; aShift < 8; ++aShift) {
    pHeader[pOffset + static_cast<std::size_t>(aShift)] =
        static_cast<unsigned char>((pValue >> (8 * aShift)) & 0xFFU);
  }
}

}  // namespace

SnowStormBundlePackerForward::SnowStormBundlePackerForward(const fs::path& pOutputDirectory,
                                                           std::uint64_t pStorageFileSize,
                                                           FileWriterDelegate& pWriter)
    : mWriter(pWriter),
      mOutputDirectory(pOutputDirectory),
      mStorageFileSize(pStorageFileSize) {}

SnowStormBundlePackerForward::~SnowStormBundlePackerForward() {
}

bool SnowStormBundlePackerForward::openNextArchive(std::string* pError) {
  if (mCurrentArchiveIndex >= 0) {
    if (!sealCurrentArchive(pError)) {
      return false;
    }
  }

  ++mCurrentArchiveIndex;
  mPositionInArchive = 0;
  mCurrentArchiveFirstFileOffset = 0;
  mCurrentArchiveHasRecoveryStart = false;
  mCurrentArchiveHeaderWritten = false;
  mCurrentArchivePath =
      mOutputDirectory / SnowStormUtils::bundleNameWithRecoveryMarker(static_cast<std::uint64_t>(mCurrentArchiveIndex), false);
  if (!mWriter.truncate(mCurrentArchivePath, 0, pError)) {
    if (pError != nullptr) {
      if (pError->empty()) {
        *pError = "Failed to open output archive file: " + mCurrentArchivePath.string();
      }
    }
    return false;
  }
  const std::uint64_t aInitialFirstOffset = mHasPendingArchiveFirstFileOffset ? mPendingArchiveFirstFileOffset : 0ULL;
  const bool aInitialHasRecoveryStart =
      mHasPendingArchiveFirstFileOffset ? mPendingArchiveHasRecoveryStart : false;
  mHasPendingArchiveFirstFileOffset = false;
  mPendingArchiveHasRecoveryStart = false;
  if (!setCurrentArchiveFirstFileOffset(aInitialFirstOffset, aInitialHasRecoveryStart, pError)) {
    return false;
  }
  return true;
}

bool SnowStormBundlePackerForward::sealCurrentArchive(std::string* pError) {
  if (mCurrentArchiveIndex < 0) {
    return true;
  }
  ++mArchivesSealed;
  mPositionInArchive = 0;
  return true;
}

bool SnowStormBundlePackerForward::writePage(const unsigned char* pPage, std::string* pError) {
  if (pPage == nullptr) {
    if (pError != nullptr) {
      *pError = "writePage requires non-null page";
    }
    return false;
  }
  if (mCurrentArchiveIndex < 0 || mPositionInArchive == mStorageFileSize) {
    if (!openNextArchive(pError)) {
      return false;
    }
  }

  if (!mWriter.append(mCurrentArchivePath, pPage, BLOCK_SIZE_LAYER_3, pError)) {
    if (pError != nullptr) {
      if (pError->empty()) {
        *pError = "Failed while writing encrypted archive page";
      }
    }
    return false;
  }
  mPositionInArchive += BLOCK_SIZE_LAYER_3;
  return true;
}

bool SnowStormBundlePackerForward::setCurrentArchiveFirstFileOffset(std::uint64_t pFirstFileOffset,
                                                                    bool pHasRecoveryStart,
                                                                    std::string* pError) {
  if (mCurrentArchiveIndex < 0 || mPositionInArchive == mStorageFileSize) {
    mPendingArchiveFirstFileOffset = pFirstFileOffset;
    mPendingArchiveHasRecoveryStart = pHasRecoveryStart;
    mHasPendingArchiveFirstFileOffset = true;
    return true;
  }
  if (mCurrentArchiveHeaderWritten &&
      mCurrentArchiveFirstFileOffset == pFirstFileOffset &&
      mCurrentArchiveHasRecoveryStart == pHasRecoveryStart) {
    return true;
  }

  std::array<unsigned char, 16> aHeader{};
  appendU32(aHeader, 0, PLAIN_TEXT_HEADER_MAGIC_SIGNATURE_U32);
  appendU16(aHeader, 4, PLAIN_TEXT_HEADER_VERSION);
  appendU16(aHeader, 6, pHasRecoveryStart ? kPlainHeaderFlagHasRecoveryStart : 0U);
  appendU64(aHeader, 8, pFirstFileOffset);

  if (!mWriter.writeAt(mCurrentArchivePath, 0, aHeader.data(), kPlainHeaderSize, pError)) {
    if (pError != nullptr) {
      if (pError->empty()) {
        *pError = "Failed while writing plain archive header";
      }
    }
    return false;
  }

  mCurrentArchiveFirstFileOffset = pFirstFileOffset;
  mCurrentArchiveHasRecoveryStart = pHasRecoveryStart;
  mCurrentArchiveHeaderWritten = true;

  const fs::path aDesiredPath = mOutputDirectory / SnowStormUtils::bundleNameWithRecoveryMarker(
                                                     static_cast<std::uint64_t>(mCurrentArchiveIndex),
                                                     pHasRecoveryStart);
  if (aDesiredPath != mCurrentArchivePath) {
    if (!mWriter.renameFile(mCurrentArchivePath, aDesiredPath, pError)) {
      if (pError != nullptr && pError->empty()) {
        *pError = "Failed to rename archive to recovery-marked filename";
      }
      return false;
    }
    mCurrentArchivePath = aDesiredPath;
  }
  return true;
}

bool SnowStormBundlePackerForward::finalize(std::string* pError) {
  return sealCurrentArchive(pError);
}

bool SnowStormBundlePackerForward::sealCurrentArchiveWithZeroPages(const unsigned char* pEncryptedZeroPage, std::string* pError) {
  if (mCurrentArchiveIndex < 0) {
    return true;
  }
  while (mPositionInArchive < mStorageFileSize) {
    if (!writePage(pEncryptedZeroPage, pError)) {
      return false;
    }
  }
  return sealCurrentArchive(pError);
}

std::uint64_t SnowStormBundlePackerForward::archiveCount() const {
  return mArchivesSealed;
}

std::uint64_t SnowStormBundlePackerForward::positionInArchive() const {
  return mPositionInArchive;
}

std::int64_t SnowStormBundlePackerForward::currentArchiveIndex() const {
  return mCurrentArchiveIndex;
}

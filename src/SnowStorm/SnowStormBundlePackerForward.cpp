#include "SnowStorm/SnowStormBundlePackerForward.hpp"

#include <string>

#include "Globals.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

SnowStormBundlePackerForward::SnowStormBundlePackerForward(const fs::path& pOutputDirectory,
                                                           std::uint64_t pStorageFileSize)
    : mOutputDirectory(pOutputDirectory),
      mStorageFileSize(pStorageFileSize) {}

SnowStormBundlePackerForward::~SnowStormBundlePackerForward() {
  mArchiveFile.close();
}

bool SnowStormBundlePackerForward::openNextArchive(std::string* pError) {
  if (mCurrentArchiveIndex >= 0) {
    if (!sealCurrentArchive(pError)) {
      return false;
    }
  }

  ++mCurrentArchiveIndex;
  mPositionInArchive = 0;
  const fs::path aArchivePath = mOutputDirectory / SnowStormUtils::bundleName(static_cast<std::uint64_t>(mCurrentArchiveIndex));
  mArchiveFile.close();
  mArchiveFile.clear();
  mArchiveFile.open(aArchivePath, std::ios::binary | std::ios::trunc);
  if (!mArchiveFile) {
    if (pError != nullptr) {
      *pError = "Failed to open output archive file: " + aArchivePath.string();
    }
    return false;
  }
  return true;
}

bool SnowStormBundlePackerForward::sealCurrentArchive(std::string* pError) {
  if (mCurrentArchiveIndex < 0) {
    return true;
  }
  mArchiveFile.flush();
  if (!mArchiveFile) {
    if (pError != nullptr) {
      *pError = "Failed while flushing archive file";
    }
    return false;
  }
  mArchiveFile.close();
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

  mArchiveFile.write(reinterpret_cast<const char*>(pPage), static_cast<std::streamsize>(BLOCK_SIZE_LAYER_3));
  if (!mArchiveFile) {
    if (pError != nullptr) {
      *pError = "Failed while writing encrypted archive page";
    }
    return false;
  }
  mPositionInArchive += BLOCK_SIZE_LAYER_3;
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

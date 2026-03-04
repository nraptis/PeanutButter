#include "SnowStorm/SnowStormBundleWriter.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

SnowStormBundleWriter::SnowStormBundleWriter(SnowStorm& pSnowStorm,
                                             const fs::path& pOutputDirectory,
                                             std::uint64_t pArchiveTotal,
                                             SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod,
                                             std::uint64_t pFilesTotal)
    : mSnowStorm(pSnowStorm),
      mOutputDirectory(pOutputDirectory),
      mArchiveTotal(pArchiveTotal),
      mSnowStormProgressMethod(std::move(pSnowStormProgressMethod)),
      mFilesTotal(pFilesTotal) {}

SnowStormBundleWriter::~SnowStormBundleWriter() {
  try {
    close();
  } catch (...) {
  }
}

void SnowStormBundleWriter::setFilesDone(std::uint64_t pFilesDone) {
  mFilesDone = pFilesDone;
}

void SnowStormBundleWriter::writeBytes(const char* pData, std::uint64_t pSize) {
  std::uint64_t aOffset = 0;
  while (aOffset < pSize) {
    flushBlockIfFull();

    if (mCurrentArchiveIndex < 0 || mPositionInArchive == mSnowStorm.storageFileSize()) {
      openNextArchive();
    }

    const std::uint64_t aBlockRemaining = mSnowStorm.blockSize() - mPositionInBlock;
    const std::uint64_t aArchiveRemaining = mSnowStorm.storageFileSize() - mPositionInArchive;
    const std::uint64_t aChunk = std::min<std::uint64_t>(pSize - aOffset,
                                                          std::min<std::uint64_t>(aBlockRemaining, aArchiveRemaining));

    std::memcpy(const_cast<unsigned char*>(mSnowStorm.mWriteBuffer) + static_cast<std::size_t>(mPositionInBlock),
                pData + static_cast<std::ptrdiff_t>(aOffset),
                static_cast<std::size_t>(aChunk));

    mPositionInBlock += aChunk;
    mSnowStorm.mWriteBufferIndex = static_cast<unsigned int>(mPositionInBlock);
    aOffset += aChunk;
    flushBlockIfFull();
  }
}

void SnowStormBundleWriter::writeBytes(const std::vector<char>& pData) {
  if (!pData.empty()) {
    writeBytes(pData.data(), static_cast<std::uint64_t>(pData.size()));
  }
}

void SnowStormBundleWriter::close() {
  if (mClosed) {
    return;
  }
  mClosed = true;

  try {
    sealCurrentArchive();
  } catch (...) {
    mArchiveFile.close();
    throw;
  }

  mArchiveFile.close();
}

std::uint64_t SnowStormBundleWriter::archiveCount() const {
  return mArchivesSealed;
}

void SnowStormBundleWriter::flushBlockIfFull() {
  if (mPositionInBlock == mSnowStorm.blockSize()) {
    writeBlock();
  }
}

void SnowStormBundleWriter::writeBlock() {
  std::string aError;
  if (!mSnowStorm.mCrypt->encrypt(mSnowStorm.mWriteBuffer,
                                  const_cast<unsigned char*>(mSnowStorm.mCryptBuffer),
                                  mSnowStorm.blockSize(),
                                  &aError)) {
    throw std::runtime_error("Encrypt block failed: " + aError);
  }

  mArchiveFile.write(reinterpret_cast<const char*>(mSnowStorm.mCryptBuffer),
                     static_cast<std::streamsize>(mSnowStorm.blockSize()));
  if (!mArchiveFile) {
    throw std::runtime_error("Failed while writing encrypted archive block");
  }

  mPositionInArchive += mSnowStorm.blockSize();
  mPositionInBlock = 0;
  mSnowStorm.mWriteBufferIndex = 0;
  mSnowStorm.mCryptBufferIndex = static_cast<unsigned int>(mSnowStorm.blockSize());
}

void SnowStormBundleWriter::openNextArchive() {
  if (mCurrentArchiveIndex >= 0) {
    sealCurrentArchive();
  }

  ++mCurrentArchiveIndex;
  mPositionInArchive = 0;
  mPositionInBlock = 0;

  const fs::path aArchivePath = mOutputDirectory / SnowStormUtils::bundleName(static_cast<std::uint64_t>(mCurrentArchiveIndex));
  mArchiveFile.close();
  mArchiveFile.clear();
  mArchiveFile.open(aArchivePath, std::ios::binary | std::ios::trunc);
  if (!mArchiveFile) {
    throw std::runtime_error("Failed to open output archive file: " + aArchivePath.string());
  }
}

void SnowStormBundleWriter::sealCurrentArchive() {
  if (mCurrentArchiveIndex < 0) {
    return;
  }

  if (mPositionInBlock > 0) {
    std::memset(const_cast<unsigned char*>(mSnowStorm.mWriteBuffer) + static_cast<std::size_t>(mPositionInBlock),
                0,
                static_cast<std::size_t>(mSnowStorm.blockSize() - mPositionInBlock));
    writeBlock();
  }

  std::memset(const_cast<unsigned char*>(mSnowStorm.mWriteBuffer),
              0,
              static_cast<std::size_t>(mSnowStorm.blockSize()));

  while (mPositionInArchive < mSnowStorm.storageFileSize()) {
    writeBlock();
  }

  mArchiveFile.flush();
  if (!mArchiveFile) {
    throw std::runtime_error("Failed while flushing archive file");
  }
  mArchiveFile.close();

  ++mArchivesSealed;
  if (mSnowStormProgressMethod) {
    mSnowStormProgressMethod(mArchivesSealed, mArchiveTotal, mFilesDone, mFilesTotal);
  }

  mPositionInArchive = 0;
  mPositionInBlock = 0;
}

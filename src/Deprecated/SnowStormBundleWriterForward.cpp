#include "SnowStorm/SnowStormBundleWriterForward.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

#include "Globals.hpp"

SnowStormBundleWriterForward::SnowStormBundleWriterForward(SnowStorm& pSnowStorm,
                                                           const std::filesystem::path& pOutputDirectory,
                                                           std::uint64_t pArchiveTotal,
                                                           SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod,
                                                           std::uint64_t pFilesTotal)
    : mSnowStorm(pSnowStorm),
      mPacker(pOutputDirectory, pSnowStorm.storageFileSize()),
      mArchiveTotal(pArchiveTotal),
      mSnowStormProgressMethod(std::move(pSnowStormProgressMethod)),
      mFilesTotal(pFilesTotal),
      mLayer1Input(static_cast<std::size_t>(gBlockSizeLayer1), 0) {}

SnowStormBundleWriterForward::~SnowStormBundleWriterForward() {
  try {
    close();
  } catch (...) {
  }
}

void SnowStormBundleWriterForward::setFilesDone(std::uint64_t pFilesDone) {
  mFilesDone = pFilesDone;
}

void SnowStormBundleWriterForward::flushLayer1Input() {
  std::string aError;
  if (!mSealer.pushLayer1PlainPage(mLayer1Input.data(), &aError)) {
    throw std::runtime_error("Sealer layer1 push failed: " + aError);
  }
  mPositionInLayer1 = 0;
}

void SnowStormBundleWriterForward::emitReadyPages() {
  while (mSealer.hasReadyPage()) {
    const unsigned char* aPage = mSealer.popReadyPage();
    std::string aError;
    if (!mPacker.writePage(aPage, &aError)) {
      throw std::runtime_error("Packer writePage failed: " + aError);
    }
  }
}

void SnowStormBundleWriterForward::writeBytes(const char* pData, std::uint64_t pSize) {
  std::uint64_t aOffset = 0;
  while (aOffset < pSize) {
    const std::uint64_t aRemaining = gBlockSizeLayer1 - mPositionInLayer1;
    const std::uint64_t aChunk = std::min<std::uint64_t>(aRemaining, pSize - aOffset);
    std::memcpy(mLayer1Input.data() + static_cast<std::size_t>(mPositionInLayer1),
                pData + static_cast<std::ptrdiff_t>(aOffset),
                static_cast<std::size_t>(aChunk));
    mPositionInLayer1 += aChunk;
    aOffset += aChunk;

    if (mPositionInLayer1 == gBlockSizeLayer1) {
      flushLayer1Input();
      emitReadyPages();
    }
  }
}

void SnowStormBundleWriterForward::writeBytes(const std::vector<char>& pData) {
  if (!pData.empty()) {
    writeBytes(pData.data(), static_cast<std::uint64_t>(pData.size()));
  }
}

void SnowStormBundleWriterForward::close() {
  if (mClosed) {
    return;
  }
  mClosed = true;

  if (mPositionInLayer1 > 0) {
    std::memset(mLayer1Input.data() + static_cast<std::size_t>(mPositionInLayer1),
                0,
                static_cast<std::size_t>(gBlockSizeLayer1 - mPositionInLayer1));
    flushLayer1Input();
    emitReadyPages();
  }

  std::string aError;
  if (mSealer.flushPending(&aError)) {
    emitReadyPages();
  } else if (!aError.empty()) {
    throw std::runtime_error("Sealer flushPending failed: " + aError);
  }

  std::vector<unsigned char> aEncryptedZeroPage;
  if (!mSealer.buildEncryptedZeroPage(aEncryptedZeroPage, &aError)) {
    throw std::runtime_error("Sealer buildEncryptedZeroPage failed: " + aError);
  }
  if (!mPacker.sealCurrentArchiveWithZeroPages(aEncryptedZeroPage.data(), &aError)) {
    throw std::runtime_error("Packer sealCurrentArchiveWithZeroPages failed: " + aError);
  }

  if (mSnowStormProgressMethod && mPacker.archiveCount() > 0) {
    mSnowStormProgressMethod(mPacker.archiveCount(), mArchiveTotal, mFilesDone, mFilesTotal);
  }
}

std::uint64_t SnowStormBundleWriterForward::archiveCount() const {
  return mPacker.archiveCount();
}

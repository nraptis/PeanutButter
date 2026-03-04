#include "SnowStorm/SnowStormBundleWriterBackward.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "Globals.hpp"
#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

SnowStormBundleWriterBackward::SnowStormBundleWriterBackward(SnowStorm& pSnowStorm,
                                                             const fs::path& pInputDirectory,
                                                             SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod)
    : mSnowStorm(pSnowStorm),
      mPacker(pInputDirectory, std::move(pSnowStormProgressMethod)),
      mEncryptedPage(static_cast<std::size_t>(gBlockSizeLayer3), 0),
      mPlainPage(static_cast<std::size_t>(gBlockSizeLayer3), 0) {}

SnowStormBundleWriterBackward::~SnowStormBundleWriterBackward() {
  try {
    close();
  } catch (...) {
  }
}

bool SnowStormBundleWriterBackward::loadNextPlainPage() {
  std::uint64_t aBytesRead = 0;
  std::string aError;
  if (!mPacker.readPage(mEncryptedPage.data(), &aBytesRead, &aError)) {
    if (!aError.empty()) {
      throw std::runtime_error(aError);
    }
    mPageOffset = 0;
    mPageValidBytes = 0;
    return false;
  }

  if (!mSealer.decryptLayer3Page(mEncryptedPage.data(), mPlainPage.data(), &aError)) {
    throw std::runtime_error("Decrypt block failed: " + aError);
  }

  mPageOffset = 0;
  mPageValidBytes = aBytesRead;
  return true;
}

std::vector<char> SnowStormBundleWriterBackward::readExact(std::uint64_t pSize) {
  std::vector<char> aOutput;
  aOutput.reserve(static_cast<std::size_t>(pSize));

  std::uint64_t aRemaining = pSize;
  while (aRemaining > 0) {
    if (mPageOffset >= mPageValidBytes) {
      if (!loadNextPlainPage()) {
        throw std::runtime_error("Reached end of bundle set");
      }
    }

    const std::uint64_t aAvailable = mPageValidBytes - mPageOffset;
    const std::uint64_t aTake = std::min<std::uint64_t>(aRemaining, aAvailable);
    const char* aBegin = reinterpret_cast<const char*>(mPlainPage.data() + mPageOffset);
    aOutput.insert(aOutput.end(), aBegin, aBegin + static_cast<std::ptrdiff_t>(aTake));

    mPageOffset += aTake;
    aRemaining -= aTake;
  }

  return aOutput;
}

void SnowStormBundleWriterBackward::setFilesDone(std::uint64_t pFilesDone) {
  mPacker.setFilesDone(pFilesDone);
}

std::uint64_t SnowStormBundleWriterBackward::archiveTotal() const {
  return mPacker.archiveTotal();
}

std::uint64_t SnowStormBundleWriterBackward::archivesTouched() const {
  return mPacker.archivesTouched();
}

void SnowStormBundleWriterBackward::close() {
  if (mClosed) {
    return;
  }
  mClosed = true;
  mPacker.close();
}

std::string readEntryName(SnowStormBundleWriterBackward& pReader) {
  const std::vector<char> aLengthRaw = pReader.readExact(2);
  const std::uint16_t aLength = SnowStormUtils::readUInt16(aLengthRaw);
  const std::vector<char> aNameRaw = pReader.readExact(aLength);
  const std::string aName(aNameRaw.begin(), aNameRaw.end());

  if (aName.empty() || aName == "." || aName == ".." ||
      aName.find('/') != std::string::npos || aName.find('\\') != std::string::npos) {
    throw std::runtime_error(
        "Illegal entry name (len=" + std::to_string(aName.size()) +
        ", preview=\"" + SnowStormUtils::sanitizeNamePreview(aName) + "\")");
  }

  return aName;
}

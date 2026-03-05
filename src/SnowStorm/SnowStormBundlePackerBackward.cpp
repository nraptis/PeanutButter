#include "SnowStorm/SnowStormBundlePackerBackward.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "Globals.hpp"
#include "IO/FileReaderDelegate.hpp"

namespace fs = std::filesystem;

namespace {
constexpr std::uint32_t kPlainHeaderMagic = PLAIN_TEXT_HEADER_MAGIC_SIGNATURE_U32;
constexpr std::uint64_t kPlainHeaderSize = PLAIN_TEXT_HEADER_LENGTH;

struct CandidateInfo {
  std::string mPrefix;
  int mWidth = 0;
  std::uint64_t mIndex = 0;
  fs::path mPath;
};

std::uint16_t readU16(const unsigned char* pIn, std::size_t pOffset) {
  return static_cast<std::uint16_t>(pIn[pOffset + 0]) |
         static_cast<std::uint16_t>(pIn[pOffset + 1] << 8);
}

std::uint32_t readU32(const unsigned char* pIn, std::size_t pOffset) {
  return static_cast<std::uint32_t>(pIn[pOffset + 0]) |
         static_cast<std::uint32_t>(pIn[pOffset + 1] << 8) |
         static_cast<std::uint32_t>(pIn[pOffset + 2] << 16) |
         static_cast<std::uint32_t>(pIn[pOffset + 3] << 24);
}

std::uint64_t readU64(const unsigned char* pIn, std::size_t pOffset) {
  std::uint64_t aValue = 0;
  for (int aShift = 0; aShift < 8; ++aShift) {
    aValue |= static_cast<std::uint64_t>(pIn[pOffset + static_cast<std::size_t>(aShift)]) << (8 * aShift);
  }
  return aValue;
}

bool parseCandidate(const fs::path& pPath, CandidateInfo& pOut) {
  const std::string aName = pPath.filename().string();
  if (aName.size() <= gBundleFilePrefix.size() + gBundleFileSuffix.size()) {
    return false;
  }
  if (aName.rfind(gBundleFilePrefix, 0) != 0) {
    return false;
  }
  if (aName.substr(aName.size() - gBundleFileSuffix.size()) != gBundleFileSuffix) {
    return false;
  }
  std::string aDigits = aName.substr(gBundleFilePrefix.size(),
                                     aName.size() - gBundleFilePrefix.size() - gBundleFileSuffix.size());
  if (!aDigits.empty() && (aDigits.back() == 'R' || aDigits.back() == 'N')) {
    aDigits.pop_back();
  }
  if (aDigits.empty()) {
    return false;
  }
  for (char aChar : aDigits) {
    if (std::isdigit(static_cast<unsigned char>(aChar)) == 0) {
      return false;
    }
  }
  pOut.mPrefix = gBundleFilePrefix;
  pOut.mWidth = static_cast<int>(aDigits.size());
  pOut.mIndex = std::stoull(aDigits);
  pOut.mPath = pPath;
  return true;
}

}  // namespace

SnowStormBundlePackerBackward::SnowStormBundlePackerBackward(const fs::path& pInputDirectory,
                                                             SnowStormProgressMethod pSnowStormProgressMethod,
                                                             FileReaderDelegate& pReader,
                                                             std::uint64_t pStartArchiveIndex)
    : mReader(pReader),
      mSnowStormProgressMethod(std::move(pSnowStormProgressMethod)),
      mStartArchiveIndex(pStartArchiveIndex) {
  mBundlePaths = discoverBundlePaths(pInputDirectory);
  std::string aError;
  if (!loadBundle(0, &aError)) {
    throw std::runtime_error(aError);
  }
}

SnowStormBundlePackerBackward::~SnowStormBundlePackerBackward() {
  close();
}

std::vector<fs::path> SnowStormBundlePackerBackward::discoverBundlePaths(const fs::path& pInputDirectory) {
  std::map<std::uint64_t, fs::path> aByIndex;
  std::vector<fs::path> aEntries;
  std::string aListError;
  if (!mReader.listDirectory(pInputDirectory, aEntries, &aListError)) {
    throw std::runtime_error(aListError.empty() ? "Failed to list archive directory" : aListError);
  }
  for (const auto& aPath : aEntries) {
    CandidateInfo aCandidate;
    if (!parseCandidate(aPath, aCandidate)) {
      continue;
    }
    if (!mReader.isRegularFile(aPath)) {
      continue;
    }
    aByIndex[aCandidate.mIndex] = aCandidate.mPath;
  }
  if (aByIndex.empty()) {
    throw std::runtime_error("No archive files found");
  }
  if (aByIndex.count(mStartArchiveIndex) == 0) {
    throw std::runtime_error("Recovery start archive index not found");
  }
  std::vector<fs::path> aOutput;
  for (std::uint64_t aIndex = mStartArchiveIndex;; ++aIndex) {
    auto aFound = aByIndex.find(aIndex);
    if (aFound == aByIndex.end()) {
      break;
    }
    aOutput.push_back(aFound->second);
  }
  if (aOutput.empty()) {
    throw std::runtime_error("No archive files found");
  }
  return aOutput;
}

bool SnowStormBundlePackerBackward::loadBundle(std::size_t pIndex, std::string* pError) {
  mCurrentFileOffset = 0;
  if (!mReader.fileSize(mBundlePaths[pIndex], &mCurrentFileSize, pError)) {
    if (pError != nullptr) {
      if (pError->empty()) {
        *pError = "Failed to open archive file: " + mBundlePaths[pIndex].string();
      }
    }
    return false;
  }

  mCurrentArchiveFirstFileOffset = 0;
  mCurrentArchiveHasPlainHeader = false;
  std::vector<unsigned char> aHeader;
  std::string aHeaderError;
  if (mReader.readRange(mBundlePaths[pIndex], 0, kPlainHeaderSize, aHeader, &aHeaderError) &&
      aHeader.size() == static_cast<std::size_t>(kPlainHeaderSize) &&
      readU32(aHeader.data(), 0) == kPlainHeaderMagic &&
      readU16(aHeader.data(), 4) == 1U) {
    mCurrentArchiveFirstFileOffset = readU64(aHeader.data(), 8);
    mCurrentArchiveHasPlainHeader = true;
    mCurrentFileOffset = kPlainHeaderSize;
  }

  mBundleIndex = pIndex;
  mArchivesTouched = std::max<std::uint64_t>(mArchivesTouched, pIndex + 1);
  if (mSnowStormProgressMethod) {
    mSnowStormProgressMethod(pIndex + 1, mBundlePaths.size(), mFilesDone, 0);
  }
  return true;
}

bool SnowStormBundlePackerBackward::readPage(unsigned char* pDestination,
                                             std::uint64_t* pBytesRead,
                                             std::string* pError) {
  if (pDestination == nullptr || pBytesRead == nullptr) {
    if (pError != nullptr) {
      *pError = "readPage requires non-null pointers";
    }
    return false;
  }

  while (true) {
    if (mCurrentFileOffset < mCurrentFileSize) {
      const std::uint64_t aRemaining = mCurrentFileSize - mCurrentFileOffset;
      const std::uint64_t aWant = std::min<std::uint64_t>(BLOCK_SIZE_LAYER_3, aRemaining);
      std::vector<unsigned char> aBytes;
      if (!mReader.readRange(mBundlePaths[mBundleIndex], mCurrentFileOffset, aWant, aBytes, pError)) {
        return false;
      }
      const std::uint64_t aGot = static_cast<std::uint64_t>(aBytes.size());
      if (aGot == 0) {
        mCurrentFileOffset = mCurrentFileSize;
        continue;
      }
      if (aGot > 0) {
        std::memcpy(pDestination, aBytes.data(), static_cast<std::size_t>(aGot));
      }
      if (aGot < BLOCK_SIZE_LAYER_3) {
        std::memset(pDestination + static_cast<std::size_t>(aGot),
                    0,
                    static_cast<std::size_t>(BLOCK_SIZE_LAYER_3 - aGot));
      }
      mCurrentFileOffset += aGot;
      *pBytesRead = aGot;
      return true;
    }

    if (mBundleIndex + 1 >= mBundlePaths.size()) {
      *pBytesRead = 0;
      return false;
    }
    if (!loadBundle(mBundleIndex + 1, pError)) {
      return false;
    }
  }
}

void SnowStormBundlePackerBackward::setFilesDone(std::uint64_t pFilesDone) {
  mFilesDone = pFilesDone;
}

std::uint64_t SnowStormBundlePackerBackward::currentArchiveFirstFileOffset() const {
  return mCurrentArchiveFirstFileOffset;
}

bool SnowStormBundlePackerBackward::currentArchiveHasPlainHeader() const {
  return mCurrentArchiveHasPlainHeader;
}

std::uint64_t SnowStormBundlePackerBackward::archiveTotal() const {
  return mBundlePaths.size();
}

std::uint64_t SnowStormBundlePackerBackward::archivesTouched() const {
  return mArchivesTouched;
}

void SnowStormBundlePackerBackward::close() {
  if (mClosed) {
    return;
  }
  mClosed = true;
}

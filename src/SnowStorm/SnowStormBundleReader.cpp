#include "SnowStorm/SnowStormBundleReader.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

namespace {

struct CandidateInfo {
  std::string mPrefix;
  int mWidth = 0;
  std::uint64_t mIndex = 0;
  fs::path mPath;
};

bool parseCandidate(const fs::path& pPath, CandidateInfo& pOut) {
  if (!fs::is_regular_file(pPath)) {
    return false;
  }
  if (pPath.extension() != ".jag") {
    return false;
  }

  const std::string aStem = pPath.stem().string();
  std::size_t aScan = aStem.size();
  while (aScan > 0 && std::isdigit(static_cast<unsigned char>(aStem[aScan - 1])) != 0) {
    --aScan;
  }
  if (aScan == aStem.size()) {
    return false;
  }

  pOut.mPrefix = aStem.substr(0, aScan);
  const std::string aDigits = aStem.substr(aScan);
  pOut.mWidth = static_cast<int>(aDigits.size());
  pOut.mIndex = std::stoull(aDigits);
  pOut.mPath = pPath;
  return true;
}

std::vector<fs::path> discoverBundlePaths(const fs::path& pInputDirectory) {
  std::map<std::pair<std::string, int>, std::map<std::uint64_t, fs::path>> aGrouped;
  for (const auto& aEntry : fs::directory_iterator(pInputDirectory)) {
    CandidateInfo aCandidate;
    if (!parseCandidate(aEntry.path(), aCandidate)) {
      continue;
    }
    aGrouped[{aCandidate.mPrefix, aCandidate.mWidth}][aCandidate.mIndex] = aCandidate.mPath;
  }

  std::vector<std::pair<std::string, int>> aValidGroups;
  for (const auto& aPair : aGrouped) {
    if (aPair.second.count(0) != 0) {
      aValidGroups.push_back(aPair.first);
    }
  }

  if (aValidGroups.empty()) {
    throw std::runtime_error("Could not infer archive naming in input folder");
  }
  if (aValidGroups.size() > 1) {
    throw std::runtime_error("Ambiguous archive naming groups in input folder");
  }

  const auto& aGroup = aGrouped[aValidGroups.front()];
  std::vector<fs::path> aOutput;
  for (std::uint64_t aIndex = 0;; ++aIndex) {
    auto aFound = aGroup.find(aIndex);
    if (aFound == aGroup.end()) {
      break;
    }
    aOutput.push_back(aFound->second);
  }

  if (aOutput.empty()) {
    throw std::runtime_error("No archive files found");
  }

  return aOutput;
}

}  // namespace

SnowStormBundleReader::SnowStormBundleReader(SnowStorm& pSnowStorm,
                                             const fs::path& pInputDirectory,
                                             SnowStorm::SnowStormProgressMethod pSnowStormProgressMethod)
    : mSnowStorm(pSnowStorm),
      mSnowStormProgressMethod(std::move(pSnowStormProgressMethod)) {
  mBundlePaths = discoverBundlePaths(pInputDirectory);
  const std::uint64_t aBaseSize = fs::file_size(mBundlePaths.front());
  if (aBaseSize == 0) {
    throw std::runtime_error("Archive 0000 has invalid size");
  }
  for (const auto& aPath : mBundlePaths) {
    if (fs::file_size(aPath) < aBaseSize) {
      throw std::runtime_error("Archive " + aPath.filename().string() + " is smaller than archive 0000");
    }
  }
  loadBundle(0);
}

SnowStormBundleReader::~SnowStormBundleReader() {
  try {
    close();
  } catch (...) {
  }
}

std::vector<char> SnowStormBundleReader::readExact(std::uint64_t pSize) {
  std::vector<char> aOutput;
  aOutput.reserve(static_cast<std::size_t>(pSize));

  std::uint64_t aRemaining = pSize;
  while (aRemaining > 0) {
    if (mBlockOffset >= mBlockValidBytes) {
      while (!loadNextBlock()) {
        if (mBundleIndex + 1 >= mBundlePaths.size()) {
          throw std::runtime_error("Reached end of bundle set");
        }
        loadBundle(mBundleIndex + 1);
      }
    }

    const std::uint64_t aAvailable = mBlockValidBytes - mBlockOffset;
    const std::uint64_t aTake = std::min<std::uint64_t>(aRemaining, aAvailable);

    const char* aBegin = reinterpret_cast<const char*>(mSnowStorm.mWriteBuffer + mBlockOffset);
    aOutput.insert(aOutput.end(), aBegin, aBegin + static_cast<std::ptrdiff_t>(aTake));

    mBlockOffset += aTake;
    aRemaining -= aTake;
  }

  return aOutput;
}

void SnowStormBundleReader::setFilesDone(std::uint64_t pFilesDone) {
  mFilesDone = pFilesDone;
}

std::uint64_t SnowStormBundleReader::archiveTotal() const {
  return mBundlePaths.size();
}

std::uint64_t SnowStormBundleReader::archivesTouched() const {
  return mArchivesTouched;
}

void SnowStormBundleReader::close() {
  if (mClosed) {
    return;
  }
  mClosed = true;
  mInputFile.close();
}

void SnowStormBundleReader::loadBundle(std::size_t pIndex) {
  mInputFile.close();
  mInputFile.clear();
  mInputFile.open(mBundlePaths[pIndex], std::ios::binary);
  if (!mInputFile) {
    throw std::runtime_error("Failed to open archive file: " + mBundlePaths[pIndex].string());
  }

  mBundleIndex = pIndex;
  mBlockOffset = 0;
  mBlockValidBytes = 0;
  mArchivesTouched = std::max<std::uint64_t>(mArchivesTouched, pIndex + 1);

  if (mSnowStormProgressMethod) {
    mSnowStormProgressMethod(pIndex + 1, mBundlePaths.size(), mFilesDone, 0);
  }
}

bool SnowStormBundleReader::loadNextBlock() {
  mInputFile.read(reinterpret_cast<char*>(const_cast<unsigned char*>(mSnowStorm.mCryptBuffer)),
                  static_cast<std::streamsize>(mSnowStorm.blockSize()));
  const std::streamsize aBytesRead = mInputFile.gcount();
  if (aBytesRead <= 0) {
    mBlockOffset = 0;
    mBlockValidBytes = 0;
    return false;
  }

  if (static_cast<std::uint64_t>(aBytesRead) < mSnowStorm.blockSize()) {
    std::memset(const_cast<unsigned char*>(mSnowStorm.mCryptBuffer) + static_cast<std::size_t>(aBytesRead),
                0,
                static_cast<std::size_t>(mSnowStorm.blockSize() - static_cast<std::uint64_t>(aBytesRead)));
  }

  std::string aError;
  if (!mSnowStorm.mCrypt->decrypt(mSnowStorm.mCryptBuffer,
                                  const_cast<unsigned char*>(mSnowStorm.mWriteBuffer),
                                  mSnowStorm.blockSize(),
                                  &aError)) {
    throw std::runtime_error("Decrypt block failed: " + aError);
  }

  mBlockOffset = 0;
  mBlockValidBytes = static_cast<std::uint64_t>(aBytesRead);
  mSnowStorm.mCryptBufferIndex = static_cast<unsigned int>(aBytesRead);
  mSnowStorm.mWriteBufferIndex = static_cast<unsigned int>(mBlockValidBytes);
  return true;
}

std::string readEntryName(SnowStormBundleReader& pReader) {
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

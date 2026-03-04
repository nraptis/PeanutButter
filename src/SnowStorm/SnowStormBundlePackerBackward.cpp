#include "SnowStorm/SnowStormBundlePackerBackward.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "Globals.hpp"

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

}  // namespace

SnowStormBundlePackerBackward::SnowStormBundlePackerBackward(const fs::path& pInputDirectory,
                                                             SnowStormProgressMethod pSnowStormProgressMethod)
    : mSnowStormProgressMethod(std::move(pSnowStormProgressMethod)) {
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

bool SnowStormBundlePackerBackward::loadBundle(std::size_t pIndex, std::string* pError) {
  mInputFile.close();
  mInputFile.clear();
  mInputFile.open(mBundlePaths[pIndex], std::ios::binary);
  if (!mInputFile) {
    if (pError != nullptr) {
      *pError = "Failed to open archive file: " + mBundlePaths[pIndex].string();
    }
    return false;
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
    mInputFile.read(reinterpret_cast<char*>(pDestination), static_cast<std::streamsize>(BLOCK_SIZE_LAYER_3));
    const std::streamsize aBytes = mInputFile.gcount();
    if (aBytes > 0) {
      if (static_cast<std::uint64_t>(aBytes) < BLOCK_SIZE_LAYER_3) {
        std::memset(pDestination + static_cast<std::size_t>(aBytes),
                    0,
                    static_cast<std::size_t>(BLOCK_SIZE_LAYER_3 - static_cast<std::uint64_t>(aBytes)));
      }
      *pBytesRead = static_cast<std::uint64_t>(aBytes);
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
  mInputFile.close();
}

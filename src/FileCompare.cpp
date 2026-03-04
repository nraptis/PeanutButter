#include "FileCompare.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::map<std::string, fs::path> collectFiles(const fs::path& pRoot) {
  std::map<std::string, fs::path> aFiles;
  for (const auto& aEntry : fs::recursive_directory_iterator(pRoot)) {
    if (!aEntry.is_regular_file()) {
      continue;
    }
    const fs::path aRel = fs::relative(aEntry.path(), pRoot);
    aFiles[aRel.generic_string()] = aEntry.path();
  }
  return aFiles;
}

bool filesEqual(const fs::path& pA, const fs::path& pB) {
  std::error_code aErrorCodeA;
  std::error_code aErrorCodeB;
  const std::uintmax_t aSizeA = fs::file_size(pA, aErrorCodeA);
  const std::uintmax_t aSizeB = fs::file_size(pB, aErrorCodeB);
  if (aErrorCodeA || aErrorCodeB || aSizeA != aSizeB) {
    return false;
  }

  std::ifstream aInA(pA, std::ios::binary);
  std::ifstream aInB(pB, std::ios::binary);
  if (!aInA || !aInB) {
    return false;
  }

  std::vector<char> aBufferA(64 * 1024);
  std::vector<char> aBufferB(64 * 1024);
  while (aInA && aInB) {
    aInA.read(aBufferA.data(), static_cast<std::streamsize>(aBufferA.size()));
    aInB.read(aBufferB.data(), static_cast<std::streamsize>(aBufferB.size()));
    const std::streamsize aReadA = aInA.gcount();
    const std::streamsize aReadB = aInB.gcount();
    if (aReadA != aReadB) {
      return false;
    }
    if (aReadA <= 0) {
      break;
    }
    if (!std::equal(aBufferA.begin(), aBufferA.begin() + aReadA, aBufferB.begin())) {
      return false;
    }
  }

  return true;
}

}  // namespace

namespace FileCompare {

bool Test(const LogStream& pLogStream,
          const fs::path& pSource,
          const fs::path& pDestination,
          std::string* pError) {
  if (!fs::exists(pSource) || !fs::is_directory(pSource)) {
    if (pError != nullptr) {
      *pError = "ERROR: source is not a directory: " + pSource.string();
    }
    return false;
  }
  if (!fs::exists(pDestination) || !fs::is_directory(pDestination)) {
    if (pError != nullptr) {
      *pError = "ERROR: destination is not a directory: " + pDestination.string();
    }
    return false;
  }

  const auto aLog = [&](const std::string& pLine) {
    if (pLogStream) {
      pLogStream(pLine);
    }
  };

  const std::map<std::string, fs::path> aSourceFiles = collectFiles(pSource);
  const std::map<std::string, fs::path> aDestinationFiles = collectFiles(pDestination);

  aLog("Source \"" + pSource.string() + "\" has " + std::to_string(aSourceFiles.size()) + " files");
  aLog("Destination \"" + pDestination.string() + "\" has " + std::to_string(aDestinationFiles.size()) + " files");

  std::set<std::string> aSourceSet;
  std::set<std::string> aDestinationSet;
  for (const auto& [aRel, _] : aSourceFiles) {
    (void)_;
    aSourceSet.insert(aRel);
  }
  for (const auto& [aRel, _] : aDestinationFiles) {
    (void)_;
    aDestinationSet.insert(aRel);
  }

  std::vector<std::string> aOnlyInSource;
  std::vector<std::string> aOnlyInDestination;
  std::vector<std::string> aCommon;
  std::set_difference(
      aSourceSet.begin(), aSourceSet.end(),
      aDestinationSet.begin(), aDestinationSet.end(),
      std::back_inserter(aOnlyInSource));
  std::set_difference(
      aDestinationSet.begin(), aDestinationSet.end(),
      aSourceSet.begin(), aSourceSet.end(),
      std::back_inserter(aOnlyInDestination));
  std::set_intersection(
      aSourceSet.begin(), aSourceSet.end(),
      aDestinationSet.begin(), aDestinationSet.end(),
      std::back_inserter(aCommon));

  std::size_t aDataMismatches = 0;
  for (const std::string& aRel : aCommon) {
    if (!filesEqual(aSourceFiles.at(aRel), aDestinationFiles.at(aRel))) {
      aLog("DATA_MISMATCH: " + aRel);
      ++aDataMismatches;
    }
  }
  for (const std::string& aRel : aOnlyInSource) {
    aLog("ONLY_IN_SOURCE: " + aRel);
  }
  for (const std::string& aRel : aOnlyInDestination) {
    aLog("ONLY_IN_DEST: " + aRel);
  }

  const std::size_t aExistenceMismatches = aOnlyInSource.size() + aOnlyInDestination.size();
  if (aDataMismatches == 0 && aExistenceMismatches == 0) {
    aLog("COMPARE_OK: " + std::to_string(aCommon.size()) + " files match exactly");
    return true;
  }

  const std::string aSummary =
      "COMPARE_FAIL: data_mismatches=" + std::to_string(aDataMismatches) +
      " existence_mismatches=" + std::to_string(aExistenceMismatches) +
      " matches=" + std::to_string(aCommon.size() - aDataMismatches);
  aLog(aSummary);
  if (pError != nullptr) {
    *pError = aSummary;
  }
  return false;
}

}  // namespace FileCompare

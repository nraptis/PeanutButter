#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ShouldBundleResult.hpp"
#include "Globals.hpp"
#include "SnowStorm/SnowStormEngine.hpp"

namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t kTestArchiveSize = BLOCK_SIZE_LAYER_3 * 10ULL;

struct TempDir {
  fs::path mPath;

  TempDir() {
    const fs::path aBase = fs::temp_directory_path();
    for (int aAttempt = 0; aAttempt < 128; ++aAttempt) {
      const fs::path aCandidate = aBase / ("pb_test_" + std::to_string(std::rand()) + "_" + std::to_string(aAttempt));
      std::error_code aErrorCode;
      if (fs::create_directories(aCandidate, aErrorCode)) {
        mPath = aCandidate;
        return;
      }
    }
    throw std::runtime_error("Failed to create temporary test directory");
  }

  ~TempDir() {
    if (!mPath.empty()) {
      std::error_code aErrorCode;
      fs::remove_all(mPath, aErrorCode);
    }
  }
};

void require(bool pCondition, const std::string& pMessage) {
  if (!pCondition) {
    throw std::runtime_error(pMessage);
  }
}

void writeFile(const fs::path& pPath, const std::string& pContents) {
  fs::create_directories(pPath.parent_path());
  std::ofstream aOut(pPath, std::ios::binary | std::ios::trunc);
  if (!aOut) {
    throw std::runtime_error("Failed to create file: " + pPath.string());
  }
  aOut.write(pContents.data(), static_cast<std::streamsize>(pContents.size()));
}

std::string readFile(const fs::path& pPath) {
  std::ifstream aIn(pPath, std::ios::binary);
  if (!aIn) {
    throw std::runtime_error("Failed to read file: " + pPath.string());
  }
  return std::string((std::istreambuf_iterator<char>(aIn)), std::istreambuf_iterator<char>());
}

void testShouldBundleMissingSourceReturnsNo() {
  TempDir aTemp;
  SnowStormEngine aEngine(kTestArchiveSize);
  const ShouldBundleResult aResult = aEngine.shouldBundle(aTemp.mPath / "missing", aTemp.mPath / "archive");
  require(aResult.mDecision == ShouldBundleDecision::No, "Expected shouldBundle missing source => No");
}

void testShouldBundleDestinationIsFileReturnsNo() {
  TempDir aTemp;
  const fs::path aSource = aTemp.mPath / "src";
  fs::create_directories(aSource);
  writeFile(aSource / "a.txt", "abc");
  const fs::path aDestinationFile = aTemp.mPath / "archive_file";
  writeFile(aDestinationFile, "not a directory");

  SnowStormEngine aEngine(kTestArchiveSize);
  const ShouldBundleResult aResult = aEngine.shouldBundle(aSource, aDestinationFile);
  require(aResult.mDecision == ShouldBundleDecision::No, "Expected shouldBundle destination file => No");
}

void testShouldBundleNonEmptyDestinationPrompts() {
  TempDir aTemp;
  const fs::path aSource = aTemp.mPath / "src";
  const fs::path aDestination = aTemp.mPath / "archive";
  fs::create_directories(aSource);
  fs::create_directories(aDestination);
  writeFile(aSource / "a.txt", "abc");
  writeFile(aDestination / "old.txt", "old");

  SnowStormEngine aEngine(kTestArchiveSize);
  const ShouldBundleResult aResult = aEngine.shouldBundle(aSource, aDestination);
  require(aResult.mDecision == ShouldBundleDecision::Prompt, "Expected shouldBundle non-empty destination => Prompt");
}

void testPlainDestinationResolvesNearInput() {
  TempDir aTemp;
  const fs::path aSource = aTemp.mPath / "src";
  fs::create_directories(aSource);
  writeFile(aSource / "a.txt", "abc");

  SnowStormEngine aEngine(kTestArchiveSize);
  const ShouldBundleResult aResult = aEngine.shouldBundle(aSource, "archive");
  require(aResult.mDecision == ShouldBundleDecision::Yes, "Expected shouldBundle plain-name destination => Yes");
  require(aResult.mResolvedDestination == (aSource.parent_path() / "archive"),
          "Expected resolved destination to be source parent / archive");
}

void testRoundTripNestedAndEmptyFiles() {
  TempDir aTemp;
  const fs::path aSource = aTemp.mPath / "src";
  const fs::path aArchive = aTemp.mPath / "archive";
  const fs::path aOut = aTemp.mPath / "out";

  fs::create_directories(aSource / "nested");
  writeFile(aSource / "a.txt", "alpha");
  writeFile(aSource / "nested" / "b.bin", std::string("\x00\x01\x02\x03", 4));
  writeFile(aSource / "nested" / "empty.txt", "");

  SnowStormEngine aEngine(kTestArchiveSize);
  const SnowStormBundleStats aBundleStats = aEngine.bundle(aSource, aArchive, nullptr);
  require(aBundleStats.mFileCount == 3, "Expected 3 files bundled");

  const SnowStormUnbundleStats aUnbundleStats = aEngine.unbundle(aArchive, aOut, nullptr);
  require(aUnbundleStats.mFilesUnbundled == 3, "Expected 3 files unbundled");
  require(readFile(aOut / "a.txt") == "alpha", "Roundtrip mismatch: a.txt");
  require(readFile(aOut / "nested" / "b.bin") == std::string("\x00\x01\x02\x03", 4), "Roundtrip mismatch: b.bin");
  require(readFile(aOut / "nested" / "empty.txt").empty(), "Roundtrip mismatch: empty.txt");
}

void testShouldUnbundleNonDirectoryReturnsNo() {
  TempDir aTemp;
  const fs::path aSourceFile = aTemp.mPath / "archive_file";
  writeFile(aSourceFile, "x");

  SnowStormEngine aEngine(kTestArchiveSize);
  const ShouldBundleResult aResult = aEngine.shouldUnbundle(aSourceFile, aTemp.mPath / "out");
  require(aResult.mDecision == ShouldBundleDecision::No, "Expected shouldUnbundle file source => No");
}

void testUnbundleAmbiguousArchiveGroupsThrows() {
  TempDir aTemp;
  const fs::path aArchive = aTemp.mPath / "archive";
  fs::create_directories(aArchive);
  writeFile(aArchive / "a_0000.jag", "x");
  writeFile(aArchive / "b_0000.jag", "y");

  SnowStormEngine aEngine(kTestArchiveSize);
  bool aThrew = false;
  try {
    (void)aEngine.unbundle(aArchive, aTemp.mPath / "out", nullptr);
  } catch (const std::exception& aError) {
    aThrew = (std::string(aError.what()).find("Ambiguous archive naming groups") != std::string::npos);
  }
  require(aThrew, "Expected ambiguous naming groups to throw");
}

void testUnbundleTruncatedArchiveThrows() {
  TempDir aTemp;
  const fs::path aSource = aTemp.mPath / "src";
  const fs::path aArchive = aTemp.mPath / "archive";
  const fs::path aOut = aTemp.mPath / "out";
  fs::create_directories(aSource);
  writeFile(aSource / "a.txt", std::string(8192, 'z'));

  SnowStormEngine aEngine(kTestArchiveSize);
  (void)aEngine.bundle(aSource, aArchive, nullptr);

  const fs::path aJag0 = aArchive / "snowstorm_0000.jag";
  fs::resize_file(aJag0, 16);

  bool aThrew = false;
  try {
    (void)aEngine.unbundle(aArchive, aOut, nullptr);
  } catch (const std::exception&) {
    aThrew = true;
  }
  require(aThrew, "Expected truncated archive to throw");
}

}  // namespace

int main() {
  std::srand(12345);

  const std::vector<std::pair<std::string, std::function<void()>>> aTests = {
      {"shouldBundle missing source => No", testShouldBundleMissingSourceReturnsNo},
      {"shouldBundle destination file => No", testShouldBundleDestinationIsFileReturnsNo},
      {"shouldBundle non-empty destination => Prompt", testShouldBundleNonEmptyDestinationPrompts},
      {"plain destination resolves near input", testPlainDestinationResolvesNearInput},
      {"bundle/unbundle roundtrip nested + empty files", testRoundTripNestedAndEmptyFiles},
      {"shouldUnbundle source file => No", testShouldUnbundleNonDirectoryReturnsNo},
      {"unbundle ambiguous archive groups throws", testUnbundleAmbiguousArchiveGroupsThrows},
      {"unbundle truncated archive throws", testUnbundleTruncatedArchiveThrows},
  };

  int aFailures = 0;
  for (const auto& aTest : aTests) {
    try {
      aTest.second();
      std::cout << "[PASS] " << aTest.first << "\n";
    } catch (const std::exception& aError) {
      ++aFailures;
      std::cerr << "[FAIL] " << aTest.first << ": " << aError.what() << "\n";
    } catch (...) {
      ++aFailures;
      std::cerr << "[FAIL] " << aTest.first << ": unknown exception\n";
    }
  }

  if (aFailures != 0) {
    std::cerr << aFailures << " test(s) failed\n";
    return 1;
  }
  std::cout << "All tests passed (" << aTests.size() << ")\n";
  return 0;
}

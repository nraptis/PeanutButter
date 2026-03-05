#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "IO/FileReaderMock.hpp"
#include "IO/FileWriterMock.hpp"
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

fs::path findArchiveByIndex(const fs::path& pArchiveDir, std::uint64_t pIndex) {
  std::string aDigits = std::to_string(pIndex);
  if (aDigits.size() < static_cast<std::size_t>(gBundleLeadingZeros)) {
    aDigits = std::string(static_cast<std::size_t>(gBundleLeadingZeros) - aDigits.size(), '0') + aDigits;
  }
  const std::string aLegacyName = gBundleFilePrefix + aDigits + gBundleFileSuffix;
  const std::string aRecoveryName = gBundleFilePrefix + aDigits + "R" + gBundleFileSuffix;
  const std::string aNoRecoveryName = gBundleFilePrefix + aDigits + "N" + gBundleFileSuffix;

  for (const auto& aEntry : fs::directory_iterator(pArchiveDir)) {
    const std::string aName = aEntry.path().filename().string();
    if (aName == aLegacyName || aName == aRecoveryName || aName == aNoRecoveryName) {
      return aEntry.path();
    }
  }
  throw std::runtime_error("Archive index not found in directory: " + pArchiveDir.string());
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

void testUnbundleNoMatchingArchiveNamesThrows() {
  TempDir aTemp;
  const fs::path aArchive = aTemp.mPath / "archive";
  fs::create_directories(aArchive);
  writeFile(aArchive / "a_0000.jag", "x");
  writeFile(aArchive / "b_0000.jag", "y");

  const std::string aOldPrefix = gBundleFilePrefix;
  const std::string aOldSuffix = gBundleFileSuffix;
  gBundleFilePrefix = "snowstorm_";
  gBundleFileSuffix = ".jag";

  SnowStormEngine aEngine(kTestArchiveSize);
  bool aThrew = false;
  try {
    (void)aEngine.unbundle(aArchive, aTemp.mPath / "out", nullptr);
  } catch (const std::exception&) {
    aThrew = true;
  }

  gBundleFilePrefix = aOldPrefix;
  gBundleFileSuffix = aOldSuffix;
  require(aThrew, "Expected missing matching archive naming to throw");
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

  const fs::path aJag0 = findArchiveByIndex(aArchive, 0);
  fs::resize_file(aJag0, 16);

  bool aThrew = false;
  try {
    (void)aEngine.unbundle(aArchive, aOut, nullptr);
  } catch (const std::exception&) {
    aThrew = true;
  }
  require(aThrew, "Expected truncated archive to throw");
}

void testMultiArchiveRoundTripLargeFile() {
  TempDir aTemp;
  const fs::path aSource = aTemp.mPath / "src";
  const fs::path aArchive = aTemp.mPath / "archive";
  const fs::path aOut = aTemp.mPath / "out";
  fs::create_directories(aSource);

  std::string aPayload;
  aPayload.reserve(2500000);
  for (int aIndex = 0; aIndex < 2500000; ++aIndex) {
    aPayload.push_back(static_cast<char>('A' + (aIndex % 26)));
  }
  writeFile(aSource / "big.bin", aPayload);

  SnowStormEngine aEngine(BLOCK_SIZE_LAYER_3 * 1ULL);
  const SnowStormBundleStats aBundleStats = aEngine.bundle(aSource, aArchive, nullptr);
  require(aBundleStats.mArchiveCount >= 2, "Expected multiple archives in large-file test");

  const SnowStormUnbundleStats aStats = aEngine.unbundle(aArchive, aOut, nullptr);
  require(aStats.mFilesUnbundled == 1, "Expected large file to roundtrip across archives");
  require(readFile(aOut / "big.bin") == aPayload, "Expected exact large-file roundtrip");
}

void testUnbundleRecoversAfterFirstArchiveCorruption() {
  TempDir aTemp;
  const fs::path aSource = aTemp.mPath / "src";
  const fs::path aArchive = aTemp.mPath / "archive";
  const fs::path aOut = aTemp.mPath / "out";
  fs::create_directories(aSource);

  std::string aPayload;
  aPayload.reserve(6000);
  for (int aIndex = 0; aIndex < 6000; ++aIndex) {
    aPayload.push_back(static_cast<char>('a' + (aIndex % 26)));
  }
  const std::string aName = "blob.bin";
  writeFile(aSource / aName, aPayload);
  writeFile(aSource / "zz_after.txt", "tail-ok");

  SnowStormEngine aEngine(kTestArchiveSize);
  (void)aEngine.bundle(aSource, aArchive, nullptr);

  const fs::path aJag0 = findArchiveByIndex(aArchive, 0);
  fs::resize_file(aJag0, 0);

  bool aThrew = false;
  try {
    (void)aEngine.unbundle(aArchive, aOut, nullptr);
  } catch (const std::exception&) {
    aThrew = true;
  }
  require(aThrew, "Expected corruption of first archive to throw");
}

void testInMemoryRoundTripWithoutDisk() {
  auto aStore = std::make_shared<FileMockStore>();
  FileReaderMock aReader(aStore);
  FileWriterMock aWriter(aStore);

  const fs::path aSource = "/mem/src";
  const fs::path aArchive = "/mem/archive";
  const fs::path aOut = "/mem/out";
  aReader.addDirectory(aSource);
  aReader.addFile(aSource / "one.txt", std::vector<unsigned char>{'o', 'n', 'e'});
  aReader.addDirectory(aSource / "nested");
  aReader.addFile(aSource / "nested" / "two.bin", std::vector<unsigned char>{0, 1, 2, 3, 4});

  SnowStormEngine aEngine(kTestArchiveSize, aReader, aWriter);
  const SnowStormBundleStats aBundleStats = aEngine.bundle(aSource, aArchive, nullptr);
  require(aBundleStats.mFileCount == 2, "Expected 2 files bundled in memory");

  const SnowStormUnbundleStats aUnbundleStats = aEngine.unbundle(aArchive, aOut, nullptr);
  require(aUnbundleStats.mFilesUnbundled == 2, "Expected 2 files unbundled in memory");

  std::vector<unsigned char> aBytes;
  std::string aError;
  require(aReader.readRange(aOut / "one.txt", 0, 1024, aBytes, &aError), "Expected one.txt in memory output");
  require(std::string(aBytes.begin(), aBytes.end()) == "one", "Expected one.txt contents in memory output");
  require(aReader.readRange(aOut / "nested" / "two.bin", 0, 1024, aBytes, &aError), "Expected two.bin in memory output");
  require(aBytes == std::vector<unsigned char>({0, 1, 2, 3, 4}), "Expected two.bin contents in memory output");
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
      {"unbundle no matching archive names throws", testUnbundleNoMatchingArchiveNamesThrows},
      {"unbundle truncated archive throws", testUnbundleTruncatedArchiveThrows},
      {"multi-archive roundtrip large file", testMultiArchiveRoundTripLargeFile},
      {"unbundle recovers after first archive corruption", testUnbundleRecoversAfterFirstArchiveCorruption},
      {"in-memory roundtrip without disk", testInMemoryRoundTripWithoutDisk},
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

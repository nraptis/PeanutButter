#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "FileCompare.hpp"
#include "Globals.hpp"
#include "IO/FileReaderImplementation.hpp"
#include "IO/FileWriterImplementation.hpp"
#include "LayeredEncryption/LayeredEncryptionMockEmpty.hpp"
#include "SnowStorm/SnowStormEngine.hpp"

namespace fs = std::filesystem;

namespace {

std::string makeRunDir(const char* pPrefix, int pRun) {
  std::ostringstream aOut;
  aOut << pPrefix << "_" << std::setw(3) << std::setfill('0') << pRun;
  return aOut.str();
}

std::string buildFruitText(std::size_t pTargetSize, std::mt19937_64& pRng) {
  static const std::vector<std::string> kWords = {
      "apple", "banana", "orange", "grape", "mango", "pear", "peach", "plum", "apricot", "cherry",
      "lime", "lemon", "papaya", "kiwi", "fig", "guava", "melon", "berry", "coconut", "pineapple"};
  std::uniform_int_distribution<std::size_t> aWordDist(0, kWords.size() - 1);
  std::string aOut;
  aOut.reserve(pTargetSize);
  while (aOut.size() < pTargetSize) {
    if (!aOut.empty()) {
      aOut.push_back(' ');
    }
    aOut += kWords[aWordDist(pRng)];
  }
  if (aOut.size() > pTargetSize) {
    aOut.resize(pTargetSize);
  }
  return aOut;
}

void require(bool pCondition, const std::string& pMessage) {
  if (!pCondition) {
    throw std::runtime_error(pMessage);
  }
}

fs::path testBasePath() {
#ifdef PEANUTBUTTER_SOURCE_DIR
  return fs::path(PEANUTBUTTER_SOURCE_DIR) / "testdata";
#else
  return fs::path("testdata");
#endif
}

}  // namespace

int main() {
  try {
    std::mt19937_64 aRng(424242ULL);
    std::uniform_int_distribution<int> aFileCountDist(1, 10);
    std::uniform_int_distribution<int> aFileSizeDist(100, 200 * 1024);

    const fs::path aBase = testBasePath();
    fs::create_directories(aBase);

    FileReaderImplementation aReader;
    FileWriterImplementation aWriter;
    LayeredEncryptionMockEmpty aNoEncryption;

    for (int aRun = 1; aRun <= 100; ++aRun) {
      const fs::path aInput = aBase / makeRunDir("input", aRun);
      const fs::path aArchive = aBase / makeRunDir("archive", aRun);
      const fs::path aUnpack = aBase / makeRunDir("unpack", aRun);

      std::error_code aEc;
      fs::remove_all(aInput, aEc);
      fs::remove_all(aArchive, aEc);
      fs::remove_all(aUnpack, aEc);
      fs::create_directories(aInput);

      const int aFileCount = aFileCountDist(aRng);
      for (int aFileIdx = 0; aFileIdx < aFileCount; ++aFileIdx) {
        const std::size_t aSize = static_cast<std::size_t>(aFileSizeDist(aRng));
        const std::string aText = buildFruitText(aSize, aRng);
        const fs::path aFilePath = aInput / ("file_" + std::to_string(aFileIdx) + ".txt");
        fs::create_directories(aFilePath.parent_path());
        std::ofstream aOut(aFilePath, std::ios::binary | std::ios::trunc);
        aOut.write(aText.data(), static_cast<std::streamsize>(aText.size()));
      }

      SnowStormEngine aEngine(BLOCK_SIZE_LAYER_3 * 2ULL, aReader, aWriter, aNoEncryption);
      const SnowStormBundleStats aBundleStats = aEngine.bundle(aInput, aArchive, nullptr);
      require(aBundleStats.mFileCount == static_cast<std::uint64_t>(aFileCount),
              "Run " + std::to_string(aRun) + ": bundle file count mismatch");

      const SnowStormUnbundleStats aUnbundleStats = aEngine.unbundle(aArchive, aUnpack, nullptr);
      require(aUnbundleStats.mFilesUnbundled == static_cast<std::uint64_t>(aFileCount),
              "Run " + std::to_string(aRun) + ": unbundle file count mismatch");

      std::string aCompareError;
      const bool aSame = FileCompare::Test(nullptr, aInput, aUnpack, &aCompareError);
      require(aSame, "Run " + std::to_string(aRun) + ": compare failed: " + aCompareError);
    }

    std::cout << "Test100RealRunsNoEncryption passed\n";
    return 0;
  } catch (const std::exception& aError) {
    std::cerr << "Test100RealRunsNoEncryption failed: " << aError.what() << "\n";
    return 1;
  }
}

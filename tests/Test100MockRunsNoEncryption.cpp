#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Globals.hpp"
#include "IO/FileReaderMock.hpp"
#include "IO/FileWriterMock.hpp"
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

}  // namespace

int main() {
  try {
    std::mt19937_64 aRng(777777ULL);
    std::uniform_int_distribution<int> aFileCountDist(1, 10);
    std::uniform_int_distribution<int> aFileSizeDist(100, 200 * 1024);

    LayeredEncryptionMockEmpty aNoEncryption;

    for (int aRun = 1; aRun <= 100; ++aRun) {
      auto aStore = std::make_shared<FileMockStore>();
      FileReaderMock aReader(aStore);
      FileWriterMock aWriter(aStore);

      const fs::path aInput = fs::path("/testdata") / makeRunDir("input", aRun);
      const fs::path aArchive = fs::path("/testdata") / makeRunDir("archive", aRun);
      const fs::path aUnpack = fs::path("/testdata") / makeRunDir("unpack", aRun);
      aReader.addDirectory(aInput);

      const int aFileCount = aFileCountDist(aRng);
      std::vector<std::vector<unsigned char>> aExpected;
      aExpected.reserve(static_cast<std::size_t>(aFileCount));
      for (int aFileIdx = 0; aFileIdx < aFileCount; ++aFileIdx) {
        const std::size_t aSize = static_cast<std::size_t>(aFileSizeDist(aRng));
        const std::string aText = buildFruitText(aSize, aRng);
        std::vector<unsigned char> aBytes(aText.begin(), aText.end());
        aExpected.push_back(aBytes);
        aReader.addFile(aInput / ("file_" + std::to_string(aFileIdx) + ".txt"), aBytes);
      }

      SnowStormEngine aEngine(BLOCK_SIZE_LAYER_3 * 2ULL, aReader, aWriter, aNoEncryption);
      const SnowStormBundleStats aBundleStats = aEngine.bundle(aInput, aArchive, nullptr);
      require(aBundleStats.mFileCount == static_cast<std::uint64_t>(aFileCount),
              "Run " + std::to_string(aRun) + ": bundle file count mismatch");
      const SnowStormUnbundleStats aUnbundleStats = aEngine.unbundle(aArchive, aUnpack, nullptr);
      require(aUnbundleStats.mFilesUnbundled == static_cast<std::uint64_t>(aFileCount),
              "Run " + std::to_string(aRun) + ": unbundle file count mismatch");

      for (int aFileIdx = 0; aFileIdx < aFileCount; ++aFileIdx) {
        const fs::path aPath = aUnpack / ("file_" + std::to_string(aFileIdx) + ".txt");
        std::uint64_t aSize = 0;
        std::string aSizeError;
        require(aReader.fileSize(aPath, &aSize, &aSizeError),
                "Run " + std::to_string(aRun) + ": missing output file");
        std::vector<unsigned char> aBytes;
        std::string aReadError;
        require(aReader.readRange(aPath, 0, aSize, aBytes, &aReadError),
                "Run " + std::to_string(aRun) + ": read output file failed");
        require(aBytes == aExpected[static_cast<std::size_t>(aFileIdx)],
                "Run " + std::to_string(aRun) + ": output mismatch");
      }
    }

    std::cout << "Test100MockRunsNoEncryption passed\n";
    return 0;
  } catch (const std::exception& aError) {
    std::cerr << "Test100MockRunsNoEncryption failed: " << aError.what() << "\n";
    return 1;
  }
}


#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <functional>

#include "FileCompare.hpp"
#include "Globals.hpp"
#include "IO/FileReaderImplementation.hpp"
#include "IO/FileReaderMock.hpp"
#include "IO/FileWriterImplementation.hpp"
#include "IO/FileWriterMock.hpp"
#include "PeanutButterDelegate.hpp"
#include "PeanutButterImplementation.hpp"

namespace fs = std::filesystem;

namespace {

struct TestDelegate final : public PeanutButterDelegate {
  void logMessage(const std::string&) override {}
  bool showOkCancelDialog(const std::string&, const std::string&) override { return true; }
};

struct FileEntry {
  fs::path mRelativePath;
  std::vector<unsigned char> mBytes;
};

void require(bool pCondition, const std::string& pMessage) {
  if (!pCondition) {
    throw std::runtime_error(pMessage);
  }
}

void writeRealFile(const fs::path& pRoot, const FileEntry& pEntry) {
  const fs::path aPath = pRoot / pEntry.mRelativePath;
  fs::create_directories(aPath.parent_path());
  std::ofstream aOut(aPath, std::ios::binary | std::ios::trunc);
  if (!aOut) {
    throw std::runtime_error("Failed to create file: " + aPath.string());
  }
  if (!pEntry.mBytes.empty()) {
    aOut.write(reinterpret_cast<const char*>(pEntry.mBytes.data()),
               static_cast<std::streamsize>(pEntry.mBytes.size()));
  }
}

std::vector<FileEntry> buildRandomTree(std::mt19937_64& pRng) {
  std::vector<FileEntry> aEntries;
  std::uniform_int_distribution<int> aSubdirDist(0, 2);
  std::uniform_int_distribution<int> aFilesDist(1, 4);
  std::uniform_int_distribution<int> aByteDist(0, 255);
  std::uniform_int_distribution<int> aSizeDist(262144, 5242880);  // 0.25MB to 5MB

  std::function<void(const fs::path&, int)> aBuild = [&](const fs::path& pRelDir, int pDepth) {
    const int aFileCount = aFilesDist(pRng);
    for (int aIndex = 0; aIndex < aFileCount; ++aIndex) {
      FileEntry aEntry;
      aEntry.mRelativePath = pRelDir / ("file_" + std::to_string(pDepth) + "_" + std::to_string(aIndex) + ".bin");
      const int aSize = aSizeDist(pRng);
      aEntry.mBytes.resize(static_cast<std::size_t>(aSize), 0);
      for (std::size_t i = 0; i < aEntry.mBytes.size(); ++i) {
        aEntry.mBytes[i] = static_cast<unsigned char>(aByteDist(pRng));
      }
      aEntries.push_back(std::move(aEntry));
    }
    if (pDepth >= 3 || aEntries.size() >= 10) {
      return;
    }
    const int aSubdirCount = aSubdirDist(pRng);
    for (int aIndex = 0; aIndex < aSubdirCount; ++aIndex) {
      if (aEntries.size() >= 10) {
        break;
      }
      aBuild(pRelDir / ("dir_" + std::to_string(pDepth) + "_" + std::to_string(aIndex)), pDepth + 1);
    }
  };

  aBuild(fs::path(), 1);
  require(!aEntries.empty(), "Random tree generator created no files");
  return aEntries;
}

std::vector<unsigned char> readMockFile(FileReaderMock& pReader, const fs::path& pPath) {
  std::uint64_t aSize = 0;
  std::string aSizeError;
  if (!pReader.fileSize(pPath, &aSize, &aSizeError)) {
    throw std::runtime_error("Failed getting mock file size: " + pPath.generic_string());
  }
  std::vector<unsigned char> aBytes;
  std::string aError;
  if (!pReader.readRange(pPath, 0, aSize, aBytes, &aError)) {
    throw std::runtime_error("Failed reading mock file: " + pPath.generic_string());
  }
  return aBytes;
}

std::vector<fs::path> collectRecoveryArchivesOnDisk(const fs::path& pArchiveDir) {
  std::vector<fs::path> aCandidates;
  for (const auto& aEntry : fs::directory_iterator(pArchiveDir)) {
    const std::string aName = aEntry.path().filename().string();
    if (aName.rfind(gBundleFilePrefix, 0) == 0 &&
        aName.size() > gBundleFilePrefix.size() + gBundleFileSuffix.size() &&
        aName.substr(aName.size() - gBundleFileSuffix.size()) == gBundleFileSuffix) {
      aCandidates.push_back(aEntry.path());
    }
  }
  std::sort(aCandidates.begin(), aCandidates.end());
  return aCandidates;
}

std::vector<fs::path> collectRecoveryArchivesMock(FileReaderMock& pReader, const fs::path& pArchiveDir) {
  std::vector<fs::path> aFiles;
  std::string aError;
  if (!pReader.listFilesRecursive(pArchiveDir, aFiles, &aError)) {
    throw std::runtime_error("Failed listing mock archives");
  }
  std::vector<fs::path> aCandidates;
  for (const auto& aPath : aFiles) {
    const std::string aName = aPath.filename().string();
    if (aName.rfind(gBundleFilePrefix, 0) == 0 &&
        aName.size() > gBundleFilePrefix.size() + gBundleFileSuffix.size() &&
        aName.substr(aName.size() - gBundleFileSuffix.size()) == gBundleFileSuffix) {
      aCandidates.push_back(aPath);
    }
  }
  std::sort(aCandidates.begin(), aCandidates.end());
  return aCandidates;
}

}  // namespace

int main() {
  try {
    std::mt19937_64 aRng(123456789ULL);
    const std::vector<FileEntry> aEntries = buildRandomTree(aRng);

    const fs::path aSourceDir = "test_mock_file_tools";
    const fs::path aArchiveDir = "test_mock_file_tools_archive";
    const fs::path aUnarchiveDir = "test_mock_file_tools_unarchive";
    const fs::path aRecoverDir = "test_mock_file_tools_recover";

    std::error_code aEc;
    fs::remove_all(aSourceDir, aEc);
    fs::remove_all(aArchiveDir, aEc);
    fs::remove_all(aUnarchiveDir, aEc);
    fs::remove_all(aRecoverDir, aEc);
    fs::create_directories(aSourceDir);

    for (const FileEntry& aEntry : aEntries) {
      writeRealFile(aSourceDir, aEntry);
    }

    TestDelegate aDelegate;
    FileReaderImplementation aRealReader;
    FileWriterImplementation aRealWriter;
    const std::uint64_t aArchiveSize = BLOCK_SIZE_LAYER_3 * 2ULL;
    PeanutButterImplementation aRealImpl(aDelegate, aArchiveSize, aRealReader, aRealWriter);
    const SnowStormBundleStats aRealBundleStats = aRealImpl.pack(aSourceDir, aArchiveDir);
    require(aRealBundleStats.mFileCount == aEntries.size(), "Real pack file count mismatch");

    const SnowStormUnbundleStats aRealUnbundleStats = aRealImpl.unpack(aArchiveDir, aUnarchiveDir);
    require(aRealUnbundleStats.mFilesUnbundled == aEntries.size(), "Real unpack file count mismatch");

    std::string aCompareError;
    const bool aSame = FileCompare::Test(nullptr, aSourceDir, aUnarchiveDir, &aCompareError);
    require(aSame, "Real unpack compare failed: " + aCompareError);

    const std::vector<fs::path> aRealRecoveryCandidates = collectRecoveryArchivesOnDisk(aArchiveDir);
    require(!aRealRecoveryCandidates.empty(), "No real archives produced");
    bool aFoundRealRecover = false;
    PeanutButterImplementation::RecoveryStart aRealRecoverStart;
    for (std::size_t i = 0; i < aRealRecoveryCandidates.size(); ++i) {
      try {
        aRealRecoverStart = PeanutButterImplementation::resolveRecoveryStartFile(
            aRealRecoveryCandidates[i], gBundleFilePrefix, gBundleFileSuffix, aRealReader);
        aFoundRealRecover = true;
        break;
      } catch (const std::exception&) {
      }
    }
    if (!aFoundRealRecover) {
      aRealRecoverStart.mArchiveDirectory = aArchiveDir;
      aRealRecoverStart.mArchiveIndex = 0;
      aRealRecoverStart.mByteOffsetInArchive = 0;
    }
    const SnowStormUnbundleStats aRealRecoverStats = aRealImpl.recover(aRealRecoverStart, aRecoverDir);
    require(aRealRecoverStats.mFilesUnbundled > 0, "Real recover produced zero files");

    auto aStore = std::make_shared<FileMockStore>();
    FileReaderMock aMockReader(aStore);
    FileWriterMock aMockWriter(aStore);
    const fs::path aMockSource = "/mock/test_mock_file_tools";
    const fs::path aMockArchive = "/mock/test_mock_file_tools_archive";
    const fs::path aMockUnarchive = "/mock/test_mock_file_tools_unarchive";
    const fs::path aMockRecover = "/mock/test_mock_file_tools_recover";
    aMockReader.addDirectory(aMockSource);
    for (const FileEntry& aEntry : aEntries) {
      aMockReader.addFile(aMockSource / aEntry.mRelativePath, aEntry.mBytes);
    }

    PeanutButterImplementation aMockImpl(aDelegate, aArchiveSize, aMockReader, aMockWriter);
    const SnowStormBundleStats aMockBundleStats = aMockImpl.pack(aMockSource, aMockArchive);
    require(aMockBundleStats.mFileCount == aEntries.size(), "Mock pack file count mismatch");
    const SnowStormUnbundleStats aMockUnbundleStats = aMockImpl.unpack(aMockArchive, aMockUnarchive);
    require(aMockUnbundleStats.mFilesUnbundled == aEntries.size(), "Mock unpack file count mismatch");

    for (const FileEntry& aEntry : aEntries) {
      const std::vector<unsigned char> aOut = readMockFile(aMockReader, aMockUnarchive / aEntry.mRelativePath);
      require(aOut == aEntry.mBytes, "Mock unpack content mismatch: " + aEntry.mRelativePath.generic_string());
    }

    const std::vector<fs::path> aMockRecoveryCandidates = collectRecoveryArchivesMock(aMockReader, aMockArchive);
    require(!aMockRecoveryCandidates.empty(), "No mock archives produced");
    bool aFoundMockRecover = false;
    PeanutButterImplementation::RecoveryStart aMockRecoverStart;
    for (std::size_t i = 0; i < aMockRecoveryCandidates.size(); ++i) {
      try {
        aMockRecoverStart = PeanutButterImplementation::resolveRecoveryStartFile(
            aMockRecoveryCandidates[i], gBundleFilePrefix, gBundleFileSuffix, aMockReader);
        aFoundMockRecover = true;
        break;
      } catch (const std::exception&) {
      }
    }
    if (!aFoundMockRecover) {
      aMockRecoverStart.mArchiveDirectory = aMockArchive;
      aMockRecoverStart.mArchiveIndex = 0;
      aMockRecoverStart.mByteOffsetInArchive = 0;
    }
    const SnowStormUnbundleStats aMockRecoverStats = aMockImpl.recover(aMockRecoverStart, aMockRecover);
    require(aMockRecoverStats.mFilesUnbundled > 0, "Mock recover produced zero files");

    std::cout << "TestMockFileWriterAndReader passed\n";
    return 0;
  } catch (const std::exception& aError) {
    std::cerr << "TestMockFileWriterAndReader failed: " << aError.what() << "\n";
    return 1;
  }
}

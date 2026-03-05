#include "IO/FileReaderMock.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace {

void ensureParentDirectories(FileMockStore& pStore, const fs::path& pPath) {
  fs::path aCur = pPath.parent_path();
  while (!aCur.empty()) {
    pStore.mDirectories.insert(normalizeStorePath(aCur));
    const fs::path aNext = aCur.parent_path();
    if (aNext == aCur) {
      break;
    }
    aCur = aNext;
  }
}

}  // namespace

FileReaderMock::FileReaderMock()
    : mStore(std::make_shared<FileMockStore>()) {}

FileReaderMock::FileReaderMock(std::shared_ptr<FileMockStore> pStore)
    : mStore(std::move(pStore)) {
  if (!mStore) {
    mStore = std::make_shared<FileMockStore>();
  }
}

std::shared_ptr<FileMockStore> FileReaderMock::store() const {
  return mStore;
}

void FileReaderMock::addDirectory(const fs::path& pPath) {
  mStore->mDirectories.insert(normalizeStorePath(pPath));
  ensureParentDirectories(*mStore, pPath);
}

void FileReaderMock::addFile(const fs::path& pPath, const std::vector<unsigned char>& pBytes) {
  const std::string aPath = normalizeStorePath(pPath);
  mStore->mFiles[aPath] = pBytes;
  ensureParentDirectories(*mStore, pPath);
}

bool FileReaderMock::exists(const fs::path& pPath) const {
  const std::string aPath = normalizeStorePath(pPath);
  return mStore->mFiles.count(aPath) != 0 || mStore->mDirectories.count(aPath) != 0;
}

bool FileReaderMock::isDirectory(const fs::path& pPath) const {
  return mStore->mDirectories.count(normalizeStorePath(pPath)) != 0;
}

bool FileReaderMock::isRegularFile(const fs::path& pPath) const {
  return mStore->mFiles.count(normalizeStorePath(pPath)) != 0;
}

bool FileReaderMock::fileSize(const fs::path& pPath, std::uint64_t* pOutSize, std::string* pError) const {
  if (pOutSize == nullptr) {
    if (pError != nullptr) {
      *pError = "fileSize requires non-null output";
    }
    return false;
  }
  const auto aIt = mStore->mFiles.find(normalizeStorePath(pPath));
  if (aIt == mStore->mFiles.end()) {
    if (pError != nullptr) {
      *pError = "File not found: " + pPath.generic_string();
    }
    return false;
  }
  *pOutSize = static_cast<std::uint64_t>(aIt->second.size());
  return true;
}

bool FileReaderMock::listDirectory(const fs::path& pPath,
                                   std::vector<fs::path>& pEntries,
                                   std::string* pError) const {
  pEntries.clear();
  const std::string aDir = normalizeStorePath(pPath);
  if (!isDirectory(pPath)) {
    if (pError != nullptr) {
      *pError = "Directory not found: " + aDir;
    }
    return false;
  }
  const std::string aPrefix = aDir.empty() ? "" : (aDir + "/");
  std::set<std::string> aChildren;
  for (const auto& aDirEntry : mStore->mDirectories) {
    if (aDirEntry.rfind(aPrefix, 0) != 0) {
      continue;
    }
    const std::string aRest = aDirEntry.substr(aPrefix.size());
    const std::size_t aSlash = aRest.find('/');
    if (!aRest.empty() && aSlash == std::string::npos) {
      aChildren.insert(aPrefix + aRest);
    }
  }
  for (const auto& aFile : mStore->mFiles) {
    if (aFile.first.rfind(aPrefix, 0) != 0) {
      continue;
    }
    const std::string aRest = aFile.first.substr(aPrefix.size());
    const std::size_t aSlash = aRest.find('/');
    if (!aRest.empty() && aSlash == std::string::npos) {
      aChildren.insert(aPrefix + aRest);
    }
  }
  for (const std::string& aChild : aChildren) {
    pEntries.push_back(fs::path(aChild));
  }
  return true;
}

bool FileReaderMock::listFilesRecursive(const fs::path& pRoot,
                                        std::vector<fs::path>& pFiles,
                                        std::string* pError) const {
  pFiles.clear();
  if (!isDirectory(pRoot)) {
    if (pError != nullptr) {
      *pError = "Directory not found: " + pRoot.generic_string();
    }
    return false;
  }
  const std::string aRoot = normalizeStorePath(pRoot);
  const std::string aPrefix = aRoot.empty() ? "" : (aRoot + "/");
  for (const auto& aFile : mStore->mFiles) {
    if (aFile.first.rfind(aPrefix, 0) == 0) {
      pFiles.push_back(fs::path(aFile.first));
    }
  }
  std::sort(pFiles.begin(), pFiles.end(), [](const fs::path& pA, const fs::path& pB) {
    return pA.generic_string() < pB.generic_string();
  });
  return true;
}

bool FileReaderMock::readRange(const fs::path& pPath,
                               std::uint64_t pOffset,
                               std::uint64_t pByteCount,
                               std::vector<unsigned char>& pOutBytes,
                               std::string* pError) const {
  pOutBytes.clear();
  const auto aIt = mStore->mFiles.find(normalizeStorePath(pPath));
  if (aIt == mStore->mFiles.end()) {
    if (pError != nullptr) {
      *pError = "File not found: " + pPath.generic_string();
    }
    return false;
  }
  const std::vector<unsigned char>& aBytes = aIt->second;
  const std::uint64_t aStart = std::min<std::uint64_t>(pOffset, aBytes.size());
  const std::uint64_t aEnd = std::min<std::uint64_t>(aStart + pByteCount, aBytes.size());
  pOutBytes.insert(pOutBytes.end(),
                   aBytes.begin() + static_cast<std::ptrdiff_t>(aStart),
                   aBytes.begin() + static_cast<std::ptrdiff_t>(aEnd));
  return true;
}

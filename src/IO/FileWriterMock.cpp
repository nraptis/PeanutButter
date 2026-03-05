#include "IO/FileWriterMock.hpp"

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

FileWriterMock::FileWriterMock()
    : mStore(std::make_shared<FileMockStore>()) {}

FileWriterMock::FileWriterMock(std::shared_ptr<FileMockStore> pStore)
    : mStore(std::move(pStore)) {
  if (!mStore) {
    mStore = std::make_shared<FileMockStore>();
  }
}

std::shared_ptr<FileMockStore> FileWriterMock::store() const {
  return mStore;
}

bool FileWriterMock::createDirectories(const fs::path& pPath, std::string*) {
  fs::path aCur = pPath;
  while (!aCur.empty()) {
    mStore->mDirectories.insert(normalizeStorePath(aCur));
    const fs::path aNext = aCur.parent_path();
    if (aNext == aCur) {
      break;
    }
    aCur = aNext;
  }
  return true;
}

bool FileWriterMock::clearDirectory(const fs::path& pPath, std::string*) {
  const std::string aDir = normalizeStorePath(pPath);
  const std::string aPrefix = aDir.empty() ? "" : (aDir + "/");
  for (auto aIt = mStore->mFiles.begin(); aIt != mStore->mFiles.end();) {
    if (aIt->first.rfind(aPrefix, 0) == 0) {
      aIt = mStore->mFiles.erase(aIt);
    } else {
      ++aIt;
    }
  }
  for (auto aIt = mStore->mDirectories.begin(); aIt != mStore->mDirectories.end();) {
    if (*aIt != aDir && aIt->rfind(aPrefix, 0) == 0) {
      aIt = mStore->mDirectories.erase(aIt);
    } else {
      ++aIt;
    }
  }
  return true;
}

bool FileWriterMock::writeAt(const fs::path& pPath,
                             std::uint64_t pOffset,
                             const unsigned char* pData,
                             std::uint64_t pByteCount,
                             std::string* pError) {
  if (pData == nullptr && pByteCount != 0) {
    if (pError != nullptr) {
      *pError = "writeAt received null data";
    }
    return false;
  }
  const std::string aPath = normalizeStorePath(pPath);
  std::vector<unsigned char>& aBytes = mStore->mFiles[aPath];
  const std::size_t aNeeded = static_cast<std::size_t>(pOffset + pByteCount);
  if (aBytes.size() < aNeeded) {
    aBytes.resize(aNeeded, 0);
  }
  for (std::uint64_t aIndex = 0; aIndex < pByteCount; ++aIndex) {
    aBytes[static_cast<std::size_t>(pOffset + aIndex)] = pData[aIndex];
  }
  ensureParentDirectories(*mStore, pPath);
  return true;
}

bool FileWriterMock::append(const fs::path& pPath,
                            const unsigned char* pData,
                            std::uint64_t pByteCount,
                            std::string* pError) {
  const std::string aPath = normalizeStorePath(pPath);
  std::vector<unsigned char>& aBytes = mStore->mFiles[aPath];
  return writeAt(pPath, static_cast<std::uint64_t>(aBytes.size()), pData, pByteCount, pError);
}

bool FileWriterMock::truncate(const fs::path& pPath, std::uint64_t pSize, std::string*) {
  const std::string aPath = normalizeStorePath(pPath);
  std::vector<unsigned char>& aBytes = mStore->mFiles[aPath];
  aBytes.resize(static_cast<std::size_t>(pSize), 0);
  ensureParentDirectories(*mStore, pPath);
  return true;
}

bool FileWriterMock::renameFile(const fs::path& pFrom, const fs::path& pTo, std::string*) {
  const std::string aFrom = normalizeStorePath(pFrom);
  const std::string aTo = normalizeStorePath(pTo);
  if (aFrom == aTo) {
    return true;
  }
  auto aFound = mStore->mFiles.find(aFrom);
  if (aFound == mStore->mFiles.end()) {
    return false;
  }
  mStore->mFiles[aTo] = std::move(aFound->second);
  mStore->mFiles.erase(aFound);
  ensureParentDirectories(*mStore, pTo);
  return true;
}

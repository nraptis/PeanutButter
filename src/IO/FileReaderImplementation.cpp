#include "IO/FileReaderImplementation.hpp"

#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

bool FileReaderImplementation::exists(const fs::path& pPath) const {
  return fs::exists(pPath);
}

bool FileReaderImplementation::isDirectory(const fs::path& pPath) const {
  return fs::is_directory(pPath);
}

bool FileReaderImplementation::isRegularFile(const fs::path& pPath) const {
  return fs::is_regular_file(pPath);
}

bool FileReaderImplementation::fileSize(const fs::path& pPath, std::uint64_t* pOutSize, std::string* pError) const {
  if (pOutSize == nullptr) {
    if (pError != nullptr) {
      *pError = "fileSize requires non-null output";
    }
    return false;
  }
  std::error_code aEc;
  const std::uintmax_t aSize = fs::file_size(pPath, aEc);
  if (aEc) {
    if (pError != nullptr) {
      *pError = "Failed to read file size: " + pPath.string();
    }
    return false;
  }
  *pOutSize = static_cast<std::uint64_t>(aSize);
  return true;
}

bool FileReaderImplementation::listDirectory(const fs::path& pPath,
                                             std::vector<fs::path>& pEntries,
                                             std::string* pError) const {
  pEntries.clear();
  try {
    for (const auto& aEntry : fs::directory_iterator(pPath)) {
      pEntries.push_back(aEntry.path());
    }
    return true;
  } catch (const std::exception&) {
    if (pError != nullptr) {
      *pError = "Failed to list directory: " + pPath.string();
    }
    return false;
  }
}

bool FileReaderImplementation::listFilesRecursive(const fs::path& pRoot,
                                                  std::vector<fs::path>& pFiles,
                                                  std::string* pError) const {
  pFiles.clear();
  try {
    for (const auto& aEntry : fs::recursive_directory_iterator(pRoot)) {
      if (aEntry.is_regular_file()) {
        pFiles.push_back(aEntry.path());
      }
    }
    return true;
  } catch (const std::exception&) {
    if (pError != nullptr) {
      *pError = "Failed to scan files recursively: " + pRoot.string();
    }
    return false;
  }
}

bool FileReaderImplementation::readRange(const fs::path& pPath,
                                         std::uint64_t pOffset,
                                         std::uint64_t pByteCount,
                                         std::vector<unsigned char>& pOutBytes,
                                         std::string* pError) const {
  pOutBytes.clear();
  std::ifstream aIn(pPath, std::ios::binary);
  if (!aIn) {
    if (pError != nullptr) {
      *pError = "Failed to open file for read: " + pPath.string();
    }
    return false;
  }
  aIn.seekg(static_cast<std::streamoff>(pOffset), std::ios::beg);
  if (!aIn) {
    if (pError != nullptr) {
      *pError = "Failed to seek while reading: " + pPath.string();
    }
    return false;
  }
  pOutBytes.resize(static_cast<std::size_t>(pByteCount), 0);
  aIn.read(reinterpret_cast<char*>(pOutBytes.data()), static_cast<std::streamsize>(pByteCount));
  const std::streamsize aGot = aIn.gcount();
  if (aGot < 0) {
    if (pError != nullptr) {
      *pError = "Read failed: " + pPath.string();
    }
    return false;
  }
  pOutBytes.resize(static_cast<std::size_t>(aGot));
  return true;
}


#include "IO/FileWriterImplementation.hpp"

#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

bool FileWriterImplementation::createDirectories(const fs::path& pPath, std::string* pError) {
  std::error_code aEc;
  fs::create_directories(pPath, aEc);
  if (aEc) {
    if (pError != nullptr) {
      *pError = "Failed to create directory: " + pPath.string();
    }
    return false;
  }
  return true;
}

bool FileWriterImplementation::clearDirectory(const fs::path& pPath, std::string* pError) {
  if (!fs::exists(pPath)) {
    return true;
  }
  for (const auto& aEntry : fs::directory_iterator(pPath)) {
    std::error_code aEc;
    fs::remove_all(aEntry.path(), aEc);
    if (aEc) {
      if (pError != nullptr) {
        *pError = "Failed to clear entry: " + aEntry.path().string();
      }
      return false;
    }
  }
  return true;
}

bool FileWriterImplementation::writeAt(const fs::path& pPath,
                                       std::uint64_t pOffset,
                                       const unsigned char* pData,
                                       std::uint64_t pByteCount,
                                       std::string* pError) {
  std::fstream aFile(pPath, std::ios::binary | std::ios::in | std::ios::out);
  if (!aFile) {
    std::ofstream aCreate(pPath, std::ios::binary | std::ios::trunc);
    aCreate.close();
    aFile.open(pPath, std::ios::binary | std::ios::in | std::ios::out);
  }
  if (!aFile) {
    if (pError != nullptr) {
      *pError = "Failed to open file for random write: " + pPath.string();
    }
    return false;
  }
  aFile.seekp(static_cast<std::streamoff>(pOffset), std::ios::beg);
  if (!aFile) {
    if (pError != nullptr) {
      *pError = "Failed to seek while writing: " + pPath.string();
    }
    return false;
  }
  aFile.write(reinterpret_cast<const char*>(pData), static_cast<std::streamsize>(pByteCount));
  if (!aFile) {
    if (pError != nullptr) {
      *pError = "Failed writing file: " + pPath.string();
    }
    return false;
  }
  return true;
}

bool FileWriterImplementation::append(const fs::path& pPath,
                                      const unsigned char* pData,
                                      std::uint64_t pByteCount,
                                      std::string* pError) {
  std::ofstream aOut(pPath, std::ios::binary | std::ios::app);
  if (!aOut) {
    if (pError != nullptr) {
      *pError = "Failed to open file for append: " + pPath.string();
    }
    return false;
  }
  aOut.write(reinterpret_cast<const char*>(pData), static_cast<std::streamsize>(pByteCount));
  if (!aOut) {
    if (pError != nullptr) {
      *pError = "Failed appending file: " + pPath.string();
    }
    return false;
  }
  return true;
}

bool FileWriterImplementation::truncate(const fs::path& pPath, std::uint64_t pSize, std::string* pError) {
  std::error_code aEc;
  if (!fs::exists(pPath)) {
    std::ofstream aCreate(pPath, std::ios::binary | std::ios::trunc);
    aCreate.close();
  }
  fs::resize_file(pPath, static_cast<std::uintmax_t>(pSize), aEc);
  if (aEc) {
    if (pError != nullptr) {
      *pError = "Failed to resize file: " + pPath.string();
    }
    return false;
  }
  return true;
}

bool FileWriterImplementation::renameFile(const fs::path& pFrom,
                                          const fs::path& pTo,
                                          std::string* pError) {
  if (pFrom == pTo) {
    return true;
  }
  std::error_code aEc;
  if (fs::exists(pTo, aEc)) {
    aEc.clear();
    fs::remove(pTo, aEc);
    if (aEc) {
      if (pError != nullptr) {
        *pError = "Failed to remove existing destination file: " + pTo.string();
      }
      return false;
    }
  }
  aEc.clear();
  fs::rename(pFrom, pTo, aEc);
  if (aEc) {
    if (pError != nullptr) {
      *pError = "Failed to rename file: " + pFrom.string() + " -> " + pTo.string();
    }
    return false;
  }
  return true;
}

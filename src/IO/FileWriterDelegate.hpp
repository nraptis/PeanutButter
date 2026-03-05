#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

class FileWriterDelegate {
public:
  virtual ~FileWriterDelegate() = default;

  virtual bool createDirectories(const std::filesystem::path& pPath, std::string* pError) = 0;
  virtual bool clearDirectory(const std::filesystem::path& pPath, std::string* pError) = 0;

  virtual bool writeAt(const std::filesystem::path& pPath,
                       std::uint64_t pOffset,
                       const unsigned char* pData,
                       std::uint64_t pByteCount,
                       std::string* pError) = 0;
  virtual bool append(const std::filesystem::path& pPath,
                      const unsigned char* pData,
                      std::uint64_t pByteCount,
                      std::string* pError) = 0;
  virtual bool truncate(const std::filesystem::path& pPath, std::uint64_t pSize, std::string* pError) = 0;
  virtual bool renameFile(const std::filesystem::path& pFrom,
                          const std::filesystem::path& pTo,
                          std::string* pError) = 0;
};

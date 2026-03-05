#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class FileReaderDelegate {
public:
  virtual ~FileReaderDelegate() = default;

  virtual bool exists(const std::filesystem::path& pPath) const = 0;
  virtual bool isDirectory(const std::filesystem::path& pPath) const = 0;
  virtual bool isRegularFile(const std::filesystem::path& pPath) const = 0;
  virtual bool fileSize(const std::filesystem::path& pPath, std::uint64_t* pOutSize, std::string* pError) const = 0;

  virtual bool listDirectory(const std::filesystem::path& pPath,
                             std::vector<std::filesystem::path>& pEntries,
                             std::string* pError) const = 0;
  virtual bool listFilesRecursive(const std::filesystem::path& pRoot,
                                  std::vector<std::filesystem::path>& pFiles,
                                  std::string* pError) const = 0;

  virtual bool readRange(const std::filesystem::path& pPath,
                         std::uint64_t pOffset,
                         std::uint64_t pByteCount,
                         std::vector<unsigned char>& pOutBytes,
                         std::string* pError) const = 0;
};


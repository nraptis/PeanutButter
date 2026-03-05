#pragma once

#include "IO/FileReaderDelegate.hpp"

class FileReaderImplementation final : public FileReaderDelegate {
public:
  bool exists(const std::filesystem::path& pPath) const override;
  bool isDirectory(const std::filesystem::path& pPath) const override;
  bool isRegularFile(const std::filesystem::path& pPath) const override;
  bool fileSize(const std::filesystem::path& pPath, std::uint64_t* pOutSize, std::string* pError) const override;

  bool listDirectory(const std::filesystem::path& pPath,
                     std::vector<std::filesystem::path>& pEntries,
                     std::string* pError) const override;
  bool listFilesRecursive(const std::filesystem::path& pRoot,
                          std::vector<std::filesystem::path>& pFiles,
                          std::string* pError) const override;

  bool readRange(const std::filesystem::path& pPath,
                 std::uint64_t pOffset,
                 std::uint64_t pByteCount,
                 std::vector<unsigned char>& pOutBytes,
                 std::string* pError) const override;
};


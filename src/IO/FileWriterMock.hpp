#pragma once

#include <memory>

#include "IO/FileWriterDelegate.hpp"
#include "IO/FileMockStore.hpp"

class FileWriterMock final : public FileWriterDelegate {
public:
  FileWriterMock();
  explicit FileWriterMock(std::shared_ptr<FileMockStore> pStore);

  std::shared_ptr<FileMockStore> store() const;

  bool createDirectories(const std::filesystem::path& pPath, std::string* pError) override;
  bool clearDirectory(const std::filesystem::path& pPath, std::string* pError) override;
  bool writeAt(const std::filesystem::path& pPath,
               std::uint64_t pOffset,
               const unsigned char* pData,
               std::uint64_t pByteCount,
               std::string* pError) override;
  bool append(const std::filesystem::path& pPath,
              const unsigned char* pData,
              std::uint64_t pByteCount,
              std::string* pError) override;
  bool truncate(const std::filesystem::path& pPath, std::uint64_t pSize, std::string* pError) override;
  bool renameFile(const std::filesystem::path& pFrom,
                  const std::filesystem::path& pTo,
                  std::string* pError) override;

private:
  std::shared_ptr<FileMockStore> mStore;
};

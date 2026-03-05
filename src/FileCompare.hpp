#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace FileCompare {

using LogStream = std::function<void(const std::string&)>;

bool Test(const LogStream& pLogStream,
          const std::filesystem::path& pSource,
          const std::filesystem::path& pDestination,
          std::string* pError = nullptr);
}

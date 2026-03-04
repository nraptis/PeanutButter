#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct AppConfig {
  std::string mDefaultSourcePath = "input";
  std::string mDefaultArchivePath = "archive";
  std::string mDefaultUnarchivePath = "unzipped";
  std::string mDefaultPassword1 = "banana";
  std::string mDefaultPassword2 = "apple";
  std::uint64_t mDefaultArchiveSize = 1000000ULL;
};

AppConfig loadConfig(const std::filesystem::path& pConfigPath);

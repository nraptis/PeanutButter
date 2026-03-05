#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "Globals.hpp"

struct AppConfig {
  std::string mDefaultSourcePath = "input";
  std::string mDefaultArchivePath = "archive";
  std::string mDefaultUnarchivePath = "unzipped";
  std::string mDefaultRecoveryPath = "folder.archive_0047.jag";
  std::string mDefaultPassword1 = "banana";
  std::string mDefaultPassword2 = "apple";
  std::string mDefaultFilePrefix = "snowstorm_";
  std::string mDefaultFileSuffix = ".jag";
  std::uint64_t mDefaultArchiveSize = BLOCK_SIZE_LAYER_3;
};

AppConfig loadConfig(const std::filesystem::path& pConfigPath);

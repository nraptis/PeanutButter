#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

struct FileMockStore {
  std::map<std::string, std::vector<unsigned char>> mFiles;
  std::set<std::string> mDirectories;
};

inline std::string normalizeStorePath(const std::filesystem::path& pPath) {
  const std::filesystem::path aNormalized = pPath.lexically_normal();
  return aNormalized.generic_string();
}


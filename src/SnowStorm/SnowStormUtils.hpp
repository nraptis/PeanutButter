#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace SnowStormUtils {

namespace fs = std::filesystem;

bool shouldIgnoreEntry(const fs::path& pPath);
std::string bundleName(std::uint64_t pIndex);

void writeUInt16(std::vector<char>& pOut, std::uint16_t pValue);
void writeUInt64(std::vector<char>& pOut, std::uint64_t pValue);
std::uint16_t readUInt16(const std::vector<char>& pRaw);
std::uint64_t readUInt64(const std::vector<char>& pRaw);

void appendName(std::vector<char>& pOut, const std::string& pName);
std::vector<fs::path> sortedEntries(const fs::path& pDirectory);
void collectEntries(const fs::path& pDirectory,
                    std::vector<fs::path>& pFiles,
                    std::vector<fs::path>& pDirectories);

std::string sanitizeNamePreview(const std::string& pRaw);

bool looksLikePlainName(const fs::path& pPath);
fs::path resolveOutputPath(const fs::path& pInputPath, const fs::path& pOutputSpec);

}  // namespace SnowStormUtils

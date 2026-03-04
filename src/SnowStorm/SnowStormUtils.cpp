#include "SnowStorm/SnowStormUtils.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "Globals.hpp"

namespace SnowStormUtils {

bool shouldIgnoreEntry(const fs::path& pPath) {
  const std::string aStem = pPath.stem().string();
  return !aStem.empty() && aStem[0] == '.';
}

std::string bundleName(std::uint64_t pIndex) {
  std::string aIndexText = std::to_string(pIndex);
  if (aIndexText.size() < static_cast<std::size_t>(gBundleLeadingZeros)) {
    aIndexText = std::string(static_cast<std::size_t>(gBundleLeadingZeros) - aIndexText.size(), '0') + aIndexText;
  }
  return std::string("snowstorm_") + aIndexText + ".jag";
}

void writeUInt16(std::vector<char>& pOut, std::uint16_t pValue) {
  pOut.push_back(static_cast<char>(pValue & 0xFF));
  pOut.push_back(static_cast<char>((pValue >> 8) & 0xFF));
}

void writeUInt64(std::vector<char>& pOut, std::uint64_t pValue) {
  for (int aShift = 0; aShift < 8; ++aShift) {
    pOut.push_back(static_cast<char>((pValue >> (aShift * 8)) & 0xFF));
  }
}

std::uint16_t readUInt16(const std::vector<char>& pRaw) {
  return static_cast<std::uint16_t>(static_cast<unsigned char>(pRaw[0])) |
         static_cast<std::uint16_t>(static_cast<unsigned char>(pRaw[1]) << 8);
}

std::uint64_t readUInt64(const std::vector<char>& pRaw) {
  std::uint64_t aValue = 0;
  for (int aIndex = 0; aIndex < 8; ++aIndex) {
    aValue |= static_cast<std::uint64_t>(static_cast<unsigned char>(pRaw[static_cast<std::size_t>(aIndex)])) << (aIndex * 8);
  }
  return aValue;
}

void appendName(std::vector<char>& pOut, const std::string& pName) {
  if (pName.size() > 0xFFFFu) {
    throw std::runtime_error("Entry name too long: " + pName);
  }
  writeUInt16(pOut, static_cast<std::uint16_t>(pName.size()));
  pOut.insert(pOut.end(), pName.begin(), pName.end());
}

std::vector<fs::path> sortedEntries(const fs::path& pDirectory) {
  std::vector<fs::path> aOutput;
  for (const auto& aEntry : fs::directory_iterator(pDirectory)) {
    aOutput.push_back(aEntry.path());
  }
  std::sort(aOutput.begin(), aOutput.end(), [](const fs::path& pLeft, const fs::path& pRight) {
    return pLeft.filename().string() < pRight.filename().string();
  });
  return aOutput;
}

void collectEntries(const fs::path& pDirectory,
                    std::vector<fs::path>& pFiles,
                    std::vector<fs::path>& pDirectories) {
  pFiles.clear();
  pDirectories.clear();

  for (const auto& aPath : sortedEntries(pDirectory)) {
    if (shouldIgnoreEntry(aPath)) {
      continue;
    }
    if (fs::is_regular_file(aPath)) {
      pFiles.push_back(aPath);
    } else if (fs::is_directory(aPath)) {
      pDirectories.push_back(aPath);
    } else {
      throw std::runtime_error("Unsupported filesystem entry: " + aPath.string());
    }
  }

  if (pFiles.size() > 0xFFFFu || pDirectories.size() > 0xFFFFu) {
    throw std::runtime_error("Too many entries in directory: " + pDirectory.string());
  }
}

std::string sanitizeNamePreview(const std::string& pRaw) {
  constexpr std::size_t cMaxPreview = 96;
  static constexpr char cHex[] = "0123456789ABCDEF";

  std::string aOutput;
  const std::size_t aCount = std::min<std::size_t>(pRaw.size(), cMaxPreview);
  aOutput.reserve(aCount * 4);

  for (std::size_t aIndex = 0; aIndex < aCount; ++aIndex) {
    const unsigned char aValue = static_cast<unsigned char>(pRaw[aIndex]);
    if (std::isprint(aValue) != 0) {
      aOutput.push_back(static_cast<char>(aValue));
    } else {
      aOutput.push_back('\\');
      aOutput.push_back('x');
      aOutput.push_back(cHex[(aValue >> 4) & 0x0F]);
      aOutput.push_back(cHex[aValue & 0x0F]);
    }
  }

  if (pRaw.size() > cMaxPreview) {
    aOutput += "...";
  }

  return aOutput;
}

bool looksLikePlainName(const fs::path& pPath) {
  return pPath.parent_path().empty() && pPath.filename() == pPath;
}

fs::path resolveOutputPath(const fs::path& pInputPath, const fs::path& pOutputSpec) {
  if (pOutputSpec.empty()) {
    throw std::runtime_error("Output path must not be empty");
  }

  if (looksLikePlainName(pOutputSpec)) {
    fs::path aInputPath = pInputPath;
    if (aInputPath.is_relative()) {
      aInputPath = fs::absolute(aInputPath);
    }
    aInputPath = aInputPath.lexically_normal();
    if (aInputPath.filename().empty()) {
      aInputPath = aInputPath.parent_path();
    }
    fs::path aParentPath = aInputPath.parent_path();
    if (aParentPath.empty()) {
      aParentPath = aInputPath;
    }
    return aParentPath / pOutputSpec;
  }

  if (fs::exists(pOutputSpec) && fs::is_directory(pOutputSpec)) {
    return pOutputSpec;
  }

  return pOutputSpec;
}

}  // namespace SnowStormUtils

#include "ConfigLoader.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace {

std::string findJsonStringValue(const std::string& pJsonText, const std::string& pKey, const std::string& pFallback) {
  const std::string aNeedle = std::string("\"") + pKey + "\"";
  const std::size_t aKeyPos = pJsonText.find(aNeedle);
  if (aKeyPos == std::string::npos) {
    return pFallback;
  }
  const std::size_t aColonPos = pJsonText.find(':', aKeyPos + aNeedle.size());
  if (aColonPos == std::string::npos) {
    return pFallback;
  }
  const std::size_t aFirstQuote = pJsonText.find('"', aColonPos + 1);
  if (aFirstQuote == std::string::npos) {
    return pFallback;
  }
  const std::size_t aSecondQuote = pJsonText.find('"', aFirstQuote + 1);
  if (aSecondQuote == std::string::npos) {
    return pFallback;
  }
  return pJsonText.substr(aFirstQuote + 1, aSecondQuote - (aFirstQuote + 1));
}

std::uint64_t findJsonUInt64Value(const std::string& pJsonText, const std::string& pKey, std::uint64_t pFallback) {
  const std::string aNeedle = std::string("\"") + pKey + "\"";
  const std::size_t aKeyPos = pJsonText.find(aNeedle);
  if (aKeyPos == std::string::npos) {
    return pFallback;
  }
  const std::size_t aColonPos = pJsonText.find(':', aKeyPos + aNeedle.size());
  if (aColonPos == std::string::npos) {
    return pFallback;
  }

  std::size_t aPos = aColonPos + 1;
  while (aPos < pJsonText.size() && std::isspace(static_cast<unsigned char>(pJsonText[aPos])) != 0) {
    ++aPos;
  }

  std::size_t aEnd = aPos;
  while (aEnd < pJsonText.size() && std::isdigit(static_cast<unsigned char>(pJsonText[aEnd])) != 0) {
    ++aEnd;
  }
  if (aEnd == aPos) {
    return pFallback;
  }

  try {
    return std::stoull(pJsonText.substr(aPos, aEnd - aPos));
  } catch (...) {
    return pFallback;
  }
}

}  // namespace

AppConfig loadConfig(const std::filesystem::path& pConfigPath) {
  AppConfig aConfig;

  std::ifstream aInput(pConfigPath);
  if (!aInput) {
    return aConfig;
  }

  std::ostringstream aBuffer;
  aBuffer << aInput.rdbuf();
  const std::string aJsonText = aBuffer.str();

  aConfig.mDefaultSourcePath = findJsonStringValue(aJsonText, "default_source_path", aConfig.mDefaultSourcePath);
  aConfig.mDefaultArchivePath = findJsonStringValue(aJsonText, "default_archive_path", aConfig.mDefaultArchivePath);
  aConfig.mDefaultUnarchivePath = findJsonStringValue(aJsonText, "default_unarchive_path", aConfig.mDefaultUnarchivePath);
  aConfig.mDefaultPassword1 = findJsonStringValue(aJsonText, "default_password_1", aConfig.mDefaultPassword1);
  aConfig.mDefaultPassword2 = findJsonStringValue(aJsonText, "default_password_2", aConfig.mDefaultPassword2);
  aConfig.mDefaultArchiveSize = findJsonUInt64Value(aJsonText, "default_archive_size", aConfig.mDefaultArchiveSize);

  return aConfig;
}

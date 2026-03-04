#include "ConfigLoader.hpp"

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

  return aConfig;
}

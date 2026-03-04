#include "SnowStorm/SnowStormCountResult.hpp"

#include <string>
#include <vector>

#include "SnowStorm/SnowStormUtils.hpp"

namespace fs = std::filesystem;

SnowStormCountResult countDirectory(const fs::path& pDirectory) {
  std::vector<fs::path> aFiles;
  std::vector<fs::path> aDirectories;
  SnowStormUtils::collectEntries(pDirectory, aFiles, aDirectories);

  SnowStormCountResult aResult;
  aResult.mFileCount = aFiles.size();
  aResult.mFolderCount = aDirectories.size();
  aResult.mPayloadBytes += 2;

  for (const auto& aFile : aFiles) {
    const std::string aName = aFile.filename().string();
    aResult.mPayloadBytes += 2 + aName.size() + 8 + fs::file_size(aFile);
  }

  aResult.mPayloadBytes += 2;
  for (const auto& aDirectory : aDirectories) {
    const std::string aName = aDirectory.filename().string();
    const SnowStormCountResult aChild = countDirectory(aDirectory);
    aResult.mFileCount += aChild.mFileCount;
    aResult.mFolderCount += aChild.mFolderCount;
    aResult.mPayloadBytes += 2 + aName.size() + aChild.mPayloadBytes;
  }

  return aResult;
}

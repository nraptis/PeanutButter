#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ConfigLoader.hpp"
#include "Globals.hpp"
#include "IO/FileReaderImplementation.hpp"
#include "PeanutButterDelegate.hpp"
#include "PeanutButterImplementation.hpp"

namespace fs = std::filesystem;

namespace {

#if !defined(NDEBUG)
inline constexpr bool kInferRelativePathsFromCwd = true;
#else
inline constexpr bool kInferRelativePathsFromCwd = false;
#endif

fs::path inferredBaseDirectory(const char* pArgv0) {
  if (kInferRelativePathsFromCwd) {
    return fs::current_path();
  }
  if (pArgv0 != nullptr && *pArgv0 != '\0') {
    fs::path aExecPath = fs::absolute(fs::path(pArgv0));
    if (aExecPath.has_parent_path()) {
      return aExecPath.parent_path();
    }
  }
  return fs::current_path();
}

fs::path resolveUserPath(const fs::path& pInput, const char* pArgv0) {
  if (pInput.empty() || pInput.is_absolute()) {
    return pInput;
  }
  return (inferredBaseDirectory(pArgv0) / pInput).lexically_normal();
}

fs::path resolveConfigPath(const char* pArgv0) {
  const fs::path aBase = inferredBaseDirectory(pArgv0);
  const fs::path aPrimary = aBase / "config.json";
  if (fs::exists(aPrimary)) {
    return aPrimary;
  }
  const fs::path aFallback = aBase / "PeanutButter" / "config.json";
  if (fs::exists(aFallback)) {
    return aFallback;
  }
  return aPrimary;
}

void printUsage() {
  std::cout << "Usage:\n";
  std::cout << "  peanutbutter_runner pack <source> <archive>\n";
  std::cout << "  peanutbutter_runner unpack <archive> <unzipped>\n";
  std::cout << "  peanutbutter_runner recover <recovery_start_file> <unzipped>\n";
}

bool isAvailableArchiveSize(std::uint64_t pBytes) {
  for (const ArchiveSizeOption& aOption : archive_size_options) {
    if (aOption.mBytes == pBytes) {
      return true;
    }
  }
  return false;
}

std::uint64_t fallbackArchiveSize() {
  const std::uint64_t aTarget = BLOCK_SIZE_LAYER_3 * 200ULL;
  for (const ArchiveSizeOption& aOption : archive_size_options) {
    if (aOption.mBytes == aTarget) {
      return aOption.mBytes;
    }
  }
  if (!archive_size_options.empty()) {
    return archive_size_options.front().mBytes;
  }
  return BLOCK_SIZE_LAYER_3;
}

class ConsoleDelegate final : public PeanutButterDelegate {
public:
  void logMessage(const std::string& pMessage) override {
    std::cout << pMessage << "\n";
  }

  bool showOkCancelDialog(const std::string& pTitle, const std::string& pMessage) override {
    std::cout << "[" << pTitle << "] " << pMessage << "\n";
    std::cout << "Continue? [y/N]: ";
    std::string aLine;
    std::getline(std::cin, aLine);
    return !aLine.empty() && (aLine[0] == 'y' || aLine[0] == 'Y');
  }
};

}  // namespace

int main(int argc, char** argv) {
  try {
    if ((gArchiveSize % BLOCK_SIZE_LAYER_3) != 0) {
      throw std::runtime_error("FATAL: gArchiveSize must be an exact multiple of BLOCK_SIZE_LAYER_3.");
    }

    const AppConfig aConfig = loadConfig(resolveConfigPath((argc > 0) ? argv[0] : nullptr));
    gBundleFilePrefix = aConfig.mDefaultFilePrefix;
    gBundleFileSuffix = aConfig.mDefaultFileSuffix;
    std::uint64_t aArchiveSize = aConfig.mDefaultArchiveSize;
    if (!isAvailableArchiveSize(aArchiveSize)) {
      const std::uint64_t aFallback = fallbackArchiveSize();
      std::cout << "Config event: default_archive_size=" << aArchiveSize
                << " is not in available sizes. Falling back to " << aFallback << ".\n";
      aArchiveSize = aFallback;
    }
    if ((aArchiveSize % BLOCK_SIZE_LAYER_3) != 0) {
      throw std::runtime_error("FATAL: configured archive size must be an exact multiple of BLOCK_SIZE_LAYER_3.");
    }

    ConsoleDelegate aDelegate;
    PeanutButterImplementation aImplementation(aDelegate, aArchiveSize);

    if (argc < 2) {
      printUsage();
      return 0;
    }

    const std::string aMode = argv[1];
    if (aMode == "pack") {
      const fs::path aSource = resolveUserPath((argc >= 3) ? fs::path(argv[2]) : fs::path(aConfig.mDefaultSourcePath),
                                               (argc > 0) ? argv[0] : nullptr);
      const fs::path aArchive = resolveUserPath((argc >= 4) ? fs::path(argv[3]) : fs::path(aConfig.mDefaultArchivePath),
                                                (argc > 0) ? argv[0] : nullptr);
      const SnowStormBundleStats aStats = aImplementation.pack(aSource, aArchive);
      std::cout << "Packed " << aStats.mFileCount << " files into " << aStats.mArchiveCount << " archives.\n";
      return 0;
    }

    if (aMode == "unpack") {
      const fs::path aArchive = resolveUserPath((argc >= 3) ? fs::path(argv[2]) : fs::path(aConfig.mDefaultArchivePath),
                                                (argc > 0) ? argv[0] : nullptr);
      const fs::path aUnzipped = resolveUserPath((argc >= 4) ? fs::path(argv[3]) : fs::path(aConfig.mDefaultUnarchivePath),
                                                 (argc > 0) ? argv[0] : nullptr);
      const SnowStormUnbundleStats aStats = aImplementation.unpack(aArchive, aUnzipped);
      std::cout << "Unpacked " << aStats.mFilesUnbundled << " files from " << aStats.mArchivesTouched << " archives.\n";
      return 0;
    }

    if (aMode == "recover") {
      const fs::path aRecoveryStartFile =
          resolveUserPath((argc >= 3) ? fs::path(argv[2]) : fs::path(aConfig.mDefaultRecoveryPath),
                          (argc > 0) ? argv[0] : nullptr);
      const fs::path aUnzipped =
          resolveUserPath((argc >= 4) ? fs::path(argv[3]) : fs::path(aConfig.mDefaultUnarchivePath),
                          (argc > 0) ? argv[0] : nullptr);
      FileReaderImplementation aReader;
      const PeanutButterImplementation::RecoveryStart aStart = PeanutButterImplementation::resolveRecoveryStartFile(
          aRecoveryStartFile, gBundleFilePrefix, gBundleFileSuffix, aReader);
      const SnowStormUnbundleStats aStats = aImplementation.recover(aStart, aUnzipped);
      std::cout << "Recovered " << aStats.mFilesUnbundled << " files from " << aStats.mArchivesTouched << " archives.\n";
      return 0;
    }

    printUsage();
    return 1;
  } catch (const std::exception& aError) {
    std::cerr << "Fatal: " << aError.what() << "\n";
    return 1;
  }
}

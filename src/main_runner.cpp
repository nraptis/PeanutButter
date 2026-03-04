#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ConfigLoader.hpp"
#include "Globals.hpp"
#include "SandStorm/SandStorm.hpp"
#include "SnowStorm/SnowStorm.hpp"

namespace fs = std::filesystem;

namespace {

#if !defined(NDEBUG)
inline constexpr bool kInferRelativePathsFromCwd = true;
#else
inline constexpr bool kInferRelativePathsFromCwd = false;
#endif

std::u32string toU32(const std::string& pText) {
  std::u32string aOutput;
  aOutput.reserve(pText.size());
  for (const unsigned char aChar : pText) {
    aOutput.push_back(static_cast<char32_t>(aChar));
  }
  return aOutput;
}

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
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (gBlockSize != kPermanentBlockSize) {
      throw std::runtime_error("FATAL: block size is permanently fixed at 500,000 bytes.");
    }
    if ((gArchiveSize % kPermanentBlockSize) != 0) {
      throw std::runtime_error("FATAL: gArchiveSize must be an exact multiple of 500,000 bytes.");
    }

    const AppConfig aConfig = loadConfig(resolveConfigPath((argc > 0) ? argv[0] : nullptr));

    SandStorm aCrypt(gBlockSize,
                     toU32(aConfig.mDefaultPassword1),
                     toU32(aConfig.mDefaultPassword2));
    SnowStorm aSnowStorm(gBlockSize, gArchiveSize, &aCrypt);

    if (argc < 2) {
      printUsage();
      return 0;
    }

    const std::string aMode = argv[1];
    auto aProgress = [](std::uint64_t pArchiveIndex,
                        std::uint64_t pArchiveTotal,
                        std::uint64_t pFilesDone,
                        std::uint64_t pFilesTotal) {
      std::cout << "Progress " << pArchiveIndex << "/" << pArchiveTotal
                << " archives, " << pFilesDone << "/" << pFilesTotal << " files\n";
    };

    if (aMode == "pack") {
      const fs::path aSource = resolveUserPath((argc >= 3) ? fs::path(argv[2]) : fs::path(aConfig.mDefaultSourcePath),
                                               (argc > 0) ? argv[0] : nullptr);
      const fs::path aArchive = resolveUserPath((argc >= 4) ? fs::path(argv[3]) : fs::path(aConfig.mDefaultArchivePath),
                                                (argc > 0) ? argv[0] : nullptr);
      const ShouldBundleResult aGate = aSnowStorm.shouldBundle(aSource, aArchive);
      if (aGate.mDecision != ShouldBundleDecision::Yes) {
        throw std::runtime_error("Cannot pack: " + aGate.mMessage);
      }
      const BundleStats aStats = aSnowStorm.bundle(aSource, aArchive, aProgress);
      std::cout << "Packed " << aStats.mFileCount << " files into " << aStats.mArchiveCount << " archives.\n";
      return 0;
    }

    if (aMode == "unpack") {
      const fs::path aArchive = resolveUserPath((argc >= 3) ? fs::path(argv[2]) : fs::path(aConfig.mDefaultArchivePath),
                                                (argc > 0) ? argv[0] : nullptr);
      const fs::path aUnzipped = resolveUserPath((argc >= 4) ? fs::path(argv[3]) : fs::path(aConfig.mDefaultUnarchivePath),
                                                 (argc > 0) ? argv[0] : nullptr);
      const ShouldBundleResult aGate = aSnowStorm.shouldUnbundle(aArchive, aUnzipped);
      if (aGate.mDecision != ShouldBundleDecision::Yes) {
        throw std::runtime_error("Cannot unpack: " + aGate.mMessage);
      }
      const UnbundleStats aStats = aSnowStorm.unbundle(aArchive, aUnzipped, aProgress);
      std::cout << "Unpacked " << aStats.mFilesUnbundled << " files from " << aStats.mArchivesTouched << " archives.\n";
      return 0;
    }

    printUsage();
    return 1;
  } catch (const std::exception& aError) {
    std::cerr << "Fatal: " << aError.what() << "\n";
    return 1;
  }
}

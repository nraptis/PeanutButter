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

fs::path resolveConfigPath() {
  const fs::path aPrimary = fs::current_path() / "config.json";
  if (fs::exists(aPrimary)) {
    return aPrimary;
  }
  const fs::path aFallback = fs::current_path() / "PeanutButter" / "config.json";
  if (fs::exists(aFallback)) {
    return aFallback;
  }
  return aPrimary;
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

void printUsage() {
  std::cout << "Usage:\n";
  std::cout << "  recovery_runner <recovery_start_file> <unarchive_folder>\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 3) {
      printUsage();
      return 1;
    }
    const AppConfig aConfig = loadConfig(resolveConfigPath());
    gBundleFilePrefix = aConfig.mDefaultFilePrefix;
    gBundleFileSuffix = aConfig.mDefaultFileSuffix;
    ConsoleDelegate aDelegate;
    PeanutButterImplementation aImplementation(aDelegate, aConfig.mDefaultArchiveSize);
    FileReaderImplementation aReader;
    const PeanutButterImplementation::RecoveryStart aStart = PeanutButterImplementation::resolveRecoveryStartFile(
        fs::path(argv[1]), gBundleFilePrefix, gBundleFileSuffix, aReader);
    const SnowStormUnbundleStats aStats = aImplementation.recover(aStart, fs::path(argv[2]));
    std::cout << "Recovered " << aStats.mFilesUnbundled << " files from " << aStats.mArchivesTouched << " archives.\n";
    return 0;
  } catch (const std::exception& aError) {
    std::cerr << "Fatal: " << aError.what() << "\n";
    return 1;
  }
}

#include "PeanutButterImplementation.hpp"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "Globals.hpp"
#include "IO/FileReaderImplementation.hpp"
#include "IO/FileWriterImplementation.hpp"
#include "SnowStorm/SnowStormEngine.hpp"

namespace fs = std::filesystem;

namespace {

bool canonicalContains(const fs::path& pParent, const fs::path& pChild) {
  std::error_code aParentEc;
  std::error_code aChildEc;
  const fs::path aCanonicalParent = fs::weakly_canonical(pParent, aParentEc);
  const fs::path aCanonicalChild = fs::weakly_canonical(pChild, aChildEc);
  if (aParentEc || aChildEc) {
    return false;
  }
  auto aParentIt = aCanonicalParent.begin();
  auto aChildIt = aCanonicalChild.begin();
  for (; aParentIt != aCanonicalParent.end() && aChildIt != aCanonicalChild.end(); ++aParentIt, ++aChildIt) {
    if (*aParentIt != *aChildIt) {
      return false;
    }
  }
  return aParentIt == aCanonicalParent.end();
}

std::uint64_t parseArchiveIndexFromName(const std::string& pName,
                                        const std::string& pPrefix,
                                        const std::string& pSuffix) {
  if (pPrefix.empty() || pSuffix.empty()) {
    throw std::runtime_error("file_prefix and file_suffix must not be empty.");
  }
  if (pName.size() <= pPrefix.size() + pSuffix.size()) {
    throw std::runtime_error("Recovery file does not match prefix/suffix.");
  }
  if (pName.rfind(pPrefix, 0) != 0 || pName.substr(pName.size() - pSuffix.size()) != pSuffix) {
    throw std::runtime_error("Recovery file does not match file_prefix/file_suffix.");
  }
  std::string aDigits = pName.substr(pPrefix.size(), pName.size() - pPrefix.size() - pSuffix.size());
  if (!aDigits.empty() && (aDigits.back() == 'R' || aDigits.back() == 'N')) {
    aDigits.pop_back();
  }
  if (aDigits.empty()) {
    throw std::runtime_error("Recovery file does not contain archive index digits.");
  }
  for (char aChar : aDigits) {
    if (std::isdigit(static_cast<unsigned char>(aChar)) == 0) {
      throw std::runtime_error("Recovery file contains non-digit archive index.");
    }
  }
  return std::stoull(aDigits);
}

std::uint64_t readRecoveryStartOffset(const fs::path& pFile, const FileReaderDelegate& pReader) {
  constexpr std::uint16_t kPlainHeaderFlagHasRecoveryStart = 0x0001U;
  constexpr std::size_t kHeaderSize = PLAIN_TEXT_HEADER_LENGTH;
  std::vector<unsigned char> aHeader;
  std::string aReadError;
  if (!pReader.readRange(pFile, 0, kHeaderSize, aHeader, &aReadError)) {
    throw std::runtime_error(aReadError.empty() ? ("Cannot open recovery archive: " + pFile.string()) : aReadError);
  }
  if (aHeader.size() != kHeaderSize) {
    throw std::runtime_error("Recovery archive header is truncated.");
  }
  const std::uint32_t aMagic =
      static_cast<std::uint32_t>(aHeader[0]) |
      (static_cast<std::uint32_t>(aHeader[1]) << 8) |
      (static_cast<std::uint32_t>(aHeader[2]) << 16) |
      (static_cast<std::uint32_t>(aHeader[3]) << 24);
  if (aMagic != PLAIN_TEXT_HEADER_MAGIC_SIGNATURE_U32) {
    throw std::runtime_error("Recovery archive header is missing.");
  }
  const std::uint16_t aFlags =
      static_cast<std::uint16_t>(aHeader[6]) |
      static_cast<std::uint16_t>(static_cast<std::uint16_t>(aHeader[7]) << 8);
  const bool aHasRecoveryStart = (aFlags & kPlainHeaderFlagHasRecoveryStart) != 0U;
  std::uint64_t aOffset = 0;
  for (int aShift = 0; aShift < 8; ++aShift) {
    aOffset |= static_cast<std::uint64_t>(aHeader[8 + static_cast<std::size_t>(aShift)]) << (8 * aShift);
  }

  std::uint64_t aFileSize = 0;
  std::string aSizeError;
  if (!pReader.fileSize(pFile, &aFileSize, &aSizeError) || aFileSize <= kHeaderSize) {
    throw std::runtime_error("Recovery archive file size is invalid.");
  }
  if (!aHasRecoveryStart) {
    if (aOffset == 0 || aOffset >= (aFileSize - kHeaderSize)) {
      throw std::runtime_error("No file starts are found in this archive.");
    }
  } else if (aOffset >= (aFileSize - kHeaderSize)) {
    throw std::runtime_error("No file starts are found in this archive.");
  }
  return aOffset;
}

}  // namespace

PeanutButterImplementation::PeanutButterImplementation(PeanutButterDelegate& pDelegate, std::uint64_t pArchiveSize)
    : mDelegate(pDelegate), mArchiveSize(pArchiveSize) {
  static FileReaderImplementation sReader;
  static FileWriterImplementation sWriter;
  mReader = &sReader;
  mWriter = &sWriter;
}

PeanutButterImplementation::PeanutButterImplementation(PeanutButterDelegate& pDelegate,
                                                       std::uint64_t pArchiveSize,
                                                       FileReaderDelegate& pReader,
                                                       FileWriterDelegate& pWriter)
    : mDelegate(pDelegate),
      mArchiveSize(pArchiveSize),
      mReader(&pReader),
      mWriter(&pWriter) {}

PeanutButterImplementation::RecoveryStart PeanutButterImplementation::resolveRecoveryStartFile(
    const fs::path& pRecoveryStartFile,
    const std::string& pFilePrefix,
    const std::string& pFileSuffix,
    const FileReaderDelegate& pReader) {
  if (!pReader.exists(pRecoveryStartFile) || !pReader.isRegularFile(pRecoveryStartFile)) {
    throw std::runtime_error("Recover start file is missing: " + pRecoveryStartFile.string());
  }
  RecoveryStart aStart;
  aStart.mArchiveDirectory = pRecoveryStartFile.parent_path();
  aStart.mArchiveIndex = parseArchiveIndexFromName(pRecoveryStartFile.filename().string(), pFilePrefix, pFileSuffix);
  // Special-case archive 0000: always fall back to normal unpack start.
  if (aStart.mArchiveIndex == 0) {
    aStart.mByteOffsetInArchive = 0;
    return aStart;
  }

  try {
    aStart.mByteOffsetInArchive = readRecoveryStartOffset(pRecoveryStartFile, pReader);
  } catch (const std::exception& aError) {
    const std::string aMessage = aError.what();
    if (aMessage.find("Recovery archive header is missing.") != std::string::npos) {
      std::cerr << "Recover warning: bad start magic for '" << pRecoveryStartFile.string()
                << "'. Falling back to offset 0.\n";
      aStart.mByteOffsetInArchive = 0;
    } else {
      throw;
    }
  }
  return aStart;
}

void PeanutButterImplementation::clearDestinationIfNeeded(const fs::path& pInput,
                                                          const ShouldBundleResult& pShould,
                                                          const char* pPromptMessage) {
  if (pShould.mDecision != ShouldBundleDecision::Prompt) {
    return;
  }
  const std::string aPrompt = pShould.mMessage + "\n\n" + pPromptMessage;
  if (!mDelegate.showOkCancelDialog("snowstorm", aPrompt)) {
    throw std::runtime_error("Operation canceled by user.");
  }
  if (canonicalContains(pShould.mResolvedDestination, pInput)) {
    throw std::runtime_error("Refusing to clear destination because it contains the source path: " +
                             pShould.mResolvedDestination.string());
  }
  std::string aClearError;
  if (!mWriter->clearDirectory(pShould.mResolvedDestination, &aClearError)) {
    throw std::runtime_error(aClearError.empty() ? "Failed to clear destination directory." : aClearError);
  }
  mDelegate.logMessage("Cleared destination directory: " + pShould.mResolvedDestination.string());
}

SnowStormBundleStats PeanutButterImplementation::pack(const fs::path& pSource, const fs::path& pArchive) {
  if ((mArchiveSize % BLOCK_SIZE_LAYER_3) != 0) {
    throw std::runtime_error("Archive size must be an exact multiple of BLOCK_SIZE_LAYER_3.");
  }
  SnowStormEngine aEngine(mArchiveSize, *mReader, *mWriter);
  const ShouldBundleResult aShould = aEngine.shouldBundle(pSource, pArchive);
  if (aShould.mDecision == ShouldBundleDecision::No) {
    throw std::runtime_error(aShould.mMessage);
  }
  clearDestinationIfNeeded(pSource, aShould, "Press OK to clear destination and continue.");
  mDelegate.logMessage("Pack started.");
  mDelegate.logMessage("BLOCK SIZE: using BLOCK_SIZE_LAYER_3 = " + std::to_string(BLOCK_SIZE_LAYER_3) + " bytes.");
  return aEngine.bundle(
      pSource,
      pArchive,
      [&](std::uint64_t pArchiveIdx, std::uint64_t pArchiveTotal, std::uint64_t pFilesDone, std::uint64_t pFilesTotal) {
        mDelegate.logMessage("Packing " + std::to_string(pArchiveIdx) + " of " + std::to_string(pArchiveTotal) +
                             " Archives, " + std::to_string(pFilesDone) + " of " + std::to_string(pFilesTotal) + " Files");
      });
}

SnowStormUnbundleStats PeanutButterImplementation::runUnbundle(const fs::path& pArchive,
                                                               const fs::path& pUnarchive,
                                                               std::uint64_t pStartArchiveIndex,
                                                               std::uint64_t pStartByteOffsetInArchive,
                                                               const char* pVerbProgress,
                                                               const char* pVerbSummary) {
  if ((mArchiveSize % BLOCK_SIZE_LAYER_3) != 0) {
    throw std::runtime_error("Archive size must be an exact multiple of BLOCK_SIZE_LAYER_3.");
  }
  SnowStormEngine aEngine(mArchiveSize, *mReader, *mWriter);
  const ShouldBundleResult aShould = aEngine.shouldUnbundle(pArchive, pUnarchive);
  if (aShould.mDecision == ShouldBundleDecision::No) {
    throw std::runtime_error(aShould.mMessage);
  }
  clearDestinationIfNeeded(pArchive, aShould, "Press OK to clear destination and continue.");
  mDelegate.logMessage(std::string(pVerbSummary) + " started.");
  mDelegate.logMessage("BLOCK SIZE: using BLOCK_SIZE_LAYER_3 = " + std::to_string(BLOCK_SIZE_LAYER_3) + " bytes.");
  return aEngine.unbundle(
      pArchive,
      pUnarchive,
      [&](std::uint64_t pArchiveIdx, std::uint64_t pArchiveTotal, std::uint64_t pFilesDone, std::uint64_t) {
        mDelegate.logMessage(std::string(pVerbProgress) + " " + std::to_string(pArchiveIdx) + " of " +
                             std::to_string(pArchiveTotal) + " Archives, " + std::to_string(pFilesDone) + " Files");
      },
      pStartArchiveIndex,
      pStartByteOffsetInArchive);
}

SnowStormUnbundleStats PeanutButterImplementation::unpack(const fs::path& pArchive, const fs::path& pUnarchive) {
  return runUnbundle(pArchive, pUnarchive, 0, 0, "Unpacking", "Unpack");
}

SnowStormUnbundleStats PeanutButterImplementation::recover(const RecoveryStart& pRecoveryStart,
                                                           const fs::path& pUnarchive) {
  return runUnbundle(pRecoveryStart.mArchiveDirectory,
                     pUnarchive,
                     pRecoveryStart.mArchiveIndex,
                     pRecoveryStart.mByteOffsetInArchive,
                     "Recovering",
                     "Recover");
}

bool PeanutButterImplementation::sanity(const fs::path& pSource,
                                        const fs::path& pDestination,
                                        std::string* pError) {
  mDelegate.logMessage("Sanity started.");
  return FileCompare::Test(
      [&](const std::string& pLine) { mDelegate.logMessage(pLine); },
      pSource,
      pDestination,
      pError);
}

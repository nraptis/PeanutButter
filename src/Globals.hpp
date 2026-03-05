#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#define RECOVERY_CHUNK_SIZE (6ULL)

#define BLOCK_SIZE_LAYER_1 (250000ULL + RECOVERY_CHUNK_SIZE)
#define BLOCK_SIZE_LAYER_2 (BLOCK_SIZE_LAYER_1 + BLOCK_SIZE_LAYER_1)
#define BLOCK_SIZE_LAYER_3 (BLOCK_SIZE_LAYER_2 + BLOCK_SIZE_LAYER_2)
#define PLAIN_TEXT_HEADER_LENGTH (16U)
#define PLAIN_TEXT_HEADER_MAGIC_SIGNATURE_U32 (0x48464250U)  // "PBFH" in little-endian
#define PLAIN_TEXT_HEADER_MAGIC_SIGNATURE_BYTES {0x50U, 0x42U, 0x46U, 0x48U}
#define PLAIN_TEXT_HEADER_VERSION (1U)

constexpr std::size_t MAX_STORABLE_PATH_LENGTH = 2048;

struct ArchiveSizeOption {
  const char* mLabel = "";
  std::uint64_t mBytes = 0;
};

extern std::uint64_t gArchiveSize;
extern int gBundleLeadingZeros;
extern std::string gBundleFilePrefix;
extern std::string gBundleFileSuffix;
extern const std::vector<ArchiveSizeOption> archive_size_options;

extern unsigned char gTempL3BufferA[BLOCK_SIZE_LAYER_3];
extern unsigned char gTempL3BufferB[BLOCK_SIZE_LAYER_3];
extern unsigned char gTempL3BufferC[BLOCK_SIZE_LAYER_3];

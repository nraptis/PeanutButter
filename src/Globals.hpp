#pragma once

#include <cstdint>
#include <vector>

class EncryptionLayer;

#if defined(PEANUTBUTTER_UNIT_TEST_MODE)
#define PEANUTBUTTER_TEST_SCALE_DIVISOR (10000ULL)
#else
#define PEANUTBUTTER_TEST_SCALE_DIVISOR (1ULL)
#endif

#define BLOCK_SIZE_LAYER_1 (250000ULL / PEANUTBUTTER_TEST_SCALE_DIVISOR)
#define BLOCK_SIZE_LAYER_2 (500000ULL / PEANUTBUTTER_TEST_SCALE_DIVISOR)
#define BLOCK_SIZE_LAYER_3 (1000000ULL / PEANUTBUTTER_TEST_SCALE_DIVISOR)

struct ArchiveSizeOption {
  const char* mLabel = "";
  std::uint64_t mBytes = 0;
};

extern std::uint64_t gBlockSize;
extern std::uint64_t gArchiveSize;
extern int gBundleLeadingZeros;
extern const std::vector<ArchiveSizeOption> archive_size_options;

extern unsigned char gTempL3BufferA[BLOCK_SIZE_LAYER_3];
extern unsigned char gTempL3BufferB[BLOCK_SIZE_LAYER_3];
extern unsigned char gTempL3BufferC[BLOCK_SIZE_LAYER_3];

extern EncryptionLayer* gEncryptionLayer1;
extern EncryptionLayer* gEncryptionLayer2;
extern EncryptionLayer* gEncryptionLayer3;

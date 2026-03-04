#include "Globals.hpp"

#include "SandStorm/EncryptionLayer.hpp"

#define PB_SCALE_VALUE(V) ((V) / PEANUTBUTTER_TEST_SCALE_DIVISOR)

std::uint64_t gBlockSize = BLOCK_SIZE_LAYER_3;
std::uint64_t gArchiveSize = PB_SCALE_VALUE(1000000000ULL);

int gBundleLeadingZeros = 4;

const std::vector<ArchiveSizeOption> archive_size_options = {
    {"1 MB (1,000,000)", PB_SCALE_VALUE(1000000ULL)},
    {"10 MB (10,000,000)", PB_SCALE_VALUE(10000000ULL)},
    {"50 MB (50,000,000)", PB_SCALE_VALUE(50000000ULL)},
    {"100 MB (100,000,000)", PB_SCALE_VALUE(100000000ULL)},
    {"200 MB (200,000,000)", PB_SCALE_VALUE(200000000ULL)},
    {"250 MB (250,000,000)", PB_SCALE_VALUE(250000000ULL)},
    {"500 MB (500,000,000)", PB_SCALE_VALUE(500000000ULL)},
    {"750 MB (750,000,000)", PB_SCALE_VALUE(750000000ULL)},
    {"1 GB (1,000,000,000)", PB_SCALE_VALUE(1000000000ULL)},
    {"2 GB (2,000,000,000)", PB_SCALE_VALUE(2000000000ULL)},
};

unsigned char gTempL3BufferA[BLOCK_SIZE_LAYER_3] = {};
unsigned char gTempL3BufferB[BLOCK_SIZE_LAYER_3] = {};
unsigned char gTempL3BufferC[BLOCK_SIZE_LAYER_3] = {};

EncryptionLayer* gEncryptionLayer1 = nullptr;
EncryptionLayer* gEncryptionLayer2 = nullptr;
EncryptionLayer* gEncryptionLayer3 = nullptr;

#undef PB_SCALE_VALUE

// Release mode:
// cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
// cmake --build build-release --target peanutbutter -j4

// Fixing QT:
// cmake -S /Users/naraptis/Desktop/PeanutButter -B /Users/naraptis/Desktop/PeanutButter/build-local \
//   -DCMAKE_PREFIX_PATH="$(qtpaths6 --query QT_INSTALL_PREFIX)"
// cmake --build /Users/naraptis/Desktop/PeanutButter/build-local -j4

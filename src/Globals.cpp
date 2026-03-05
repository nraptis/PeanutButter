#include "Globals.hpp"
std::uint64_t gArchiveSize = BLOCK_SIZE_LAYER_3 * 1000ULL;

int gBundleLeadingZeros = 5;
std::string gBundleFilePrefix = "peanut_butter_";
std::string gBundleFileSuffix = ".pbss";

const std::vector<ArchiveSizeOption> archive_size_options = {
    {"1 MB (~1 block)", BLOCK_SIZE_LAYER_3 * 1ULL},
    {"5 MB (~5 blocks)", BLOCK_SIZE_LAYER_3 * 5ULL},
    {"10 MB (~10 blocks)", BLOCK_SIZE_LAYER_3 * 10ULL},
    {"50 MB (~50 blocks)", BLOCK_SIZE_LAYER_3 * 50ULL},
    {"100 MB (~100 blocks)", BLOCK_SIZE_LAYER_3 * 100ULL},
    {"200 MB (~200 blocks)", BLOCK_SIZE_LAYER_3 * 200ULL},
    {"250 MB (~250 blocks)", BLOCK_SIZE_LAYER_3 * 250ULL},
    {"500 MB (~500 blocks)", BLOCK_SIZE_LAYER_3 * 500ULL},
    {"750 MB (~750 blocks)", BLOCK_SIZE_LAYER_3 * 750ULL},
    {"1 GB (~1000 blocks)", BLOCK_SIZE_LAYER_3 * 1000ULL},
    {"2 GB (~2000 blocks)", BLOCK_SIZE_LAYER_3 * 2000ULL},
};

unsigned char gTempL3BufferA[BLOCK_SIZE_LAYER_3] = {};
unsigned char gTempL3BufferB[BLOCK_SIZE_LAYER_3] = {};
unsigned char gTempL3BufferC[BLOCK_SIZE_LAYER_3] = {};

// Release mode:
// cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
// cmake --build build-release --target peanutbutter -j4

// Fixing QT:
// cmake -S /Users/naraptis/Desktop/PeanutButter -B /Users/naraptis/Desktop/PeanutButter/build-local \
//   -DCMAKE_PREFIX_PATH="$(qtpaths6 --query QT_INSTALL_PREFIX)"
// cmake --build /Users/naraptis/Desktop/PeanutButter/build-local -j4

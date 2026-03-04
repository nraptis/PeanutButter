#include "Globals.hpp"

std::uint64_t gBlockSize = 262144ULL;
std::uint64_t gArchiveSize = 1073741824ULL;

//std::uint64_t gBlockSize = 8ULL;
//std::uint64_t gArchiveSize = 16LL;

int gBundleLeadingZeros = 4;


// cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
// cmake --build build-release --target peanutbutter -j4

#include "Globals.hpp"

// !!! EXTREMELY IMPORTANT !!!
// Block size is permanently locked to 500,000 bytes.
// Do not change this value. Archive sizing and validation depend on it.
std::uint64_t gBlockSize = kPermanentBlockSize;
std::uint64_t gArchiveSize = 1000000000ULL;

int gBundleLeadingZeros = 4;

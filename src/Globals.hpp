#pragma once

#include <cstdint>

// PERMANENT AND NON-NEGOTIABLE: the block size is fixed to exactly 500,000 bytes.
inline constexpr std::uint64_t kPermanentBlockSize = 500000ULL;

extern std::uint64_t gBlockSize;
extern std::uint64_t gArchiveSize;
extern int gBundleLeadingZeros;

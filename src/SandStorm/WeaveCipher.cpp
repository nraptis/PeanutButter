#include "WeaveCipher.hpp"

#include <algorithm>
#include <vector>

WeaveCipher::WeaveCipher(int pCount, int pFrontStride, int pBackStride)
    : mCount(pCount),
      mFrontStride(pFrontStride),
      mBackStride(pBackStride) {}

bool WeaveCipher::process(const unsigned char* pSource,
                          unsigned char* pDestination,
                          std::uint64_t pSize,
                          std::string* pError) const {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "WeaveCipher process requires non-null source and destination buffers";
    }
    return false;
  }

  std::vector<unsigned char> aBytes(pSource, pSource + static_cast<std::size_t>(pSize));

  if (mCount <= 0) {
    if (pError != nullptr) {
      *pError = "WeaveCipher requires mCount > 0";
    }
    return false;
  }
  if (mFrontStride <= 0) {
    if (pError != nullptr) {
      *pError = "WeaveCipher requires mFrontStride > 0";
    }
    return false;
  }
  if (mBackStride <= 0) {
    if (pError != nullptr) {
      *pError = "WeaveCipher requires mBackStride > 0";
    }
    return false;
  }

  int aCount = mCount;
  int aFrontStride = mFrontStride;
  int aBackStride = mBackStride;

  if (aBytes.empty()) {
    return true;
  }

  std::size_t aFront = 0;
  std::size_t aBack = aBytes.size() - 1;

  while (true) {
    int aSwaps = aCount;
    while (aSwaps > 0 && aFront < aBack) {
      std::swap(aBytes[aFront], aBytes[aBack]);
      --aSwaps;
      ++aFront;
      --aBack;
    }
    if (aFront >= aBack) {
      break;
    }

    int aSkips = aFrontStride;
    while (aSkips > 0 && aFront < aBack) {
      --aSkips;
      ++aFront;
    }
    if (aFront >= aBack) {
      break;
    }

    aSkips = aBackStride;
    while (aSkips > 0 && aFront < aBack) {
      --aSkips;
      --aBack;
    }
    if (aFront >= aBack) {
      break;
    }
  }

  for (std::size_t aIndex = 0; aIndex < aBytes.size(); ++aIndex) {
    pDestination[aIndex] = aBytes[aIndex];
  }
  return true;
}

bool WeaveCipher::encrypt(const unsigned char* pSource,
                          unsigned char* pDestination,
                          std::uint64_t pSize,
                          std::string* pError) {
  return process(pSource, pDestination, pSize, pError);
}

bool WeaveCipher::decrypt(const unsigned char* pSource,
                          unsigned char* pDestination,
                          std::uint64_t pSize,
                          std::string* pError) {
  return process(pSource, pDestination, pSize, pError);
}

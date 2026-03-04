#include "SplintCipher.hpp"

#include <cstring>

#include "Globals.hpp"

bool SplintCipher::encrypt(const unsigned char* pSource,
                           unsigned char* pDestination,
                           std::uint64_t pSize,
                           std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "SplintCipher encrypt requires non-null source and destination buffers";
    }
    return false;
  }

  const std::size_t aSize = static_cast<std::size_t>(pSize);
  if (aSize > BLOCK_SIZE_LAYER_3) {
    if (pError != nullptr) {
      *pError = "SplintCipher encrypt size exceeds L3 temp buffer size";
    }
    return false;
  }
  const std::size_t aHalf = (aSize + 1U) / 2U;

  const std::size_t aList2Size = aSize - aHalf;
  std::memcpy(gTempL3BufferA, pSource, aHalf);
  std::memcpy(gTempL3BufferB, pSource + aHalf, aList2Size);

  std::size_t aOut = 0;
  std::size_t aIndex1 = 0;
  std::size_t aIndex2 = 0;
  while (aIndex1 < aHalf && aIndex2 < aList2Size) {
    pDestination[aOut++] = gTempL3BufferA[aIndex1++];
    pDestination[aOut++] = gTempL3BufferB[aIndex2++];
  }
  while (aIndex1 < aHalf) {
    pDestination[aOut++] = gTempL3BufferA[aIndex1++];
  }
  while (aIndex2 < aList2Size) {
    pDestination[aOut++] = gTempL3BufferB[aIndex2++];
  }
  return true;
}

bool SplintCipher::decrypt(const unsigned char* pSource,
                           unsigned char* pDestination,
                           std::uint64_t pSize,
                           std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "SplintCipher decrypt requires non-null source and destination buffers";
    }
    return false;
  }

  const std::size_t aSize = static_cast<std::size_t>(pSize);
  if (aSize > BLOCK_SIZE_LAYER_3) {
    if (pError != nullptr) {
      *pError = "SplintCipher decrypt size exceeds L3 temp buffer size";
    }
    return false;
  }
  
  std::size_t aList1Size = 0;
  std::size_t aList2Size = 0;
  for (std::size_t aIndex = 0; aIndex < aSize; ++aIndex) {
    if ((aIndex & 1U) == 0U) {
      gTempL3BufferA[aList1Size++] = pSource[aIndex];
    } else {
      gTempL3BufferB[aList2Size++] = pSource[aIndex];
    }
  }

  std::memcpy(pDestination, gTempL3BufferA, aList1Size);
  std::memcpy(pDestination + aList1Size, gTempL3BufferB, aList2Size);
  return true;
}

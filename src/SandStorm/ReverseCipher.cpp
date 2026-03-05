#include "ReverseCipher.hpp"

bool ReverseCipher::encrypt(const unsigned char* pSource,
                            unsigned char* pDestination,
                            std::uint64_t pSize,
                            std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "ReverseCipher encrypt requires non-null source and destination buffers";
    }
    return false;
  }
  for (std::uint64_t aIndex = 0; aIndex < pSize; ++aIndex) {
    pDestination[aIndex] = pSource[pSize - 1 - aIndex];
  }
  return true;
}

bool ReverseCipher::decrypt(const unsigned char* pSource,
                            unsigned char* pDestination,
                            std::uint64_t pSize,
                            std::string* pError) {
  return encrypt(pSource, pDestination, pSize, pError);
}


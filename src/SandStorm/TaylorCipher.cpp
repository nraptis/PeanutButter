#include "TaylorCipher.hpp"

bool TaylorCipher::encrypt(const unsigned char* pSource,
                           unsigned char* pDestination,
                           std::uint64_t pSize,
                           std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "TaylorCipher encrypt requires non-null source and destination buffers";
    }
    return false;
  }
  for (std::uint64_t aIndex = 0; aIndex < pSize; ++aIndex) {
    const std::uint64_t aSourceIndex = pSize - 1 - aIndex;
    pDestination[aIndex] = static_cast<unsigned char>(~pSource[aSourceIndex]);
  }
  return true;
}

bool TaylorCipher::decrypt(const unsigned char* pSource,
                           unsigned char* pDestination,
                           std::uint64_t pSize,
                           std::string* pError) {
  return encrypt(pSource, pDestination, pSize, pError);
}

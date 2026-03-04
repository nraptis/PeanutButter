#include "TaylorCipher.hpp"

void TaylorCipher::transformWindow(const unsigned char* pSource,
                                   unsigned char* pDestination,
                                   std::size_t pLength) const {
  for (std::size_t aIndex = 0; aIndex < pLength; ++aIndex) {
    const std::size_t aSourceIndex = pLength - 1 - aIndex;
    pDestination[aIndex] = static_cast<unsigned char>(~pSource[aSourceIndex]);
  }
}

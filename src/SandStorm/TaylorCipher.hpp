#pragma once

#include <cstddef>

class TaylorCipher {
 public:
  TaylorCipher() = default;

  void transformWindow(const unsigned char* pSource,
                       unsigned char* pDestination,
                       std::size_t pLength) const;
};

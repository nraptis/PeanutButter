#pragma once

#include <cstdint>
#include <string>

#include "Cipher.hpp"

class WeaveCipher : public Cipher {
public:
  explicit WeaveCipher(int pCount = 1, int pFrontStride = 1, int pBackStride = 0);

  bool encrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

  bool decrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

private:
  bool process(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) const;

  int mCount = 1;
  int mFrontStride = 1;
  int mBackStride = 0;
};

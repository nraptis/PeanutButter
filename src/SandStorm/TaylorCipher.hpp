#pragma once

#include <cstdint>
#include <string>

#include "Cipher.hpp"

class TaylorCipher : public Cipher {
public:
  TaylorCipher() = default;

  bool encrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

  bool decrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;
};

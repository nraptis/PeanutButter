#pragma once

#include "Cipher.hpp"

class ReverseCipher : public Cipher {
public:
  ReverseCipher() = default;

  bool encrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

  bool decrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;
};


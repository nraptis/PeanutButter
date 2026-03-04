#pragma once

#include <cstdint>
#include <string>

#include "Cipher.hpp"
#include "TaylorCipher.hpp"

class SandStorm : public Cipher {
public:
  SandStorm(std::uint64_t pBlockSize,
            std::u32string pPassword1,
            std::u32string pPassword2);

  bool encrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

  bool decrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

private:
  std::uint64_t mBlockSize;
  std::u32string mPassword1;
  std::u32string mPassword2;
  TaylorCipher mCipher;
};

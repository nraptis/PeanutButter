#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Cipher.hpp"

class PasswordCiper : public Cipher {
public:
  explicit PasswordCiper(const unsigned char* pPassword);

  bool encrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

  bool decrypt(const unsigned char* pSource,
               unsigned char* pDestination,
               std::uint64_t pSize,
               std::string* pError) override;

private:
  std::vector<unsigned char> mPassword;
};

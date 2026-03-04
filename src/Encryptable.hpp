#pragma once

#include <cstdint>
#include <string>

class Encryptable {
 public:
  virtual ~Encryptable() = default;

  virtual bool encrypt(const unsigned char* pSource,
                       unsigned char* pDestination,
                       std::uint64_t pSize,
                       std::string* pError) = 0;

  virtual bool decrypt(const unsigned char* pSource,
                       unsigned char* pDestination,
                       std::uint64_t pSize,
                       std::string* pError) = 0;
};

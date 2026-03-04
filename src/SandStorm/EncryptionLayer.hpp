#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Cipher.hpp"

class EncryptionLayer : public Cipher {
public:
  explicit EncryptionLayer(std::uint64_t pBlockSize);
  void addCipher(std::unique_ptr<Cipher> pCipher);

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
  std::vector<Cipher*> mCipherList;
  std::vector<std::unique_ptr<Cipher>> mOwnedCiphers;
  std::vector<unsigned char> mBufferA;
  std::vector<unsigned char> mBufferB;
};

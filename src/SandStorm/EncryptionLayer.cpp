#include "EncryptionLayer.hpp"

#include <cstring>

EncryptionLayer::EncryptionLayer(std::uint64_t pBlockSize)
    : mBlockSize(pBlockSize),
      mBufferA(static_cast<std::size_t>(pBlockSize), 0),
      mBufferB(static_cast<std::size_t>(pBlockSize), 0) {}

void EncryptionLayer::addCipher(std::unique_ptr<Cipher> pCipher) {
  if (pCipher == nullptr) {
    return;
  }
  mCipherList.push_back(pCipher.get());
  mOwnedCiphers.push_back(std::move(pCipher));
}

bool EncryptionLayer::encrypt(const unsigned char* pSource,
                              unsigned char* pDestination,
                              std::uint64_t pSize,
                              std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "EncryptionLayer encrypt requires non-null source and destination buffers";
    }
    return false;
  }
  if (pSize == 0) {
    return true;
  }
  if (mBlockSize == 0) {
    if (pError != nullptr) {
      *pError = "EncryptionLayer block size must be greater than zero";
    }
    return false;
  }
  if (pSize != mBlockSize) {
    if (pError != nullptr) {
      *pError = "EncryptionLayer size mismatch";
    }
    return false;
  }

  const std::size_t aSize = static_cast<std::size_t>(pSize);
  const unsigned char* aPreviousBuffer = pSource;

  int aCipherIndex = 0;
  while (aCipherIndex < static_cast<int>(mCipherList.size())) {
    if (aPreviousBuffer == pSource) {
      if (!mCipherList[static_cast<std::size_t>(aCipherIndex)]->encrypt(pSource, mBufferA.data(), pSize, pError)) {
        return false;
      }
      aPreviousBuffer = mBufferA.data();
    } else if (aPreviousBuffer == mBufferA.data()) {
      if (!mCipherList[static_cast<std::size_t>(aCipherIndex)]->encrypt(mBufferA.data(), mBufferB.data(), pSize, pError)) {
        return false;
      }
      aPreviousBuffer = mBufferB.data();
    } else {
      if (!mCipherList[static_cast<std::size_t>(aCipherIndex)]->encrypt(mBufferB.data(), mBufferA.data(), pSize, pError)) {
        return false;
      }
      aPreviousBuffer = mBufferA.data();
    }
    ++aCipherIndex;
  }

  std::memcpy(pDestination, aPreviousBuffer, aSize);
  return true;
}

bool EncryptionLayer::decrypt(const unsigned char* pSource,
                              unsigned char* pDestination,
                              std::uint64_t pSize,
                              std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "EncryptionLayer decrypt requires non-null source and destination buffers";
    }
    return false;
  }
  if (pSize == 0) {
    return true;
  }
  if (mBlockSize == 0) {
    if (pError != nullptr) {
      *pError = "EncryptionLayer block size must be greater than zero";
    }
    return false;
  }
  if (pSize != mBlockSize) {
    if (pError != nullptr) {
      *pError = "EncryptionLayer size mismatch";
    }
    return false;
  }

  const std::size_t aSize = static_cast<std::size_t>(pSize);
  const unsigned char* aPreviousBuffer = pSource;

  int aCipherIndex = static_cast<int>(mCipherList.size()) - 1;
  while (aCipherIndex >= 0) {
    if (aPreviousBuffer == pSource) {
      if (!mCipherList[static_cast<std::size_t>(aCipherIndex)]->decrypt(pSource, mBufferA.data(), pSize, pError)) {
        return false;
      }
      aPreviousBuffer = mBufferA.data();
    } else if (aPreviousBuffer == mBufferA.data()) {
      if (!mCipherList[static_cast<std::size_t>(aCipherIndex)]->decrypt(mBufferA.data(), mBufferB.data(), pSize, pError)) {
        return false;
      }
      aPreviousBuffer = mBufferB.data();
    } else {
      if (!mCipherList[static_cast<std::size_t>(aCipherIndex)]->decrypt(mBufferB.data(), mBufferA.data(), pSize, pError)) {
        return false;
      }
      aPreviousBuffer = mBufferA.data();
    }
    --aCipherIndex;
  }

  std::memcpy(pDestination, aPreviousBuffer, aSize);
  return true;
}

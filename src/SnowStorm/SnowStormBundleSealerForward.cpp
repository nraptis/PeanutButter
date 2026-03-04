#include "SnowStorm/SnowStormBundleSealerForward.hpp"

#include <cstring>

#include "Crypt.hpp"

SnowStormBundleSealerForward::SnowStormBundleSealerForward()
    : mLayer1(Crypt::getEncryptionLayer1()),
      mLayer2(Crypt::getEncryptionLayer2()),
      mLayer3(Crypt::getEncryptionLayer3()),
      mBufferSealed(BLOCK_SIZE_LAYER_3, 0),
      mBufferLayer1Unencrypted(BLOCK_SIZE_LAYER_3, 0),
      mBufferLayer1Encrypted(BLOCK_SIZE_LAYER_3, 0) {
}

bool SnowStormBundleSealerForward::sealPage(const unsigned char* pPage, std::string* pError) {
  if (pPage == nullptr) {
    if (pError != nullptr) {
      *pError = "sealPage requires a non-null source page";
    }
    return false;
  }
  if (mLayer1 == nullptr || mLayer2 == nullptr || mLayer3 == nullptr) {
    if (pError != nullptr) {
      *pError = "One or more encryption layers are null";
    }
    return false;
  }

  std::memcpy(mBufferLayer1Unencrypted.data(), pPage, BLOCK_SIZE_LAYER_3);

  for (std::size_t aOffset = 0; aOffset < BLOCK_SIZE_LAYER_3; aOffset += BLOCK_SIZE_LAYER_1) {
    if (!mLayer1->encrypt(mBufferLayer1Unencrypted.data() + aOffset,
                          mBufferLayer1Encrypted.data() + aOffset,
                          BLOCK_SIZE_LAYER_1,
                          pError)) {
      if (pError != nullptr && pError->empty()) {
        *pError = "Layer1 encryption failed";
      }
      return false;
    }
  }

  for (std::size_t aOffset = 0; aOffset < BLOCK_SIZE_LAYER_3; aOffset += BLOCK_SIZE_LAYER_2) {
    if (!mLayer2->encrypt(mBufferLayer1Encrypted.data() + aOffset,
                          mBufferLayer1Unencrypted.data() + aOffset,
                          BLOCK_SIZE_LAYER_2,
                          pError)) {
      if (pError != nullptr && pError->empty()) {
        *pError = "Layer2 encryption failed";
      }
      return false;
    }
  }

  if (!mLayer3->encrypt(mBufferLayer1Unencrypted.data(), mBufferSealed.data(), BLOCK_SIZE_LAYER_3, pError)) {
    if (pError != nullptr && pError->empty()) {
      *pError = "Layer3 encryption failed";
    }
    return false;
  }
  return true;
}

bool SnowStormBundleSealerForward::sealZeroPage(std::string* pError) {
  std::memset(mBufferLayer1Unencrypted.data(), 0, sizeof(mBufferLayer1Unencrypted));
  return sealPage(mBufferLayer1Unencrypted.data(), pError);
}

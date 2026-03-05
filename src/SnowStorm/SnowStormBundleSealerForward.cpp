#include "SnowStorm/SnowStormBundleSealerForward.hpp"

#include <cstring>

SnowStormBundleSealerForward::SnowStormBundleSealerForward(EncryptionLayer* pLayer1,
                                                           EncryptionLayer* pLayer2,
                                                           EncryptionLayer* pLayer3)
    : mLayer1(pLayer1),
      mLayer2(pLayer2),
      mLayer3(pLayer3),
      mBufferSealed(BLOCK_SIZE_LAYER_3, 0),
      mBufferA(BLOCK_SIZE_LAYER_3, 0),
      mBufferB(BLOCK_SIZE_LAYER_3, 0) {
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

  std::memcpy(mBufferA.data(), pPage, BLOCK_SIZE_LAYER_3);

  for (std::size_t aOffset = 0; aOffset < BLOCK_SIZE_LAYER_3; aOffset += BLOCK_SIZE_LAYER_1) {
    if (!mLayer1->encrypt(mBufferA.data() + aOffset,
                          mBufferB.data() + aOffset,
                          BLOCK_SIZE_LAYER_1,
                          pError)) {
      if (pError != nullptr && pError->empty()) {
        *pError = "Layer1 encryption failed";
      }
      return false;
    }
  }

  for (std::size_t aOffset = 0; aOffset < BLOCK_SIZE_LAYER_3; aOffset += BLOCK_SIZE_LAYER_2) {
    if (!mLayer2->encrypt(mBufferB.data() + aOffset,
                          mBufferA.data() + aOffset,
                          BLOCK_SIZE_LAYER_2,
                          pError)) {
      if (pError != nullptr && pError->empty()) {
        *pError = "Layer2 encryption failed";
      }
      return false;
    }
  }

  if (!mLayer3->encrypt(mBufferA.data(), mBufferSealed.data(), BLOCK_SIZE_LAYER_3, pError)) {
    if (pError != nullptr && pError->empty()) {
      *pError = "Layer3 encryption failed";
    }
    return false;
  }
  return true;
}

bool SnowStormBundleSealerForward::sealZeroPage(std::string* pError) {
  std::memset(mBufferA.data(), 0, sizeof(mBufferA));
  return sealPage(mBufferA.data(), pError);
}

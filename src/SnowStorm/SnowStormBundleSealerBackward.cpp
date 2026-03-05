#include "SnowStorm/SnowStormBundleSealerBackward.hpp"

#include <cstring>

#include "Globals.hpp"

SnowStormBundleSealerBackward::SnowStormBundleSealerBackward(EncryptionLayer* pLayer1,
                                                             EncryptionLayer* pLayer2,
                                                             EncryptionLayer* pLayer3)
    : mLayer1(pLayer1),
      mLayer2(pLayer2),
      mLayer3(pLayer3),
      mBufferA(BLOCK_SIZE_LAYER_3, 0),
      mBufferB(BLOCK_SIZE_LAYER_3, 0) {
}

bool SnowStormBundleSealerBackward::unsealPage(const unsigned char* pInputPage,
                                               unsigned char* pOutputPage,
                                               std::string* pError) {
  if (pInputPage == nullptr || pOutputPage == nullptr) {
    if (pError != nullptr) {
      *pError = "unsealPage requires non-null buffers";
    }
    return false;
  }

  if (mLayer3 == nullptr ||
      !mLayer3->decrypt(pInputPage, mBufferA.data(), BLOCK_SIZE_LAYER_3, pError)) {
    if (pError != nullptr && pError->empty()) {
      *pError = "Layer3 decrypt failed";
    }
    return false;
  }

  for (std::size_t aOffset = 0; aOffset < BLOCK_SIZE_LAYER_3; aOffset += BLOCK_SIZE_LAYER_2) {
    if (mLayer2 == nullptr ||
        !mLayer2->decrypt(mBufferA.data() + aOffset,
                          mBufferB.data() + aOffset,
                          BLOCK_SIZE_LAYER_2,
                          pError)) {
      if (pError != nullptr && pError->empty()) {
        *pError = "Layer2 decrypt failed";
      }
      return false;
    }
  }

  for (std::size_t aOffset = 0; aOffset < BLOCK_SIZE_LAYER_3; aOffset += BLOCK_SIZE_LAYER_1) {
    if (mLayer1 == nullptr ||
        !mLayer1->decrypt(mBufferB.data() + aOffset,
                          pOutputPage + aOffset,
                          BLOCK_SIZE_LAYER_1,
                          pError)) {
      if (pError != nullptr && pError->empty()) {
        *pError = "Layer1 decrypt failed";
      }
      return false;
    }
  }

  return true;
}

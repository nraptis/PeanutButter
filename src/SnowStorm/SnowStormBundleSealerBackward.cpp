#include "SnowStorm/SnowStormBundleSealerBackward.hpp"

#include <cstring>

#include "Crypt.hpp"
#include "Globals.hpp"

SnowStormBundleSealerBackward::SnowStormBundleSealerBackward()
    : mLayer1(Crypt::getEncryptionLayer1()),
      mLayer2(Crypt::getEncryptionLayer2()),
      mLayer3(Crypt::getEncryptionLayer3()),
      mBufferTierA(BLOCK_SIZE_LAYER_3, 0),
      mBufferTierB(BLOCK_SIZE_LAYER_3, 0) {
}

bool SnowStormBundleSealerBackward::decryptLayer3Page(const unsigned char* pInputPage,
                                                       unsigned char* pOutputPage,
                                                       std::string* pError) {
  if (pInputPage == nullptr || pOutputPage == nullptr) {
    if (pError != nullptr) {
      *pError = "decryptLayer3Page requires non-null buffers";
    }
    return false;
  }

  if (mLayer3 == nullptr ||
      !mLayer3->decrypt(pInputPage, mBufferTierA.data(), BLOCK_SIZE_LAYER_3, pError)) {
    if (pError != nullptr && pError->empty()) {
      *pError = "Layer3 decrypt failed";
    }
    return false;
  }

  for (std::size_t aOffset = 0; aOffset < BLOCK_SIZE_LAYER_3; aOffset += BLOCK_SIZE_LAYER_2) {
    if (mLayer2 == nullptr ||
        !mLayer2->decrypt(mBufferTierA.data() + aOffset,
                          mBufferTierB.data() + aOffset,
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
        !mLayer1->decrypt(mBufferTierB.data() + aOffset,
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

#include "LayeredEncryption/LayeredEncryptionMockEmpty.hpp"

#include <memory>

#include "Globals.hpp"
#include "SandStorm/EncryptionLayer.hpp"

LayeredEncryptionMockEmpty::LayeredEncryptionMockEmpty() {
  mLayer1 = std::make_unique<EncryptionLayer>(BLOCK_SIZE_LAYER_1);
  mLayer2 = std::make_unique<EncryptionLayer>(BLOCK_SIZE_LAYER_2);
  mLayer3 = std::make_unique<EncryptionLayer>(BLOCK_SIZE_LAYER_3);
}

EncryptionLayer* LayeredEncryptionMockEmpty::getEncryptionLayer1() {
  return mLayer1.get();
}

EncryptionLayer* LayeredEncryptionMockEmpty::getEncryptionLayer2() {
  return mLayer2.get();
}

EncryptionLayer* LayeredEncryptionMockEmpty::getEncryptionLayer3() {
  return mLayer3.get();
}

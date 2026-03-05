#include "LayeredEncryption/LayeredEncryptionImplementation.hpp"

#include <memory>

#include "Globals.hpp"
#include "SandStorm/EncryptionLayer.hpp"
#include "SandStorm/PasswordCiper.hpp"
#include "SandStorm/SplintCipher.hpp"
#include "SandStorm/WeaveCipher.hpp"

LayeredEncryptionImplementation::LayeredEncryptionImplementation() {
  mLayer1 = std::make_unique<EncryptionLayer>(BLOCK_SIZE_LAYER_1);
  mLayer1->addCipher(std::make_unique<PasswordCiper>(reinterpret_cast<const unsigned char*>("dog")));

  mLayer2 = std::make_unique<EncryptionLayer>(BLOCK_SIZE_LAYER_2);
  mLayer2->addCipher(std::make_unique<SplintCipher>());

  mLayer3 = std::make_unique<EncryptionLayer>(BLOCK_SIZE_LAYER_3);
  mLayer3->addCipher(std::make_unique<PasswordCiper>(reinterpret_cast<const unsigned char*>("alligator")));
  mLayer3->addCipher(std::make_unique<WeaveCipher>(2, 1, 1));
  mLayer3->addCipher(std::make_unique<PasswordCiper>(reinterpret_cast<const unsigned char*>("mouse")));
}

EncryptionLayer* LayeredEncryptionImplementation::getEncryptionLayer1() {
  return mLayer1.get();
}

EncryptionLayer* LayeredEncryptionImplementation::getEncryptionLayer2() {
  return mLayer2.get();
}

EncryptionLayer* LayeredEncryptionImplementation::getEncryptionLayer3() {
  return mLayer3.get();
}

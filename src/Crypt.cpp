#include "Crypt.hpp"

#include <memory>

#include "SandStorm/PasswordCiper.hpp"
#include "SandStorm/WeaveCipher.hpp"
#include "SandStorm/SplintCipher.hpp"


#include "Globals.hpp"

EncryptionLayer* Crypt::getEncryptionLayer1() {
  auto* aLayer = new EncryptionLayer(BLOCK_SIZE_LAYER_1);
  aLayer->addCipher(std::make_unique<PasswordCiper>(reinterpret_cast<const unsigned char*>("dog")));
  return aLayer;
}

EncryptionLayer* Crypt::getEncryptionLayer2() {
  auto* aLayer = new EncryptionLayer(BLOCK_SIZE_LAYER_2);
  aLayer->addCipher(std::make_unique<SplintCipher>());
  return aLayer;
}

EncryptionLayer* Crypt::getEncryptionLayer3() {
  auto* aLayer = new EncryptionLayer(BLOCK_SIZE_LAYER_3);
  aLayer->addCipher(std::make_unique<PasswordCiper>(reinterpret_cast<const unsigned char*>("alligator")));
  aLayer->addCipher(std::make_unique<WeaveCipher>(2, 1, 1));
  aLayer->addCipher(std::make_unique<PasswordCiper>(reinterpret_cast<const unsigned char*>("mouse")));
  return aLayer;
}

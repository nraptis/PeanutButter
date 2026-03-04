#pragma once

#include "SandStorm/EncryptionLayer.hpp"

class Crypt {
public:
  static EncryptionLayer* getEncryptionLayer1();
  static EncryptionLayer* getEncryptionLayer2();
  static EncryptionLayer* getEncryptionLayer3();
};

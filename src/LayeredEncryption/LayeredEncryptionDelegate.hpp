#pragma once

class EncryptionLayer;

class LayeredEncryptionDelegate {
public:
  virtual ~LayeredEncryptionDelegate() = default;

  virtual EncryptionLayer* getEncryptionLayer1() = 0;
  virtual EncryptionLayer* getEncryptionLayer2() = 0;
  virtual EncryptionLayer* getEncryptionLayer3() = 0;
};

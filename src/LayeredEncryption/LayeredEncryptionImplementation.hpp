#pragma once

#include "LayeredEncryption/LayeredEncryptionDelegate.hpp"

#include <memory>

#include "SandStorm/EncryptionLayer.hpp"

class LayeredEncryptionImplementation final : public LayeredEncryptionDelegate {
public:
  LayeredEncryptionImplementation();

  EncryptionLayer* getEncryptionLayer1() override;
  EncryptionLayer* getEncryptionLayer2() override;
  EncryptionLayer* getEncryptionLayer3() override;

private:
  std::unique_ptr<EncryptionLayer> mLayer1;
  std::unique_ptr<EncryptionLayer> mLayer2;
  std::unique_ptr<EncryptionLayer> mLayer3;
};

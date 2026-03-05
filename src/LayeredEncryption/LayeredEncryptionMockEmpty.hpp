#pragma once

#include <memory>

#include "SandStorm/EncryptionLayer.hpp"
#include "LayeredEncryption/LayeredEncryptionDelegate.hpp"

class LayeredEncryptionMockEmpty final : public LayeredEncryptionDelegate {
public:
  LayeredEncryptionMockEmpty();

  EncryptionLayer* getEncryptionLayer1() override;
  EncryptionLayer* getEncryptionLayer2() override;
  EncryptionLayer* getEncryptionLayer3() override;

private:
  std::unique_ptr<EncryptionLayer> mLayer1;
  std::unique_ptr<EncryptionLayer> mLayer2;
  std::unique_ptr<EncryptionLayer> mLayer3;
};

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SandStorm/EncryptionLayer.hpp"
#include "Globals.hpp"

class SnowStormBundleSealerForward {
public:
  SnowStormBundleSealerForward();

  bool sealPage(const unsigned char* pPage, std::string* pError);
  bool sealZeroPage(std::string* pError);

  std::vector<unsigned char> mBufferSealed;

private:
  std::unique_ptr<EncryptionLayer> mLayer1;
  std::unique_ptr<EncryptionLayer> mLayer2;
  std::unique_ptr<EncryptionLayer> mLayer3;
  std::vector<unsigned char> mBufferLayer1Unencrypted;
  std::vector<unsigned char> mBufferLayer1Encrypted;
};

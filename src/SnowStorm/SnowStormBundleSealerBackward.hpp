#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SandStorm/EncryptionLayer.hpp"
#include "Globals.hpp"

class SnowStormBundleSealerBackward {
public:
  SnowStormBundleSealerBackward();

  bool decryptLayer3Page(const unsigned char* pInputPage,
                         unsigned char* pOutputPage,
                         std::string* pError);

private:
  std::unique_ptr<EncryptionLayer> mLayer1;
  std::unique_ptr<EncryptionLayer> mLayer2;
  std::unique_ptr<EncryptionLayer> mLayer3;
  std::vector<unsigned char> mBufferTierA;
  std::vector<unsigned char> mBufferTierB;
};

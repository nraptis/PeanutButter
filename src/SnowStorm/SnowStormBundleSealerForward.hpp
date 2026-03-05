#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SandStorm/EncryptionLayer.hpp"
#include "Globals.hpp"

class SnowStormBundleSealerForward {
public:
  SnowStormBundleSealerForward(EncryptionLayer* pLayer1, EncryptionLayer* pLayer2, EncryptionLayer* pLayer3);

  bool sealPage(const unsigned char* pPage, std::string* pError);
  bool sealZeroPage(std::string* pError);

  std::vector<unsigned char> mBufferSealed;

private:
  EncryptionLayer* mLayer1 = nullptr;
  EncryptionLayer* mLayer2 = nullptr;
  EncryptionLayer* mLayer3 = nullptr;
  std::vector<unsigned char> mBufferA;
  std::vector<unsigned char> mBufferB;
};

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SandStorm/EncryptionLayer.hpp"
#include "Globals.hpp"

class SnowStormBundleSealerBackward {
public:
  SnowStormBundleSealerBackward(EncryptionLayer* pLayer1, EncryptionLayer* pLayer2, EncryptionLayer* pLayer3);

  bool unsealPage(const unsigned char* pInputPage,
                  unsigned char* pOutputPage,
                  std::string* pError);

private:
  EncryptionLayer* mLayer1 = nullptr;
  EncryptionLayer* mLayer2 = nullptr;
  EncryptionLayer* mLayer3 = nullptr;
  std::vector<unsigned char> mBufferA;
  std::vector<unsigned char> mBufferB;
};

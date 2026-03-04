#include "SandStorm/SandStorm.hpp"

SandStorm::SandStorm(std::uint64_t pBlockSize,
                     std::u32string pPassword1,
                     std::u32string pPassword2)
    : mBlockSize(pBlockSize),
      mPassword1(std::move(pPassword1)),
      mPassword2(std::move(pPassword2)) {}

bool SandStorm::encrypt(const unsigned char* pSource,
                        unsigned char* pDestination,
                        std::uint64_t pSize,
                        std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "SandStorm encrypt requires non-null source and destination buffers";
    }
    return false;
  }
  if (pSize == 0) {
    return true;
  }
  if (mBlockSize == 0) {
    if (pError != nullptr) {
      *pError = "SandStorm block size must be greater than zero";
    }
    return false;
  }

  // Password fields are intentionally retained as scaffolding for key scheduling.
  (void)mPassword1;
  (void)mPassword2;

  return mCipher.encrypt(pSource, pDestination, pSize, pError);
}

bool SandStorm::decrypt(const unsigned char* pSource,
                        unsigned char* pDestination,
                        std::uint64_t pSize,
                        std::string* pError) {
  return encrypt(pSource, pDestination, pSize, pError);
}

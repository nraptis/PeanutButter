#include "PasswordCiper.hpp"

#include <cstring>

PasswordCiper::PasswordCiper(const unsigned char* pPassword) {
  if (pPassword == nullptr) {
    return;
  }
  const std::size_t aLength = std::strlen(reinterpret_cast<const char*>(pPassword));
  mPassword.assign(pPassword, pPassword + aLength);
}

bool PasswordCiper::encrypt(const unsigned char* pSource,
                            unsigned char* pDestination,
                            std::uint64_t pSize,
                            std::string* pError) {
  if (pSource == nullptr || pDestination == nullptr) {
    if (pError != nullptr) {
      *pError = "PasswordCiper encrypt requires non-null source and destination buffers";
    }
    return false;
  }

  if (mPassword.empty()) {
    std::memcpy(pDestination, pSource, static_cast<std::size_t>(pSize));
    return true;
  }

  const std::size_t aPasswordSize = mPassword.size();
  for (std::uint64_t aIndex = 0; aIndex < pSize; ++aIndex) {
    pDestination[aIndex] = static_cast<unsigned char>(pSource[aIndex] ^ mPassword[aIndex % aPasswordSize]);
  }
  return true;
}

bool PasswordCiper::decrypt(const unsigned char* pSource,
                            unsigned char* pDestination,
                            std::uint64_t pSize,
                            std::string* pError) {
  return encrypt(pSource, pDestination, pSize, pError);
}

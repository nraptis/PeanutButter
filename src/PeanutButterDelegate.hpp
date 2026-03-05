#pragma once

#include <string>

class PeanutButterDelegate {
public:
  virtual ~PeanutButterDelegate() = default;

  virtual void logMessage(const std::string& pMessage) = 0;
  virtual bool showOkCancelDialog(const std::string& pTitle, const std::string& pMessage) = 0;
};


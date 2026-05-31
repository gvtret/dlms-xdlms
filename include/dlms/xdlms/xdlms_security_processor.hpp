#pragma once

#include "dlms/security/security_types.hpp"

#include <cstdint>
#include <vector>

namespace dlms {
namespace security {
class CipheredApduProcessor;
}
namespace xdlms {

class IXdlmsSecurityProcessor
{
public:
  virtual ~IXdlmsSecurityProcessor();

  virtual dlms::security::SecurityStatus Protect(
    dlms::security::SecurityByteView plainApdu,
    std::vector<std::uint8_t>& protectedApdu) const = 0;

  virtual dlms::security::SecurityStatus Unprotect(
    dlms::security::SecurityByteView protectedApdu,
    std::vector<std::uint8_t>& plainApdu) const = 0;
};

class CipheredXdlmsSecurityProcessor : public IXdlmsSecurityProcessor
{
public:
  explicit CipheredXdlmsSecurityProcessor(
    dlms::security::CipheredApduProcessor& processor);

  dlms::security::SecurityStatus Protect(
    dlms::security::SecurityByteView plainApdu,
    std::vector<std::uint8_t>& protectedApdu) const;

  dlms::security::SecurityStatus Unprotect(
    dlms::security::SecurityByteView protectedApdu,
    std::vector<std::uint8_t>& plainApdu) const;

private:
  dlms::security::CipheredApduProcessor& processor_;
};

} // namespace xdlms
} // namespace dlms

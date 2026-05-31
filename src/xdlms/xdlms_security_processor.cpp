#include "dlms/xdlms/xdlms_security_processor.hpp"

#include "dlms/security/ciphered_apdu_processor.hpp"

namespace dlms {
namespace xdlms {

IXdlmsSecurityProcessor::~IXdlmsSecurityProcessor()
{
}

CipheredXdlmsSecurityProcessor::CipheredXdlmsSecurityProcessor(
  dlms::security::CipheredApduProcessor& processor)
  : processor_(processor)
{
}

dlms::security::SecurityStatus CipheredXdlmsSecurityProcessor::Protect(
  dlms::security::SecurityByteView plainApdu,
  std::vector<std::uint8_t>& protectedApdu) const
{
  return processor_.Protect(plainApdu, protectedApdu);
}

dlms::security::SecurityStatus CipheredXdlmsSecurityProcessor::Unprotect(
  dlms::security::SecurityByteView protectedApdu,
  std::vector<std::uint8_t>& plainApdu) const
{
  return processor_.Unprotect(protectedApdu, plainApdu);
}

} // namespace xdlms
} // namespace dlms

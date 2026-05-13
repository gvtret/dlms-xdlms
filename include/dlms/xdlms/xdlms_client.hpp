#pragma once

#include "dlms/association/association_client.hpp"
#include "dlms/profile/apdu_channel.hpp"
#include "dlms/xdlms/xdlms_types.hpp"

namespace dlms {
namespace security {
class CipheredApduProcessor;
}
namespace xdlms {

class XdlmsClient
{
public:
  XdlmsClient(
    dlms::profile::IApduChannel& channel,
    dlms::association::AssociationClient& association);

  XdlmsClient(
    dlms::profile::IApduChannel& channel,
    dlms::association::AssociationClient& association,
    dlms::security::CipheredApduProcessor& security);

  XdlmsStatus Get(
    const CosemAttributeDescriptor& descriptor,
    GetResult& result);

  XdlmsStatus Get(
    const CosemAttributeDescriptor& descriptor,
    const ServiceOptions& options,
    GetResult& result);

  XdlmsStatus Set(
    const CosemAttributeDescriptor& descriptor,
    const std::vector<std::uint8_t>& encodedData,
    SetResult& result);

  XdlmsStatus Action(
    const CosemMethodDescriptor& descriptor,
    bool hasParameter,
    const std::vector<std::uint8_t>& encodedParameter,
    ActionResult& result);

private:
  XdlmsClient(const XdlmsClient&);
  XdlmsClient& operator=(const XdlmsClient&);

  dlms::profile::IApduChannel& channel_;
  dlms::association::AssociationClient& association_;
  dlms::security::CipheredApduProcessor* security_;
  InvokeIdAllocator invokeIds_;
};

} // namespace xdlms
} // namespace dlms

#pragma once

#include "dlms/association/association_client.hpp"
#include "dlms/profile/apdu_channel.hpp"
#include "dlms/xdlms/xdlms_types.hpp"

namespace dlms {
namespace xdlms {

class XdlmsClient
{
public:
  XdlmsClient(
    dlms::profile::IApduChannel& channel,
    dlms::association::AssociationClient& association);

  XdlmsStatus Get(
    const CosemAttributeDescriptor& descriptor,
    GetResult& result);

private:
  XdlmsClient(const XdlmsClient&);
  XdlmsClient& operator=(const XdlmsClient&);

  dlms::profile::IApduChannel& channel_;
  dlms::association::AssociationClient& association_;
  InvokeIdAllocator invokeIds_;
};

} // namespace xdlms
} // namespace dlms

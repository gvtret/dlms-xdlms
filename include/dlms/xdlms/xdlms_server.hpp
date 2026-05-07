#pragma once

#include "dlms/xdlms/xdlms_status.hpp"
#include "dlms/xdlms/xdlms_types.hpp"

#include <cstdint>

namespace dlms {
namespace xdlms {

struct GetIndication
{
  std::uint8_t invokeId;
  ServiceOptions options;
  CosemAttributeDescriptor descriptor;
};

class IXdlmsServerHandler
{
public:
  virtual ~IXdlmsServerHandler();

  virtual XdlmsStatus HandleGet(
    const GetIndication& indication,
    GetResult& result) = 0;
};

class XdlmsServerDispatcher
{
public:
  explicit XdlmsServerDispatcher(IXdlmsServerHandler& handler);

  XdlmsStatus DispatchGet(
    const GetIndication& indication,
    GetResult& result);

private:
  IXdlmsServerHandler& handler_;
};

GetIndication EmptyGetIndication();
XdlmsStatus ValidateInvokeId(std::uint8_t invokeId);

} // namespace xdlms
} // namespace dlms

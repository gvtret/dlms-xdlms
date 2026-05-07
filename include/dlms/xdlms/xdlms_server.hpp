#pragma once

#include "dlms/xdlms/xdlms_status.hpp"
#include "dlms/xdlms/xdlms_types.hpp"

#include <cstdint>
#include <vector>

namespace dlms {
namespace xdlms {

struct GetIndication
{
  std::uint8_t invokeId;
  ServiceOptions options;
  CosemAttributeDescriptor descriptor;
};

struct SetIndication
{
  std::uint8_t invokeId;
  ServiceOptions options;
  CosemAttributeDescriptor descriptor;
  std::vector<std::uint8_t> data;
};

class IXdlmsServerHandler
{
public:
  virtual ~IXdlmsServerHandler();

  virtual XdlmsStatus HandleGet(
    const GetIndication& indication,
    GetResult& result) = 0;

  virtual XdlmsStatus HandleSet(
    const SetIndication& indication,
    SetResult& result);
};

class XdlmsServerDispatcher
{
public:
  explicit XdlmsServerDispatcher(IXdlmsServerHandler& handler);

  XdlmsStatus DispatchGet(
    const GetIndication& indication,
    GetResult& result);

  XdlmsStatus DispatchSet(
    const SetIndication& indication,
    SetResult& result);

private:
  IXdlmsServerHandler& handler_;
};

class XdlmsServerApduProcessor
{
public:
  explicit XdlmsServerApduProcessor(XdlmsServerDispatcher& dispatcher);

  XdlmsStatus ProcessRequest(
    const std::vector<std::uint8_t>& requestApdu,
    std::vector<std::uint8_t>& responseApdu);

private:
  XdlmsServerDispatcher& dispatcher_;
};

GetIndication EmptyGetIndication();
SetIndication EmptySetIndication();
XdlmsStatus ValidateInvokeId(std::uint8_t invokeId);

} // namespace xdlms
} // namespace dlms

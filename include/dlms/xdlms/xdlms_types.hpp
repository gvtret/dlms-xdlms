#pragma once

#include "dlms/xdlms/xdlms_status.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace xdlms {

class CosemLogicalName
{
public:
  CosemLogicalName();
  CosemLogicalName(
    std::uint8_t a,
    std::uint8_t b,
    std::uint8_t c,
    std::uint8_t d,
    std::uint8_t e,
    std::uint8_t f);

  const std::uint8_t* Data() const;
  std::size_t Size() const;
  std::uint8_t operator[](std::size_t index) const;

private:
  std::uint8_t bytes_[6];
};

struct CosemAttributeDescriptor
{
  std::uint16_t classId;
  CosemLogicalName instanceId;
  std::uint8_t attributeId;
};

struct CosemMethodDescriptor
{
  std::uint16_t classId;
  CosemLogicalName instanceId;
  std::uint8_t methodId;
};

struct ServiceOptions
{
  bool confirmed;
  bool highPriority;
  bool allowBlockTransfer;
  std::size_t maxBlockTransferBytes;
};

struct GetResult
{
  std::uint8_t invokeId;
  bool hasData;
  std::vector<std::uint8_t> data;
  bool hasAccessResult;
  std::uint8_t accessResult;
};

struct SetResult
{
  std::uint8_t invokeId;
  std::uint8_t accessResult;
};

struct ActionResult
{
  std::uint8_t invokeId;
  std::uint8_t actionResult;
  bool hasData;
  std::vector<std::uint8_t> data;
};

class InvokeIdAllocator
{
public:
  InvokeIdAllocator();

  std::uint8_t Next();

private:
  std::uint8_t next_;
};

ServiceOptions DefaultServiceOptions();
CosemAttributeDescriptor EmptyCosemAttributeDescriptor();
CosemMethodDescriptor EmptyCosemMethodDescriptor();
GetResult EmptyGetResult();
SetResult EmptySetResult();
ActionResult EmptyActionResult();
XdlmsStatus ValidateDescriptor(
  const CosemAttributeDescriptor& descriptor);
XdlmsStatus ValidateMethodDescriptor(
  const CosemMethodDescriptor& descriptor);

} // namespace xdlms
} // namespace dlms

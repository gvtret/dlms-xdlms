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

struct ServiceOptions
{
  bool confirmed;
  bool highPriority;
};

struct GetResult
{
  std::uint8_t invokeId;
  bool hasData;
  std::vector<std::uint8_t> data;
  bool hasAccessResult;
  std::uint8_t accessResult;
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
GetResult EmptyGetResult();
XdlmsStatus ValidateDescriptor(
  const CosemAttributeDescriptor& descriptor);

} // namespace xdlms
} // namespace dlms

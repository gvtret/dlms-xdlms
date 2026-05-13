#include "dlms/xdlms/xdlms_types.hpp"

namespace dlms {
namespace xdlms {

CosemLogicalName::CosemLogicalName()
{
  for (std::size_t i = 0; i < Size(); ++i) {
    bytes_[i] = 0;
  }
}

CosemLogicalName::CosemLogicalName(
  std::uint8_t a,
  std::uint8_t b,
  std::uint8_t c,
  std::uint8_t d,
  std::uint8_t e,
  std::uint8_t f)
{
  bytes_[0] = a;
  bytes_[1] = b;
  bytes_[2] = c;
  bytes_[3] = d;
  bytes_[4] = e;
  bytes_[5] = f;
}

const std::uint8_t* CosemLogicalName::Data() const
{
  return bytes_;
}

std::size_t CosemLogicalName::Size() const
{
  return 6u;
}

std::uint8_t CosemLogicalName::operator[](std::size_t index) const
{
  return index < Size() ? bytes_[index] : 0;
}

InvokeIdAllocator::InvokeIdAllocator()
  : next_(1u)
{
}

std::uint8_t InvokeIdAllocator::Next()
{
  const std::uint8_t current = next_;
  next_ = static_cast<std::uint8_t>((next_ + 1u) & 0x0Fu);
  if (next_ == 0u) {
    next_ = 1u;
  }
  return current;
}

ServiceOptions DefaultServiceOptions()
{
  ServiceOptions options;
  options.confirmed = true;
  options.highPriority = false;
  options.allowBlockTransfer = true;
  options.maxBlockTransferBytes = 65536u;
  options.maxSetBlockPayloadBytes = 1024u;
  options.maxActionBlockPayloadBytes = 1024u;
  return options;
}

CosemAttributeDescriptor EmptyCosemAttributeDescriptor()
{
  CosemAttributeDescriptor descriptor;
  descriptor.classId = 0;
  descriptor.instanceId = CosemLogicalName();
  descriptor.attributeId = 0;
  return descriptor;
}

CosemMethodDescriptor EmptyCosemMethodDescriptor()
{
  CosemMethodDescriptor descriptor;
  descriptor.classId = 0;
  descriptor.instanceId = CosemLogicalName();
  descriptor.methodId = 0;
  return descriptor;
}

GetResult EmptyGetResult()
{
  GetResult result;
  result.invokeId = 0;
  result.hasData = false;
  result.data.clear();
  result.hasAccessResult = false;
  result.accessResult = 0;
  return result;
}

SetResult EmptySetResult()
{
  SetResult result;
  result.invokeId = 0;
  result.accessResult = 0;
  return result;
}

ActionResult EmptyActionResult()
{
  ActionResult result;
  result.invokeId = 0;
  result.actionResult = 0;
  result.hasData = false;
  result.data.clear();
  return result;
}

XdlmsStatus ValidateDescriptor(
  const CosemAttributeDescriptor& descriptor)
{
  if (descriptor.classId == 0 || descriptor.attributeId == 0) {
    return XdlmsStatus::InvalidArgument;
  }

  bool hasAnyInstanceByte = false;
  for (std::size_t i = 0; i < descriptor.instanceId.Size(); ++i) {
    if (descriptor.instanceId[i] != 0u) {
      hasAnyInstanceByte = true;
      break;
    }
  }

  return hasAnyInstanceByte
    ? XdlmsStatus::Ok
    : XdlmsStatus::InvalidArgument;
}

XdlmsStatus ValidateMethodDescriptor(
  const CosemMethodDescriptor& descriptor)
{
  if (descriptor.classId == 0 || descriptor.methodId == 0) {
    return XdlmsStatus::InvalidArgument;
  }

  bool hasAnyInstanceByte = false;
  for (std::size_t i = 0; i < descriptor.instanceId.Size(); ++i) {
    if (descriptor.instanceId[i] != 0u) {
      hasAnyInstanceByte = true;
      break;
    }
  }

  return hasAnyInstanceByte
    ? XdlmsStatus::Ok
    : XdlmsStatus::InvalidArgument;
}

} // namespace xdlms
} // namespace dlms

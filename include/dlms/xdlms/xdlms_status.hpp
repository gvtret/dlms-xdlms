#pragma once

namespace dlms {
namespace xdlms {

enum class XdlmsStatus
{
  Ok,
  InvalidArgument,
  InvalidState,
  NotAssociated,
  SendFailed,
  ReceiveFailed,
  EncodeFailed,
  DecodeFailed,
  InvokeIdMismatch,
  ServiceRejected,
  BlockTransferRequired,
  UnsupportedFeature,
  SecurityFailed,
  InternalError
};

const char* XdlmsStatusName(XdlmsStatus status);

} // namespace xdlms
} // namespace dlms

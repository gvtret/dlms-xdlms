#pragma once

namespace dlms {
namespace xdlms_client {

enum class XdlmsClientStatus
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
  InternalError
};

const char* XdlmsClientStatusName(XdlmsClientStatus status);

} // namespace xdlms_client
} // namespace dlms

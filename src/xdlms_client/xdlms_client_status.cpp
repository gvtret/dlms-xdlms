#include "dlms/xdlms_client/xdlms_client_status.hpp"

namespace dlms {
namespace xdlms_client {

const char* XdlmsClientStatusName(XdlmsClientStatus status)
{
  switch (status) {
  case XdlmsClientStatus::Ok:
    return "Ok";
  case XdlmsClientStatus::InvalidArgument:
    return "InvalidArgument";
  case XdlmsClientStatus::InvalidState:
    return "InvalidState";
  case XdlmsClientStatus::NotAssociated:
    return "NotAssociated";
  case XdlmsClientStatus::SendFailed:
    return "SendFailed";
  case XdlmsClientStatus::ReceiveFailed:
    return "ReceiveFailed";
  case XdlmsClientStatus::EncodeFailed:
    return "EncodeFailed";
  case XdlmsClientStatus::DecodeFailed:
    return "DecodeFailed";
  case XdlmsClientStatus::InvokeIdMismatch:
    return "InvokeIdMismatch";
  case XdlmsClientStatus::ServiceRejected:
    return "ServiceRejected";
  case XdlmsClientStatus::BlockTransferRequired:
    return "BlockTransferRequired";
  case XdlmsClientStatus::UnsupportedFeature:
    return "UnsupportedFeature";
  case XdlmsClientStatus::InternalError:
    return "InternalError";
  }

  return "Unknown";
}

} // namespace xdlms_client
} // namespace dlms

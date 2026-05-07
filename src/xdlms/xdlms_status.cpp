#include "dlms/xdlms/xdlms_status.hpp"

namespace dlms {
namespace xdlms {

const char* XdlmsStatusName(XdlmsStatus status)
{
  switch (status) {
  case XdlmsStatus::Ok:
    return "Ok";
  case XdlmsStatus::InvalidArgument:
    return "InvalidArgument";
  case XdlmsStatus::InvalidState:
    return "InvalidState";
  case XdlmsStatus::NotAssociated:
    return "NotAssociated";
  case XdlmsStatus::SendFailed:
    return "SendFailed";
  case XdlmsStatus::ReceiveFailed:
    return "ReceiveFailed";
  case XdlmsStatus::EncodeFailed:
    return "EncodeFailed";
  case XdlmsStatus::DecodeFailed:
    return "DecodeFailed";
  case XdlmsStatus::InvokeIdMismatch:
    return "InvokeIdMismatch";
  case XdlmsStatus::ServiceRejected:
    return "ServiceRejected";
  case XdlmsStatus::BlockTransferRequired:
    return "BlockTransferRequired";
  case XdlmsStatus::UnsupportedFeature:
    return "UnsupportedFeature";
  case XdlmsStatus::InternalError:
    return "InternalError";
  }

  return "Unknown";
}

} // namespace xdlms
} // namespace dlms

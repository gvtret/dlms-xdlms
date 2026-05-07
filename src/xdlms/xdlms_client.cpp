#include "dlms/xdlms/xdlms_client.hpp"

#include "dlms/apdu/apdu_writer.hpp"
#include "dlms/apdu/data.hpp"
#include "dlms/apdu/get.hpp"
#include "dlms/apdu/xdlms.hpp"

namespace dlms {
namespace xdlms {
namespace {

std::uint8_t MakeInvokeIdAndPriority(
  std::uint8_t invokeId,
  const ServiceOptions& options)
{
  std::uint8_t value = static_cast<std::uint8_t>(invokeId & 0x0Fu);
  if (options.confirmed) {
    value = static_cast<std::uint8_t>(value | 0x80u);
  }
  if (options.highPriority) {
    value = static_cast<std::uint8_t>(value | 0x40u);
  }
  return value;
}

dlms::apdu::LogicalName ToApduLogicalName(
  const CosemLogicalName& logicalName)
{
  return dlms::apdu::LogicalName(
    logicalName[0],
    logicalName[1],
    logicalName[2],
    logicalName[3],
    logicalName[4],
    logicalName[5]);
}

XdlmsStatus CopyEncodedData(
  const dlms::apdu::DlmsData& data,
  GetResult& result)
{
  std::uint8_t buffer[2048] = {};
  dlms::apdu::ApduWriter writer(buffer, sizeof(buffer));
  const dlms::apdu::ApduStatus status =
    dlms::apdu::EncodeDlmsData(data, writer);
  if (status != dlms::apdu::ApduStatus::Ok) {
    return XdlmsStatus::DecodeFailed;
  }

  result.data.assign(buffer, buffer + writer.WrittenSize());
  result.hasData = true;
  return XdlmsStatus::Ok;
}

} // namespace

XdlmsClient::XdlmsClient(
  dlms::profile::IApduChannel& channel,
  dlms::association::AssociationClient& association)
  : channel_(channel)
  , association_(association)
  , invokeIds_()
{
}

XdlmsStatus XdlmsClient::Get(
  const CosemAttributeDescriptor& descriptor,
  GetResult& result)
{
  result = EmptyGetResult();

  XdlmsStatus status = ValidateDescriptor(descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  if (!association_.IsAssociated()) {
    return XdlmsStatus::NotAssociated;
  }

  const std::uint8_t invokeId = invokeIds_.Next();
  const std::uint8_t invokeIdAndPriority =
    MakeInvokeIdAndPriority(invokeId, DefaultServiceOptions());

  dlms::apdu::XdlmsApdu request = dlms::apdu::MakeGetRequestNormal(
    invokeIdAndPriority,
    descriptor.classId,
    ToApduLogicalName(descriptor.instanceId),
    descriptor.attributeId);

  std::vector<std::uint8_t> encodedRequest;
  if (dlms::apdu::EncodeXdlmsApdu(request, encodedRequest) !=
      dlms::apdu::ApduStatus::Ok) {
    return XdlmsStatus::EncodeFailed;
  }

  dlms::profile::ProfileByteView view = {};
  view.data = encodedRequest.empty() ? 0 : &encodedRequest[0];
  view.size = encodedRequest.size();
  if (channel_.SendApdu(view) != dlms::profile::ProfileStatus::Ok) {
    return XdlmsStatus::SendFailed;
  }

  std::vector<std::uint8_t> encodedResponse;
  if (channel_.ReceiveApdu(encodedResponse) != dlms::profile::ProfileStatus::Ok) {
    return XdlmsStatus::ReceiveFailed;
  }

  dlms::apdu::XdlmsApdu response;
  if (dlms::apdu::DecodeXdlmsApdu(
        encodedResponse.empty() ? 0 : &encodedResponse[0],
        encodedResponse.size(),
        response) != dlms::apdu::ApduStatus::Ok) {
    return XdlmsStatus::DecodeFailed;
  }

  if (response.kind != dlms::apdu::XdlmsApduKind::GetResponse) {
    return XdlmsStatus::DecodeFailed;
  }

  if (response.getResponseAny.choice != dlms::apdu::GetResponseChoice::Normal) {
    return response.getResponseAny.choice ==
        dlms::apdu::GetResponseChoice::WithDataBlock
      ? XdlmsStatus::BlockTransferRequired
      : XdlmsStatus::UnsupportedFeature;
  }

  if ((response.getResponse.invokeIdAndPriority & 0x0Fu) != invokeId) {
    return XdlmsStatus::InvokeIdMismatch;
  }

  result.invokeId = invokeId;
  if (response.getResponse.resultChoice ==
      dlms::apdu::GetDataResultChoice::DataAccessError) {
    result.hasAccessResult = true;
    result.accessResult = response.getResponse.dataAccessError;
    return XdlmsStatus::ServiceRejected;
  }

  return CopyEncodedData(response.getResponse.data, result);
}

} // namespace xdlms
} // namespace dlms

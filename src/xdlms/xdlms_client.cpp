#include "dlms/xdlms/xdlms_client.hpp"

#include "dlms/apdu/action.hpp"
#include "dlms/apdu/apdu_writer.hpp"
#include "dlms/apdu/data.hpp"
#include "dlms/apdu/get.hpp"
#include "dlms/apdu/set.hpp"
#include "dlms/apdu/xdlms.hpp"
#include "dlms/security/ciphered_apdu_processor.hpp"

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

dlms::apdu::CosemAttributeDescriptor ToApduDescriptor(
  const CosemAttributeDescriptor& descriptor)
{
  dlms::apdu::CosemAttributeDescriptor apduDescriptor;
  apduDescriptor.classId = descriptor.classId;
  for (std::size_t i = 0; i < descriptor.instanceId.Size(); ++i) {
    apduDescriptor.logicalName[i] = descriptor.instanceId[i];
  }
  apduDescriptor.attributeId = descriptor.attributeId;
  return apduDescriptor;
}

dlms::apdu::CosemMethodDescriptor ToApduDescriptor(
  const CosemMethodDescriptor& descriptor)
{
  dlms::apdu::CosemMethodDescriptor apduDescriptor;
  apduDescriptor.classId = descriptor.classId;
  for (std::size_t i = 0; i < descriptor.instanceId.Size(); ++i) {
    apduDescriptor.logicalName[i] = descriptor.instanceId[i];
  }
  apduDescriptor.methodId = descriptor.methodId;
  return apduDescriptor;
}

XdlmsStatus DecodeEncodedData(
  const std::vector<std::uint8_t>& encodedData,
  dlms::apdu::DlmsData& output)
{
  if (encodedData.empty()) {
    return XdlmsStatus::InvalidArgument;
  }

  return dlms::apdu::DecodeDlmsData(
      &encodedData[0],
      encodedData.size(),
      8,
      output) == dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::EncodeFailed;
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

XdlmsStatus CopyEncodedData(
  const dlms::apdu::DlmsData& data,
  ActionResult& result)
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

XdlmsStatus SendAndReceive(
  dlms::profile::IApduChannel& channel,
  dlms::security::CipheredApduProcessor* security,
  const dlms::apdu::XdlmsApdu& request,
  dlms::apdu::XdlmsApdu& response)
{
  std::vector<std::uint8_t> encodedRequest;
  if (dlms::apdu::EncodeXdlmsApdu(request, encodedRequest) !=
      dlms::apdu::ApduStatus::Ok) {
    return XdlmsStatus::EncodeFailed;
  }

  std::vector<std::uint8_t> outboundRequest = encodedRequest;
  if (security != 0) {
    dlms::security::SecurityByteView plain;
    plain.data = encodedRequest.empty() ? 0 : &encodedRequest[0];
    plain.size = encodedRequest.size();
    if (security->Protect(plain, outboundRequest) !=
        dlms::security::SecurityStatus::Ok) {
      return XdlmsStatus::SecurityFailed;
    }
  }

  dlms::profile::ProfileByteView view = {};
  view.data = outboundRequest.empty() ? 0 : &outboundRequest[0];
  view.size = outboundRequest.size();
  if (channel.SendApdu(view) != dlms::profile::ProfileStatus::Ok) {
    return XdlmsStatus::SendFailed;
  }

  std::vector<std::uint8_t> encodedResponse;
  if (channel.ReceiveApdu(encodedResponse) != dlms::profile::ProfileStatus::Ok) {
    return XdlmsStatus::ReceiveFailed;
  }

  std::vector<std::uint8_t> inboundResponse = encodedResponse;
  if (security != 0) {
    dlms::security::SecurityByteView protectedApdu;
    protectedApdu.data = encodedResponse.empty() ? 0 : &encodedResponse[0];
    protectedApdu.size = encodedResponse.size();
    if (security->Unprotect(protectedApdu, inboundResponse) !=
        dlms::security::SecurityStatus::Ok) {
      return XdlmsStatus::SecurityFailed;
    }
  }

  return dlms::apdu::DecodeXdlmsApdu(
      inboundResponse.empty() ? 0 : &inboundResponse[0],
      inboundResponse.size(),
      response) == dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::DecodeFailed;
}

} // namespace

XdlmsClient::XdlmsClient(
  dlms::profile::IApduChannel& channel,
  dlms::association::AssociationClient& association)
  : channel_(channel)
  , association_(association)
  , security_(0)
  , invokeIds_()
{
}

XdlmsClient::XdlmsClient(
  dlms::profile::IApduChannel& channel,
  dlms::association::AssociationClient& association,
  dlms::security::CipheredApduProcessor& security)
  : channel_(channel)
  , association_(association)
  , security_(&security)
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

  dlms::apdu::XdlmsApdu response;
  status = SendAndReceive(channel_, security_, request, response);
  if (status != XdlmsStatus::Ok) {
    return status;
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

XdlmsStatus XdlmsClient::Set(
  const CosemAttributeDescriptor& descriptor,
  const std::vector<std::uint8_t>& encodedData,
  SetResult& result)
{
  result = EmptySetResult();

  XdlmsStatus status = ValidateDescriptor(descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  dlms::apdu::DlmsData data;
  status = DecodeEncodedData(encodedData, data);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  if (!association_.IsAssociated()) {
    return XdlmsStatus::NotAssociated;
  }

  const std::uint8_t invokeId = invokeIds_.Next();
  const std::uint8_t invokeIdAndPriority =
    MakeInvokeIdAndPriority(invokeId, DefaultServiceOptions());

  dlms::apdu::XdlmsApdu request;
  request.kind = dlms::apdu::XdlmsApduKind::SetRequest;
  request.setRequestAny.choice = dlms::apdu::SetRequestChoice::Normal;
  request.setRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  request.setRequestAny.normal.descriptor = ToApduDescriptor(descriptor);
  request.setRequestAny.normal.hasSelection = false;
  request.setRequestAny.data = data;

  dlms::apdu::XdlmsApdu response;
  status = SendAndReceive(channel_, security_, request, response);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  if (response.kind != dlms::apdu::XdlmsApduKind::SetResponse) {
    return XdlmsStatus::DecodeFailed;
  }

  if (response.setResponseAny.choice != dlms::apdu::SetResponseChoice::Normal) {
    return response.setResponseAny.choice == dlms::apdu::SetResponseChoice::DataBlock ||
        response.setResponseAny.choice == dlms::apdu::SetResponseChoice::LastDataBlock
      ? XdlmsStatus::BlockTransferRequired
      : XdlmsStatus::UnsupportedFeature;
  }

  if ((response.setResponseAny.invokeIdAndPriority & 0x0Fu) != invokeId) {
    return XdlmsStatus::InvokeIdMismatch;
  }

  result.invokeId = invokeId;
  result.accessResult = response.setResponseAny.result;
  return result.accessResult == 0u
    ? XdlmsStatus::Ok
    : XdlmsStatus::ServiceRejected;
}

XdlmsStatus XdlmsClient::Action(
  const CosemMethodDescriptor& descriptor,
  bool hasParameter,
  const std::vector<std::uint8_t>& encodedParameter,
  ActionResult& result)
{
  result = EmptyActionResult();

  XdlmsStatus status = ValidateMethodDescriptor(descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  dlms::apdu::DlmsData parameter;
  if (hasParameter) {
    status = DecodeEncodedData(encodedParameter, parameter);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
  }

  if (!association_.IsAssociated()) {
    return XdlmsStatus::NotAssociated;
  }

  const std::uint8_t invokeId = invokeIds_.Next();
  const std::uint8_t invokeIdAndPriority =
    MakeInvokeIdAndPriority(invokeId, DefaultServiceOptions());

  dlms::apdu::XdlmsApdu request;
  request.kind = dlms::apdu::XdlmsApduKind::ActionRequest;
  request.actionRequestAny.choice = dlms::apdu::ActionRequestChoice::Normal;
  request.actionRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  request.actionRequestAny.normal.descriptor = ToApduDescriptor(descriptor);
  request.actionRequestAny.normal.hasInvocationParameter = hasParameter;
  request.actionRequestAny.normal.invocationParameter = parameter;

  dlms::apdu::XdlmsApdu response;
  status = SendAndReceive(channel_, security_, request, response);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  if (response.kind != dlms::apdu::XdlmsApduKind::ActionResponse) {
    return XdlmsStatus::DecodeFailed;
  }

  if (response.actionResponseAny.choice !=
      dlms::apdu::ActionResponseChoice::Normal) {
    return response.actionResponseAny.choice ==
        dlms::apdu::ActionResponseChoice::WithPblock
      ? XdlmsStatus::BlockTransferRequired
      : XdlmsStatus::UnsupportedFeature;
  }

  if ((response.actionResponseAny.invokeIdAndPriority & 0x0Fu) != invokeId) {
    return XdlmsStatus::InvokeIdMismatch;
  }

  result.invokeId = invokeId;
  result.actionResult = response.actionResponseAny.normal.result;
  if (response.actionResponseAny.normal.hasReturnParameter) {
    status = CopyEncodedData(
      response.actionResponseAny.normal.returnParameter,
      result);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
  }

  return result.actionResult == 0u
    ? XdlmsStatus::Ok
    : XdlmsStatus::ServiceRejected;
}

} // namespace xdlms
} // namespace dlms

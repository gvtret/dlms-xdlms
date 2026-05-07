#include "dlms/xdlms/xdlms_server.hpp"

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

ServiceOptions ParseServiceOptions(std::uint8_t invokeIdAndPriority)
{
  ServiceOptions options;
  options.confirmed = (invokeIdAndPriority & 0x80u) != 0u;
  options.highPriority = (invokeIdAndPriority & 0x40u) != 0u;
  return options;
}

CosemAttributeDescriptor ToXdlmsDescriptor(
  const dlms::apdu::CosemAttributeDescriptor& descriptor)
{
  CosemAttributeDescriptor xdlmsDescriptor;
  xdlmsDescriptor.classId = descriptor.classId;
  xdlmsDescriptor.instanceId = CosemLogicalName(
    descriptor.logicalName[0],
    descriptor.logicalName[1],
    descriptor.logicalName[2],
    descriptor.logicalName[3],
    descriptor.logicalName[4],
    descriptor.logicalName[5]);
  xdlmsDescriptor.attributeId = descriptor.attributeId;
  return xdlmsDescriptor;
}

XdlmsStatus DecodeEncodedData(
  const std::vector<std::uint8_t>& encodedData,
  dlms::apdu::DlmsData& output)
{
  if (encodedData.empty()) {
    return XdlmsStatus::EncodeFailed;
  }

  return dlms::apdu::DecodeDlmsData(
      &encodedData[0],
      encodedData.size(),
      8,
      output) == dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::EncodeFailed;
}

XdlmsStatus EncodeResponse(
  std::uint8_t invokeIdAndPriority,
  const GetResult& result,
  std::vector<std::uint8_t>& responseApdu)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::GetResponse;
  response.getResponseAny.choice = dlms::apdu::GetResponseChoice::Normal;
  response.getResponseAny.invokeIdAndPriority = invokeIdAndPriority;

  if (result.hasAccessResult) {
    response.getResponseAny.result.choice =
      dlms::apdu::GetDataResultChoice::DataAccessError;
    response.getResponseAny.result.dataAccessError = result.accessResult;
  } else if (result.hasData) {
    response.getResponseAny.result.choice =
      dlms::apdu::GetDataResultChoice::Data;
    const XdlmsStatus status =
      DecodeEncodedData(result.data, response.getResponseAny.result.data);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
  } else {
    return XdlmsStatus::InternalError;
  }

  response.getResponse.invokeIdAndPriority = invokeIdAndPriority;
  response.getResponse.resultChoice = response.getResponseAny.result.choice;
  response.getResponse.data = response.getResponseAny.result.data;
  response.getResponse.dataAccessError =
    response.getResponseAny.result.dataAccessError;

  return dlms::apdu::EncodeXdlmsApdu(response, responseApdu) ==
      dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::EncodeFailed;
}

} // namespace

IXdlmsServerHandler::~IXdlmsServerHandler()
{
}

XdlmsServerDispatcher::XdlmsServerDispatcher(IXdlmsServerHandler& handler)
  : handler_(handler)
{
}

XdlmsStatus XdlmsServerDispatcher::DispatchGet(
  const GetIndication& indication,
  GetResult& result)
{
  XdlmsStatus status = ValidateInvokeId(indication.invokeId);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  status = ValidateDescriptor(indication.descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  GetResult handlerResult = EmptyGetResult();
  status = handler_.HandleGet(indication, handlerResult);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  handlerResult.invokeId = indication.invokeId;
  result = handlerResult;
  return XdlmsStatus::Ok;
}

XdlmsServerApduProcessor::XdlmsServerApduProcessor(
  XdlmsServerDispatcher& dispatcher)
  : dispatcher_(dispatcher)
{
}

XdlmsStatus XdlmsServerApduProcessor::ProcessRequest(
  const std::vector<std::uint8_t>& requestApdu,
  std::vector<std::uint8_t>& responseApdu)
{
  responseApdu.clear();

  dlms::apdu::XdlmsApdu request;
  if (dlms::apdu::DecodeXdlmsApdu(
        requestApdu.empty() ? 0 : &requestApdu[0],
        requestApdu.size(),
        request) != dlms::apdu::ApduStatus::Ok) {
    return XdlmsStatus::DecodeFailed;
  }

  if (request.kind != dlms::apdu::XdlmsApduKind::GetRequest) {
    return XdlmsStatus::UnsupportedFeature;
  }

  if (request.getRequestAny.choice != dlms::apdu::GetRequestChoice::Normal) {
    return XdlmsStatus::UnsupportedFeature;
  }

  if (request.getRequest.hasSelectiveAccess) {
    return XdlmsStatus::UnsupportedFeature;
  }

  GetIndication indication = EmptyGetIndication();
  indication.invokeId =
    static_cast<std::uint8_t>(request.getRequest.invokeIdAndPriority & 0x0Fu);
  indication.options =
    ParseServiceOptions(request.getRequest.invokeIdAndPriority);
  indication.descriptor = ToXdlmsDescriptor(request.getRequest.descriptor);

  if (!indication.options.confirmed) {
    return XdlmsStatus::UnsupportedFeature;
  }

  GetResult result = EmptyGetResult();
  const XdlmsStatus status = dispatcher_.DispatchGet(indication, result);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  const std::uint8_t responseInvokeIdAndPriority =
    MakeInvokeIdAndPriority(indication.invokeId, indication.options);
  return EncodeResponse(responseInvokeIdAndPriority, result, responseApdu);
}

GetIndication EmptyGetIndication()
{
  GetIndication indication;
  indication.invokeId = 0u;
  indication.options = DefaultServiceOptions();
  indication.descriptor = EmptyCosemAttributeDescriptor();
  return indication;
}

XdlmsStatus ValidateInvokeId(std::uint8_t invokeId)
{
  return invokeId >= 1u && invokeId <= 15u
    ? XdlmsStatus::Ok
    : XdlmsStatus::InvalidArgument;
}

} // namespace xdlms
} // namespace dlms

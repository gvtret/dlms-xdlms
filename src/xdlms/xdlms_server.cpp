#include "dlms/xdlms/xdlms_server.hpp"

#include "dlms/apdu/data.hpp"
#include "dlms/apdu/get.hpp"
#include "dlms/apdu/set.hpp"
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

XdlmsStatus EncodeDataBytes(
  const dlms::apdu::DlmsData& data,
  std::vector<std::uint8_t>& output)
{
  std::vector<std::uint8_t> buffer(65535u);
  dlms::apdu::ApduWriter writer(&buffer[0], buffer.size());
  if (dlms::apdu::EncodeDlmsData(data, writer) !=
      dlms::apdu::ApduStatus::Ok) {
    return XdlmsStatus::EncodeFailed;
  }

  output.assign(buffer.begin(), buffer.begin() + writer.WrittenSize());
  return XdlmsStatus::Ok;
}

XdlmsStatus EncodeGetResponse(
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

XdlmsStatus EncodeSetResponse(
  std::uint8_t invokeIdAndPriority,
  const SetResult& result,
  std::vector<std::uint8_t>& responseApdu)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::SetResponse;
  response.setResponseAny.choice = dlms::apdu::SetResponseChoice::Normal;
  response.setResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.setResponseAny.result = result.accessResult;
  response.setResponse.invokeIdAndPriority = invokeIdAndPriority;
  response.setResponse.result = result.accessResult;

  return dlms::apdu::EncodeXdlmsApdu(response, responseApdu) ==
      dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::EncodeFailed;
}

XdlmsStatus ProcessGetRequest(
  const dlms::apdu::XdlmsApdu& request,
  XdlmsServerDispatcher& dispatcher,
  std::vector<std::uint8_t>& responseApdu)
{
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
  const XdlmsStatus status = dispatcher.DispatchGet(indication, result);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  const std::uint8_t responseInvokeIdAndPriority =
    MakeInvokeIdAndPriority(indication.invokeId, indication.options);
  return EncodeGetResponse(responseInvokeIdAndPriority, result, responseApdu);
}

XdlmsStatus ProcessSetRequest(
  const dlms::apdu::XdlmsApdu& request,
  XdlmsServerDispatcher& dispatcher,
  std::vector<std::uint8_t>& responseApdu)
{
  if (request.setRequestAny.choice != dlms::apdu::SetRequestChoice::Normal) {
    return XdlmsStatus::UnsupportedFeature;
  }

  if (request.setRequest.hasSelectiveAccess) {
    return XdlmsStatus::UnsupportedFeature;
  }

  SetIndication indication = EmptySetIndication();
  indication.invokeId =
    static_cast<std::uint8_t>(request.setRequest.invokeIdAndPriority & 0x0Fu);
  indication.options =
    ParseServiceOptions(request.setRequest.invokeIdAndPriority);
  indication.descriptor = ToXdlmsDescriptor(request.setRequest.descriptor);

  if (!indication.options.confirmed) {
    return XdlmsStatus::UnsupportedFeature;
  }

  XdlmsStatus status = EncodeDataBytes(request.setRequest.data, indication.data);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  SetResult result = EmptySetResult();
  status = dispatcher.DispatchSet(indication, result);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  const std::uint8_t responseInvokeIdAndPriority =
    MakeInvokeIdAndPriority(indication.invokeId, indication.options);
  return EncodeSetResponse(responseInvokeIdAndPriority, result, responseApdu);
}

} // namespace

IXdlmsServerHandler::~IXdlmsServerHandler()
{
}

XdlmsStatus IXdlmsServerHandler::HandleSet(
  const SetIndication& indication,
  SetResult& result)
{
  (void)indication;
  (void)result;
  return XdlmsStatus::UnsupportedFeature;
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

XdlmsStatus XdlmsServerDispatcher::DispatchSet(
  const SetIndication& indication,
  SetResult& result)
{
  XdlmsStatus status = ValidateInvokeId(indication.invokeId);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  status = ValidateDescriptor(indication.descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  if (indication.data.empty()) {
    return XdlmsStatus::InvalidArgument;
  }

  SetResult handlerResult = EmptySetResult();
  status = handler_.HandleSet(indication, handlerResult);
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

  switch (request.kind) {
    case dlms::apdu::XdlmsApduKind::GetRequest:
      return ProcessGetRequest(request, dispatcher_, responseApdu);

    case dlms::apdu::XdlmsApduKind::SetRequest:
      return ProcessSetRequest(request, dispatcher_, responseApdu);

    default:
      return XdlmsStatus::UnsupportedFeature;
  }
}

GetIndication EmptyGetIndication()
{
  GetIndication indication;
  indication.invokeId = 0u;
  indication.options = DefaultServiceOptions();
  indication.descriptor = EmptyCosemAttributeDescriptor();
  return indication;
}

SetIndication EmptySetIndication()
{
  SetIndication indication;
  indication.invokeId = 0u;
  indication.options = DefaultServiceOptions();
  indication.descriptor = EmptyCosemAttributeDescriptor();
  indication.data.clear();
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

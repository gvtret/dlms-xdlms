#include "dlms/xdlms/xdlms_server.hpp"

#include "dlms/apdu/action.hpp"
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

ServiceOptions ParseServiceOptions(std::uint8_t invokeIdAndPriority)
{
  ServiceOptions options = DefaultServiceOptions();
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

CosemMethodDescriptor ToXdlmsDescriptor(
  const dlms::apdu::CosemMethodDescriptor& descriptor)
{
  CosemMethodDescriptor xdlmsDescriptor;
  xdlmsDescriptor.classId = descriptor.classId;
  xdlmsDescriptor.instanceId = CosemLogicalName(
    descriptor.logicalName[0],
    descriptor.logicalName[1],
    descriptor.logicalName[2],
    descriptor.logicalName[3],
    descriptor.logicalName[4],
    descriptor.logicalName[5]);
  xdlmsDescriptor.methodId = descriptor.methodId;
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

XdlmsStatus ValidateEncodedActionParameter(
  const std::vector<std::uint8_t>& encodedData)
{
  if (encodedData.empty()) {
    return XdlmsStatus::DecodeFailed;
  }

  dlms::apdu::DlmsData data;
  return dlms::apdu::DecodeDlmsData(
      &encodedData[0],
      encodedData.size(),
      8,
      data) == dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::DecodeFailed;
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

XdlmsStatus EncodeActionResponse(
  std::uint8_t invokeIdAndPriority,
  const ActionResult& result,
  std::vector<std::uint8_t>& responseApdu)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::ActionResponse;
  response.actionResponseAny.choice =
    dlms::apdu::ActionResponseChoice::Normal;
  response.actionResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.actionResponseAny.normal.result = result.actionResult;
  response.actionResponseAny.normal.hasReturnParameter = result.hasData;

  if (result.hasData) {
    const XdlmsStatus status =
      DecodeEncodedData(result.data,
                        response.actionResponseAny.normal.returnParameter);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
  }

  response.actionResponse.invokeIdAndPriority = invokeIdAndPriority;
  response.actionResponse.result = result.actionResult;
  response.actionResponse.hasReturnParameter = result.hasData;
  response.actionResponse.returnParameter =
    response.actionResponseAny.normal.returnParameter;

  return dlms::apdu::EncodeXdlmsApdu(response, responseApdu) ==
      dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::EncodeFailed;
}

XdlmsStatus EncodeActionNextPblockResponse(
  std::uint8_t invokeIdAndPriority,
  std::uint32_t blockNumber,
  std::vector<std::uint8_t>& responseApdu)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::ActionResponse;
  response.actionResponseAny.choice =
    dlms::apdu::ActionResponseChoice::NextPblock;
  response.actionResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.actionResponseAny.blockNumber = blockNumber;

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

XdlmsStatus ProcessActionRequest(
  const dlms::apdu::XdlmsApdu& request,
  XdlmsServerDispatcher& dispatcher,
  ActionRequestBlockState& actionBlocks,
  std::vector<std::uint8_t>& responseApdu)
{
  if (request.actionRequestAny.choice ==
      dlms::apdu::ActionRequestChoice::WithFirstPblock) {
    if (actionBlocks.active) {
      actionBlocks = EmptyActionRequestBlockState();
      return XdlmsStatus::DecodeFailed;
    }
    if (request.actionRequestAny.dataBlock.blockNumber != 1u) {
      return XdlmsStatus::DecodeFailed;
    }

    const std::uint8_t invokeId = static_cast<std::uint8_t>(
      request.actionRequestAny.invokeIdAndPriority & 0x0Fu);
    const ServiceOptions options =
      ParseServiceOptions(request.actionRequestAny.invokeIdAndPriority);
    if (!options.confirmed) {
      return XdlmsStatus::UnsupportedFeature;
    }

    if (request.actionRequestAny.dataBlock.rawData.size >
        options.maxBlockTransferBytes) {
      return XdlmsStatus::DecodeFailed;
    }
    if (request.actionRequestAny.dataBlock.rawData.size != 0u &&
        request.actionRequestAny.dataBlock.rawData.data == 0) {
      return XdlmsStatus::DecodeFailed;
    }

    std::vector<std::uint8_t> data;
    if (request.actionRequestAny.dataBlock.rawData.size != 0u) {
      data.assign(
        request.actionRequestAny.dataBlock.rawData.data,
        request.actionRequestAny.dataBlock.rawData.data +
          request.actionRequestAny.dataBlock.rawData.size);
    }

    if (!request.actionRequestAny.dataBlock.lastBlock) {
      actionBlocks.active = true;
      actionBlocks.invokeId = invokeId;
      actionBlocks.options = options;
      actionBlocks.descriptor =
        ToXdlmsDescriptor(request.actionRequestAny.normal.descriptor);
      actionBlocks.nextBlockNumber = 2u;
      actionBlocks.data = data;
      return EncodeActionNextPblockResponse(
        request.actionRequestAny.invokeIdAndPriority,
        1u,
        responseApdu);
    }

    XdlmsStatus status = ValidateEncodedActionParameter(data);
    if (status != XdlmsStatus::Ok) {
      return status;
    }

    ActionIndication indication = EmptyActionIndication();
    indication.invokeId = invokeId;
    indication.options = options;
    indication.descriptor =
      ToXdlmsDescriptor(request.actionRequestAny.normal.descriptor);
    indication.hasParameter = true;
    indication.parameter = data;

    ActionResult result = EmptyActionResult();
    status = dispatcher.DispatchAction(indication, result);
    if (status != XdlmsStatus::Ok) {
      return status;
    }

    const std::uint8_t responseInvokeIdAndPriority =
      MakeInvokeIdAndPriority(indication.invokeId, indication.options);
    return EncodeActionResponse(responseInvokeIdAndPriority, result, responseApdu);
  }

  if (request.actionRequestAny.choice ==
      dlms::apdu::ActionRequestChoice::WithPblock) {
    if (!actionBlocks.active) {
      return XdlmsStatus::DecodeFailed;
    }
    if ((request.actionRequestAny.invokeIdAndPriority & 0x0Fu) !=
        actionBlocks.invokeId) {
      actionBlocks = EmptyActionRequestBlockState();
      return XdlmsStatus::InvokeIdMismatch;
    }
    if (request.actionRequestAny.dataBlock.blockNumber !=
        actionBlocks.nextBlockNumber) {
      actionBlocks = EmptyActionRequestBlockState();
      return XdlmsStatus::DecodeFailed;
    }
    if (request.actionRequestAny.dataBlock.rawData.size >
          actionBlocks.options.maxBlockTransferBytes ||
        actionBlocks.data.size() >
          actionBlocks.options.maxBlockTransferBytes -
          request.actionRequestAny.dataBlock.rawData.size) {
      actionBlocks = EmptyActionRequestBlockState();
      return XdlmsStatus::DecodeFailed;
    }
    if (request.actionRequestAny.dataBlock.rawData.size != 0u &&
        request.actionRequestAny.dataBlock.rawData.data == 0) {
      actionBlocks = EmptyActionRequestBlockState();
      return XdlmsStatus::DecodeFailed;
    }

    if (request.actionRequestAny.dataBlock.rawData.size != 0u) {
      actionBlocks.data.insert(
        actionBlocks.data.end(),
        request.actionRequestAny.dataBlock.rawData.data,
        request.actionRequestAny.dataBlock.rawData.data +
          request.actionRequestAny.dataBlock.rawData.size);
    }

    const std::uint32_t acceptedBlock =
      request.actionRequestAny.dataBlock.blockNumber;
    if (!request.actionRequestAny.dataBlock.lastBlock) {
      ++actionBlocks.nextBlockNumber;
      return EncodeActionNextPblockResponse(
        request.actionRequestAny.invokeIdAndPriority,
        acceptedBlock,
        responseApdu);
    }

    ActionIndication indication = EmptyActionIndication();
    indication.invokeId = actionBlocks.invokeId;
    indication.options = actionBlocks.options;
    indication.descriptor = actionBlocks.descriptor;
    indication.hasParameter = true;
    indication.parameter = actionBlocks.data;
    actionBlocks = EmptyActionRequestBlockState();

    XdlmsStatus status =
      ValidateEncodedActionParameter(indication.parameter);
    if (status != XdlmsStatus::Ok) {
      return status;
    }

    ActionResult result = EmptyActionResult();
    status = dispatcher.DispatchAction(indication, result);
    if (status != XdlmsStatus::Ok) {
      return status;
    }

    const std::uint8_t responseInvokeIdAndPriority =
      MakeInvokeIdAndPriority(indication.invokeId, indication.options);
    return EncodeActionResponse(responseInvokeIdAndPriority, result, responseApdu);
  }

  if (request.actionRequestAny.choice !=
      dlms::apdu::ActionRequestChoice::Normal) {
    return XdlmsStatus::UnsupportedFeature;
  }

  ActionIndication indication = EmptyActionIndication();
  indication.invokeId = static_cast<std::uint8_t>(
    request.actionRequest.invokeIdAndPriority & 0x0Fu);
  indication.options =
    ParseServiceOptions(request.actionRequest.invokeIdAndPriority);
  indication.descriptor = ToXdlmsDescriptor(request.actionRequest.descriptor);
  indication.hasParameter = request.actionRequest.hasInvocationParameter;

  if (!indication.options.confirmed) {
    return XdlmsStatus::UnsupportedFeature;
  }

  if (indication.hasParameter) {
    const XdlmsStatus status =
      EncodeDataBytes(request.actionRequest.invocationParameter,
                      indication.parameter);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
  }

  ActionResult result = EmptyActionResult();
  const XdlmsStatus status = dispatcher.DispatchAction(indication, result);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  const std::uint8_t responseInvokeIdAndPriority =
    MakeInvokeIdAndPriority(indication.invokeId, indication.options);
  return EncodeActionResponse(
    responseInvokeIdAndPriority,
    result,
    responseApdu);
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

XdlmsStatus IXdlmsServerHandler::HandleAction(
  const ActionIndication& indication,
  ActionResult& result)
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

XdlmsStatus XdlmsServerDispatcher::DispatchAction(
  const ActionIndication& indication,
  ActionResult& result)
{
  XdlmsStatus status = ValidateInvokeId(indication.invokeId);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  status = ValidateMethodDescriptor(indication.descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  if (indication.hasParameter && indication.parameter.empty()) {
    return XdlmsStatus::InvalidArgument;
  }

  ActionResult handlerResult = EmptyActionResult();
  status = handler_.HandleAction(indication, handlerResult);
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
  , security_(0)
  , actionBlocks_(EmptyActionRequestBlockState())
{
}

XdlmsServerApduProcessor::XdlmsServerApduProcessor(
  XdlmsServerDispatcher& dispatcher,
  dlms::security::CipheredApduProcessor& security)
  : dispatcher_(dispatcher)
  , security_(&security)
  , actionBlocks_(EmptyActionRequestBlockState())
{
}

XdlmsStatus XdlmsServerApduProcessor::ProcessRequest(
  const std::vector<std::uint8_t>& requestApdu,
  std::vector<std::uint8_t>& responseApdu)
{
  responseApdu.clear();

  std::vector<std::uint8_t> plainRequest = requestApdu;
  if (security_ != 0) {
    dlms::security::SecurityByteView protectedApdu;
    protectedApdu.data = requestApdu.empty() ? 0 : &requestApdu[0];
    protectedApdu.size = requestApdu.size();
    if (security_->Unprotect(protectedApdu, plainRequest) !=
        dlms::security::SecurityStatus::Ok) {
      return XdlmsStatus::SecurityFailed;
    }
  }

  dlms::apdu::XdlmsApdu request;
  if (dlms::apdu::DecodeXdlmsApdu(
        plainRequest.empty() ? 0 : &plainRequest[0],
        plainRequest.size(),
        request) != dlms::apdu::ApduStatus::Ok) {
    return XdlmsStatus::DecodeFailed;
  }

  XdlmsStatus status = XdlmsStatus::UnsupportedFeature;
  switch (request.kind) {
    case dlms::apdu::XdlmsApduKind::GetRequest:
      status = ProcessGetRequest(request, dispatcher_, responseApdu);
      break;

    case dlms::apdu::XdlmsApduKind::SetRequest:
      status = ProcessSetRequest(request, dispatcher_, responseApdu);
      break;

    case dlms::apdu::XdlmsApduKind::ActionRequest:
      status = ProcessActionRequest(
        request,
        dispatcher_,
        actionBlocks_,
        responseApdu);
      break;

    default:
      return XdlmsStatus::UnsupportedFeature;
  }

  if (status != XdlmsStatus::Ok || security_ == 0) {
    return status;
  }

  std::vector<std::uint8_t> plainResponse = responseApdu;
  dlms::security::SecurityByteView plain;
  plain.data = plainResponse.empty() ? 0 : &plainResponse[0];
  plain.size = plainResponse.size();
  if (security_->Protect(plain, responseApdu) !=
      dlms::security::SecurityStatus::Ok) {
    responseApdu.clear();
    return XdlmsStatus::SecurityFailed;
  }

  return XdlmsStatus::Ok;
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

ActionIndication EmptyActionIndication()
{
  ActionIndication indication;
  indication.invokeId = 0u;
  indication.options = DefaultServiceOptions();
  indication.descriptor = EmptyCosemMethodDescriptor();
  indication.hasParameter = false;
  indication.parameter.clear();
  return indication;
}

ActionRequestBlockState EmptyActionRequestBlockState()
{
  ActionRequestBlockState state;
  state.active = false;
  state.invokeId = 0u;
  state.options = DefaultServiceOptions();
  state.descriptor = EmptyCosemMethodDescriptor();
  state.nextBlockNumber = 1u;
  state.data.clear();
  return state;
}

XdlmsStatus ValidateInvokeId(std::uint8_t invokeId)
{
  return invokeId >= 1u && invokeId <= 15u
    ? XdlmsStatus::Ok
    : XdlmsStatus::InvalidArgument;
}

} // namespace xdlms
} // namespace dlms

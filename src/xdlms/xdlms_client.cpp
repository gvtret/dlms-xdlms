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
  std::vector<std::uint8_t>& decodedResponseBytes,
  dlms::apdu::XdlmsApdu& response)
{
  decodedResponseBytes.clear();

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

  decodedResponseBytes = inboundResponse;
  return dlms::apdu::DecodeXdlmsApdu(
      decodedResponseBytes.empty() ? 0 : &decodedResponseBytes[0],
      decodedResponseBytes.size(),
      response) == dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::DecodeFailed;
}

class BlockTransferManager
{
public:
  explicit BlockTransferManager(std::size_t maxBytes)
    : maxBytes_(maxBytes)
    , nextBlockNumber_(1u)
  {
  }

  XdlmsStatus AppendBlock(const dlms::apdu::DataBlockG& block)
  {
    if (block.blockNumber != nextBlockNumber_) {
      return XdlmsStatus::DecodeFailed;
    }
    if (block.rawData.size > maxBytes_ ||
        data_.size() > maxBytes_ - block.rawData.size) {
      return XdlmsStatus::DecodeFailed;
    }
    if (block.rawData.size != 0u && block.rawData.data == 0) {
      return XdlmsStatus::DecodeFailed;
    }

    if (block.rawData.size != 0u) {
      data_.insert(
        data_.end(),
        block.rawData.data,
        block.rawData.data + block.rawData.size);
    }
    ++nextBlockNumber_;
    return XdlmsStatus::Ok;
  }

  const std::vector<std::uint8_t>& Data() const
  {
    return data_;
  }

private:
  std::size_t maxBytes_;
  std::uint32_t nextBlockNumber_;
  std::vector<std::uint8_t> data_;
};

dlms::apdu::XdlmsApdu MakeGetRequestNext(
  std::uint8_t invokeIdAndPriority,
  std::uint32_t blockNumber)
{
  dlms::apdu::XdlmsApdu request;
  request.kind = dlms::apdu::XdlmsApduKind::GetRequest;
  request.getRequestAny.choice = dlms::apdu::GetRequestChoice::Next;
  request.getRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  request.getRequestAny.blockNumber = blockNumber;
  return request;
}

dlms::apdu::XdlmsApdu MakeActionRequestNextPblock(
  std::uint8_t invokeIdAndPriority,
  std::uint32_t blockNumber)
{
  dlms::apdu::XdlmsApdu request;
  request.kind = dlms::apdu::XdlmsApduKind::ActionRequest;
  request.actionRequestAny.choice =
    dlms::apdu::ActionRequestChoice::NextPblock;
  request.actionRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  request.actionRequestAny.blockNumber = blockNumber;
  return request;
}

dlms::apdu::XdlmsApdu MakeActionRequestBlock(
  dlms::apdu::ActionRequestChoice choice,
  std::uint8_t invokeIdAndPriority,
  const CosemMethodDescriptor& descriptor,
  std::uint32_t blockNumber,
  bool lastBlock,
  const std::uint8_t* data,
  std::size_t size)
{
  dlms::apdu::XdlmsApdu request;
  request.kind = dlms::apdu::XdlmsApduKind::ActionRequest;
  request.actionRequestAny.choice = choice;
  request.actionRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  request.actionRequestAny.normal.descriptor = ToApduDescriptor(descriptor);
  request.actionRequestAny.dataBlock.lastBlock = lastBlock;
  request.actionRequestAny.dataBlock.blockNumber = blockNumber;
  request.actionRequestAny.dataBlock.rawData.data = data;
  request.actionRequestAny.dataBlock.rawData.size = size;
  return request;
}

XdlmsStatus ReceiveGetResponse(
  dlms::profile::IApduChannel& channel,
  dlms::security::CipheredApduProcessor* security,
  const dlms::apdu::XdlmsApdu& request,
  std::vector<std::uint8_t>& decodedResponseBytes,
  dlms::apdu::XdlmsApdu& response)
{
  const XdlmsStatus status =
    SendAndReceive(channel, security, request, decodedResponseBytes, response);
  if (status != XdlmsStatus::Ok) {
    return status;
  }
  return response.kind == dlms::apdu::XdlmsApduKind::GetResponse
    ? XdlmsStatus::Ok
    : XdlmsStatus::DecodeFailed;
}

XdlmsStatus ReceiveActionResponse(
  dlms::profile::IApduChannel& channel,
  dlms::security::CipheredApduProcessor* security,
  const dlms::apdu::XdlmsApdu& request,
  std::vector<std::uint8_t>& decodedResponseBytes,
  dlms::apdu::XdlmsApdu& response)
{
  const XdlmsStatus status =
    SendAndReceive(channel, security, request, decodedResponseBytes, response);
  if (status != XdlmsStatus::Ok) {
    return status;
  }
  return response.kind == dlms::apdu::XdlmsApduKind::ActionResponse
    ? XdlmsStatus::Ok
    : XdlmsStatus::DecodeFailed;
}

dlms::apdu::XdlmsApdu MakeSetRequestBlock(
  dlms::apdu::SetRequestChoice choice,
  std::uint8_t invokeIdAndPriority,
  const CosemAttributeDescriptor& descriptor,
  std::uint32_t blockNumber,
  bool lastBlock,
  const std::uint8_t* data,
  std::size_t size)
{
  dlms::apdu::XdlmsApdu request;
  request.kind = dlms::apdu::XdlmsApduKind::SetRequest;
  request.setRequestAny.choice = choice;
  request.setRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  request.setRequestAny.normal.descriptor = ToApduDescriptor(descriptor);
  request.setRequestAny.normal.hasSelection = false;
  request.setRequestAny.dataBlock.lastBlock = lastBlock;
  request.setRequestAny.dataBlock.blockNumber = blockNumber;
  request.setRequestAny.dataBlock.rawData.data = data;
  request.setRequestAny.dataBlock.rawData.size = size;
  return request;
}

XdlmsStatus ValidateSetBlockResponse(
  const dlms::apdu::XdlmsApdu& response,
  std::uint8_t invokeId,
  std::uint32_t expectedBlockNumber,
  bool finalBlock,
  SetResult& result)
{
  if (response.kind != dlms::apdu::XdlmsApduKind::SetResponse) {
    return XdlmsStatus::DecodeFailed;
  }
  if ((response.setResponseAny.invokeIdAndPriority & 0x0Fu) != invokeId) {
    return XdlmsStatus::InvokeIdMismatch;
  }
  if (response.setResponseAny.blockNumber != expectedBlockNumber) {
    return XdlmsStatus::DecodeFailed;
  }

  if (!finalBlock) {
    return response.setResponseAny.choice ==
        dlms::apdu::SetResponseChoice::DataBlock
      ? XdlmsStatus::Ok
      : XdlmsStatus::DecodeFailed;
  }

  if (response.setResponseAny.choice !=
      dlms::apdu::SetResponseChoice::LastDataBlock) {
    return XdlmsStatus::DecodeFailed;
  }

  result.invokeId = invokeId;
  result.accessResult = response.setResponseAny.result;
  return result.accessResult == 0u
    ? XdlmsStatus::Ok
    : XdlmsStatus::ServiceRejected;
}

XdlmsStatus ValidateActionNextPblockResponse(
  const dlms::apdu::XdlmsApdu& response,
  std::uint8_t invokeId,
  std::uint32_t expectedBlockNumber)
{
  if (response.kind != dlms::apdu::XdlmsApduKind::ActionResponse) {
    return XdlmsStatus::DecodeFailed;
  }
  if ((response.actionResponseAny.invokeIdAndPriority & 0x0Fu) != invokeId) {
    return XdlmsStatus::InvokeIdMismatch;
  }
  if (response.actionResponseAny.choice !=
      dlms::apdu::ActionResponseChoice::NextPblock) {
    return XdlmsStatus::DecodeFailed;
  }
  return response.actionResponseAny.blockNumber == expectedBlockNumber
    ? XdlmsStatus::Ok
    : XdlmsStatus::DecodeFailed;
}

XdlmsStatus CopyActionResponse(
  const dlms::apdu::ActionResponse& response,
  std::uint8_t invokeId,
  ActionResult& result)
{
  if ((response.invokeIdAndPriority & 0x0Fu) != invokeId) {
    return XdlmsStatus::InvokeIdMismatch;
  }

  result.invokeId = invokeId;
  result.actionResult = response.normal.result;
  if (response.normal.hasReturnParameter) {
    const XdlmsStatus status = CopyEncodedData(
      response.normal.returnParameter,
      result);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
  }

  return result.actionResult == 0u
    ? XdlmsStatus::Ok
    : XdlmsStatus::ServiceRejected;
}

XdlmsStatus DecodeActionBlockPayload(
  std::uint8_t invokeIdAndPriority,
  const std::vector<std::uint8_t>& payload,
  dlms::apdu::XdlmsApdu& response)
{
  std::vector<std::uint8_t> bytes;
  bytes.reserve(payload.size() + 3u);
  bytes.push_back(0xC7u);
  bytes.push_back(0x01u);
  bytes.push_back(invokeIdAndPriority);
  bytes.insert(bytes.end(), payload.begin(), payload.end());

  return dlms::apdu::DecodeXdlmsApdu(&bytes[0], bytes.size(), response) ==
      dlms::apdu::ApduStatus::Ok
    ? XdlmsStatus::Ok
    : XdlmsStatus::DecodeFailed;
}

XdlmsStatus CopyFinalActionResponse(
  dlms::profile::IApduChannel& channel,
  dlms::security::CipheredApduProcessor* security,
  std::uint8_t invokeId,
  std::uint8_t invokeIdAndPriority,
  const ServiceOptions& options,
  dlms::apdu::XdlmsApdu& response,
  ActionResult& result)
{
  if (response.actionResponseAny.choice !=
      dlms::apdu::ActionResponseChoice::Normal) {
    if (response.actionResponseAny.choice !=
        dlms::apdu::ActionResponseChoice::WithPblock) {
      return XdlmsStatus::UnsupportedFeature;
    }
    if (!options.allowBlockTransfer) {
      return XdlmsStatus::BlockTransferRequired;
    }

    BlockTransferManager blocks(options.maxBlockTransferBytes);
    std::vector<std::uint8_t> decodedResponseBytes;
    for (;;) {
      if ((response.actionResponseAny.invokeIdAndPriority & 0x0Fu) != invokeId) {
        return XdlmsStatus::InvokeIdMismatch;
      }

      XdlmsStatus status = blocks.AppendBlock(response.actionResponseAny.dataBlock);
      if (status != XdlmsStatus::Ok) {
        return status;
      }
      if (response.actionResponseAny.dataBlock.lastBlock) {
        response = dlms::apdu::XdlmsApdu();
        status = DecodeActionBlockPayload(
          invokeIdAndPriority,
          blocks.Data(),
          response);
        if (status != XdlmsStatus::Ok) {
          return status;
        }
        return CopyActionResponse(response.actionResponseAny, invokeId, result);
      }

      const std::uint32_t acknowledgedBlock =
        response.actionResponseAny.dataBlock.blockNumber;
      response = dlms::apdu::XdlmsApdu();
      decodedResponseBytes.clear();
      status = ReceiveActionResponse(
        channel,
        security,
        MakeActionRequestNextPblock(invokeIdAndPriority, acknowledgedBlock),
        decodedResponseBytes,
        response);
      if (status != XdlmsStatus::Ok) {
        return status;
      }
      if (response.actionResponseAny.choice !=
          dlms::apdu::ActionResponseChoice::WithPblock) {
        return XdlmsStatus::DecodeFailed;
      }
    }
  }

  const XdlmsStatus status =
    CopyActionResponse(response.actionResponseAny, invokeId, result);
  return status == XdlmsStatus::Ok ? XdlmsStatus::Ok : status;
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
  return Get(descriptor, DefaultServiceOptions(), result);
}

XdlmsStatus XdlmsClient::Get(
  const CosemAttributeDescriptor& descriptor,
  const ServiceOptions& options,
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
    MakeInvokeIdAndPriority(invokeId, options);

  dlms::apdu::XdlmsApdu request = dlms::apdu::MakeGetRequestNormal(
    invokeIdAndPriority,
    descriptor.classId,
    ToApduLogicalName(descriptor.instanceId),
    descriptor.attributeId);

  std::vector<std::uint8_t> decodedResponseBytes;
  dlms::apdu::XdlmsApdu response;
  status = ReceiveGetResponse(
    channel_,
    security_,
    request,
    decodedResponseBytes,
    response);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  if (response.getResponseAny.choice != dlms::apdu::GetResponseChoice::Normal) {
    if (response.getResponseAny.choice !=
        dlms::apdu::GetResponseChoice::WithDataBlock) {
      return XdlmsStatus::UnsupportedFeature;
    }
    if (!options.allowBlockTransfer) {
      return XdlmsStatus::BlockTransferRequired;
    }

    BlockTransferManager blocks(options.maxBlockTransferBytes);
    for (;;) {
      if ((response.getResponseAny.invokeIdAndPriority & 0x0Fu) != invokeId) {
        return XdlmsStatus::InvokeIdMismatch;
      }

      status = blocks.AppendBlock(response.getResponseAny.dataBlock);
      if (status != XdlmsStatus::Ok) {
        return status;
      }
      if (response.getResponseAny.dataBlock.lastBlock) {
        result.invokeId = invokeId;
        result.data = blocks.Data();
        result.hasData = true;
        return XdlmsStatus::Ok;
      }

      const std::uint32_t acknowledgedBlock =
        response.getResponseAny.dataBlock.blockNumber;
      response = dlms::apdu::XdlmsApdu();
      decodedResponseBytes.clear();
      status = ReceiveGetResponse(
        channel_,
        security_,
        MakeGetRequestNext(invokeIdAndPriority, acknowledgedBlock),
        decodedResponseBytes,
        response);
      if (status != XdlmsStatus::Ok) {
        return status;
      }
      if (response.getResponseAny.choice !=
          dlms::apdu::GetResponseChoice::WithDataBlock) {
        return XdlmsStatus::DecodeFailed;
      }
    }
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
  return Set(descriptor, encodedData, DefaultServiceOptions(), result);
}

XdlmsStatus XdlmsClient::Set(
  const CosemAttributeDescriptor& descriptor,
  const std::vector<std::uint8_t>& encodedData,
  const ServiceOptions& options,
  SetResult& result)
{
  result = EmptySetResult();

  XdlmsStatus status = ValidateDescriptor(descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  const bool useBlocks =
    encodedData.size() > options.maxSetBlockPayloadBytes;
  if (!useBlocks) {
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
      MakeInvokeIdAndPriority(invokeId, options);

    dlms::apdu::XdlmsApdu request;
    request.kind = dlms::apdu::XdlmsApduKind::SetRequest;
    request.setRequestAny.choice = dlms::apdu::SetRequestChoice::Normal;
    request.setRequestAny.invokeIdAndPriority = invokeIdAndPriority;
    request.setRequestAny.normal.descriptor = ToApduDescriptor(descriptor);
    request.setRequestAny.normal.hasSelection = false;
    request.setRequestAny.data = data;

    dlms::apdu::XdlmsApdu response;
    std::vector<std::uint8_t> decodedResponseBytes;
    status = SendAndReceive(
      channel_,
      security_,
      request,
      decodedResponseBytes,
      response);
    if (status != XdlmsStatus::Ok) {
      return status;
    }

    if (response.kind != dlms::apdu::XdlmsApduKind::SetResponse) {
      return XdlmsStatus::DecodeFailed;
    }

    if (response.setResponseAny.choice !=
        dlms::apdu::SetResponseChoice::Normal) {
      return response.setResponseAny.choice ==
          dlms::apdu::SetResponseChoice::DataBlock ||
          response.setResponseAny.choice ==
          dlms::apdu::SetResponseChoice::LastDataBlock
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

  if (encodedData.empty() || options.maxSetBlockPayloadBytes == 0u) {
    return XdlmsStatus::InvalidArgument;
  }
  if (!options.allowBlockTransfer) {
    return XdlmsStatus::BlockTransferRequired;
  }
  if (!association_.IsAssociated()) {
    return XdlmsStatus::NotAssociated;
  }

  const std::uint8_t invokeId = invokeIds_.Next();
  const std::uint8_t invokeIdAndPriority =
    MakeInvokeIdAndPriority(invokeId, options);
  std::size_t offset = 0u;
  std::uint32_t blockNumber = 1u;
  while (offset < encodedData.size()) {
    const std::size_t remaining = encodedData.size() - offset;
    const std::size_t blockSize =
      remaining < options.maxSetBlockPayloadBytes
        ? remaining
        : options.maxSetBlockPayloadBytes;
    const bool finalBlock = offset + blockSize == encodedData.size();
    const dlms::apdu::SetRequestChoice choice = blockNumber == 1u
      ? dlms::apdu::SetRequestChoice::WithFirstDataBlock
      : dlms::apdu::SetRequestChoice::WithDataBlock;
    const dlms::apdu::XdlmsApdu request = MakeSetRequestBlock(
      choice,
      invokeIdAndPriority,
      descriptor,
      blockNumber,
      finalBlock,
      &encodedData[offset],
      blockSize);

    dlms::apdu::XdlmsApdu response;
    std::vector<std::uint8_t> decodedResponseBytes;
    status = SendAndReceive(
      channel_,
      security_,
      request,
      decodedResponseBytes,
      response);
    if (status != XdlmsStatus::Ok) {
      return status;
    }

    status = ValidateSetBlockResponse(
      response,
      invokeId,
      blockNumber,
      finalBlock,
      result);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
    if (finalBlock) {
      return XdlmsStatus::Ok;
    }

    offset += blockSize;
    ++blockNumber;
  }

  return XdlmsStatus::InternalError;
}

XdlmsStatus XdlmsClient::Action(
  const CosemMethodDescriptor& descriptor,
  bool hasParameter,
  const std::vector<std::uint8_t>& encodedParameter,
  ActionResult& result)
{
  return Action(
    descriptor,
    hasParameter,
    encodedParameter,
    DefaultServiceOptions(),
    result);
}

XdlmsStatus XdlmsClient::Action(
  const CosemMethodDescriptor& descriptor,
  bool hasParameter,
  const std::vector<std::uint8_t>& encodedParameter,
  const ServiceOptions& options,
  ActionResult& result)
{
  result = EmptyActionResult();

  XdlmsStatus status = ValidateMethodDescriptor(descriptor);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  const bool useBlocks =
    hasParameter &&
    encodedParameter.size() > options.maxActionBlockPayloadBytes;
  dlms::apdu::DlmsData parameter;
  if (hasParameter && !useBlocks) {
    status = DecodeEncodedData(encodedParameter, parameter);
    if (status != XdlmsStatus::Ok) {
      return status;
    }
  }

  if (useBlocks && options.maxActionBlockPayloadBytes == 0u) {
    return XdlmsStatus::InvalidArgument;
  }
  if (useBlocks && !options.allowBlockTransfer) {
    return XdlmsStatus::BlockTransferRequired;
  }

  if (!association_.IsAssociated()) {
    return XdlmsStatus::NotAssociated;
  }

  const std::uint8_t invokeId = invokeIds_.Next();
  const std::uint8_t invokeIdAndPriority =
    MakeInvokeIdAndPriority(invokeId, options);

  if (useBlocks) {
    std::size_t offset = 0u;
    std::uint32_t blockNumber = 1u;
    for (;;) {
      const std::size_t remaining = encodedParameter.size() - offset;
      const std::size_t blockSize =
        remaining < options.maxActionBlockPayloadBytes
          ? remaining
          : options.maxActionBlockPayloadBytes;
      const bool finalBlock = offset + blockSize == encodedParameter.size();
      const dlms::apdu::ActionRequestChoice choice = blockNumber == 1u
        ? dlms::apdu::ActionRequestChoice::WithFirstPblock
        : dlms::apdu::ActionRequestChoice::WithPblock;
      const dlms::apdu::XdlmsApdu requestBlock = MakeActionRequestBlock(
        choice,
        invokeIdAndPriority,
        descriptor,
        blockNumber,
        finalBlock,
        &encodedParameter[offset],
        blockSize);

      dlms::apdu::XdlmsApdu response;
      std::vector<std::uint8_t> decodedResponseBytes;
      status = ReceiveActionResponse(
        channel_,
        security_,
        requestBlock,
        decodedResponseBytes,
        response);
      if (status != XdlmsStatus::Ok) {
        return status;
      }

      if (finalBlock) {
        return CopyFinalActionResponse(
          channel_,
          security_,
          invokeId,
          invokeIdAndPriority,
          options,
          response,
          result);
      }

      status = ValidateActionNextPblockResponse(
        response,
        invokeId,
        blockNumber);
      if (status != XdlmsStatus::Ok) {
        return status;
      }

      offset += blockSize;
      ++blockNumber;
    }
  }

  dlms::apdu::XdlmsApdu request;
  request.kind = dlms::apdu::XdlmsApduKind::ActionRequest;
  request.actionRequestAny.choice = dlms::apdu::ActionRequestChoice::Normal;
  request.actionRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  request.actionRequestAny.normal.descriptor = ToApduDescriptor(descriptor);
  request.actionRequestAny.normal.hasInvocationParameter = hasParameter;
  request.actionRequestAny.normal.invocationParameter = parameter;

  dlms::apdu::XdlmsApdu response;
  std::vector<std::uint8_t> decodedResponseBytes;
  status = ReceiveActionResponse(
    channel_,
    security_,
    request,
    decodedResponseBytes,
    response);
  if (status != XdlmsStatus::Ok) {
    return status;
  }

  return CopyFinalActionResponse(
    channel_,
    security_,
    invokeId,
    invokeIdAndPriority,
    options,
    response,
    result);
}

} // namespace xdlms
} // namespace dlms

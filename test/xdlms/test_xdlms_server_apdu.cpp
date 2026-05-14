#include "dlms/apdu/data.hpp"
#include "dlms/apdu/action.hpp"
#include "dlms/apdu/get.hpp"
#include "dlms/apdu/set.hpp"
#include "dlms/apdu/xdlms.hpp"
#include "dlms/xdlms/xdlms_server.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

class FakeServerHandler : public dlms::xdlms::IXdlmsServerHandler
{
public:
  FakeServerHandler()
    : status(dlms::xdlms::XdlmsStatus::Ok)
    , setStatus(dlms::xdlms::XdlmsStatus::Ok)
    , actionStatus(dlms::xdlms::XdlmsStatus::Ok)
    , calls(0)
    , setCalls(0)
    , actionCalls(0)
    , lastIndication(dlms::xdlms::EmptyGetIndication())
    , lastSetIndication(dlms::xdlms::EmptySetIndication())
    , lastActionIndication(dlms::xdlms::EmptyActionIndication())
    , result(dlms::xdlms::EmptyGetResult())
    , setResult(dlms::xdlms::EmptySetResult())
    , actionResult(dlms::xdlms::EmptyActionResult())
  {
  }

  dlms::xdlms::XdlmsStatus HandleGet(
    const dlms::xdlms::GetIndication& indication,
    dlms::xdlms::GetResult& output)
  {
    ++calls;
    lastIndication = indication;
    output = result;
    return status;
  }

  dlms::xdlms::XdlmsStatus HandleSet(
    const dlms::xdlms::SetIndication& indication,
    dlms::xdlms::SetResult& output)
  {
    ++setCalls;
    lastSetIndication = indication;
    output = setResult;
    return setStatus;
  }

  dlms::xdlms::XdlmsStatus HandleAction(
    const dlms::xdlms::ActionIndication& indication,
    dlms::xdlms::ActionResult& output)
  {
    ++actionCalls;
    lastActionIndication = indication;
    output = actionResult;
    return actionStatus;
  }

  dlms::xdlms::XdlmsStatus status;
  dlms::xdlms::XdlmsStatus setStatus;
  dlms::xdlms::XdlmsStatus actionStatus;
  int calls;
  int setCalls;
  int actionCalls;
  dlms::xdlms::GetIndication lastIndication;
  dlms::xdlms::SetIndication lastSetIndication;
  dlms::xdlms::ActionIndication lastActionIndication;
  dlms::xdlms::GetResult result;
  dlms::xdlms::SetResult setResult;
  dlms::xdlms::ActionResult actionResult;
};

std::vector<std::uint8_t> EncodeApdu(const dlms::apdu::XdlmsApdu& apdu)
{
  std::vector<std::uint8_t> output;
  EXPECT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::EncodeXdlmsApdu(apdu, output));
  return output;
}

std::vector<std::uint8_t> MakeGetRequest(
  std::uint8_t invokeIdAndPriority)
{
  return EncodeApdu(dlms::apdu::MakeGetRequestNormal(
    invokeIdAndPriority,
    3u,
    dlms::apdu::LogicalName(1, 0, 1, 8, 0, 255),
    2u));
}

std::vector<std::uint8_t> MakeGetNextRequest(
  std::uint8_t invokeIdAndPriority,
  std::uint32_t blockNumber)
{
  dlms::apdu::XdlmsApdu apdu;
  apdu.kind = dlms::apdu::XdlmsApduKind::GetRequest;
  apdu.getRequestAny.choice = dlms::apdu::GetRequestChoice::Next;
  apdu.getRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  apdu.getRequestAny.blockNumber = blockNumber;
  return EncodeApdu(apdu);
}

dlms::apdu::DlmsData MakeLongUnsignedData(std::uint16_t value)
{
  dlms::apdu::DlmsData data;
  data.type = dlms::apdu::DlmsDataType::LongUnsigned;
  data.unsignedValue = value;
  return data;
}

void FillSetDescriptor(dlms::apdu::CosemAttributeDescriptorWithSelection& normal)
{
  normal.descriptor.classId = 3u;
  normal.descriptor.logicalName[0] = 1u;
  normal.descriptor.logicalName[1] = 0u;
  normal.descriptor.logicalName[2] = 1u;
  normal.descriptor.logicalName[3] = 8u;
  normal.descriptor.logicalName[4] = 0u;
  normal.descriptor.logicalName[5] = 255u;
  normal.descriptor.attributeId = 2u;
  normal.hasSelection = false;
}

void FillActionDescriptor(
  dlms::apdu::CosemMethodDescriptorWithParameter& normal)
{
  normal.descriptor.classId = 3u;
  normal.descriptor.logicalName[0] = 1u;
  normal.descriptor.logicalName[1] = 0u;
  normal.descriptor.logicalName[2] = 1u;
  normal.descriptor.logicalName[3] = 8u;
  normal.descriptor.logicalName[4] = 0u;
  normal.descriptor.logicalName[5] = 255u;
  normal.descriptor.methodId = 1u;
}

std::vector<std::uint8_t> MakeSetRequest(
  std::uint8_t invokeIdAndPriority,
  std::uint16_t value)
{
  dlms::apdu::XdlmsApdu apdu;
  apdu.kind = dlms::apdu::XdlmsApduKind::SetRequest;
  apdu.setRequestAny.choice = dlms::apdu::SetRequestChoice::Normal;
  apdu.setRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  FillSetDescriptor(apdu.setRequestAny.normal);
  apdu.setRequestAny.data = MakeLongUnsignedData(value);
  return EncodeApdu(apdu);
}

std::vector<std::uint8_t> MakeUnsupportedSetRequest(
  dlms::apdu::SetRequestChoice choice)
{
  const std::uint8_t raw = 0x00u;
  dlms::apdu::XdlmsApdu apdu;
  apdu.kind = dlms::apdu::XdlmsApduKind::SetRequest;
  apdu.setRequestAny.choice = choice;
  apdu.setRequestAny.invokeIdAndPriority = 0x81u;
  FillSetDescriptor(apdu.setRequestAny.normal);
  apdu.setRequestAny.data = MakeLongUnsignedData(1u);
  apdu.setRequestAny.dataBlock.lastBlock = true;
  apdu.setRequestAny.dataBlock.blockNumber = 1u;
  apdu.setRequestAny.dataBlock.rawData.data = &raw;
  apdu.setRequestAny.dataBlock.rawData.size = 1u;
  return EncodeApdu(apdu);
}

std::vector<std::uint8_t> MakeSetBlockRequest(
  dlms::apdu::SetRequestChoice choice,
  std::uint8_t invokeIdAndPriority,
  std::uint32_t blockNumber,
  bool lastBlock,
  const std::vector<std::uint8_t>& rawData)
{
  dlms::apdu::XdlmsApdu apdu;
  apdu.kind = dlms::apdu::XdlmsApduKind::SetRequest;
  apdu.setRequestAny.choice = choice;
  apdu.setRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  FillSetDescriptor(apdu.setRequestAny.normal);
  apdu.setRequestAny.dataBlock.lastBlock = lastBlock;
  apdu.setRequestAny.dataBlock.blockNumber = blockNumber;
  apdu.setRequestAny.dataBlock.rawData.data =
    rawData.empty() ? 0 : &rawData[0];
  apdu.setRequestAny.dataBlock.rawData.size = rawData.size();
  return EncodeApdu(apdu);
}

std::vector<std::uint8_t> MakeActionRequest(
  std::uint8_t invokeIdAndPriority,
  bool hasParameter,
  std::uint16_t value)
{
  dlms::apdu::XdlmsApdu apdu;
  apdu.kind = dlms::apdu::XdlmsApduKind::ActionRequest;
  apdu.actionRequestAny.choice = dlms::apdu::ActionRequestChoice::Normal;
  apdu.actionRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  FillActionDescriptor(apdu.actionRequestAny.normal);
  apdu.actionRequestAny.normal.hasInvocationParameter = hasParameter;
  apdu.actionRequestAny.normal.invocationParameter =
    MakeLongUnsignedData(value);
  return EncodeApdu(apdu);
}

std::vector<std::uint8_t> MakeUnsupportedActionRequest(
  dlms::apdu::ActionRequestChoice choice)
{
  const std::uint8_t raw = 0x00u;
  dlms::apdu::XdlmsApdu apdu;
  apdu.kind = dlms::apdu::XdlmsApduKind::ActionRequest;
  apdu.actionRequestAny.choice = choice;
  apdu.actionRequestAny.invokeIdAndPriority = 0x81u;
  FillActionDescriptor(apdu.actionRequestAny.normal);
  apdu.actionRequestAny.normal.hasInvocationParameter = true;
  apdu.actionRequestAny.normal.invocationParameter =
    MakeLongUnsignedData(1u);
  apdu.actionRequestAny.blockNumber = 1u;
  apdu.actionRequestAny.dataBlock.lastBlock = true;
  apdu.actionRequestAny.dataBlock.blockNumber = 1u;
  apdu.actionRequestAny.dataBlock.rawData.data = &raw;
  apdu.actionRequestAny.dataBlock.rawData.size = 1u;
  return EncodeApdu(apdu);
}

std::vector<std::uint8_t> MakeActionBlockRequest(
  dlms::apdu::ActionRequestChoice choice,
  std::uint8_t invokeIdAndPriority,
  std::uint32_t blockNumber,
  bool lastBlock,
  const std::vector<std::uint8_t>& rawData)
{
  dlms::apdu::XdlmsApdu apdu;
  apdu.kind = dlms::apdu::XdlmsApduKind::ActionRequest;
  apdu.actionRequestAny.choice = choice;
  apdu.actionRequestAny.invokeIdAndPriority = invokeIdAndPriority;
  FillActionDescriptor(apdu.actionRequestAny.normal);
  apdu.actionRequestAny.dataBlock.lastBlock = lastBlock;
  apdu.actionRequestAny.dataBlock.blockNumber = blockNumber;
  apdu.actionRequestAny.dataBlock.rawData.data =
    rawData.empty() ? 0 : &rawData[0];
  apdu.actionRequestAny.dataBlock.rawData.size = rawData.size();
  return EncodeApdu(apdu);
}

std::vector<std::uint8_t> MakeEncodedLongUnsigned(
  std::uint16_t value)
{
  const dlms::apdu::DlmsData data = MakeLongUnsignedData(value);

  std::uint8_t buffer[16] = {};
  dlms::apdu::ApduWriter writer(buffer, sizeof(buffer));
  EXPECT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::EncodeDlmsData(data, writer));
  return std::vector<std::uint8_t>(buffer, buffer + writer.WrittenSize());
}

std::vector<std::uint8_t> MakeEncodedOctetString(std::size_t size)
{
  std::vector<std::uint8_t> bytes(size);
  for (std::size_t i = 0u; i < bytes.size(); ++i) {
    bytes[i] = static_cast<std::uint8_t>(i & 0xffu);
  }

  dlms::apdu::DlmsData data;
  data.type = dlms::apdu::DlmsDataType::OctetString;
  data.bytes.data = bytes.empty() ? 0 : &bytes[0];
  data.bytes.size = bytes.size();

  std::vector<std::uint8_t> output(size + 8u);
  dlms::apdu::ApduWriter writer(&output[0], output.size());
  EXPECT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::EncodeDlmsData(data, writer));
  output.resize(writer.WrittenSize());
  return output;
}

dlms::apdu::XdlmsApdu DecodeResponse(
  const std::vector<std::uint8_t>& response)
{
  dlms::apdu::XdlmsApdu apdu;
  EXPECT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::DecodeXdlmsApdu(
              response.empty() ? 0 : &response[0],
              response.size(),
              apdu));
  return apdu;
}

} // namespace

TEST(XdlmsServerApduProcessor, ProcessGetRequestNormalEncodesDataResponse)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  handler.result.hasData = true;
  handler.result.data = MakeEncodedLongUnsigned(0x1234u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(MakeGetRequest(0xC6u), response));

  ASSERT_EQ(1, handler.calls);
  EXPECT_EQ(6u, handler.lastIndication.invokeId);
  EXPECT_TRUE(handler.lastIndication.options.confirmed);
  EXPECT_TRUE(handler.lastIndication.options.highPriority);
  EXPECT_EQ(3u, handler.lastIndication.descriptor.classId);
  EXPECT_EQ(2u, handler.lastIndication.descriptor.attributeId);

  const dlms::apdu::XdlmsApdu decoded = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::GetResponse, decoded.kind);
  EXPECT_EQ(dlms::apdu::GetResponseChoice::Normal,
            decoded.getResponseAny.choice);
  EXPECT_EQ(0xC6u, decoded.getResponseAny.invokeIdAndPriority);
  EXPECT_EQ(dlms::apdu::GetDataResultChoice::Data,
            decoded.getResponseAny.result.choice);
  EXPECT_EQ(dlms::apdu::DlmsDataType::LongUnsigned,
            decoded.getResponseAny.result.data.type);
  EXPECT_EQ(0x1234u, decoded.getResponseAny.result.data.unsignedValue);
}

TEST(XdlmsServerApduProcessor, GetResponseBlocksAreServedByGetNext)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ServiceOptions options =
    dlms::xdlms::DefaultServiceOptions();
  options.maxGetBlockPayloadBytes = 3u;
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher, options);
  std::vector<std::uint8_t> response;
  handler.result.hasData = true;
  handler.result.data = MakeEncodedOctetString(4u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(MakeGetRequest(0xC6u), response));

  ASSERT_EQ(1, handler.calls);
  dlms::apdu::XdlmsApdu firstBlock = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::GetResponseChoice::WithDataBlock,
            firstBlock.getResponseAny.choice);
  EXPECT_EQ(0xC6u, firstBlock.getResponseAny.invokeIdAndPriority);
  EXPECT_FALSE(firstBlock.getResponseAny.dataBlock.lastBlock);
  EXPECT_EQ(1u, firstBlock.getResponseAny.dataBlock.blockNumber);
  EXPECT_EQ(3u, firstBlock.getResponseAny.dataBlock.rawData.size);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(MakeGetNextRequest(0xC6u, 1u), response));

  EXPECT_EQ(1, handler.calls);
  dlms::apdu::XdlmsApdu finalBlock = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::GetResponseChoice::WithDataBlock,
            finalBlock.getResponseAny.choice);
  EXPECT_EQ(0xC6u, finalBlock.getResponseAny.invokeIdAndPriority);
  EXPECT_TRUE(finalBlock.getResponseAny.dataBlock.lastBlock);
  EXPECT_EQ(2u, finalBlock.getResponseAny.dataBlock.blockNumber);
  EXPECT_EQ(handler.result.data.size() - 3u,
            finalBlock.getResponseAny.dataBlock.rawData.size);
}

TEST(XdlmsServerApduProcessor, GetNextWithoutActiveBlockFailsDecode)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(MakeGetNextRequest(0x81u, 1u), response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(0, handler.calls);
}

TEST(XdlmsServerApduProcessor, GetNextRejectsWrongBlockAndResetsState)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ServiceOptions options =
    dlms::xdlms::DefaultServiceOptions();
  options.maxGetBlockPayloadBytes = 3u;
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher, options);
  std::vector<std::uint8_t> response;
  handler.result.hasData = true;
  handler.result.data = MakeEncodedOctetString(4u);

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(MakeGetRequest(0x81u), response));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(MakeGetNextRequest(0x81u, 2u), response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(MakeGetNextRequest(0x81u, 1u), response));
}

TEST(XdlmsServerApduProcessor, GetNextRejectsInvokeMismatch)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ServiceOptions options =
    dlms::xdlms::DefaultServiceOptions();
  options.maxGetBlockPayloadBytes = 3u;
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher, options);
  std::vector<std::uint8_t> response;
  handler.result.hasData = true;
  handler.result.data = MakeEncodedOctetString(4u);

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(MakeGetRequest(0x81u), response));
  EXPECT_EQ(
    dlms::xdlms::XdlmsStatus::InvokeIdMismatch,
    processor.ProcessRequest(MakeGetNextRequest(0x82u, 1u), response));
  EXPECT_TRUE(response.empty());
}

TEST(XdlmsServerApduProcessor, OversizedGetDataRequiresEnabledBlocks)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ServiceOptions options =
    dlms::xdlms::DefaultServiceOptions();
  options.allowBlockTransfer = false;
  options.maxGetBlockPayloadBytes = 3u;
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher, options);
  std::vector<std::uint8_t> response;
  handler.result.hasData = true;
  handler.result.data = MakeEncodedOctetString(4u);

  EXPECT_EQ(
    dlms::xdlms::XdlmsStatus::BlockTransferRequired,
    processor.ProcessRequest(MakeGetRequest(0x81u), response));
  EXPECT_TRUE(response.empty());
}

TEST(XdlmsServerApduProcessor, OversizedGetDataRejectsZeroBlockPayload)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ServiceOptions options =
    dlms::xdlms::DefaultServiceOptions();
  options.maxGetBlockPayloadBytes = 0u;
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher, options);
  std::vector<std::uint8_t> response;
  handler.result.hasData = true;
  handler.result.data = MakeEncodedOctetString(4u);

  EXPECT_EQ(
    dlms::xdlms::XdlmsStatus::InvalidArgument,
    processor.ProcessRequest(MakeGetRequest(0x81u), response));
  EXPECT_TRUE(response.empty());
}

TEST(XdlmsServerApduProcessor, ProcessGetRequestNormalEncodesAccessResult)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  handler.result.hasAccessResult = true;
  handler.result.accessResult = 3u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(MakeGetRequest(0x86u), response));

  const dlms::apdu::XdlmsApdu decoded = DecodeResponse(response);
  EXPECT_EQ(0x86u, decoded.getResponseAny.invokeIdAndPriority);
  EXPECT_EQ(dlms::apdu::GetDataResultChoice::DataAccessError,
            decoded.getResponseAny.result.choice);
  EXPECT_EQ(3u, decoded.getResponseAny.result.dataAccessError);
}

TEST(XdlmsServerApduProcessor, RejectsMalformedAndUnsupportedApdus)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  std::vector<std::uint8_t> malformed;
  malformed.push_back(0x00u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(malformed, response));
  EXPECT_TRUE(response.empty());

  dlms::apdu::XdlmsApdu getResponse;
  getResponse.kind = dlms::apdu::XdlmsApduKind::GetResponse;
  getResponse.getResponseAny.choice = dlms::apdu::GetResponseChoice::Normal;
  getResponse.getResponseAny.invokeIdAndPriority = 0x81u;
  getResponse.getResponseAny.result.choice =
    dlms::apdu::GetDataResultChoice::DataAccessError;
  getResponse.getResponseAny.result.dataAccessError = 3u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(EncodeApdu(getResponse), response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(0, handler.calls);
}

TEST(XdlmsServerApduProcessor, RejectsUnsupportedGetShapes)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;

  dlms::apdu::XdlmsApdu getNext;
  getNext.kind = dlms::apdu::XdlmsApduKind::GetRequest;
  getNext.getRequestAny.choice = dlms::apdu::GetRequestChoice::Next;
  getNext.getRequestAny.invokeIdAndPriority = 0x81u;
  getNext.getRequestAny.blockNumber = 1u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(EncodeApdu(getNext), response));

  dlms::apdu::XdlmsApdu selective =
    dlms::apdu::MakeGetRequestNormal(
      0x81u,
      3u,
      dlms::apdu::LogicalName(1, 0, 1, 8, 0, 255),
      2u);
  selective.getRequest.hasSelectiveAccess = true;
  selective.getRequestAny.normal.hasSelection = true;
  selective.getRequestAny.normal.selection.selector = 1u;
  selective.getRequestAny.normal.selection.parameters.type =
    dlms::apdu::DlmsDataType::NullData;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(EncodeApdu(selective), response));
  EXPECT_EQ(0, handler.calls);
}

TEST(XdlmsServerApduProcessor, RejectsUnconfirmedGetRequest)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  response.push_back(0xFFu);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(MakeGetRequest(0x06u), response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(0, handler.calls);
}

TEST(XdlmsServerApduProcessor, ProcessSetRequestNormalEncodesSuccessResponse)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  handler.setResult.accessResult = 0u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(MakeSetRequest(0xC8u, 0x1234u), response));

  ASSERT_EQ(1, handler.setCalls);
  EXPECT_EQ(8u, handler.lastSetIndication.invokeId);
  EXPECT_TRUE(handler.lastSetIndication.options.confirmed);
  EXPECT_TRUE(handler.lastSetIndication.options.highPriority);
  EXPECT_EQ(3u, handler.lastSetIndication.descriptor.classId);
  EXPECT_EQ(2u, handler.lastSetIndication.descriptor.attributeId);
  EXPECT_EQ(MakeEncodedLongUnsigned(0x1234u),
            handler.lastSetIndication.data);

  const dlms::apdu::XdlmsApdu decoded = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::SetResponse, decoded.kind);
  EXPECT_EQ(dlms::apdu::SetResponseChoice::Normal,
            decoded.setResponseAny.choice);
  EXPECT_EQ(0xC8u, decoded.setResponseAny.invokeIdAndPriority);
  EXPECT_EQ(0u, decoded.setResponseAny.result);
}

TEST(XdlmsServerApduProcessor, RejectsUnsupportedSetShapes)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(
              MakeUnsupportedSetRequest(
                dlms::apdu::SetRequestChoice::WithList),
              response));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(
              MakeUnsupportedSetRequest(
                dlms::apdu::SetRequestChoice::WithListAndFirstDataBlock),
              response));

  dlms::apdu::XdlmsApdu selective;
  selective.kind = dlms::apdu::XdlmsApduKind::SetRequest;
  selective.setRequestAny.choice = dlms::apdu::SetRequestChoice::Normal;
  selective.setRequestAny.invokeIdAndPriority = 0x81u;
  FillSetDescriptor(selective.setRequestAny.normal);
  selective.setRequestAny.normal.hasSelection = true;
  selective.setRequestAny.normal.selection.selector = 1u;
  selective.setRequestAny.normal.selection.parameters.type =
    dlms::apdu::DlmsDataType::NullData;
  selective.setRequestAny.data = MakeLongUnsignedData(1u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(EncodeApdu(selective), response));
  EXPECT_EQ(0, handler.setCalls);
}

TEST(XdlmsServerApduProcessor, SetRequestBlocksAckAndDispatch)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> data = MakeEncodedLongUnsigned(0x1234u);
  handler.setResult.accessResult = 0u;

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithFirstDataBlock,
                0xC8u,
                1u,
                false,
                std::vector<std::uint8_t>(data.begin(), data.begin() + 2)),
              response));

  EXPECT_EQ(0, handler.setCalls);
  dlms::apdu::XdlmsApdu ack = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::SetResponseChoice::DataBlock,
            ack.setResponseAny.choice);
  EXPECT_EQ(0xC8u, ack.setResponseAny.invokeIdAndPriority);
  EXPECT_EQ(1u, ack.setResponseAny.blockNumber);

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithDataBlock,
                0xC8u,
                2u,
                true,
                std::vector<std::uint8_t>(data.begin() + 2, data.end())),
              response));

  ASSERT_EQ(1, handler.setCalls);
  EXPECT_EQ(8u, handler.lastSetIndication.invokeId);
  EXPECT_TRUE(handler.lastSetIndication.options.confirmed);
  EXPECT_TRUE(handler.lastSetIndication.options.highPriority);
  EXPECT_EQ(data, handler.lastSetIndication.data);

  const dlms::apdu::XdlmsApdu finalResponse = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::SetResponseChoice::LastDataBlock,
            finalResponse.setResponseAny.choice);
  EXPECT_EQ(0xC8u, finalResponse.setResponseAny.invokeIdAndPriority);
  EXPECT_EQ(2u, finalResponse.setResponseAny.blockNumber);
  EXPECT_EQ(0u, finalResponse.setResponseAny.result);
}

TEST(XdlmsServerApduProcessor, SetFirstRequestBlockCanBeFinal)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> data = MakeEncodedLongUnsigned(0x1234u);
  handler.setResult.accessResult = 3u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithFirstDataBlock,
                0x88u,
                1u,
                true,
                data),
              response));

  ASSERT_EQ(1, handler.setCalls);
  EXPECT_EQ(data, handler.lastSetIndication.data);
  const dlms::apdu::XdlmsApdu decoded = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::SetResponseChoice::LastDataBlock,
            decoded.setResponseAny.choice);
  EXPECT_EQ(1u, decoded.setResponseAny.blockNumber);
  EXPECT_EQ(3u, decoded.setResponseAny.result);
}

TEST(XdlmsServerApduProcessor, RejectsInvalidSetRequestBlocks)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> data = MakeEncodedLongUnsigned(0x1234u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithDataBlock,
                0x88u,
                1u,
                true,
                data),
              response));

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithFirstDataBlock,
                0x88u,
                2u,
                false,
                data),
              response));

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithFirstDataBlock,
                0x88u,
                1u,
                false,
                std::vector<std::uint8_t>(data.begin(), data.begin() + 1)),
              response));

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithDataBlock,
                0x88u,
                3u,
                true,
                std::vector<std::uint8_t>(data.begin() + 1, data.end())),
              response));

  EXPECT_EQ(0, handler.setCalls);
}

TEST(XdlmsServerApduProcessor, RejectsSetRequestBlockInvokeMismatch)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> data = MakeEncodedLongUnsigned(0x1234u);

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithFirstDataBlock,
                0x88u,
                1u,
                false,
                std::vector<std::uint8_t>(data.begin(), data.begin() + 1)),
              response));

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvokeIdMismatch,
            processor.ProcessRequest(
              MakeSetBlockRequest(
                dlms::apdu::SetRequestChoice::WithDataBlock,
                0x89u,
                2u,
                true,
                std::vector<std::uint8_t>(data.begin() + 1, data.end())),
              response));
  EXPECT_EQ(0, handler.setCalls);
}

TEST(XdlmsServerApduProcessor, RejectsUnconfirmedSetRequest)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  response.push_back(0xFFu);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(MakeSetRequest(0x08u, 1u), response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(0, handler.setCalls);
}

TEST(XdlmsServerApduProcessor, ProcessActionRequestNormalEncodesSuccessResponse)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  handler.actionResult.actionResult = 0u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionRequest(0xC9u, true, 0x1234u),
              response));

  ASSERT_EQ(1, handler.actionCalls);
  EXPECT_EQ(9u, handler.lastActionIndication.invokeId);
  EXPECT_TRUE(handler.lastActionIndication.options.confirmed);
  EXPECT_TRUE(handler.lastActionIndication.options.highPriority);
  EXPECT_EQ(3u, handler.lastActionIndication.descriptor.classId);
  EXPECT_EQ(1u, handler.lastActionIndication.descriptor.methodId);
  EXPECT_TRUE(handler.lastActionIndication.hasParameter);
  EXPECT_EQ(MakeEncodedLongUnsigned(0x1234u),
            handler.lastActionIndication.parameter);

  const dlms::apdu::XdlmsApdu decoded = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::ActionResponse, decoded.kind);
  EXPECT_EQ(dlms::apdu::ActionResponseChoice::Normal,
            decoded.actionResponseAny.choice);
  EXPECT_EQ(0xC9u, decoded.actionResponseAny.invokeIdAndPriority);
  EXPECT_EQ(0u, decoded.actionResponseAny.normal.result);
  EXPECT_FALSE(decoded.actionResponseAny.normal.hasReturnParameter);
}

TEST(XdlmsServerApduProcessor, ProcessActionRequestNormalAllowsNoParameter)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionRequest(0x89u, false, 0u),
              response));

  ASSERT_EQ(1, handler.actionCalls);
  EXPECT_FALSE(handler.lastActionIndication.hasParameter);
  EXPECT_TRUE(handler.lastActionIndication.parameter.empty());
}

TEST(XdlmsServerApduProcessor, ProcessActionRequestNormalEncodesReturnData)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  handler.actionResult.actionResult = 0u;
  handler.actionResult.hasData = true;
  handler.actionResult.data = MakeEncodedLongUnsigned(0x4321u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionRequest(0x89u, true, 0x0001u),
              response));

  const dlms::apdu::XdlmsApdu decoded = DecodeResponse(response);
  EXPECT_EQ(0x89u, decoded.actionResponseAny.invokeIdAndPriority);
  EXPECT_EQ(0u, decoded.actionResponseAny.normal.result);
  EXPECT_TRUE(decoded.actionResponseAny.normal.hasReturnParameter);
  EXPECT_EQ(dlms::apdu::DlmsDataType::LongUnsigned,
            decoded.actionResponseAny.normal.returnParameter.type);
  EXPECT_EQ(0x4321u,
            decoded.actionResponseAny.normal.returnParameter.unsignedValue);
}

TEST(XdlmsServerApduProcessor, ProcessActionRequestNormalEncodesActionResult)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  handler.actionResult.actionResult = 1u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionRequest(0x89u, true, 0x0001u),
              response));

  const dlms::apdu::XdlmsApdu decoded = DecodeResponse(response);
  EXPECT_EQ(1u, decoded.actionResponseAny.normal.result);
  EXPECT_FALSE(decoded.actionResponseAny.normal.hasReturnParameter);
}

TEST(XdlmsServerApduProcessor, RejectsUnsupportedActionShapes)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(
              MakeUnsupportedActionRequest(
                dlms::apdu::ActionRequestChoice::NextPblock),
              response));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(
              MakeUnsupportedActionRequest(
                dlms::apdu::ActionRequestChoice::WithList),
              response));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(
              MakeUnsupportedActionRequest(
                dlms::apdu::ActionRequestChoice::WithListAndFirstPblock),
              response));
  EXPECT_EQ(0, handler.actionCalls);
}

TEST(XdlmsServerApduProcessor, ActionRequestBlocksAckAndDispatch)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> parameter =
    MakeEncodedLongUnsigned(0x1234u);
  handler.actionResult.actionResult = 0u;

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithFirstPblock,
                0xC9u,
                1u,
                false,
                std::vector<std::uint8_t>(
                  parameter.begin(),
                  parameter.begin() + 2)),
              response));

  EXPECT_EQ(0, handler.actionCalls);
  dlms::apdu::XdlmsApdu ack = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::ActionResponseChoice::NextPblock,
            ack.actionResponseAny.choice);
  EXPECT_EQ(0xC9u, ack.actionResponseAny.invokeIdAndPriority);
  EXPECT_EQ(1u, ack.actionResponseAny.blockNumber);

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithPblock,
                0xC9u,
                2u,
                true,
                std::vector<std::uint8_t>(
                  parameter.begin() + 2,
                  parameter.end())),
              response));

  ASSERT_EQ(1, handler.actionCalls);
  EXPECT_EQ(9u, handler.lastActionIndication.invokeId);
  EXPECT_TRUE(handler.lastActionIndication.options.confirmed);
  EXPECT_TRUE(handler.lastActionIndication.options.highPriority);
  EXPECT_TRUE(handler.lastActionIndication.hasParameter);
  EXPECT_EQ(parameter, handler.lastActionIndication.parameter);

  const dlms::apdu::XdlmsApdu finalResponse = DecodeResponse(response);
  EXPECT_EQ(dlms::apdu::ActionResponseChoice::Normal,
            finalResponse.actionResponseAny.choice);
  EXPECT_EQ(0xC9u, finalResponse.actionResponseAny.invokeIdAndPriority);
}

TEST(XdlmsServerApduProcessor, ActionFirstRequestBlockCanBeFinal)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> parameter =
    MakeEncodedLongUnsigned(0x1234u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithFirstPblock,
                0x89u,
                1u,
                true,
                parameter),
              response));

  ASSERT_EQ(1, handler.actionCalls);
  EXPECT_EQ(parameter, handler.lastActionIndication.parameter);
  EXPECT_EQ(dlms::apdu::ActionResponseChoice::Normal,
            DecodeResponse(response).actionResponseAny.choice);
}

TEST(XdlmsServerApduProcessor, RejectsInvalidActionRequestBlocks)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> parameter =
    MakeEncodedLongUnsigned(0x1234u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithPblock,
                0x89u,
                1u,
                true,
                parameter),
              response));

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithFirstPblock,
                0x89u,
                2u,
                false,
                parameter),
              response));

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithFirstPblock,
                0x89u,
                1u,
                false,
                std::vector<std::uint8_t>(
                  parameter.begin(),
                  parameter.begin() + 1)),
              response));

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithPblock,
                0x89u,
                3u,
                true,
                std::vector<std::uint8_t>(
                  parameter.begin() + 1,
                  parameter.end())),
              response));

  EXPECT_EQ(0, handler.actionCalls);
}

TEST(XdlmsServerApduProcessor, RejectsActionRequestBlockInvokeMismatch)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  const std::vector<std::uint8_t> parameter =
    MakeEncodedLongUnsigned(0x1234u);

  ASSERT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithFirstPblock,
                0x89u,
                1u,
                false,
                std::vector<std::uint8_t>(
                  parameter.begin(),
                  parameter.begin() + 1)),
              response));

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvokeIdMismatch,
            processor.ProcessRequest(
              MakeActionBlockRequest(
                dlms::apdu::ActionRequestChoice::WithPblock,
                0x8Au,
                2u,
                true,
                std::vector<std::uint8_t>(
                  parameter.begin() + 1,
                  parameter.end())),
              response));
  EXPECT_EQ(0, handler.actionCalls);
}

TEST(XdlmsServerApduProcessor, RejectsUnconfirmedActionRequest)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher);
  std::vector<std::uint8_t> response;
  response.push_back(0xFFu);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            processor.ProcessRequest(
              MakeActionRequest(0x09u, true, 1u),
              response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(0, handler.actionCalls);
}

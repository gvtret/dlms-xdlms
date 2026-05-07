#include "dlms/apdu/data.hpp"
#include "dlms/apdu/get.hpp"
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
    , calls(0)
    , lastIndication(dlms::xdlms::EmptyGetIndication())
    , result(dlms::xdlms::EmptyGetResult())
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

  dlms::xdlms::XdlmsStatus status;
  int calls;
  dlms::xdlms::GetIndication lastIndication;
  dlms::xdlms::GetResult result;
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

std::vector<std::uint8_t> MakeEncodedLongUnsigned(
  std::uint16_t value)
{
  dlms::apdu::DlmsData data;
  data.type = dlms::apdu::DlmsDataType::LongUnsigned;
  data.unsignedValue = value;

  std::uint8_t buffer[16] = {};
  dlms::apdu::ApduWriter writer(buffer, sizeof(buffer));
  EXPECT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::EncodeDlmsData(data, writer));
  return std::vector<std::uint8_t>(buffer, buffer + writer.WrittenSize());
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

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
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

TEST(XdlmsServerApduProcessor, RejectsUnconfirmedAndHandlerFailures)
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

  handler.status = dlms::xdlms::XdlmsStatus::InvalidState;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidState,
            processor.ProcessRequest(MakeGetRequest(0x86u), response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(1, handler.calls);
}

#include "dlms/apdu/data.hpp"
#include "dlms/apdu/get.hpp"
#include "dlms/apdu/xdlms.hpp"
#include "dlms/association/association_client.hpp"
#include "dlms/profile/apdu_channel.hpp"
#include "dlms/xdlms/xdlms_client.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <deque>
#include <vector>

namespace {

class FakeApduChannel : public dlms::profile::IApduChannel
{
public:
  FakeApduChannel()
    : openStatus(dlms::profile::ProfileStatus::Ok)
    , closeStatus(dlms::profile::ProfileStatus::Ok)
    , sendStatus(dlms::profile::ProfileStatus::Ok)
    , receiveStatus(dlms::profile::ProfileStatus::Ok)
    , open(false)
    , sendCalls(0)
    , receiveCalls(0)
  {
  }

  dlms::profile::ProfileStatus Open()
  {
    open = true;
    return openStatus;
  }

  dlms::profile::ProfileStatus Close()
  {
    open = false;
    return closeStatus;
  }

  bool IsOpen() const
  {
    return open;
  }

  dlms::profile::ProfileStatus SendApdu(dlms::profile::ProfileByteView apdu)
  {
    ++sendCalls;
    sent.assign(apdu.data, apdu.data + apdu.size);
    sentHistory.push_back(sent);
    return sendStatus;
  }

  dlms::profile::ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu)
  {
    ++receiveCalls;
    if (receiveStatus == dlms::profile::ProfileStatus::Ok) {
      if (!receiveQueue.empty()) {
        apdu = receiveQueue.front();
        receiveQueue.pop_front();
      } else {
        apdu = nextReceive;
      }
    }
    return receiveStatus;
  }

  dlms::profile::ProfileStatus ReceiveApdu(
    dlms::profile::ProfileMutableBuffer output)
  {
    ++receiveCalls;
    if (receiveStatus != dlms::profile::ProfileStatus::Ok) {
      return receiveStatus;
    }
    if (output.size < nextReceive.size()) {
      return dlms::profile::ProfileStatus::OutputBufferTooSmall;
    }
    for (std::size_t i = 0; i < nextReceive.size(); ++i) {
      output.data[i] = nextReceive[i];
    }
    if (output.writtenSize != 0) {
      *output.writtenSize = nextReceive.size();
    }
    return dlms::profile::ProfileStatus::Ok;
  }

  dlms::profile::ProfileStatus openStatus;
  dlms::profile::ProfileStatus closeStatus;
  dlms::profile::ProfileStatus sendStatus;
  dlms::profile::ProfileStatus receiveStatus;
  bool open;
  int sendCalls;
  int receiveCalls;
  std::vector<std::uint8_t> sent;
  std::vector<std::vector<std::uint8_t> > sentHistory;
  std::vector<std::uint8_t> nextReceive;
  std::deque<std::vector<std::uint8_t> > receiveQueue;
};

std::vector<std::uint8_t> MakeAareBytes()
{
  const std::uint8_t kAare[] = {
    0x61, 0x4E, 0x80, 0x02, 0x02, 0x84, 0xA1, 0x09,
    0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01,
    0x01, 0xA2, 0x03, 0x02, 0x01, 0x00, 0xA3, 0x05,
    0xA1, 0x03, 0x02, 0x01, 0x0E, 0x88, 0x02, 0x07,
    0x80, 0x89, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08,
    0x02, 0x02, 0xAA, 0x12, 0x80, 0x10, 0xC6, 0x69,
    0x73, 0x51, 0xFF, 0x4A, 0xEC, 0x29, 0xCD, 0xBA,
    0xAB, 0xF2, 0xFB, 0xE3, 0x46, 0x7C, 0xBE, 0x10,
    0x04, 0x0E, 0x08, 0x00, 0x06, 0x5F, 0x1F, 0x04,
    0x00, 0x40, 0x18, 0x1D, 0x02, 0x00, 0x00, 0x07};
  return std::vector<std::uint8_t>(kAare, kAare + sizeof(kAare));
}

dlms::xdlms::CosemAttributeDescriptor MakeDescriptor()
{
  dlms::xdlms::CosemAttributeDescriptor descriptor =
    dlms::xdlms::EmptyCosemAttributeDescriptor();
  descriptor.classId = 7u;
  descriptor.instanceId = dlms::xdlms::CosemLogicalName(1, 0, 99, 1, 0, 255);
  descriptor.attributeId = 7u;
  return descriptor;
}

dlms::xdlms::CosemMethodDescriptor MakeMethodDescriptor()
{
  dlms::xdlms::CosemMethodDescriptor descriptor =
    dlms::xdlms::EmptyCosemMethodDescriptor();
  descriptor.classId = 7u;
  descriptor.instanceId = dlms::xdlms::CosemLogicalName(1, 0, 99, 1, 0, 255);
  descriptor.methodId = 1u;
  return descriptor;
}

std::vector<std::uint8_t> MakeLongUnsignedBytes(std::uint16_t value)
{
  std::vector<std::uint8_t> output;
  output.push_back(0x12u);
  output.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
  output.push_back(static_cast<std::uint8_t>(value & 0xFFu));
  return output;
}

std::vector<std::uint8_t> EncodeResponse(
  const dlms::apdu::XdlmsApdu& response)
{
  std::vector<std::uint8_t> output;
  EXPECT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::EncodeXdlmsApdu(response, output));
  return output;
}

std::vector<std::uint8_t> MakeDataResponse(
  std::uint8_t invokeIdAndPriority)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::GetResponse;
  response.getResponse.invokeIdAndPriority = invokeIdAndPriority;
  response.getResponse.resultChoice = dlms::apdu::GetDataResultChoice::Data;
  response.getResponse.data.type = dlms::apdu::DlmsDataType::LongUnsigned;
  response.getResponse.data.unsignedValue = 0x09F1u;
  return EncodeResponse(response);
}

std::vector<std::uint8_t> MakeAccessErrorResponse(
  std::uint8_t invokeIdAndPriority)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::GetResponse;
  response.getResponse.invokeIdAndPriority = invokeIdAndPriority;
  response.getResponse.resultChoice =
    dlms::apdu::GetDataResultChoice::DataAccessError;
  response.getResponse.dataAccessError = 0x0Cu;
  return EncodeResponse(response);
}

std::vector<std::uint8_t> MakeBlockResponse(
  std::uint8_t invokeIdAndPriority,
  std::uint32_t blockNumber,
  bool lastBlock,
  const std::vector<std::uint8_t>& rawData)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::GetResponse;
  response.getResponseAny.choice =
    dlms::apdu::GetResponseChoice::WithDataBlock;
  response.getResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.getResponseAny.dataBlock.lastBlock = lastBlock;
  response.getResponseAny.dataBlock.blockNumber = blockNumber;
  response.getResponseAny.dataBlock.rawData.data =
    rawData.empty() ? 0 : &rawData[0];
  response.getResponseAny.dataBlock.rawData.size = rawData.size();
  return EncodeResponse(response);
}

std::vector<std::uint8_t> MakeSetResponse(
  std::uint8_t invokeIdAndPriority,
  std::uint8_t result)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::SetResponse;
  response.setResponseAny.choice = dlms::apdu::SetResponseChoice::Normal;
  response.setResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.setResponseAny.result = result;
  return EncodeResponse(response);
}

std::vector<std::uint8_t> MakeSetBlockResponse(
  std::uint8_t invokeIdAndPriority)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::SetResponse;
  response.setResponseAny.choice = dlms::apdu::SetResponseChoice::DataBlock;
  response.setResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.setResponseAny.blockNumber = 1u;
  return EncodeResponse(response);
}

std::vector<std::uint8_t> MakeActionResponse(
  std::uint8_t invokeIdAndPriority,
  std::uint8_t result,
  bool hasReturnParameter)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::ActionResponse;
  response.actionResponseAny.choice =
    dlms::apdu::ActionResponseChoice::Normal;
  response.actionResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.actionResponseAny.normal.result = result;
  response.actionResponseAny.normal.hasReturnParameter = hasReturnParameter;
  response.actionResponseAny.normal.returnParameter.type =
    dlms::apdu::DlmsDataType::LongUnsigned;
  response.actionResponseAny.normal.returnParameter.unsignedValue = 0x2468u;
  return EncodeResponse(response);
}

std::vector<std::uint8_t> MakeActionBlockResponse(
  std::uint8_t invokeIdAndPriority)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::ActionResponse;
  response.actionResponseAny.choice =
    dlms::apdu::ActionResponseChoice::WithPblock;
  response.actionResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.actionResponseAny.dataBlock.lastBlock = false;
  response.actionResponseAny.dataBlock.blockNumber = 1u;
  response.actionResponseAny.dataBlock.rawData.data = 0;
  response.actionResponseAny.dataBlock.rawData.size = 0;
  return EncodeResponse(response);
}

void Establish(dlms::association::AssociationClient& association,
               FakeApduChannel& channel)
{
  channel.nextReceive = MakeAareBytes();
  ASSERT_EQ(dlms::association::AssociationStatus::Ok, association.Open());
  ASSERT_EQ(dlms::association::AssociationStatus::Ok, association.Establish());
  ASSERT_TRUE(association.IsAssociated());
}

} // namespace

TEST(XdlmsClient, GetSendsNormalRequestAndCopiesDataResult)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive = MakeDataResponse(0x81u);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            client.Get(MakeDescriptor(), result));

  ASSERT_EQ(13u, channel.sent.size());
  EXPECT_EQ(0xC0u, channel.sent[0]);
  EXPECT_EQ(0x01u, channel.sent[1]);
  EXPECT_EQ(0x81u, channel.sent[2]);
  EXPECT_EQ(1u, result.invokeId);
  EXPECT_TRUE(result.hasData);
  ASSERT_EQ(3u, result.data.size());
  EXPECT_EQ(0x12u, result.data[0]);
  EXPECT_EQ(0x09u, result.data[1]);
  EXPECT_EQ(0xF1u, result.data[2]);
  EXPECT_FALSE(result.hasAccessResult);
}

TEST(XdlmsClient, GetRequiresAssociatedAssociation)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::NotAssociated,
            client.Get(MakeDescriptor(), result));
  EXPECT_EQ(0, channel.sendCalls);
}

TEST(XdlmsClient, GetRejectsInvalidDescriptorBeforeSend)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            client.Get(dlms::xdlms::EmptyCosemAttributeDescriptor(), result));
  EXPECT_EQ(1, channel.sendCalls);
}

TEST(XdlmsClient, GetMapsSendAndReceiveFailures)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  channel.sendStatus = dlms::profile::ProfileStatus::WriteFailed;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::SendFailed,
            client.Get(MakeDescriptor(), result));

  channel.sendStatus = dlms::profile::ProfileStatus::Ok;
  channel.receiveStatus = dlms::profile::ProfileStatus::Timeout;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ReceiveFailed,
            client.Get(MakeDescriptor(), result));
}

TEST(XdlmsClient, GetMapsMalformedResponseToDecodeFailed)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive.clear();
  channel.nextReceive.push_back(0x00u);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            client.Get(MakeDescriptor(), result));
}

TEST(XdlmsClient, GetRejectsInvokeIdMismatch)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive = MakeDataResponse(0x82u);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvokeIdMismatch,
            client.Get(MakeDescriptor(), result));
}

TEST(XdlmsClient, GetReturnsServiceRejectedForAccessResult)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive = MakeAccessErrorResponse(0x81u);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ServiceRejected,
            client.Get(MakeDescriptor(), result));
  EXPECT_TRUE(result.hasAccessResult);
  EXPECT_EQ(0x0Cu, result.accessResult);
}

TEST(XdlmsClient, GetReportsBlockTransferRequired)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive =
    MakeBlockResponse(0x81u, 1u, false, MakeLongUnsignedBytes(0x1111u));

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::ServiceOptions options =
    dlms::xdlms::DefaultServiceOptions();
  options.allowBlockTransfer = false;
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::BlockTransferRequired,
            client.Get(MakeDescriptor(), options, result));
}

TEST(XdlmsClient, GetCollectsResponseDataBlocks)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.receiveQueue.push_back(
    MakeBlockResponse(0x81u, 1u, false, MakeLongUnsignedBytes(0x1111u)));
  channel.receiveQueue.push_back(
    MakeBlockResponse(0x81u, 2u, true, MakeLongUnsignedBytes(0x2222u)));

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            client.Get(MakeDescriptor(), result));

  EXPECT_EQ(3, channel.sendCalls);
  ASSERT_EQ(3u, channel.sentHistory.size());
  dlms::apdu::XdlmsApdu nextRequest;
  ASSERT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::DecodeXdlmsApdu(
              &channel.sentHistory[2][0],
              channel.sentHistory[2].size(),
              nextRequest));
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::GetRequest, nextRequest.kind);
  EXPECT_EQ(dlms::apdu::GetRequestChoice::Next,
            nextRequest.getRequestAny.choice);
  EXPECT_EQ(1u, nextRequest.getRequestAny.blockNumber);

  EXPECT_EQ(1u, result.invokeId);
  EXPECT_TRUE(result.hasData);
  std::vector<std::uint8_t> expected = MakeLongUnsignedBytes(0x1111u);
  const std::vector<std::uint8_t> second = MakeLongUnsignedBytes(0x2222u);
  expected.insert(expected.end(), second.begin(), second.end());
  EXPECT_EQ(expected, result.data);
}

TEST(XdlmsClient, GetRejectsOutOfOrderResponseDataBlocks)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.receiveQueue.push_back(
    MakeBlockResponse(0x81u, 1u, false, MakeLongUnsignedBytes(0x1111u)));
  channel.receiveQueue.push_back(
    MakeBlockResponse(0x81u, 3u, true, MakeLongUnsignedBytes(0x2222u)));

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            client.Get(MakeDescriptor(), result));
}

TEST(XdlmsClient, SetSendsNormalRequestAndCopiesResult)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive = MakeSetResponse(0x81u, 0u);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::SetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(0x4321u), result));

  dlms::apdu::XdlmsApdu request;
  ASSERT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::DecodeXdlmsApdu(
              &channel.sent[0], channel.sent.size(), request));
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::SetRequest, request.kind);
  EXPECT_EQ(dlms::apdu::SetRequestChoice::Normal,
            request.setRequestAny.choice);
  EXPECT_EQ(0x81u, request.setRequestAny.invokeIdAndPriority);
  EXPECT_EQ(7u, request.setRequestAny.normal.descriptor.classId);
  EXPECT_EQ(7u, request.setRequestAny.normal.descriptor.attributeId);
  EXPECT_EQ(dlms::apdu::DlmsDataType::LongUnsigned,
            request.setRequestAny.data.type);
  EXPECT_EQ(0x4321u, request.setRequestAny.data.unsignedValue);
  EXPECT_EQ(1u, result.invokeId);
  EXPECT_EQ(0u, result.accessResult);
}

TEST(XdlmsClient, SetRejectsInvalidInputsAndRequiresAssociation)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::SetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            client.Set(
              dlms::xdlms::EmptyCosemAttributeDescriptor(),
              MakeLongUnsignedBytes(1u),
              result));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            client.Set(MakeDescriptor(), std::vector<std::uint8_t>(), result));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::NotAssociated,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(1u), result));
  EXPECT_EQ(0, channel.sendCalls);
}

TEST(XdlmsClient, SetMapsResponseErrors)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::SetResult result;

  channel.nextReceive = MakeSetResponse(0x81u, 3u);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ServiceRejected,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(1u), result));
  EXPECT_EQ(3u, result.accessResult);

  channel.nextReceive = MakeSetResponse(0x83u, 0u);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvokeIdMismatch,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(1u), result));

  channel.nextReceive = MakeSetBlockResponse(0x83u);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::BlockTransferRequired,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(1u), result));
}

TEST(XdlmsClient, SetMapsChannelAndDecodeFailures)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::SetResult result;

  channel.sendStatus = dlms::profile::ProfileStatus::WriteFailed;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::SendFailed,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(1u), result));

  channel.sendStatus = dlms::profile::ProfileStatus::Ok;
  channel.receiveStatus = dlms::profile::ProfileStatus::Timeout;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ReceiveFailed,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(1u), result));

  channel.receiveStatus = dlms::profile::ProfileStatus::Ok;
  channel.nextReceive.clear();
  channel.nextReceive.push_back(0u);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            client.Set(MakeDescriptor(), MakeLongUnsignedBytes(1u), result));
}

TEST(XdlmsClient, ActionSendsNormalRequestAndCopiesReturnParameter)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive = MakeActionResponse(0x81u, 0u, true);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::ActionResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            client.Action(
              MakeMethodDescriptor(),
              true,
              MakeLongUnsignedBytes(0x4321u),
              result));

  dlms::apdu::XdlmsApdu request;
  ASSERT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::DecodeXdlmsApdu(
              &channel.sent[0], channel.sent.size(), request));
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::ActionRequest, request.kind);
  EXPECT_EQ(dlms::apdu::ActionRequestChoice::Normal,
            request.actionRequestAny.choice);
  EXPECT_EQ(0x81u, request.actionRequestAny.invokeIdAndPriority);
  EXPECT_EQ(7u, request.actionRequestAny.normal.descriptor.classId);
  EXPECT_EQ(1u, request.actionRequestAny.normal.descriptor.methodId);
  EXPECT_TRUE(request.actionRequestAny.normal.hasInvocationParameter);
  EXPECT_EQ(0x4321u,
            request.actionRequestAny.normal.invocationParameter.unsignedValue);
  EXPECT_EQ(1u, result.invokeId);
  EXPECT_EQ(0u, result.actionResult);
  EXPECT_TRUE(result.hasData);
  EXPECT_EQ(MakeLongUnsignedBytes(0x2468u), result.data);
}

TEST(XdlmsClient, ActionAllowsNoInvocationParameter)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive = MakeActionResponse(0x81u, 0u, false);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::ActionResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));

  dlms::apdu::XdlmsApdu request;
  ASSERT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::DecodeXdlmsApdu(
              &channel.sent[0], channel.sent.size(), request));
  EXPECT_FALSE(request.actionRequestAny.normal.hasInvocationParameter);
  EXPECT_FALSE(result.hasData);
}

TEST(XdlmsClient, ActionRejectsInvalidInputsAndRequiresAssociation)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::ActionResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            client.Action(
              dlms::xdlms::EmptyCosemMethodDescriptor(),
              true,
              MakeLongUnsignedBytes(1u),
              result));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            client.Action(
              MakeMethodDescriptor(),
              true,
              std::vector<std::uint8_t>(),
              result));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::NotAssociated,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));
  EXPECT_EQ(0, channel.sendCalls);
}

TEST(XdlmsClient, ActionMapsResponseErrors)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::ActionResult result;

  channel.nextReceive = MakeActionResponse(0x81u, 3u, false);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ServiceRejected,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));
  EXPECT_EQ(3u, result.actionResult);

  channel.nextReceive = MakeActionResponse(0x83u, 0u, false);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvokeIdMismatch,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));

  channel.nextReceive = MakeActionBlockResponse(0x83u);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::BlockTransferRequired,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));
}

TEST(XdlmsClient, ActionMapsChannelAndDecodeFailures)
{
  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::ActionResult result;

  channel.sendStatus = dlms::profile::ProfileStatus::WriteFailed;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::SendFailed,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));

  channel.sendStatus = dlms::profile::ProfileStatus::Ok;
  channel.receiveStatus = dlms::profile::ProfileStatus::Timeout;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ReceiveFailed,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));

  channel.receiveStatus = dlms::profile::ProfileStatus::Ok;
  channel.nextReceive.clear();
  channel.nextReceive.push_back(0u);
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::DecodeFailed,
            client.Action(
              MakeMethodDescriptor(),
              false,
              std::vector<std::uint8_t>(),
              result));
}

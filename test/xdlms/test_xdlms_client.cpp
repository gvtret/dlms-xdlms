#include "dlms/apdu/data.hpp"
#include "dlms/apdu/get.hpp"
#include "dlms/apdu/xdlms.hpp"
#include "dlms/association/association_client.hpp"
#include "dlms/profile/apdu_channel.hpp"
#include "dlms/xdlms/xdlms_client.hpp"

#include <gtest/gtest.h>

#include <cstdint>
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
    return sendStatus;
  }

  dlms::profile::ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu)
  {
    ++receiveCalls;
    if (receiveStatus == dlms::profile::ProfileStatus::Ok) {
      apdu = nextReceive;
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
  std::vector<std::uint8_t> nextReceive;
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
  std::uint8_t invokeIdAndPriority)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::GetResponse;
  response.getResponseAny.choice =
    dlms::apdu::GetResponseChoice::WithDataBlock;
  response.getResponseAny.invokeIdAndPriority = invokeIdAndPriority;
  response.getResponseAny.dataBlock.lastBlock = false;
  response.getResponseAny.dataBlock.blockNumber = 1u;
  response.getResponseAny.dataBlock.rawData.data = 0;
  response.getResponseAny.dataBlock.rawData.size = 0;
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
  channel.nextReceive = MakeBlockResponse(0x81u);

  dlms::xdlms::XdlmsClient client(channel, association);
  dlms::xdlms::GetResult result;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::BlockTransferRequired,
            client.Get(MakeDescriptor(), result));
}

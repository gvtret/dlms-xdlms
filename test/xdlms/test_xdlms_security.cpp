#include "dlms/apdu/data.hpp"
#include "dlms/apdu/get.hpp"
#include "dlms/apdu/xdlms.hpp"
#include "dlms/association/association_client.hpp"
#include "dlms/profile/apdu_channel.hpp"
#include "dlms/security/ciphered_apdu_processor.hpp"
#include "dlms/security/in_memory_invocation_counter_store.hpp"
#include "dlms/security/in_memory_key_store.hpp"
#include "dlms/xdlms/xdlms_client.hpp"
#include "dlms/xdlms/xdlms_server.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

class FakeApduChannel : public dlms::profile::IApduChannel
{
public:
  FakeApduChannel()
    : sendStatus(dlms::profile::ProfileStatus::Ok)
    , receiveStatus(dlms::profile::ProfileStatus::Ok)
    , open(false)
    , sendCalls(0)
    , receiveCalls(0)
  {
  }

  dlms::profile::ProfileStatus Open()
  {
    open = true;
    return dlms::profile::ProfileStatus::Ok;
  }

  dlms::profile::ProfileStatus Close()
  {
    open = false;
    return dlms::profile::ProfileStatus::Ok;
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
    for (std::size_t i = 0u; i < nextReceive.size(); ++i) {
      output.data[i] = nextReceive[i];
    }
    if (output.writtenSize != 0) {
      *output.writtenSize = nextReceive.size();
    }
    return dlms::profile::ProfileStatus::Ok;
  }

  dlms::profile::ProfileStatus sendStatus;
  dlms::profile::ProfileStatus receiveStatus;
  bool open;
  int sendCalls;
  int receiveCalls;
  std::vector<std::uint8_t> sent;
  std::vector<std::uint8_t> nextReceive;
};

class FakeServerHandler : public dlms::xdlms::IXdlmsServerHandler
{
public:
  FakeServerHandler()
    : calls(0)
  {
  }

  dlms::xdlms::XdlmsStatus HandleGet(
    const dlms::xdlms::GetIndication& indication,
    dlms::xdlms::GetResult& result)
  {
    ++calls;
    lastIndication = indication;
    result = dlms::xdlms::EmptyGetResult();
    result.hasData = true;
    result.data = MakeEncodedLongUnsigned(0x2468u);
    return dlms::xdlms::XdlmsStatus::Ok;
  }

  static std::vector<std::uint8_t> MakeEncodedLongUnsigned(
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

  int calls;
  dlms::xdlms::GetIndication lastIndication;
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

dlms::security::SecurityByteView ViewOf(
  const std::vector<std::uint8_t>& data)
{
  dlms::security::SecurityByteView view;
  view.data = data.empty() ? 0 : &data[0];
  view.size = data.size();
  return view;
}

dlms::security::SecurityKey MakeKey(
  dlms::security::SecurityKeyRole role,
  std::uint8_t seed)
{
  dlms::security::SecurityKey key = dlms::security::EmptySecurityKey(role);
  key.size = 16u;
  for (std::size_t i = 0u; i < key.size; ++i) {
    key.bytes[i] = static_cast<std::uint8_t>(seed + i);
  }
  return key;
}

void InstallKeys(dlms::security::InMemoryKeyStore& keys)
{
  ASSERT_EQ(
    dlms::security::SecurityStatus::Ok,
    keys.SetKey(
      MakeKey(
        dlms::security::SecurityKeyRole::GlobalUnicastEncryption,
        0x10u)));
  ASSERT_EQ(
    dlms::security::SecurityStatus::Ok,
    keys.SetKey(
      MakeKey(
        dlms::security::SecurityKeyRole::Authentication,
        0x80u)));
}

dlms::security::SecurityContext MakeContext(
  dlms::security::SecurityRole role)
{
  dlms::security::SecurityContext context =
    dlms::security::EmptySecurityContext();
  context.policy = dlms::security::SecurityPolicy::AuthenticatedAndEncrypted;
  context.role = role;
  context.clientSap = 16u;
  context.serverSap = 1u;

  const std::uint8_t clientTitle[8] =
    {0x4du, 0x4du, 0x4du, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u};
  const std::uint8_t serverTitle[8] =
    {0x4du, 0x45u, 0x54u, 0x00u, 0x00u, 0x00u, 0x00u, 0x02u};

  for (std::size_t i = 0u; i < 8u; ++i) {
    context.localSystemTitle[i] =
      role == dlms::security::SecurityRole::Client
        ? clientTitle[i]
        : serverTitle[i];
    context.remoteSystemTitle[i] =
      role == dlms::security::SecurityRole::Client
        ? serverTitle[i]
        : clientTitle[i];
  }

  return context;
}

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

std::vector<std::uint8_t> MakeDataResponse(
  std::uint8_t invokeIdAndPriority)
{
  dlms::apdu::XdlmsApdu response;
  response.kind = dlms::apdu::XdlmsApduKind::GetResponse;
  response.getResponse.invokeIdAndPriority = invokeIdAndPriority;
  response.getResponse.resultChoice = dlms::apdu::GetDataResultChoice::Data;
  response.getResponse.data.type = dlms::apdu::DlmsDataType::LongUnsigned;
  response.getResponse.data.unsignedValue = 0x09F1u;
  return EncodeApdu(response);
}

dlms::xdlms::CosemAttributeDescriptor MakeDescriptor()
{
  dlms::xdlms::CosemAttributeDescriptor descriptor =
    dlms::xdlms::EmptyCosemAttributeDescriptor();
  descriptor.classId = 3u;
  descriptor.instanceId = dlms::xdlms::CosemLogicalName(1, 0, 1, 8, 0, 255);
  descriptor.attributeId = 2u;
  return descriptor;
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

TEST(XdlmsSecurity, ClientProtectsRequestAndUnprotectsResponse)
{
  dlms::security::InMemoryKeyStore keys;
  InstallKeys(keys);

  dlms::security::InMemoryInvocationCounterStore clientCounters;
  dlms::security::InMemoryInvocationCounterStore serverCounters;
  clientCounters.SetLocalCounter(1u);
  serverCounters.SetLocalCounter(1u);

  const dlms::security::SecurityContext clientContext =
    MakeContext(dlms::security::SecurityRole::Client);
  const dlms::security::SecurityContext serverContext =
    MakeContext(dlms::security::SecurityRole::Server);

  dlms::security::CipheredApduProcessor clientSecurity(
    clientContext,
    keys,
    clientCounters);
  dlms::security::CipheredApduProcessor serverSecurity(
    serverContext,
    keys,
    serverCounters);

  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);

  ASSERT_EQ(
    dlms::security::SecurityStatus::Ok,
    serverSecurity.Protect(ViewOf(MakeDataResponse(0x81u)),
                           channel.nextReceive));

  dlms::xdlms::XdlmsClient client(channel, association, clientSecurity);
  dlms::xdlms::GetResult result;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            client.Get(MakeDescriptor(), result));

  ASSERT_FALSE(channel.sent.empty());
  EXPECT_EQ(0x30u, channel.sent[0]);
  EXPECT_NE(0xC0u, channel.sent[0]);

  std::vector<std::uint8_t> unprotectedRequest;
  ASSERT_EQ(
    dlms::security::SecurityStatus::Ok,
    serverSecurity.Unprotect(ViewOf(channel.sent), unprotectedRequest));

  dlms::apdu::XdlmsApdu request;
  ASSERT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::DecodeXdlmsApdu(
              &unprotectedRequest[0],
              unprotectedRequest.size(),
              request));
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::GetRequest, request.kind);
  EXPECT_EQ(1u, result.invokeId);
  EXPECT_TRUE(result.hasData);
  EXPECT_EQ(0x12u, result.data[0]);
}

TEST(XdlmsSecurity, ClientMapsResponseAuthenticationFailure)
{
  dlms::security::InMemoryKeyStore keys;
  InstallKeys(keys);
  dlms::security::InMemoryInvocationCounterStore counters;
  counters.SetLocalCounter(1u);
  dlms::security::CipheredApduProcessor security(
    MakeContext(dlms::security::SecurityRole::Client),
    keys,
    counters);

  FakeApduChannel channel;
  dlms::association::AssociationClient association(
    channel,
    dlms::association::DefaultAssociationOptions());
  Establish(association, channel);
  channel.nextReceive = MakeDataResponse(0x81u);

  dlms::xdlms::XdlmsClient client(channel, association, security);
  dlms::xdlms::GetResult result;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::SecurityFailed,
            client.Get(MakeDescriptor(), result));
}

TEST(XdlmsSecurity, ServerUnprotectsRequestAndProtectsResponse)
{
  dlms::security::InMemoryKeyStore keys;
  InstallKeys(keys);

  dlms::security::InMemoryInvocationCounterStore clientCounters;
  dlms::security::InMemoryInvocationCounterStore serverCounters;
  clientCounters.SetLocalCounter(1u);
  serverCounters.SetLocalCounter(1u);

  dlms::security::CipheredApduProcessor clientSecurity(
    MakeContext(dlms::security::SecurityRole::Client),
    keys,
    clientCounters);
  dlms::security::CipheredApduProcessor serverSecurity(
    MakeContext(dlms::security::SecurityRole::Server),
    keys,
    serverCounters);

  std::vector<std::uint8_t> protectedRequest;
  ASSERT_EQ(
    dlms::security::SecurityStatus::Ok,
    clientSecurity.Protect(ViewOf(MakeGetRequest(0x81u)),
                           protectedRequest));

  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(
    dispatcher,
    serverSecurity);

  std::vector<std::uint8_t> protectedResponse;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            processor.ProcessRequest(protectedRequest, protectedResponse));

  EXPECT_EQ(1, handler.calls);
  ASSERT_FALSE(protectedResponse.empty());
  EXPECT_EQ(0x30u, protectedResponse[0]);
  EXPECT_NE(0xC4u, protectedResponse[0]);

  std::vector<std::uint8_t> plainResponse;
  ASSERT_EQ(
    dlms::security::SecurityStatus::Ok,
    clientSecurity.Unprotect(ViewOf(protectedResponse), plainResponse));

  dlms::apdu::XdlmsApdu response;
  ASSERT_EQ(dlms::apdu::ApduStatus::Ok,
            dlms::apdu::DecodeXdlmsApdu(
              &plainResponse[0],
              plainResponse.size(),
              response));
  EXPECT_EQ(dlms::apdu::XdlmsApduKind::GetResponse, response.kind);
}

TEST(XdlmsSecurity, ServerMapsMalformedProtectedRequest)
{
  dlms::security::InMemoryKeyStore keys;
  InstallKeys(keys);
  dlms::security::InMemoryInvocationCounterStore counters;
  dlms::security::CipheredApduProcessor security(
    MakeContext(dlms::security::SecurityRole::Server),
    keys,
    counters);

  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::XdlmsServerApduProcessor processor(dispatcher, security);

  std::vector<std::uint8_t> malformed;
  malformed.push_back(0x30u);
  malformed.push_back(0x00u);
  malformed.push_back(0x00u);

  std::vector<std::uint8_t> response;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::SecurityFailed,
            processor.ProcessRequest(malformed, response));
  EXPECT_TRUE(response.empty());
  EXPECT_EQ(0, handler.calls);
}

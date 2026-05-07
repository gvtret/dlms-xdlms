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

dlms::xdlms::CosemAttributeDescriptor MakeDescriptor()
{
  dlms::xdlms::CosemAttributeDescriptor descriptor =
    dlms::xdlms::EmptyCosemAttributeDescriptor();
  descriptor.classId = 3u;
  descriptor.instanceId = dlms::xdlms::CosemLogicalName(1, 0, 1, 8, 0, 255);
  descriptor.attributeId = 2u;
  return descriptor;
}

dlms::xdlms::GetIndication MakeIndication()
{
  dlms::xdlms::GetIndication indication =
    dlms::xdlms::EmptyGetIndication();
  indication.invokeId = 7u;
  indication.descriptor = MakeDescriptor();
  return indication;
}

} // namespace

TEST(XdlmsServer, EmptyGetIndicationUsesDefaultServiceOptions)
{
  const dlms::xdlms::GetIndication indication =
    dlms::xdlms::EmptyGetIndication();

  EXPECT_EQ(0u, indication.invokeId);
  EXPECT_TRUE(indication.options.confirmed);
  EXPECT_FALSE(indication.options.highPriority);
}

TEST(XdlmsServer, ValidateInvokeIdAcceptsOnlyFourBitNonZeroRange)
{
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateInvokeId(0u));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dlms::xdlms::ValidateInvokeId(1u));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dlms::xdlms::ValidateInvokeId(15u));
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateInvokeId(16u));
}

TEST(XdlmsServer, DispatchRejectsInvalidInvokeIdBeforeHandler)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::GetIndication indication = MakeIndication();
  dlms::xdlms::GetResult result = dlms::xdlms::EmptyGetResult();
  indication.invokeId = 0u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchGet(indication, result));
  EXPECT_EQ(0, handler.calls);
  EXPECT_EQ(0u, result.invokeId);
}

TEST(XdlmsServer, DispatchRejectsInvalidDescriptorBeforeHandler)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::GetIndication indication = MakeIndication();
  dlms::xdlms::GetResult result = dlms::xdlms::EmptyGetResult();
  indication.descriptor = dlms::xdlms::EmptyCosemAttributeDescriptor();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchGet(indication, result));
  EXPECT_EQ(0, handler.calls);
}

TEST(XdlmsServer, DispatchForwardsDescriptorAndServiceOptions)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::GetIndication indication = MakeIndication();
  dlms::xdlms::GetResult result = dlms::xdlms::EmptyGetResult();
  indication.options.highPriority = true;
  handler.result.hasData = true;
  handler.result.data.push_back(0x11u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchGet(indication, result));

  ASSERT_EQ(1, handler.calls);
  EXPECT_EQ(indication.invokeId, handler.lastIndication.invokeId);
  EXPECT_TRUE(handler.lastIndication.options.highPriority);
  EXPECT_EQ(indication.descriptor.classId,
            handler.lastIndication.descriptor.classId);
  EXPECT_EQ(indication.descriptor.attributeId,
            handler.lastIndication.descriptor.attributeId);
}

TEST(XdlmsServer, DispatchDataResponsePreservesRequestInvokeId)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::GetResult result = dlms::xdlms::EmptyGetResult();
  handler.result.invokeId = 1u;
  handler.result.hasData = true;
  handler.result.data.push_back(0x09u);
  handler.result.data.push_back(0xF1u);

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchGet(MakeIndication(), result));

  EXPECT_EQ(7u, result.invokeId);
  EXPECT_TRUE(result.hasData);
  ASSERT_EQ(2u, result.data.size());
  EXPECT_EQ(0x09u, result.data[0]);
  EXPECT_EQ(0xF1u, result.data[1]);
}

TEST(XdlmsServer, DispatchAccessResultPreservesRequestInvokeId)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::GetResult result = dlms::xdlms::EmptyGetResult();
  handler.result.hasAccessResult = true;
  handler.result.accessResult = 0x0Cu;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchGet(MakeIndication(), result));

  EXPECT_EQ(7u, result.invokeId);
  EXPECT_TRUE(result.hasAccessResult);
  EXPECT_EQ(0x0Cu, result.accessResult);
}

TEST(XdlmsServer, DispatchPropagatesHandlerFailureWithoutResultMutation)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::GetResult result = dlms::xdlms::EmptyGetResult();
  handler.status = dlms::xdlms::XdlmsStatus::UnsupportedFeature;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            dispatcher.DispatchGet(MakeIndication(), result));

  EXPECT_EQ(1, handler.calls);
  EXPECT_EQ(0u, result.invokeId);
  EXPECT_FALSE(result.hasData);
  EXPECT_FALSE(result.hasAccessResult);
}

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

class FakeSetServerHandler : public FakeServerHandler
{
public:
  FakeSetServerHandler()
    : setStatus(dlms::xdlms::XdlmsStatus::Ok)
    , actionStatus(dlms::xdlms::XdlmsStatus::Ok)
    , setCalls(0)
    , actionCalls(0)
    , lastSetIndication(dlms::xdlms::EmptySetIndication())
    , lastActionIndication(dlms::xdlms::EmptyActionIndication())
    , setResult(dlms::xdlms::EmptySetResult())
    , actionResult(dlms::xdlms::EmptyActionResult())
  {
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

  dlms::xdlms::XdlmsStatus setStatus;
  dlms::xdlms::XdlmsStatus actionStatus;
  int setCalls;
  int actionCalls;
  dlms::xdlms::SetIndication lastSetIndication;
  dlms::xdlms::ActionIndication lastActionIndication;
  dlms::xdlms::SetResult setResult;
  dlms::xdlms::ActionResult actionResult;
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

dlms::xdlms::SetIndication MakeSetIndication()
{
  dlms::xdlms::SetIndication indication =
    dlms::xdlms::EmptySetIndication();
  indication.invokeId = 9u;
  indication.descriptor = MakeDescriptor();
  indication.data.push_back(0x12u);
  indication.data.push_back(0x00u);
  indication.data.push_back(0x01u);
  return indication;
}

dlms::xdlms::CosemMethodDescriptor MakeMethodDescriptor()
{
  dlms::xdlms::CosemMethodDescriptor descriptor =
    dlms::xdlms::EmptyCosemMethodDescriptor();
  descriptor.classId = 3u;
  descriptor.instanceId = dlms::xdlms::CosemLogicalName(1, 0, 1, 8, 0, 255);
  descriptor.methodId = 1u;
  return descriptor;
}

dlms::xdlms::ActionIndication MakeActionIndication()
{
  dlms::xdlms::ActionIndication indication =
    dlms::xdlms::EmptyActionIndication();
  indication.invokeId = 10u;
  indication.descriptor = MakeMethodDescriptor();
  indication.hasParameter = true;
  indication.parameter.push_back(0x12u);
  indication.parameter.push_back(0x00u);
  indication.parameter.push_back(0x01u);
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

TEST(XdlmsServer, EmptySetIndicationUsesDefaultServiceOptions)
{
  const dlms::xdlms::SetIndication indication =
    dlms::xdlms::EmptySetIndication();

  EXPECT_EQ(0u, indication.invokeId);
  EXPECT_TRUE(indication.options.confirmed);
  EXPECT_FALSE(indication.options.highPriority);
  EXPECT_TRUE(indication.data.empty());
}

TEST(XdlmsServer, EmptySetResultDefaultsToSuccess)
{
  const dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();

  EXPECT_EQ(0u, result.invokeId);
  EXPECT_EQ(0u, result.accessResult);
}

TEST(XdlmsServer, DispatchSetRejectsInvalidInvokeIdBeforeHandler)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetIndication indication = MakeSetIndication();
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();
  indication.invokeId = 0u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchSet(indication, result));
  EXPECT_EQ(0, handler.setCalls);
  EXPECT_EQ(0u, result.invokeId);
}

TEST(XdlmsServer, DispatchSetRejectsInvalidDescriptorBeforeHandler)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetIndication indication = MakeSetIndication();
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();
  indication.descriptor = dlms::xdlms::EmptyCosemAttributeDescriptor();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchSet(indication, result));
  EXPECT_EQ(0, handler.setCalls);
}

TEST(XdlmsServer, DispatchSetRejectsEmptyDataBeforeHandler)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetIndication indication = MakeSetIndication();
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();
  indication.data.clear();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchSet(indication, result));
  EXPECT_EQ(0, handler.setCalls);
}

TEST(XdlmsServer, DispatchSetForwardsDescriptorOptionsAndData)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetIndication indication = MakeSetIndication();
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();
  indication.options.highPriority = true;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchSet(indication, result));

  ASSERT_EQ(1, handler.setCalls);
  EXPECT_EQ(indication.invokeId, handler.lastSetIndication.invokeId);
  EXPECT_TRUE(handler.lastSetIndication.options.highPriority);
  EXPECT_EQ(indication.descriptor.classId,
            handler.lastSetIndication.descriptor.classId);
  EXPECT_EQ(indication.descriptor.attributeId,
            handler.lastSetIndication.descriptor.attributeId);
  ASSERT_EQ(3u, handler.lastSetIndication.data.size());
  EXPECT_EQ(0x12u, handler.lastSetIndication.data[0]);
}

TEST(XdlmsServer, DispatchSetResponsePreservesRequestInvokeId)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();
  handler.setResult.invokeId = 1u;
  handler.setResult.accessResult = 0u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchSet(MakeSetIndication(), result));

  EXPECT_EQ(9u, result.invokeId);
  EXPECT_EQ(0u, result.accessResult);
}

TEST(XdlmsServer, DispatchSetAccessResultPreservesRequestInvokeId)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();
  handler.setResult.accessResult = 3u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchSet(MakeSetIndication(), result));

  EXPECT_EQ(9u, result.invokeId);
  EXPECT_EQ(3u, result.accessResult);
}

TEST(XdlmsServer, DispatchSetUsesDefaultUnsupportedHandler)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            dispatcher.DispatchSet(MakeSetIndication(), result));

  EXPECT_EQ(0u, result.invokeId);
  EXPECT_EQ(0u, result.accessResult);
}

TEST(XdlmsServer, DispatchSetPropagatesHandlerFailureWithoutResultMutation)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::SetResult result = dlms::xdlms::EmptySetResult();
  handler.setStatus = dlms::xdlms::XdlmsStatus::ServiceRejected;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ServiceRejected,
            dispatcher.DispatchSet(MakeSetIndication(), result));

  EXPECT_EQ(1, handler.setCalls);
  EXPECT_EQ(0u, result.invokeId);
  EXPECT_EQ(0u, result.accessResult);
}

TEST(XdlmsServer, EmptyActionIndicationUsesDefaultServiceOptions)
{
  const dlms::xdlms::ActionIndication indication =
    dlms::xdlms::EmptyActionIndication();

  EXPECT_EQ(0u, indication.invokeId);
  EXPECT_TRUE(indication.options.confirmed);
  EXPECT_FALSE(indication.options.highPriority);
  EXPECT_FALSE(indication.hasParameter);
  EXPECT_TRUE(indication.parameter.empty());
}

TEST(XdlmsServer, DispatchActionRejectsInvalidInvokeIdBeforeHandler)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ActionIndication indication = MakeActionIndication();
  dlms::xdlms::ActionResult result = dlms::xdlms::EmptyActionResult();
  indication.invokeId = 0u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchAction(indication, result));
  EXPECT_EQ(0, handler.actionCalls);
  EXPECT_EQ(0u, result.invokeId);
}

TEST(XdlmsServer, DispatchActionRejectsInvalidDescriptorBeforeHandler)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ActionIndication indication = MakeActionIndication();
  dlms::xdlms::ActionResult result = dlms::xdlms::EmptyActionResult();
  indication.descriptor = dlms::xdlms::EmptyCosemMethodDescriptor();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchAction(indication, result));
  EXPECT_EQ(0, handler.actionCalls);
}

TEST(XdlmsServer, DispatchActionRejectsEmptyPresentParameterBeforeHandler)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ActionIndication indication = MakeActionIndication();
  dlms::xdlms::ActionResult result = dlms::xdlms::EmptyActionResult();
  indication.parameter.clear();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dispatcher.DispatchAction(indication, result));
  EXPECT_EQ(0, handler.actionCalls);
}

TEST(XdlmsServer, DispatchActionForwardsDescriptorOptionsAndParameter)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ActionIndication indication = MakeActionIndication();
  dlms::xdlms::ActionResult result = dlms::xdlms::EmptyActionResult();
  indication.options.highPriority = true;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchAction(indication, result));

  ASSERT_EQ(1, handler.actionCalls);
  EXPECT_EQ(indication.invokeId, handler.lastActionIndication.invokeId);
  EXPECT_TRUE(handler.lastActionIndication.options.highPriority);
  EXPECT_EQ(indication.descriptor.classId,
            handler.lastActionIndication.descriptor.classId);
  EXPECT_EQ(indication.descriptor.methodId,
            handler.lastActionIndication.descriptor.methodId);
  ASSERT_EQ(3u, handler.lastActionIndication.parameter.size());
  EXPECT_EQ(0x12u, handler.lastActionIndication.parameter[0]);
}

TEST(XdlmsServer, DispatchActionResponsePreservesRequestInvokeId)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ActionResult result = dlms::xdlms::EmptyActionResult();
  handler.actionResult.invokeId = 1u;
  handler.actionResult.actionResult = 0u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dispatcher.DispatchAction(MakeActionIndication(), result));

  EXPECT_EQ(10u, result.invokeId);
  EXPECT_EQ(0u, result.actionResult);
}

TEST(XdlmsServer, DispatchActionUsesDefaultUnsupportedHandler)
{
  FakeServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ActionResult result = dlms::xdlms::EmptyActionResult();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::UnsupportedFeature,
            dispatcher.DispatchAction(MakeActionIndication(), result));

  EXPECT_EQ(0u, result.invokeId);
  EXPECT_EQ(0u, result.actionResult);
}

TEST(XdlmsServer, DispatchActionPropagatesHandlerFailureWithoutResultMutation)
{
  FakeSetServerHandler handler;
  dlms::xdlms::XdlmsServerDispatcher dispatcher(handler);
  dlms::xdlms::ActionResult result = dlms::xdlms::EmptyActionResult();
  handler.actionStatus = dlms::xdlms::XdlmsStatus::ServiceRejected;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::ServiceRejected,
            dispatcher.DispatchAction(MakeActionIndication(), result));

  EXPECT_EQ(1, handler.actionCalls);
  EXPECT_EQ(0u, result.invokeId);
  EXPECT_EQ(0u, result.actionResult);
}

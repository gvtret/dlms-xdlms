#include "dlms/xdlms/xdlms_types.hpp"

#include <gtest/gtest.h>

namespace {

TEST(XdlmsTypes, DefaultServiceOptionsAreConfirmedNormalPriority)
{
  const dlms::xdlms::ServiceOptions options =
    dlms::xdlms::DefaultServiceOptions();

  EXPECT_TRUE(options.confirmed);
  EXPECT_FALSE(options.highPriority);
  EXPECT_TRUE(options.allowBlockTransfer);
  EXPECT_GT(options.maxBlockTransferBytes, 0u);
}

TEST(XdlmsTypes, LogicalNameStoresSixBytes)
{
  const dlms::xdlms::CosemLogicalName name(1, 2, 3, 4, 5, 255);

  ASSERT_EQ(6u, name.Size());
  EXPECT_EQ(1u, name[0]);
  EXPECT_EQ(2u, name[1]);
  EXPECT_EQ(3u, name[2]);
  EXPECT_EQ(4u, name[3]);
  EXPECT_EQ(5u, name[4]);
  EXPECT_EQ(255u, name[5]);
  EXPECT_EQ(0u, name[6]);
}

TEST(XdlmsTypes, EmptyDescriptorIsInvalid)
{
  const dlms::xdlms::CosemAttributeDescriptor descriptor =
    dlms::xdlms::EmptyCosemAttributeDescriptor();

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateDescriptor(descriptor));
}

TEST(XdlmsTypes, DescriptorRequiresClassAttributeAndInstance)
{
  dlms::xdlms::CosemAttributeDescriptor descriptor =
    dlms::xdlms::EmptyCosemAttributeDescriptor();
  descriptor.classId = 1u;
  descriptor.instanceId =
    dlms::xdlms::CosemLogicalName(0, 0, 1, 0, 0, 255);
  descriptor.attributeId = 2u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dlms::xdlms::ValidateDescriptor(descriptor));

  descriptor.classId = 0u;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateDescriptor(descriptor));

  descriptor.classId = 1u;
  descriptor.attributeId = 0u;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateDescriptor(descriptor));

  descriptor.attributeId = 2u;
  descriptor.instanceId = dlms::xdlms::CosemLogicalName();
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateDescriptor(descriptor));
}

TEST(XdlmsTypes, MethodDescriptorRequiresClassMethodAndInstance)
{
  dlms::xdlms::CosemMethodDescriptor descriptor =
    dlms::xdlms::EmptyCosemMethodDescriptor();
  descriptor.classId = 1u;
  descriptor.instanceId =
    dlms::xdlms::CosemLogicalName(0, 0, 1, 0, 0, 255);
  descriptor.methodId = 2u;

  EXPECT_EQ(dlms::xdlms::XdlmsStatus::Ok,
            dlms::xdlms::ValidateMethodDescriptor(descriptor));

  descriptor.classId = 0u;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateMethodDescriptor(descriptor));

  descriptor.classId = 1u;
  descriptor.methodId = 0u;
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateMethodDescriptor(descriptor));

  descriptor.methodId = 2u;
  descriptor.instanceId = dlms::xdlms::CosemLogicalName();
  EXPECT_EQ(dlms::xdlms::XdlmsStatus::InvalidArgument,
            dlms::xdlms::ValidateMethodDescriptor(descriptor));
}

TEST(XdlmsTypes, EmptyGetResultClearsFields)
{
  const dlms::xdlms::GetResult result =
    dlms::xdlms::EmptyGetResult();

  EXPECT_EQ(0u, result.invokeId);
  EXPECT_FALSE(result.hasData);
  EXPECT_TRUE(result.data.empty());
  EXPECT_FALSE(result.hasAccessResult);
  EXPECT_EQ(0u, result.accessResult);
}

TEST(XdlmsTypes, EmptyActionResultClearsFields)
{
  const dlms::xdlms::ActionResult result =
    dlms::xdlms::EmptyActionResult();

  EXPECT_EQ(0u, result.invokeId);
  EXPECT_EQ(0u, result.actionResult);
  EXPECT_FALSE(result.hasData);
  EXPECT_TRUE(result.data.empty());
}

TEST(XdlmsTypes, InvokeIdAllocatorUsesStableFourBitRange)
{
  dlms::xdlms::InvokeIdAllocator allocator;

  for (std::uint8_t expected = 1u; expected <= 15u; ++expected) {
    EXPECT_EQ(expected, allocator.Next());
  }
  EXPECT_EQ(1u, allocator.Next());
  EXPECT_EQ(2u, allocator.Next());
}

TEST(XdlmsStatus, NamesStableValues)
{
  EXPECT_STREQ(
    "BlockTransferRequired",
    dlms::xdlms::XdlmsStatusName(
      dlms::xdlms::XdlmsStatus::BlockTransferRequired));
}

} // namespace

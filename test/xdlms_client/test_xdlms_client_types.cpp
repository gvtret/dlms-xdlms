#include "dlms/xdlms_client/xdlms_client_types.hpp"

#include <gtest/gtest.h>

namespace {

TEST(XdlmsClientTypes, DefaultServiceOptionsAreConfirmedNormalPriority)
{
  const dlms::xdlms_client::ServiceOptions options =
    dlms::xdlms_client::DefaultServiceOptions();

  EXPECT_TRUE(options.confirmed);
  EXPECT_FALSE(options.highPriority);
}

TEST(XdlmsClientTypes, LogicalNameStoresSixBytes)
{
  const dlms::xdlms_client::CosemLogicalName name(1, 2, 3, 4, 5, 255);

  ASSERT_EQ(6u, name.Size());
  EXPECT_EQ(1u, name[0]);
  EXPECT_EQ(2u, name[1]);
  EXPECT_EQ(3u, name[2]);
  EXPECT_EQ(4u, name[3]);
  EXPECT_EQ(5u, name[4]);
  EXPECT_EQ(255u, name[5]);
  EXPECT_EQ(0u, name[6]);
}

TEST(XdlmsClientTypes, EmptyDescriptorIsInvalid)
{
  const dlms::xdlms_client::CosemAttributeDescriptor descriptor =
    dlms::xdlms_client::EmptyCosemAttributeDescriptor();

  EXPECT_EQ(dlms::xdlms_client::XdlmsClientStatus::InvalidArgument,
            dlms::xdlms_client::ValidateDescriptor(descriptor));
}

TEST(XdlmsClientTypes, DescriptorRequiresClassAttributeAndInstance)
{
  dlms::xdlms_client::CosemAttributeDescriptor descriptor =
    dlms::xdlms_client::EmptyCosemAttributeDescriptor();
  descriptor.classId = 1u;
  descriptor.instanceId =
    dlms::xdlms_client::CosemLogicalName(0, 0, 1, 0, 0, 255);
  descriptor.attributeId = 2u;

  EXPECT_EQ(dlms::xdlms_client::XdlmsClientStatus::Ok,
            dlms::xdlms_client::ValidateDescriptor(descriptor));

  descriptor.classId = 0u;
  EXPECT_EQ(dlms::xdlms_client::XdlmsClientStatus::InvalidArgument,
            dlms::xdlms_client::ValidateDescriptor(descriptor));

  descriptor.classId = 1u;
  descriptor.attributeId = 0u;
  EXPECT_EQ(dlms::xdlms_client::XdlmsClientStatus::InvalidArgument,
            dlms::xdlms_client::ValidateDescriptor(descriptor));

  descriptor.attributeId = 2u;
  descriptor.instanceId = dlms::xdlms_client::CosemLogicalName();
  EXPECT_EQ(dlms::xdlms_client::XdlmsClientStatus::InvalidArgument,
            dlms::xdlms_client::ValidateDescriptor(descriptor));
}

TEST(XdlmsClientTypes, EmptyGetResultClearsFields)
{
  const dlms::xdlms_client::GetResult result =
    dlms::xdlms_client::EmptyGetResult();

  EXPECT_EQ(0u, result.invokeId);
  EXPECT_FALSE(result.hasData);
  EXPECT_TRUE(result.data.empty());
  EXPECT_FALSE(result.hasAccessResult);
  EXPECT_EQ(0u, result.accessResult);
}

TEST(XdlmsClientTypes, InvokeIdAllocatorUsesStableFourBitRange)
{
  dlms::xdlms_client::InvokeIdAllocator allocator;

  for (std::uint8_t expected = 1u; expected <= 15u; ++expected) {
    EXPECT_EQ(expected, allocator.Next());
  }
  EXPECT_EQ(1u, allocator.Next());
  EXPECT_EQ(2u, allocator.Next());
}

TEST(XdlmsClientStatus, NamesStableValues)
{
  EXPECT_STREQ(
    "BlockTransferRequired",
    dlms::xdlms_client::XdlmsClientStatusName(
      dlms::xdlms_client::XdlmsClientStatus::BlockTransferRequired));
}

} // namespace

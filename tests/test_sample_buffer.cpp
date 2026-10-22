// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic

#include <gtest/gtest.h>
#include <array>
#include "cymon/sample_buffer.hpp"

namespace cymon {
namespace {

void Write(SampleBuffer& buffer, float value) {
  const std::array<float, 1> frame{value};
  buffer.WriteFrame(frame);
}

TEST(SampleBufferTest, SetupValidatesLayout) {
  SampleBuffer buffer;
  EXPECT_TRUE(buffer.Setup(2, 100, 10));
  EXPECT_FALSE(buffer.Setup(0, 10, 0));
  EXPECT_FALSE(buffer.Setup(static_cast<uint8_t>(kMaxChannels + 1U), 10, 0));
  EXPECT_FALSE(buffer.Setup(2, static_cast<uint16_t>(SampleBuffer::kCapacityFloats / 2U + 1U), 0));
  EXPECT_FALSE(buffer.Setup(1, 10, 10));  // no post-trigger frame left
  EXPECT_FALSE(buffer.Setup(1, 0, 0));
  EXPECT_EQ(buffer.CapacityFrames(), SampleBuffer::kCapacityFloats / 2U);  // the valid layout stays
}

TEST(SampleBufferTest, ArmRequiresSetup) {
  SampleBuffer buffer;
  buffer.Arm();
  EXPECT_FALSE(buffer.IsArmed());
  ASSERT_TRUE(buffer.Setup(1, 10, 0));
  buffer.Arm();
  EXPECT_TRUE(buffer.IsArmed());
}

TEST(SampleBufferTest, CompletesAfterPostTriggerFrames) {
  SampleBuffer buffer;
  ASSERT_TRUE(buffer.Setup(1, 2, 0));
  buffer.Arm();
  EXPECT_TRUE(buffer.Trigger());
  EXPECT_FALSE(buffer.Trigger());
  Write(buffer, 1.0F);
  EXPECT_FALSE(buffer.IsComplete());
  Write(buffer, 2.0F);
  EXPECT_TRUE(buffer.IsComplete());
  const std::array<float, 1> frame{3.0F};
  EXPECT_FALSE(buffer.WriteFrame(frame));
}

TEST(SampleBufferTest, ReadsPreAndPostTriggerInTimeOrder) {
  SampleBuffer buffer;
  ASSERT_TRUE(buffer.Setup(1, 7, 2));
  buffer.Arm();
  for (int i = 0; i < 5; ++i) {
    Write(buffer, static_cast<float>(10 + i));
  }
  EXPECT_TRUE(buffer.PretriggerFilled());
  buffer.Trigger();
  for (int i = 0; i < 5; ++i) {
    Write(buffer, static_cast<float>(20 + i));
  }
  ASSERT_TRUE(buffer.IsComplete());

  std::array<float, 7> out{};
  ASSERT_EQ(buffer.ReadFrames(0, 7, out), 7U);
  const std::array<float, 7> expected{13.0F, 14.0F, 20.0F, 21.0F, 22.0F, 23.0F, 24.0F};
  EXPECT_EQ(out, expected);
}

TEST(SampleBufferTest, ReadsAcrossRingWrap) {
  SampleBuffer buffer;
  const auto capacity = static_cast<uint16_t>(SampleBuffer::kCapacityFloats);
  ASSERT_TRUE(buffer.Setup(1, capacity, static_cast<uint16_t>(capacity / 2U)));
  buffer.Arm();
  for (uint32_t i = 0; i < capacity + 17U; ++i) {
    Write(buffer, static_cast<float>(i));
  }
  buffer.Trigger();
  for (uint32_t i = 0; i < capacity - capacity / 2U; ++i) {
    Write(buffer, static_cast<float>(100000U + i));
  }
  ASSERT_TRUE(buffer.IsComplete());

  std::array<float, 2> out{};
  ASSERT_EQ(buffer.ReadFrames(static_cast<uint16_t>(capacity / 2U - 1U), 2, out), 2U);
  EXPECT_FLOAT_EQ(out[0], static_cast<float>(capacity + 16U));  // last pre-trigger frame
  EXPECT_FLOAT_EQ(out[1], 100000.0F);                           // trigger sample
}

TEST(SampleBufferTest, InterleavesChannelsAndLimitsToOutput) {
  SampleBuffer buffer;
  ASSERT_TRUE(buffer.Setup(2, 3, 0));
  buffer.Arm();
  buffer.Trigger();
  for (int i = 0; i < 3; ++i) {
    const std::array<float, 2> frame{static_cast<float>(i), static_cast<float>(10 * i)};
    buffer.WriteFrame(frame);
  }
  std::array<float, 5> out{};  // room for two frames only
  ASSERT_EQ(buffer.ReadFrames(1, 3, out), 2U);
  EXPECT_FLOAT_EQ(out[0], 1.0F);
  EXPECT_FLOAT_EQ(out[1], 10.0F);
  EXPECT_FLOAT_EQ(out[2], 2.0F);
  EXPECT_FLOAT_EQ(out[3], 20.0F);
}

TEST(SampleBufferTest, NothingToReadBeforeTrigger) {
  SampleBuffer buffer;
  ASSERT_TRUE(buffer.Setup(1, 10, 5));
  buffer.Arm();
  Write(buffer, 1.0F);
  std::array<float, 10> out{};
  EXPECT_EQ(buffer.ReadFrames(0, 10, out), 0U);
  EXPECT_FALSE(buffer.PretriggerFilled());
}

TEST(SampleBufferTest, ReadsOnlyWrittenPostTriggerFrames) {
  SampleBuffer buffer;
  ASSERT_TRUE(buffer.Setup(1, 10, 0));
  buffer.Arm();
  buffer.Trigger();
  for (int i = 0; i < 3; ++i) {
    Write(buffer, static_cast<float>(i));
  }
  std::array<float, 10> out{};
  EXPECT_EQ(buffer.ReadFrames(0, 10, out), 3U);
}

TEST(SampleBufferTest, EarlyTriggerZeroFillsMissingPretrigger) {
  SampleBuffer buffer;
  ASSERT_TRUE(buffer.Setup(1, 5, 3));
  buffer.Arm();
  Write(buffer, 99.0F);
  buffer.Trigger();
  Write(buffer, 42.0F);
  Write(buffer, 43.0F);
  ASSERT_TRUE(buffer.IsComplete());

  std::array<float, 5> out{};
  ASSERT_EQ(buffer.ReadFrames(0, 5, out), 5U);
  const std::array<float, 5> expected{0.0F, 0.0F, 99.0F, 42.0F, 43.0F};
  EXPECT_EQ(out, expected);
}

}  // namespace
}  // namespace cymon

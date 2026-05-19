// Copyright 2026 Tecnologic
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "cymon/sample_buffer.hpp"

namespace cymon {
namespace {

TEST(SampleBufferTest, Setup_ValidConfig_ReturnsTrue) {
  SampleBuffer buf;
  EXPECT_TRUE(buf.Setup(2, 100, 10));
}

TEST(SampleBufferTest, Setup_TooManyChannels_ReturnsFalse) {
  SampleBuffer buf;
  // kMaxChannels is 16; use 17 to exceed limit.
  EXPECT_FALSE(buf.Setup(17, 10, 0));
}

TEST(SampleBufferTest, Setup_TooManySamples_ReturnsFalse) {
  SampleBuffer buf;
  // kCapacityFloats / 2 = 512; 513 should fail.
  const uint16_t too_many =
      static_cast<uint16_t>(SampleBuffer::kCapacityFloats / 2 + 1);
  EXPECT_FALSE(buf.Setup(2, too_many, 0));
}

TEST(SampleBufferTest, Setup_PretriggerExceedsSamples_ReturnsFalse) {
  SampleBuffer buf;
  EXPECT_FALSE(buf.Setup(1, 10, 11));
}

TEST(SampleBufferTest, Arm_SetsArmedState) {
  SampleBuffer buf;
  ASSERT_TRUE(buf.Setup(1, 10, 0));
  buf.Arm();
  EXPECT_TRUE(buf.IsArmed());
}

TEST(SampleBufferTest, WriteFrame_StoresData) {
  SampleBuffer buf;
  ASSERT_TRUE(buf.Setup(1, 10, 0));
  buf.Arm();
  float v = 3.14F;
  EXPECT_TRUE(buf.WriteFrame(&v));
}

TEST(SampleBufferTest, Trigger_TransitionsToTriggered) {
  SampleBuffer buf;
  ASSERT_TRUE(buf.Setup(1, 2, 0));
  buf.Arm();
  EXPECT_TRUE(buf.Trigger());
  EXPECT_TRUE(buf.WasTriggered());
}

TEST(SampleBufferTest, WriteFrame_AfterComplete_ReturnsFalse) {
  SampleBuffer buf;
  ASSERT_TRUE(buf.Setup(1, 2, 0));
  buf.Arm();
  buf.Trigger();
  float v = 1.0F;
  buf.WriteFrame(&v);
  buf.WriteFrame(&v);
  EXPECT_TRUE(buf.IsComplete());
  EXPECT_FALSE(buf.WriteFrame(&v));
}

TEST(SampleBufferTest, ReadFrames_PostTriggerData_CorrectValues) {
  SampleBuffer buf;
  // 2 pretrigger + 5 post-trigger = 7 total, 1 channel
  ASSERT_TRUE(buf.Setup(1, 7, 2));
  buf.Arm();

  // Write 5 pre-trigger frames (values 10..14)
  for (int i = 0; i < 5; ++i) {
    float v = static_cast<float>(10 + i);
    buf.WriteFrame(&v);
  }

  buf.Trigger();

  // Write 5 post-trigger frames (values 20..24)
  for (int i = 0; i < 5; ++i) {
    float v = static_cast<float>(20 + i);
    buf.WriteFrame(&v);
  }

  EXPECT_TRUE(buf.IsComplete());

  // Read all 7 frames starting at offset 0.
  float out[7] = {};
  const uint16_t n = buf.ReadFrames(0, 7, out);
  EXPECT_EQ(n, 7U);

  // Pre-trigger frames: the 2 frames just before trigger = values 13, 14
  EXPECT_FLOAT_EQ(out[0], 13.0F);
  EXPECT_FLOAT_EQ(out[1], 14.0F);

  // Post-trigger frames: 20, 21, 22, 23, 24
  EXPECT_FLOAT_EQ(out[2], 20.0F);
  EXPECT_FLOAT_EQ(out[3], 21.0F);
  EXPECT_FLOAT_EQ(out[4], 22.0F);
  EXPECT_FLOAT_EQ(out[5], 23.0F);
  EXPECT_FLOAT_EQ(out[6], 24.0F);
}

TEST(SampleBufferTest, ReadFrames_BeforeTrigger_ReturnsZero) {
  SampleBuffer buf;
  ASSERT_TRUE(buf.Setup(1, 10, 5));
  buf.Arm();
  float v = 1.0F;
  buf.WriteFrame(&v);

  float out[10] = {};
  EXPECT_EQ(buf.ReadFrames(0, 10, out), 0U);
}

TEST(SampleBufferTest, ReadFrames_BeyondWritten_ReturnsAvailable) {
  SampleBuffer buf;
  // 0 pretrigger, 10 post-trigger frames, 1 channel
  ASSERT_TRUE(buf.Setup(1, 10, 0));
  buf.Arm();
  buf.Trigger();

  // Write only 3 post-trigger frames
  for (int i = 0; i < 3; ++i) {
    float v = static_cast<float>(i);
    buf.WriteFrame(&v);
  }

  float out[10] = {};
  // Request 10, but only 3 are available.
  const uint16_t n = buf.ReadFrames(0, 10, out);
  EXPECT_EQ(n, 3U);
}

}  // namespace
}  // namespace cymon

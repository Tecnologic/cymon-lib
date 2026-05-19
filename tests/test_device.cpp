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

#include "cymon/device.hpp"

namespace cymon {
namespace {

TEST(DeviceTest, RegisterVariable_StoresVariable) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("speed", "rpm", [] { return 42.0F; }));
  EXPECT_EQ(dev.variable_count(), 1U);

  Device::VariableInfo info;
  ASSERT_TRUE(dev.GetVariable(0, &info));
  EXPECT_STREQ(info.name, "speed");
  EXPECT_STREQ(info.unit, "rpm");
  EXPECT_EQ(info.id, 0U);
}

TEST(DeviceTest, RegisterVariable_TooManyVars_ReturnsFalse) {
  Device dev;
  for (std::size_t i = 0; i < kMaxVars; ++i) {
    ASSERT_TRUE(dev.RegisterVariable("v", "u", [] { return 0.0F; }));
  }
  EXPECT_FALSE(dev.RegisterVariable("overflow", "u", [] { return 0.0F; }));
}

TEST(DeviceTest, GetVariable_OutOfRange_ReturnsFalse) {
  Device dev;
  Device::VariableInfo info;
  EXPECT_FALSE(dev.GetVariable(0, &info));
}

TEST(DeviceTest, GetVariable_WalksFullList) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("a", "u", [] { return 0.0F; }));
  ASSERT_TRUE(dev.RegisterVariable("b", "u", [] { return 1.0F; }));
  ASSERT_TRUE(dev.RegisterVariable("c", "u", [] { return 2.0F; }));

  Device::VariableInfo info;
  EXPECT_TRUE(dev.GetVariable(0, &info));
  EXPECT_TRUE(dev.GetVariable(1, &info));
  EXPECT_TRUE(dev.GetVariable(2, &info));
  EXPECT_FALSE(dev.GetVariable(3, &info));
}

TEST(DeviceTest, HandleGetBufferInfo_ReturnsCapacity) {
  Device dev;
  const auto resp = dev.HandleGetBufferInfo();
  EXPECT_EQ(resp.buffer_bytes, static_cast<uint32_t>(kMaxBufferBytes));
  EXPECT_EQ(resp.max_channels, static_cast<uint8_t>(kMaxChannels));
  EXPECT_GT(resp.max_frames, 0U);
}

TEST(DeviceTest, HandleSetupCapture_ValidConfig_ReturnsOk) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [] { return 0.0F; }));
  ASSERT_TRUE(dev.RegisterVariable("v1", "u", [] { return 1.0F; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 0;
  cfg.channel_ids[1] = 1;
  cfg.num_channels = 2;
  cfg.num_samples = 10;
  cfg.pretrigger_samples = 2;
  cfg.sample_period_us = 100.0F;

  const auto resp = dev.HandleSetupCapture(cfg);
  EXPECT_TRUE(resp.ok);
  EXPECT_EQ(resp.error_code, Device::ErrorCode::kNone);
}

TEST(DeviceTest, HandleSetupCapture_BadChannelId_ReturnsError) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [] { return 0.0F; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 5;  // doesn't exist
  cfg.num_channels = 1;
  cfg.num_samples = 10;
  cfg.pretrigger_samples = 0;
  cfg.sample_period_us = 100.0F;

  const auto resp = dev.HandleSetupCapture(cfg);
  EXPECT_FALSE(resp.ok);
  EXPECT_EQ(resp.error_code, Device::ErrorCode::kBadChannelId);
}

TEST(DeviceTest, HandleSetupCapture_ActualPeriodReturned) {
  Device dev;
  dev.set_actual_sample_period_us(100.0F);
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [] { return 0.0F; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 0;
  cfg.num_channels = 1;
  cfg.num_samples = 10;
  cfg.pretrigger_samples = 0;
  cfg.sample_period_us = 50.0F;

  const auto resp = dev.HandleSetupCapture(cfg);
  EXPECT_TRUE(resp.ok);
  EXPECT_FLOAT_EQ(resp.actual_sample_period_us, 100.0F);
}

TEST(DeviceTest, HandleSetupCapture_DefaultsToRequestedPeriod) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [] { return 0.0F; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 0;
  cfg.num_channels = 1;
  cfg.num_samples = 10;
  cfg.pretrigger_samples = 0;
  cfg.sample_period_us = 75.0F;

  const auto resp = dev.HandleSetupCapture(cfg);
  EXPECT_TRUE(resp.ok);
  EXPECT_FLOAT_EQ(resp.actual_sample_period_us, 75.0F);
}

TEST(DeviceTest, HandleArmTrigger_ValidConfig_ReturnsOk) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [] { return 0.0F; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 0;
  cfg.num_channels = 1;
  cfg.num_samples = 10;
  cfg.pretrigger_samples = 0;
  cfg.sample_period_us = 100.0F;
  ASSERT_TRUE(dev.HandleSetupCapture(cfg).ok);

  Device::ArmTriggerConfig atcfg;
  atcfg.arm = true;
  atcfg.source_variable_id = 0;
  atcfg.trigger_mode = TriggerMode::kRising;
  atcfg.trigger_level = 1.0F;

  const auto resp = dev.HandleArmTrigger(atcfg);
  EXPECT_TRUE(resp.ok);
}

TEST(DeviceTest, HandleArmTrigger_BadSourceVar_ReturnsError) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [] { return 0.0F; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 0;
  cfg.num_channels = 1;
  cfg.num_samples = 10;
  cfg.pretrigger_samples = 0;
  cfg.sample_period_us = 100.0F;
  ASSERT_TRUE(dev.HandleSetupCapture(cfg).ok);

  Device::ArmTriggerConfig atcfg;
  atcfg.arm = true;
  atcfg.source_variable_id = 99;  // invalid
  const auto resp = dev.HandleArmTrigger(atcfg);
  EXPECT_FALSE(resp.ok);
  EXPECT_EQ(resp.error_code, Device::ErrorCode::kBadChannelId);
}

TEST(DeviceTest, HandleArmTrigger_Disarm_ReturnsOk) {
  Device dev;
  Device::ArmTriggerConfig atcfg;
  atcfg.arm = false;
  const auto resp = dev.HandleArmTrigger(atcfg);
  EXPECT_TRUE(resp.ok);
}

TEST(DeviceTest, Tick_CapturesFrames) {
  float val0 = 1.0F;
  float val1 = 2.0F;

  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [&val0] { return val0; }));
  ASSERT_TRUE(dev.RegisterVariable("v1", "u", [&val1] { return val1; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 0;
  cfg.channel_ids[1] = 1;
  cfg.num_channels = 2;
  cfg.num_samples = 4;
  cfg.pretrigger_samples = 0;
  cfg.sample_period_us = 100.0F;
  ASSERT_TRUE(dev.HandleSetupCapture(cfg).ok);

  // Arm with immediate force trigger (level very low, rising).
  Device::ArmTriggerConfig atcfg;
  atcfg.arm = true;
  atcfg.source_variable_id = 0;
  atcfg.trigger_mode = TriggerMode::kRising;
  atcfg.trigger_level = 0.5F;
  ASSERT_TRUE(dev.HandleArmTrigger(atcfg).ok);

  // Force trigger since value is already above level.
  dev.ForceTrigger();

  // Tick 4 times to fill post-trigger frames.
  for (int i = 0; i < 4; ++i) {
    dev.Tick();
  }

  const auto rs = dev.HandleReadSamples(0, 4);
  EXPECT_TRUE(rs.triggered);
  EXPECT_EQ(rs.num_channels, 2U);
  // Samples should have val0=1.0, val1=2.0 interleaved.
  EXPECT_FLOAT_EQ(rs.samples[0], 1.0F);
  EXPECT_FLOAT_EQ(rs.samples[1], 2.0F);
}

TEST(DeviceTest, ForceTrigger_TriggersCapture) {
  Device dev;
  ASSERT_TRUE(dev.RegisterVariable("v0", "u", [] { return 0.0F; }));

  Device::CaptureConfig cfg;
  cfg.channel_ids[0] = 0;
  cfg.num_channels = 1;
  cfg.num_samples = 4;
  cfg.pretrigger_samples = 0;
  cfg.sample_period_us = 100.0F;
  ASSERT_TRUE(dev.HandleSetupCapture(cfg).ok);

  Device::ArmTriggerConfig atcfg;
  atcfg.arm = true;
  atcfg.source_variable_id = 0;
  ASSERT_TRUE(dev.HandleArmTrigger(atcfg).ok);

  dev.ForceTrigger();

  // Fill post-trigger samples.
  for (int i = 0; i < 4; ++i) dev.Tick();

  const auto rs = dev.HandleReadSamples(0, 4);
  EXPECT_TRUE(rs.triggered);
}

}  // namespace
}  // namespace cymon

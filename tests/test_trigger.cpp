// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic

#include <gtest/gtest.h>
#include "cymon/trigger.hpp"

namespace cymon {
namespace {

TEST(TriggerTest, RisingEdgeFiresOnCrossing) {
  Trigger trigger;
  trigger.Configure({.mode = TriggerMode::kRising, .level = 1.0F});
  trigger.Arm(0.0F);
  EXPECT_FALSE(trigger.Evaluate(0.2F, 0.9F));
  EXPECT_TRUE(trigger.Evaluate(0.5F, 1.5F));
  EXPECT_FALSE(trigger.IsArmed());
}

TEST(TriggerTest, FallingEdgeFiresOnCrossing) {
  Trigger trigger;
  trigger.Configure({.mode = TriggerMode::kFalling, .level = 1.0F});
  trigger.Arm(2.0F);
  EXPECT_FALSE(trigger.Evaluate(1.5F, 1.2F));
  EXPECT_TRUE(trigger.Evaluate(1.5F, 0.5F));
}

TEST(TriggerTest, HysteresisFiresOnlyAfterDropBelowBand) {
  Trigger trigger;
  trigger.Configure({.mode = TriggerMode::kHysteresis, .level = 1.0F, .hysteresis_band = 0.5F});
  trigger.Arm(2.0F);  // starts above the level: waits for a drop below 0.5
  EXPECT_FALSE(trigger.Evaluate(2.0F, 0.6F));
  EXPECT_FALSE(trigger.Evaluate(0.6F, 1.2F));  // rising, but not re-armed yet
  EXPECT_FALSE(trigger.Evaluate(1.2F, 0.4F));
  EXPECT_TRUE(trigger.Evaluate(0.4F, 1.1F));
  EXPECT_FALSE(trigger.IsArmed());
}

TEST(TriggerTest, ImmediateFiresOnFirstEvaluation) {
  Trigger trigger;
  trigger.Configure({.mode = TriggerMode::kImmediate});
  trigger.Arm();
  EXPECT_TRUE(trigger.Evaluate(0.0F, 0.0F));
}

TEST(TriggerTest, DisarmedNeverFires) {
  Trigger trigger;
  trigger.Configure({.mode = TriggerMode::kRising, .level = 1.0F});
  trigger.Arm(0.0F);
  trigger.Disarm();
  EXPECT_FALSE(trigger.Evaluate(0.5F, 1.5F));
}

TEST(TriggerTest, RearmFiresAgain) {
  Trigger trigger;
  trigger.Configure({.mode = TriggerMode::kRising, .level = 1.0F});
  trigger.Arm(0.0F);
  EXPECT_TRUE(trigger.Evaluate(0.5F, 1.5F));
  EXPECT_FALSE(trigger.Evaluate(0.5F, 1.5F));
  trigger.Arm(0.0F);
  EXPECT_TRUE(trigger.Evaluate(0.0F, 2.0F));
}

TEST(TriggerTest, ValidModes) {
  EXPECT_TRUE(IsValidTriggerMode(3U));
  EXPECT_FALSE(IsValidTriggerMode(4U));
}

}  // namespace
}  // namespace cymon

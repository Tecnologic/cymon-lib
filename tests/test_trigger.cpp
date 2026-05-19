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

#include "cymon/trigger.hpp"

namespace cymon {
namespace {

TEST(TriggerTest, RisingEdge_FiresOnCrossing) {
  Trigger t;
  t.Configure({TriggerMode::kRising, 1.0F, 0.0F});
  t.Arm(0.0F);
  EXPECT_TRUE(t.Evaluate(0.5F, 1.5F));
}

TEST(TriggerTest, RisingEdge_NoFireBelowLevel) {
  Trigger t;
  t.Configure({TriggerMode::kRising, 1.0F, 0.0F});
  t.Arm(0.0F);
  EXPECT_FALSE(t.Evaluate(0.2F, 0.9F));
}

TEST(TriggerTest, FallingEdge_FiresOnCrossing) {
  Trigger t;
  t.Configure({TriggerMode::kFalling, 1.0F, 0.0F});
  t.Arm(2.0F);
  EXPECT_TRUE(t.Evaluate(1.5F, 0.5F));
}

TEST(TriggerTest, FallingEdge_NoFireAboveLevel) {
  Trigger t;
  t.Configure({TriggerMode::kFalling, 1.0F, 0.0F});
  t.Arm(2.0F);
  EXPECT_FALSE(t.Evaluate(1.5F, 1.2F));
}

TEST(TriggerTest, Hysteresis_FiresAfterDropAndRise) {
  Trigger t;
  // level=1.0, band=0.5 → lower=0.5
  t.Configure({TriggerMode::kHysteresis, 1.0F, 0.5F});
  // Arm with value above level → awaiting_low_=true
  t.Arm(2.0F);
  EXPECT_TRUE(t.IsArmed());

  // Still above lower threshold — should not fire
  EXPECT_FALSE(t.Evaluate(2.0F, 0.6F));
  EXPECT_TRUE(t.IsArmed());

  // Drop below lower threshold (0.5)
  EXPECT_FALSE(t.Evaluate(0.6F, 0.4F));
  EXPECT_TRUE(t.IsArmed());

  // Rise above level (1.0) → fires
  EXPECT_TRUE(t.Evaluate(0.4F, 1.1F));
  EXPECT_FALSE(t.IsArmed());
}

TEST(TriggerTest, Hysteresis_NoFireIfAlreadyHigh) {
  Trigger t;
  t.Configure({TriggerMode::kHysteresis, 1.0F, 0.5F});
  t.Arm(2.0F);  // awaiting_low_=true
  // Stays above level without dropping below lower
  EXPECT_FALSE(t.Evaluate(1.5F, 1.2F));
  EXPECT_FALSE(t.Evaluate(1.2F, 1.8F));
  EXPECT_TRUE(t.IsArmed());
}

TEST(TriggerTest, Disarm_PreventsEvaluation) {
  Trigger t;
  t.Configure({TriggerMode::kRising, 1.0F, 0.0F});
  t.Arm(0.0F);
  t.Disarm();
  EXPECT_FALSE(t.Evaluate(0.5F, 1.5F));
}

TEST(TriggerTest, Rearm_FiresAgain) {
  Trigger t;
  t.Configure({TriggerMode::kRising, 1.0F, 0.0F});
  t.Arm(0.0F);
  EXPECT_TRUE(t.Evaluate(0.5F, 1.5F));
  EXPECT_FALSE(t.IsArmed());

  t.Arm(0.0F);
  EXPECT_TRUE(t.Evaluate(0.0F, 2.0F));
}

}  // namespace
}  // namespace cymon

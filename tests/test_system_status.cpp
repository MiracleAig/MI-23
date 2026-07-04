#include <gtest/gtest.h>

#include "app/home/calculator_home.h"
#include "app/ui/system_status.h"

TEST(SystemStatusState, FormatsCompactLabelsForTitleBar) {
    EXPECT_STREQ(inputLayerLabel(InputLayer::Base), "L1");
    EXPECT_STREQ(inputLayerLabel(InputLayer::Second), "L2");
    EXPECT_STREQ(inputLayerLabel(InputLayer::Alpha), "L3");

    EXPECT_STREQ(compactAngleModeLabel(AngleMode::Radians), "RAD");
    EXPECT_STREQ(compactAngleModeLabel(AngleMode::Degrees), "DEG");
}

TEST(SystemStatusState, ClampsBatteryAndTracksAppTitles) {
    SystemStatusState status;

    status.setBatteryPercentage(140);
    EXPECT_EQ(status.batteryPercentage(), 100);

    status.setBatteryPercentage(-12);
    EXPECT_EQ(status.batteryPercentage(), 0);

    status.setAppTitle(appTitleForId(AppId::Matrix));
    EXPECT_STREQ(status.appTitle(), "Matrix");
}

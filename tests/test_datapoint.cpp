#include <gtest/gtest.h>
#include "../src/core/DataPoint.h"
#include <cmath>

TEST(DataPoint, Initialization)
{
    core::DataPoint dp;
    dp.setX(10.0);
    dp.setY(20.0);
    dp.rssi = -50.0;
    dp.timestamp_ms = 1234567890;
    dp.zero_latitude = 37.7749;
    dp.zero_longitude = -122.4194;

    EXPECT_DOUBLE_EQ(dp.getX(), 10.0);
    EXPECT_DOUBLE_EQ(dp.getY(), 20.0);
    EXPECT_DOUBLE_EQ(dp.rssi, -50.0);
    EXPECT_EQ(dp.timestamp_ms, 1234567890);
    EXPECT_DOUBLE_EQ(dp.zero_latitude, 37.7749);
    EXPECT_DOUBLE_EQ(dp.zero_longitude, -122.4194);
}
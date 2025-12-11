#include <gtest/gtest.h>
#include "../src/core/DataPoint.h"
#include <cmath>

// ====================
// Basic Initialization
// ====================

TEST(DataPoint, DefaultConstruction)
{
    core::DataPoint dp;

    EXPECT_EQ(dp.rssi, 0);
    EXPECT_EQ(dp.timestamp_ms, 0);
    EXPECT_TRUE(dp.ssid.empty());
    EXPECT_TRUE(dp.dev_id.empty());
    EXPECT_DOUBLE_EQ(dp.zero_latitude, 0.0);
    EXPECT_DOUBLE_EQ(dp.zero_longitude, 0.0);
}

TEST(DataPoint, ParameterizedConstruction)
{
    core::DataPoint dp;
    dp.zero_latitude = 57.0;
    dp.zero_longitude = 11.0;
    dp.setLatitude(57.7);
    dp.setLongitude(11.9);
    dp.rssi = -50;
    dp.timestamp_ms = 1234567890;
    dp.ssid = "TestSSID";
    dp.dev_id = "device1";
    dp.computeCoordinates();

    EXPECT_EQ(dp.rssi, -50);
    EXPECT_EQ(dp.timestamp_ms, 1234567890);
    EXPECT_EQ(dp.ssid, "TestSSID");
    EXPECT_EQ(dp.dev_id, "device1");
    EXPECT_DOUBLE_EQ(dp.zero_latitude, 57.0);
    EXPECT_DOUBLE_EQ(dp.zero_longitude, 11.0);
    EXPECT_TRUE(dp.validCoordinates());
}

TEST(DataPoint, UniquePointIds)
{
    core::DataPoint dp1;
    core::DataPoint dp2;
    core::DataPoint dp3;

    EXPECT_NE(dp1.point_id, dp2.point_id);
    EXPECT_NE(dp2.point_id, dp3.point_id);
    EXPECT_NE(dp1.point_id, dp3.point_id);
}

// ====================
// Setters and Getters
// ====================

TEST(DataPoint, SetGetXY)
{
    core::DataPoint dp;
    dp.setX(10.0);
    dp.setY(20.0);

    EXPECT_DOUBLE_EQ(dp.getX(), 10.0);
    EXPECT_DOUBLE_EQ(dp.getY(), 20.0);
}

TEST(DataPoint, SetGetLatLon)
{
    core::DataPoint dp;
    dp.zero_latitude = 57.0;
    dp.zero_longitude = 11.0;
    dp.setLatitude(57.5);
    dp.setLongitude(11.5);
    dp.computeCoordinates();

    EXPECT_DOUBLE_EQ(dp.getLatitude(), 57.5);
    EXPECT_DOUBLE_EQ(dp.getLongitude(), 11.5);
}

TEST(DataPoint, GetXThrowsIfNotComputed)
{
    core::DataPoint dp;
    // X not set or computed
    EXPECT_THROW(dp.getX(), std::runtime_error);
}

TEST(DataPoint, GetYThrowsIfNotComputed)
{
    core::DataPoint dp;
    // Y not set or computed
    EXPECT_THROW(dp.getY(), std::runtime_error);
}

TEST(DataPoint, GetLatitudeThrowsIfNotComputed)
{
    core::DataPoint dp;
    dp.setX(100.0);
    dp.setY(100.0);
    // Latitude not yet computed from x/y
    EXPECT_THROW(dp.getLatitude(), std::runtime_error);
}

TEST(DataPoint, GetLongitudeThrowsIfNotComputed)
{
    core::DataPoint dp;
    dp.setX(100.0);
    dp.setY(100.0);
    // Longitude not yet computed from x/y
    EXPECT_THROW(dp.getLongitude(), std::runtime_error);
}

// ====================
// Coordinate Computation
// ====================

TEST(DataPoint, ComputeXYFromLatLon)
{
    core::DataPoint dp;
    dp.zero_latitude = 57.0;
    dp.zero_longitude = 11.0;
    dp.setLatitude(57.0);
    dp.setLongitude(11.0);
    dp.computeCoordinates();

    // At zero point, x and y should be 0
    EXPECT_NEAR(dp.getX(), 0.0, 1e-9);
    EXPECT_NEAR(dp.getY(), 0.0, 1e-9);
}

TEST(DataPoint, ComputeLatLonFromXY)
{
    core::DataPoint dp;
    dp.zero_latitude = 57.0;
    dp.zero_longitude = 11.0;
    dp.setX(0.0);
    dp.setY(0.0);
    dp.computeCoordinates();

    // At x=0, y=0, should be at zero point
    EXPECT_NEAR(dp.getLatitude(), 57.0, 1e-9);
    EXPECT_NEAR(dp.getLongitude(), 11.0, 1e-9);
}

TEST(DataPoint, ComputeCoordinatesThrowsIfInsufficient)
{
    core::DataPoint dp;
    // Neither lat/lon nor x/y set
    EXPECT_THROW(dp.computeCoordinates(), std::runtime_error);
}

TEST(DataPoint, ComputeCoordinatesRoundTrip)
{
    // Start with lat/lon, compute x/y, then verify lat/lon is preserved
    double lat = 57.7;
    double lon = 11.9;
    double zero_lat = 57.0;
    double zero_lon = 11.0;

    core::DataPoint dp;
    dp.zero_latitude = zero_lat;
    dp.zero_longitude = zero_lon;
    dp.setLatitude(lat);
    dp.setLongitude(lon);
    dp.rssi = -50;
    dp.computeCoordinates();

    EXPECT_TRUE(dp.validCoordinates());
    EXPECT_NEAR(dp.getLatitude(), lat, 1e-9);
    EXPECT_NEAR(dp.getLongitude(), lon, 1e-9);

    // x/y should be non-zero since we're offset from zero point
    EXPECT_NE(dp.getX(), 0.0);
    EXPECT_NE(dp.getY(), 0.0);
}

TEST(DataPoint, ComputeCoordinatesXYToLatLonRoundTrip)
{
    core::DataPoint dp1;
    dp1.zero_latitude = 57.0;
    dp1.zero_longitude = 11.0;
    dp1.setX(1000.0); // 1km east
    dp1.setY(500.0);  // 500m north
    dp1.computeCoordinates();

    double lat = dp1.getLatitude();
    double lon = dp1.getLongitude();

    // Create new point from computed lat/lon
    core::DataPoint dp2;
    dp2.zero_latitude = 57.0;
    dp2.zero_longitude = 11.0;
    dp2.setLatitude(lat);
    dp2.setLongitude(lon);
    dp2.rssi = -50;
    dp2.computeCoordinates();

    EXPECT_NEAR(dp2.getX(), 1000.0, 0.01);
    EXPECT_NEAR(dp2.getY(), 500.0, 0.01);
}

TEST(DataPoint, SetXInvalidatesLatLon)
{
    core::DataPoint dp;
    dp.zero_latitude = 57.0;
    dp.zero_longitude = 11.0;
    dp.setLatitude(57.5);
    dp.setLongitude(11.5);
    dp.computeCoordinates();
    EXPECT_TRUE(dp.validCoordinates());

    // Setting X should invalidate lat/lon
    dp.setX(100.0);
    EXPECT_THROW(dp.getLatitude(), std::runtime_error);
    EXPECT_THROW(dp.getLongitude(), std::runtime_error);
}

TEST(DataPoint, SetLatitudeInvalidatesXY)
{
    core::DataPoint dp;
    dp.setX(100.0);
    dp.setY(100.0);

    // Setting latitude should invalidate x/y
    dp.setLatitude(57.5);
    EXPECT_THROW(dp.getX(), std::runtime_error);
    EXPECT_THROW(dp.getY(), std::runtime_error);
}

// ====================
// validCoordinates
// ====================

TEST(DataPoint, ValidCoordinates_AllSet)
{
    core::DataPoint dp;
    dp.zero_latitude = 57.0;
    dp.zero_longitude = 11.0;
    dp.setLatitude(57.5);
    dp.setLongitude(11.5);
    dp.computeCoordinates();
    EXPECT_TRUE(dp.validCoordinates());
}

TEST(DataPoint, ValidCoordinates_NotComputed)
{
    core::DataPoint dp;
    dp.setX(100.0);
    dp.setY(100.0);
    // lat/lon not computed yet
    EXPECT_FALSE(dp.validCoordinates());
}

TEST(DataPoint, ValidCoordinates_InvalidLatitude)
{
    core::DataPoint dp;
    dp.zero_latitude = 0.0;
    dp.zero_longitude = 0.0;
    dp.setLatitude(100.0); // Invalid: > 90
    dp.setLongitude(50.0);
    dp.computeCoordinates();

    EXPECT_FALSE(dp.validCoordinates());
}

TEST(DataPoint, ValidCoordinates_InvalidLongitude)
{
    core::DataPoint dp;
    dp.zero_latitude = 0.0;
    dp.zero_longitude = 0.0;
    dp.setLatitude(45.0);
    dp.setLongitude(200.0); // Invalid: > 180
    dp.computeCoordinates();

    EXPECT_FALSE(dp.validCoordinates());
}

// ====================
// distanceBetween
// ====================

TEST(DataPoint, DistanceBetween_SamePoint)
{
    double dist = core::distanceBetween(57.0, 11.0, 57.0, 11.0);
    EXPECT_DOUBLE_EQ(dist, 0.0);
}

TEST(DataPoint, DistanceBetween_KnownDistance)
{
    // ~111km per degree of latitude at this location
    double dist = core::distanceBetween(57.0, 11.0, 58.0, 11.0);
    EXPECT_NEAR(dist, 111000.0, 2000.0); // Allow 2km tolerance
}

TEST(DataPoint, DistanceBetween_Symmetric)
{
    double dist1 = core::distanceBetween(57.0, 11.0, 58.0, 12.0);
    double dist2 = core::distanceBetween(58.0, 12.0, 57.0, 11.0);
    EXPECT_DOUBLE_EQ(dist1, dist2);
}

TEST(DataPoint, DistanceBetween_SmallDistance)
{
    // Test a small distance (~100m)
    // 0.001 degrees latitude ≈ 111m
    double dist = core::distanceBetween(57.0, 11.0, 57.001, 11.0);
    EXPECT_NEAR(dist, 111.0, 5.0); // Allow 5m tolerance
}
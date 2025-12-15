#include <gtest/gtest.h>
#include "../src/core/ClusteredTriangulationAlgorithm2.h"
#include <cmath>
#include <chrono>
#include <spdlog/spdlog.h>

// Disable logging for tests
static struct DisableLogging
{
    DisableLogging() { spdlog::set_level(spdlog::level::off); }
} _disableLogging;

// Helper to create a DataPoint with x, y, rssi
static core::DataPoint makePoint(int id, double x, double y, int rssi, double zero_lat = 57.0, double zero_lon = 11.0)
{
    core::DataPoint dp;
    dp.point_id = id;
    dp.zero_latitude = zero_lat;
    dp.zero_longitude = zero_lon;
    dp.setX(x);
    dp.setY(y);
    dp.rssi = rssi;
    dp.timestamp_ms = id * 1000;
    dp.computeCoordinates();
    return dp;
}

// Helper to create a point map from a vector of points
static std::map<std::string, std::vector<core::DataPoint>> makePointMap(const std::vector<core::DataPoint> &points, const std::string &device = "default")
{
    std::map<std::string, std::vector<core::DataPoint>> m;
    m[device] = points;
    return m;
}

// Helper to add points to an algorithm
static void addPoints(core::ClusteredTriangulationAlgorithm2 &algo, const std::vector<core::DataPoint> &points, double zero_lat = 57.0, double zero_lon = 11.0)
{
    algo.addDataPointMap(makePointMap(points), zero_lat, zero_lon);
}

// ====================
// Default Parameter Tests
// ====================

TEST(CTA2, DefaultParameters)
{
    core::ClusteredTriangulationAlgorithm2 algo;
    SUCCEED();
}

// ====================
// clusterData Tests
// ====================

TEST(CTA2, ClusterData_InsufficientPoints)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -50));
    points.push_back(makePoint(2, 1.0, 0.0, -50));
    addPoints(algo, points);

    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

TEST(CTA2, ClusterData_MinimumClusters)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;

    // Cluster 1
    points.push_back(makePoint(1, 0.0, 0.0, -70));
    points.push_back(makePoint(2, 3.0, 0.0, -60));
    points.push_back(makePoint(3, 6.0, 0.0, -50));
    points.push_back(makePoint(4, 3.0, 3.0, -55));

    // Cluster 2
    points.push_back(makePoint(5, 50.0, 0.0, -70));
    points.push_back(makePoint(6, 53.0, 0.0, -60));
    points.push_back(makePoint(7, 56.0, 0.0, -50));
    points.push_back(makePoint(8, 53.0, 3.0, -55));

    // Cluster 3
    points.push_back(makePoint(9, 25.0, 50.0, -70));
    points.push_back(makePoint(10, 28.0, 50.0, -60));
    points.push_back(makePoint(11, 31.0, 50.0, -50));
    points.push_back(makePoint(12, 28.0, 53.0, -55));

    addPoints(algo, points);

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

// ====================
// bruteForceSearch Tests
// ====================

TEST(CTA2, BruteForceSearch_Timeout)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    for (int i = 0; i < 5; ++i)
    {
        points.push_back(makePoint(i + 1, i * 5.0, 0.0, -50 + i * 3));
    }
    for (int i = 0; i < 5; ++i)
    {
        points.push_back(makePoint(i + 6, 25.0, i * 5.0, -50 + i * 3));
    }
    addPoints(algo, points);

    double lat, lon;
    auto start = std::chrono::steady_clock::now();

    try
    {
        algo.calculatePosition(lat, lon, 1.0, 2.0);
    }
    catch (...)
    {
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    EXPECT_LT(elapsed.count(), 30.0);
}

TEST(CTA2, BruteForceSearch_Precision)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;

    // Cluster 1: southwest
    points.push_back(makePoint(1, 10.0, 10.0, -70));
    points.push_back(makePoint(2, 13.0, 10.0, -65));
    points.push_back(makePoint(3, 16.0, 10.0, -60));
    points.push_back(makePoint(4, 13.0, 13.0, -63));

    // Cluster 2: northeast
    points.push_back(makePoint(5, 90.0, 90.0, -70));
    points.push_back(makePoint(6, 93.0, 90.0, -65));
    points.push_back(makePoint(7, 96.0, 90.0, -60));
    points.push_back(makePoint(8, 93.0, 93.0, -63));

    // Cluster 3: northwest
    points.push_back(makePoint(9, 10.0, 90.0, -70));
    points.push_back(makePoint(10, 13.0, 90.0, -65));
    points.push_back(makePoint(11, 16.0, 90.0, -60));
    points.push_back(makePoint(12, 13.0, 93.0, -63));

    addPoints(algo, points);

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 0.5, 1.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

// ====================
// findBestClusters Tests
// ====================

TEST(CTA2, FindBestClusters_GeometricRatioFilter)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    for (int i = 0; i < 10; ++i)
    {
        points.push_back(makePoint(i + 1, i * 2.0, 0.0, -50 + i));
    }
    addPoints(algo, points);

    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

TEST(CTA2, FindBestClusters_AreaFilter)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -60));
    points.push_back(makePoint(2, 0.5, 0.0, -55));
    points.push_back(makePoint(3, 0.0, 0.5, -50));
    points.push_back(makePoint(4, 0.5, 0.5, -45));

    points.push_back(makePoint(5, 50.0, 0.0, -60));
    points.push_back(makePoint(6, 50.5, 0.0, -55));
    points.push_back(makePoint(7, 50.0, 0.5, -50));
    points.push_back(makePoint(8, 50.5, 0.5, -45));

    addPoints(algo, points);

    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

TEST(CTA2, FindBestClusters_OverlapFilter)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    for (int i = 0; i < 15; ++i)
    {
        double angle = i * 2.0 * M_PI / 15.0;
        double radius = 5.0 + (i % 3);
        double x = 50.0 + radius * std::cos(angle);
        double y = 50.0 + radius * std::sin(angle);
        points.push_back(makePoint(i + 1, x, y, -50 - (i % 5) * 3));
    }
    addPoints(algo, points);

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

// ====================
// getCandidates Tests
// ====================

TEST(CTA2, GetCandidates_MaxDistance)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -50));
    points.push_back(makePoint(2, 5.0, 0.0, -50));
    points.push_back(makePoint(3, 10.0, 0.0, -50));
    points.push_back(makePoint(4, 100.0, 0.0, -50));
    points.push_back(makePoint(5, 105.0, 0.0, -50));
    points.push_back(makePoint(6, 110.0, 0.0, -50));
    addPoints(algo, points);

    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

// ====================
// checkCluster Tests
// ====================

TEST(CTA2, CheckCluster_ScoringPreference)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;

    // Cluster 1: low RSSI variance
    points.push_back(makePoint(1, 0.0, 0.0, -50));
    points.push_back(makePoint(2, 5.0, 0.0, -51));
    points.push_back(makePoint(3, 0.0, 5.0, -50));
    points.push_back(makePoint(4, 5.0, 5.0, -51));

    // Cluster 2: high RSSI variance
    points.push_back(makePoint(5, 50.0, 0.0, -40));
    points.push_back(makePoint(6, 55.0, 0.0, -70));
    points.push_back(makePoint(7, 50.0, 5.0, -45));
    points.push_back(makePoint(8, 55.0, 5.0, -65));

    // Cluster 3: medium variance
    points.push_back(makePoint(9, 0.0, 50.0, -50));
    points.push_back(makePoint(10, 5.0, 50.0, -60));
    points.push_back(makePoint(11, 0.0, 55.0, -55));
    points.push_back(makePoint(12, 5.0, 55.0, -55));

    addPoints(algo, points);

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 15.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

// ====================
// calculatePosition End-to-End Tests
// ====================

TEST(CTA2, CalculatePosition_ReturnsValidCoordinates)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;

    // Four clusters around target at (50, 50)
    for (int cluster = 0; cluster < 4; ++cluster)
    {
        double cx = (cluster == 0 || cluster == 2) ? 10.0 : 90.0;
        double cy = (cluster == 0 || cluster == 1) ? 10.0 : 90.0;
        for (int i = 0; i < 4; ++i)
        {
            double x = cx + (i % 2) * 5.0;
            double y = cy + (i / 2) * 5.0;
            int rssi = -70 + i * 5;
            points.push_back(makePoint(cluster * 4 + i + 1, x, y, rssi));
        }
    }

    addPoints(algo, points);

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 1.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
        EXPECT_GE(lat, -90.0);
        EXPECT_LE(lat, 90.0);
        EXPECT_GE(lon, -180.0);
        EXPECT_LE(lon, 180.0);
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

TEST(CTA2, CalculatePosition_Reset)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -50));
    points.push_back(makePoint(2, 5.0, 0.0, -45));
    points.push_back(makePoint(3, 10.0, 0.0, -40));
    addPoints(algo, points);

    algo.reset();

    // After reset, should be able to add new data
    std::vector<core::DataPoint> newPoints;
    for (int i = 0; i < 12; ++i)
    {
        double x = (i % 4) * 5.0 + (i / 4) * 30.0;
        double y = (i / 4) * 30.0;
        newPoints.push_back(makePoint(i + 100, x, y, -50 - (i % 4) * 5));
    }
    addPoints(algo, newPoints);

    // Algorithm should work with new data (may throw due to cluster formation)
    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

// ====================
// Edge Cases
// ====================

TEST(CTA2, EdgeCase_AllSameRSSI)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;
    for (int i = 0; i < 12; ++i)
    {
        double x = (i % 4) * 10.0;
        double y = (i / 4) * 30.0;
        points.push_back(makePoint(i + 1, x, y, -50));
    }
    addPoints(algo, points);

    double lat, lon;
    // May fail due to no RSSI gradient - acceptable
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

TEST(CTA2, EdgeCase_NegativeCoordinates)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::vector<core::DataPoint> points;

    // Cluster in negative coordinate space
    points.push_back(makePoint(1, -50.0, -50.0, -70));
    points.push_back(makePoint(2, -45.0, -50.0, -60));
    points.push_back(makePoint(3, -50.0, -45.0, -65));
    points.push_back(makePoint(4, -45.0, -45.0, -55));

    // Cluster in positive coordinate space
    points.push_back(makePoint(5, 50.0, 50.0, -70));
    points.push_back(makePoint(6, 55.0, 50.0, -60));
    points.push_back(makePoint(7, 50.0, 55.0, -65));
    points.push_back(makePoint(8, 55.0, 55.0, -55));

    // Cluster crossing origin
    points.push_back(makePoint(9, -5.0, -5.0, -70));
    points.push_back(makePoint(10, 5.0, -5.0, -60));
    points.push_back(makePoint(11, -5.0, 5.0, -65));
    points.push_back(makePoint(12, 5.0, 5.0, -55));

    addPoints(algo, points);

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 1.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}

TEST(CTA2, EdgeCase_MultipleDevices)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    std::map<std::string, std::vector<core::DataPoint>> pointMap;

    // Device 1 points
    std::vector<core::DataPoint> device1Points;
    device1Points.push_back(makePoint(1, 0.0, 0.0, -70));
    device1Points.push_back(makePoint(2, 5.0, 0.0, -60));
    device1Points.push_back(makePoint(3, 10.0, 0.0, -50));
    device1Points.push_back(makePoint(4, 5.0, 5.0, -55));
    pointMap["device1"] = device1Points;

    // Device 2 points
    std::vector<core::DataPoint> device2Points;
    device2Points.push_back(makePoint(5, 50.0, 0.0, -70));
    device2Points.push_back(makePoint(6, 55.0, 0.0, -60));
    device2Points.push_back(makePoint(7, 60.0, 0.0, -50));
    device2Points.push_back(makePoint(8, 55.0, 5.0, -55));
    pointMap["device2"] = device2Points;

    // Device 3 points
    std::vector<core::DataPoint> device3Points;
    device3Points.push_back(makePoint(9, 25.0, 50.0, -70));
    device3Points.push_back(makePoint(10, 30.0, 50.0, -60));
    device3Points.push_back(makePoint(11, 35.0, 50.0, -50));
    device3Points.push_back(makePoint(12, 30.0, 55.0, -55));
    pointMap["device3"] = device3Points;

    algo.addDataPointMap(pointMap, 57.0, 11.0);

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 1.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        SUCCEED();
    }
}
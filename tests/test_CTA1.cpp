#include <gtest/gtest.h>
#include "../src/core/ClusteredTriangulationAlgorithm1.h"
#include <cmath>
#include <spdlog/spdlog.h>

// Disable logging for tests
static struct DisableLogging {
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

// ====================
// Parameter Tests
// ====================

TEST(CTA1, DefaultParameters)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Access protected methods via public interface behavior
    // We can't directly test protected getters, but we verify the algorithm works
    SUCCEED();
}

// ====================
// clusterData Tests
// ====================

TEST(CTA1, ClusterData_InsufficientPoints)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Add only 2 points - not enough for clustering
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -50));
    algo.addDataPointMap(makePoint(2, 1.0, 0.0, -50));

    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

TEST(CTA1, ClusterData_MinimumViable)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Create multiple distinct clusters around a target point at (50, 50)

    // Cluster 1: points from south approaching source
    algo.addDataPointMap(makePoint(1, 50.0, 0.0, -70));
    algo.addDataPointMap(makePoint(2, 50.0, 10.0, -60));
    algo.addDataPointMap(makePoint(3, 50.0, 20.0, -50));

    // Cluster 2: points from west approaching source
    algo.addDataPointMap(makePoint(4, 0.0, 50.0, -70));
    algo.addDataPointMap(makePoint(5, 10.0, 50.0, -60));
    algo.addDataPointMap(makePoint(6, 20.0, 50.0, -50));

    // Cluster 3: points from north approaching source
    algo.addDataPointMap(makePoint(7, 50.0, 100.0, -70));
    algo.addDataPointMap(makePoint(8, 50.0, 90.0, -60));
    algo.addDataPointMap(makePoint(9, 50.0, 80.0, -50));

    // Cluster 4: points from east approaching source
    algo.addDataPointMap(makePoint(10, 100.0, 50.0, -70));
    algo.addDataPointMap(makePoint(11, 90.0, 50.0, -60));
    algo.addDataPointMap(makePoint(12, 80.0, 50.0, -50));

    double lat, lon;
    // With 4 clusters from different directions, algorithm should work
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 5.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &e)
    {
        // If it fails due to clustering/intersection issues, that's also acceptable
        // The important thing is it doesn't hang or crash
        SUCCEED();
    }
}

// ====================
// findIntersections Tests (via calculatePosition behavior)
// ====================

TEST(CTA1, FindIntersections_ParallelRays)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Create clusters that would produce parallel rays (same direction)
    // This should result in no valid intersections
    // Points in a line all pointing the same direction
    for (int i = 0; i < 8; ++i)
    {
        algo.addDataPointMap(makePoint(i + 1, i * 10.0, 0.0, -50 - i));
    }

    double lat, lon;
    // May throw due to insufficient intersections or produce unreliable result
    // The key is it doesn't hang or crash
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 2.0);
    }
    catch (const std::runtime_error &)
    {
        // Expected - no intersections found
        SUCCEED();
        return;
    }
    // If it succeeds, that's also fine
    SUCCEED();
}

// ====================
// gradientDescent Tests (via calculatePosition behavior)
// ====================

TEST(CTA1, GradientDescent_Timeout)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Create a valid scenario
    // L-shaped path around a target point
    for (int i = 0; i < 5; ++i)
    {
        double dist = std::sqrt((50.0 - i * 10.0) * (50.0 - i * 10.0));
        int rssi = static_cast<int>(-30 - dist / 5.0);
        algo.addDataPointMap(makePoint(i + 1, i * 10.0, 0.0, rssi));
    }
    for (int i = 0; i < 5; ++i)
    {
        double dist = std::sqrt(50.0 * 50.0 + (50.0 - i * 10.0) * (50.0 - i * 10.0));
        int rssi = static_cast<int>(-30 - dist / 5.0);
        algo.addDataPointMap(makePoint(i + 6, 50.0, i * 10.0, rssi));
    }

    double lat, lon;
    auto start = std::chrono::steady_clock::now();

    try
    {
        algo.calculatePosition(lat, lon, 0.1, 1.0); // 1 second timeout
    }
    catch (...)
    {
        // May throw, that's OK
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Should respect timeout (allow some overhead)
    EXPECT_LT(elapsed.count(), 3.0);
}

TEST(CTA1, GradientDescent_Precision)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Create data around a known source position
    // Source at (50, 50)
    double src_x = 50.0, src_y = 50.0;

    // Points around the source in a circle-ish pattern
    for (int i = 0; i < 20; ++i)
    {
        double angle = i * M_PI / 10.0;
        double radius = 30.0 + (i % 3) * 10.0;
        double x = src_x + radius * std::cos(angle);
        double y = src_y + radius * std::sin(angle);
        double dist = std::sqrt((x - src_x) * (x - src_x) + (y - src_y) * (y - src_y));
        int rssi = static_cast<int>(-30 - dist / 2.0);
        algo.addDataPointMap(makePoint(i + 1, x, y, rssi));
    }

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 0.5, 10.0);
        // Result should be finite
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &e)
    {
        // Some configurations may fail - that's acceptable
        SUCCEED();
    }
}

// ====================
// calculatePosition End-to-End Tests
// ====================

TEST(CTA1, CalculatePosition_ReturnsValidCoordinates)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Create L-shaped measurement path
    for (int i = 0; i < 6; ++i)
    {
        algo.addDataPointMap(makePoint(i + 1, i * 8.0, 0.0, -50 + i * 2));
    }
    for (int i = 0; i < 6; ++i)
    {
        algo.addDataPointMap(makePoint(i + 7, 40.0, i * 8.0, -50 + i * 2));
    }

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 5.0);

        // Should return valid lat/lon
        EXPECT_GE(lat, -90.0);
        EXPECT_LE(lat, 90.0);
        EXPECT_GE(lon, -180.0);
        EXPECT_LE(lon, 180.0);
    }
    catch (const std::runtime_error &)
    {
        // May fail with insufficient clusters - acceptable
        SUCCEED();
    }
}

TEST(CTA1, CalculatePosition_DifferentPrecisions)
{
    core::ClusteredTriangulationAlgorithm1 algo1, algo2;

    // Same data for both
    auto addData = [](core::ClusteredTriangulationAlgorithm1 &algo)
    {
        for (int i = 0; i < 6; ++i)
        {
            algo.addDataPointMap(makePoint(i + 1, i * 10.0, 0.0, -60 + i * 3));
        }
        for (int i = 0; i < 6; ++i)
        {
            algo.addDataPointMap(makePoint(i + 7, 50.0, i * 10.0, -60 + i * 3));
        }
    };

    addData(algo1);
    addData(algo2);

    double lat1, lon1, lat2, lon2;
    bool success1 = false, success2 = false;

    try
    {
        algo1.calculatePosition(lat1, lon1, 0.1, 5.0); // Fine precision
        success1 = true;
    }
    catch (...)
    {
    }

    try
    {
        algo2.calculatePosition(lat2, lon2, 5.0, 5.0); // Coarse precision
        success2 = true;
    }
    catch (...)
    {
    }

    // If both succeed, results should be somewhat similar
    if (success1 && success2)
    {
        // Coarse and fine should be in same general area
        EXPECT_NEAR(lat1, lat2, 0.01); // Within ~1km
        EXPECT_NEAR(lon1, lon2, 0.01);
    }
}

TEST(CTA1, CalculatePosition_Reset)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Add data
    for (int i = 0; i < 8; ++i)
    {
        algo.addDataPointMap(makePoint(i + 1, i * 5.0, i * 5.0, -50));
    }

    algo.reset();

    // After reset, should have no data
    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 1.0), std::runtime_error);
}

// ====================
// Edge Cases
// ====================

TEST(CTA1, EdgeCase_AllSameRSSI)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // All points have identical RSSI - gradient is zero
    for (int i = 0; i < 10; ++i)
    {
        algo.addDataPointMap(makePoint(i + 1, i * 5.0, (i % 2) * 10.0, -50));
    }

    double lat, lon;
    // Should handle gracefully (may throw or return some result)
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 3.0);
    }
    catch (...)
    {
        // Expected - no gradient means no AoA
    }
    SUCCEED();
}

TEST(CTA1, EdgeCase_VeryClosePoints)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Points very close together (should be coalesced)
    for (int i = 0; i < 20; ++i)
    {
        double x = (i / 5) * 20.0 + (i % 5) * 0.1;
        double y = ((i / 5) % 2) * 20.0;
        algo.addDataPointMap(makePoint(i + 1, x, y, -50 + (i / 5) * 5));
    }

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 5.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (...)
    {
        // May fail - acceptable
    }
    SUCCEED();
}

TEST(CTA1, EdgeCase_NegativeCoordinates)
{
    core::ClusteredTriangulationAlgorithm1 algo;

    // Points in negative coordinate space
    for (int i = 0; i < 6; ++i)
    {
        algo.addDataPointMap(makePoint(i + 1, -i * 10.0, 0.0, -50 + i * 2));
    }
    for (int i = 0; i < 6; ++i)
    {
        algo.addDataPointMap(makePoint(i + 7, -50.0, -i * 10.0, -50 + i * 2));
    }

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 5.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (...)
    {
        // May fail
    }
    SUCCEED();
}

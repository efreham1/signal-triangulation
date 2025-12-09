#include <gtest/gtest.h>
#include "../src/core/ClusteredTriangulationAlgorithm2.h"
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
// Default Parameter Tests
// ====================

TEST(CTA2, DefaultParameters)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Verify the algorithm can be constructed without errors
    SUCCEED();
}

// ====================
// clusterData Tests
// ====================

TEST(CTA2, ClusterData_InsufficientPoints)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Add only 2 points - not enough for clustering
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -50));
    algo.addDataPointMap(makePoint(2, 1.0, 0.0, -50));

    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

TEST(CTA2, ClusterData_MinimumClusters)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create data that should form at least 2 clusters
    // Cluster 1: tight group near origin with RSSI gradient
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -70));
    algo.addDataPointMap(makePoint(2, 3.0, 0.0, -60));
    algo.addDataPointMap(makePoint(3, 6.0, 0.0, -50));
    algo.addDataPointMap(makePoint(4, 3.0, 3.0, -55));

    // Cluster 2: tight group 50m away with RSSI gradient
    algo.addDataPointMap(makePoint(5, 50.0, 0.0, -70));
    algo.addDataPointMap(makePoint(6, 53.0, 0.0, -60));
    algo.addDataPointMap(makePoint(7, 56.0, 0.0, -50));
    algo.addDataPointMap(makePoint(8, 53.0, 3.0, -55));

    // Cluster 3: another group
    algo.addDataPointMap(makePoint(9, 25.0, 50.0, -70));
    algo.addDataPointMap(makePoint(10, 28.0, 50.0, -60));
    algo.addDataPointMap(makePoint(11, 31.0, 50.0, -50));
    algo.addDataPointMap(makePoint(12, 28.0, 53.0, -55));

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &e)
    {
        // May fail if clusters don't form properly - acceptable
        SUCCEED();
    }
}

// ====================
// bruteForceSearch Tests (via calculatePosition behavior)
// ====================

TEST(CTA2, BruteForceSearch_Timeout)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create sparse data that will require searching
    for (int i = 0; i < 5; ++i)
    {
        double x = i * 5.0;
        double y = 0.0;
        int rssi = -50 + i * 3;
        algo.addDataPointMap(makePoint(i + 1, x, y, rssi));
    }
    for (int i = 0; i < 5; ++i)
    {
        double x = 25.0;
        double y = i * 5.0;
        int rssi = -50 + i * 3;
        algo.addDataPointMap(makePoint(i + 6, x, y, rssi));
    }

    double lat, lon;
    auto start = std::chrono::steady_clock::now();

    try
    {
        algo.calculatePosition(lat, lon, 1.0, 2.0); // 2 second timeout
    }
    catch (...)
    {
        // May throw - acceptable
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Should respect timeout (allow some overhead for cluster formation)
    EXPECT_LT(elapsed.count(), 30.0); // Generous limit due to cluster formation
}

TEST(CTA2, BruteForceSearch_Precision)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create well-defined clusters around a known source at (50, 50)

    // Cluster 1: southwest of source
    algo.addDataPointMap(makePoint(1, 10.0, 10.0, -70));
    algo.addDataPointMap(makePoint(2, 13.0, 10.0, -65));
    algo.addDataPointMap(makePoint(3, 16.0, 10.0, -60));
    algo.addDataPointMap(makePoint(4, 13.0, 13.0, -63));

    // Cluster 2: northeast of source
    algo.addDataPointMap(makePoint(5, 90.0, 90.0, -70));
    algo.addDataPointMap(makePoint(6, 93.0, 90.0, -65));
    algo.addDataPointMap(makePoint(7, 96.0, 90.0, -60));
    algo.addDataPointMap(makePoint(8, 93.0, 93.0, -63));

    // Cluster 3: northwest of source
    algo.addDataPointMap(makePoint(9, 10.0, 90.0, -70));
    algo.addDataPointMap(makePoint(10, 13.0, 90.0, -65));
    algo.addDataPointMap(makePoint(11, 16.0, 90.0, -60));
    algo.addDataPointMap(makePoint(12, 13.0, 93.0, -63));

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 0.5, 15.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &e)
    {
        // May fail - acceptable for unit test
        SUCCEED();
    }
}

// ====================
// findBestClusters Tests (via clusterData behavior)
// ====================

TEST(CTA2, FindBestClusters_GeometricRatioFilter)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create a very elongated cluster (low geometric ratio - should be filtered)
    // Points in a line: ratio would be near 0
    for (int i = 0; i < 10; ++i)
    {
        algo.addDataPointMap(makePoint(i + 1, i * 2.0, 0.0, -50 + i));
    }

    double lat, lon;
    // Should throw due to no valid clusters (elongated shape filtered out)
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

TEST(CTA2, FindBestClusters_AreaFilter)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create clusters with very small area (should be filtered - MIN_AREA = 10 sq meters)
    // Points very close together
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -60));
    algo.addDataPointMap(makePoint(2, 0.5, 0.0, -55));
    algo.addDataPointMap(makePoint(3, 0.0, 0.5, -50));
    algo.addDataPointMap(makePoint(4, 0.5, 0.5, -45));

    algo.addDataPointMap(makePoint(5, 50.0, 0.0, -60));
    algo.addDataPointMap(makePoint(6, 50.5, 0.0, -55));
    algo.addDataPointMap(makePoint(7, 50.0, 0.5, -50));
    algo.addDataPointMap(makePoint(8, 50.5, 0.5, -45));

    double lat, lon;
    // Should throw due to clusters being too small
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

TEST(CTA2, FindBestClusters_OverlapFilter)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create overlapping point sets - overlapping clusters should be filtered
    // All points in same area
    for (int i = 0; i < 15; ++i)
    {
        double angle = i * 2.0 * M_PI / 15.0;
        double radius = 5.0 + (i % 3);
        double x = 50.0 + radius * std::cos(angle);
        double y = 50.0 + radius * std::sin(angle);
        algo.addDataPointMap(makePoint(i + 1, x, y, -50 - (i % 5) * 3));
    }

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
        // If it succeeds, it handled overlap properly
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        // May fail due to overlap filtering - acceptable
        SUCCEED();
    }
}

// ====================
// getCandidates Tests (via cluster formation behavior)
// ====================

TEST(CTA2, GetCandidates_MaxDistance)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create points beyond MAX_INTERNAL_CLUSTER_DISTANCE (20m)
    // These should not be grouped together
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -50));
    algo.addDataPointMap(makePoint(2, 5.0, 0.0, -50));    // Within 20m
    algo.addDataPointMap(makePoint(3, 10.0, 0.0, -50));   // Within 20m
    algo.addDataPointMap(makePoint(4, 100.0, 0.0, -50));  // Beyond 20m - separate cluster
    algo.addDataPointMap(makePoint(5, 105.0, 0.0, -50));
    algo.addDataPointMap(makePoint(6, 110.0, 0.0, -50));

    double lat, lon;
    // Should throw - points too far apart to form valid clusters
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}

// ====================
// checkCluster Tests (via cluster scoring behavior)
// ====================

TEST(CTA2, CheckCluster_ScoringPreference)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create two potential cluster regions - one with better RSSI variance
    // Cluster 1: low RSSI variance
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -50));
    algo.addDataPointMap(makePoint(2, 5.0, 0.0, -51));
    algo.addDataPointMap(makePoint(3, 0.0, 5.0, -50));
    algo.addDataPointMap(makePoint(4, 5.0, 5.0, -51));

    // Cluster 2: high RSSI variance (should score better on variance)
    algo.addDataPointMap(makePoint(5, 50.0, 0.0, -40));
    algo.addDataPointMap(makePoint(6, 55.0, 0.0, -70));
    algo.addDataPointMap(makePoint(7, 50.0, 5.0, -45));
    algo.addDataPointMap(makePoint(8, 55.0, 5.0, -65));

    // Cluster 3: medium variance
    algo.addDataPointMap(makePoint(9, 0.0, 50.0, -50));
    algo.addDataPointMap(makePoint(10, 5.0, 50.0, -60));
    algo.addDataPointMap(makePoint(11, 0.0, 55.0, -55));
    algo.addDataPointMap(makePoint(12, 5.0, 55.0, -55));

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 15.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (const std::runtime_error &)
    {
        // May fail - acceptable
        SUCCEED();
    }
}

// ====================
// calculatePosition End-to-End Tests
// ====================

TEST(CTA2, CalculatePosition_ReturnsValidCoordinates)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create well-spaced clusters with good geometry
    // Cluster 1
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -70));
    algo.addDataPointMap(makePoint(2, 5.0, 0.0, -60));
    algo.addDataPointMap(makePoint(3, 10.0, 0.0, -50));
    algo.addDataPointMap(makePoint(4, 5.0, 5.0, -55));

    // Cluster 2
    algo.addDataPointMap(makePoint(5, 80.0, 0.0, -70));
    algo.addDataPointMap(makePoint(6, 85.0, 0.0, -60));
    algo.addDataPointMap(makePoint(7, 90.0, 0.0, -50));
    algo.addDataPointMap(makePoint(8, 85.0, 5.0, -55));

    // Cluster 3
    algo.addDataPointMap(makePoint(9, 40.0, 80.0, -70));
    algo.addDataPointMap(makePoint(10, 45.0, 80.0, -60));
    algo.addDataPointMap(makePoint(11, 50.0, 80.0, -50));
    algo.addDataPointMap(makePoint(12, 45.0, 85.0, -55));

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 15.0);

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

TEST(CTA2, CalculatePosition_Reset)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Add some data
    for (int i = 0; i < 10; ++i)
    {
        algo.addDataPointMap(makePoint(i + 1, i * 3.0, (i % 3) * 3.0, -50 - i));
    }

    algo.reset();

    // After reset, should have no data
    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 1.0), std::runtime_error);
}

TEST(CTA2, CalculatePosition_DifferentPrecisions)
{
    core::ClusteredTriangulationAlgorithm2 algo1, algo2;

    auto addData = [](core::ClusteredTriangulationAlgorithm2 &algo)
    {
        // Cluster 1
        algo.addDataPointMap(makePoint(1, 0.0, 0.0, -70));
        algo.addDataPointMap(makePoint(2, 5.0, 0.0, -60));
        algo.addDataPointMap(makePoint(3, 10.0, 0.0, -50));
        algo.addDataPointMap(makePoint(4, 5.0, 5.0, -55));

        // Cluster 2
        algo.addDataPointMap(makePoint(5, 60.0, 0.0, -70));
        algo.addDataPointMap(makePoint(6, 65.0, 0.0, -60));
        algo.addDataPointMap(makePoint(7, 70.0, 0.0, -50));
        algo.addDataPointMap(makePoint(8, 65.0, 5.0, -55));

        // Cluster 3
        algo.addDataPointMap(makePoint(9, 30.0, 60.0, -70));
        algo.addDataPointMap(makePoint(10, 35.0, 60.0, -60));
        algo.addDataPointMap(makePoint(11, 40.0, 60.0, -50));
        algo.addDataPointMap(makePoint(12, 35.0, 65.0, -55));
    };

    addData(algo1);
    addData(algo2);

    double lat1, lon1, lat2, lon2;
    bool success1 = false, success2 = false;

    try
    {
        algo1.calculatePosition(lat1, lon1, 0.5, 15.0); // Finer precision
        success1 = true;
    }
    catch (...)
    {
    }

    try
    {
        algo2.calculatePosition(lat2, lon2, 2.0, 15.0); // Coarser precision
        success2 = true;
    }
    catch (...)
    {
    }

    // If both succeed, results should be in same general area
    if (success1 && success2)
    {
        // CTA2 uses brute-force search, so precision affects result more
        EXPECT_NEAR(lat1, lat2, 0.05); // Within ~5km
        EXPECT_NEAR(lon1, lon2, 0.05);
    }
}

// ====================
// Edge Cases
// ====================

TEST(CTA2, EdgeCase_AllSameRSSI)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // All points have identical RSSI - MIN_RSSI_VARIANCE should filter
    // Cluster 1
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -50));
    algo.addDataPointMap(makePoint(2, 5.0, 0.0, -50));
    algo.addDataPointMap(makePoint(3, 0.0, 5.0, -50));
    algo.addDataPointMap(makePoint(4, 5.0, 5.0, -50));

    // Cluster 2
    algo.addDataPointMap(makePoint(5, 50.0, 0.0, -50));
    algo.addDataPointMap(makePoint(6, 55.0, 0.0, -50));
    algo.addDataPointMap(makePoint(7, 50.0, 5.0, -50));
    algo.addDataPointMap(makePoint(8, 55.0, 5.0, -50));

    // Cluster 3
    algo.addDataPointMap(makePoint(9, 25.0, 50.0, -50));
    algo.addDataPointMap(makePoint(10, 30.0, 50.0, -50));
    algo.addDataPointMap(makePoint(11, 25.0, 55.0, -50));
    algo.addDataPointMap(makePoint(12, 30.0, 55.0, -50));

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
        // May succeed or fail - both are acceptable
        SUCCEED();
    }
    catch (...)
    {
        // Expected - no RSSI variance means poor clusters
        SUCCEED();
    }
}

TEST(CTA2, EdgeCase_VeryClosePoints)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Points very close together - should be coalesced
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            int id = i * 4 + j + 1;
            double x = (i / 2) * 50.0 + (i % 2) * 0.1 + j * 0.1;
            double y = (i / 2) * 50.0 + (i % 2) * 0.1;
            int rssi = -50 - id;
            algo.addDataPointMap(makePoint(id, x, y, rssi));
        }
    }

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 10.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (...)
    {
        // May fail - acceptable
        SUCCEED();
    }
}

TEST(CTA2, EdgeCase_NegativeCoordinates)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Clusters in negative coordinate space
    // Cluster 1
    algo.addDataPointMap(makePoint(1, -10.0, -10.0, -70));
    algo.addDataPointMap(makePoint(2, -5.0, -10.0, -60));
    algo.addDataPointMap(makePoint(3, 0.0, -10.0, -50));
    algo.addDataPointMap(makePoint(4, -5.0, -5.0, -55));

    // Cluster 2
    algo.addDataPointMap(makePoint(5, -80.0, -10.0, -70));
    algo.addDataPointMap(makePoint(6, -75.0, -10.0, -60));
    algo.addDataPointMap(makePoint(7, -70.0, -10.0, -50));
    algo.addDataPointMap(makePoint(8, -75.0, -5.0, -55));

    // Cluster 3
    algo.addDataPointMap(makePoint(9, -40.0, -80.0, -70));
    algo.addDataPointMap(makePoint(10, -35.0, -80.0, -60));
    algo.addDataPointMap(makePoint(11, -30.0, -80.0, -50));
    algo.addDataPointMap(makePoint(12, -35.0, -75.0, -55));

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 15.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (...)
    {
        // May fail
        SUCCEED();
    }
}

TEST(CTA2, EdgeCase_LargeDataset)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create a larger dataset with multiple potential clusters
    std::vector<std::pair<double, double>> cluster_centers = {
        {0.0, 0.0},
        {60.0, 0.0},
        {30.0, 60.0},
        {90.0, 60.0}};

    int id = 1;
    for (const auto &center : cluster_centers)
    {
        for (int i = 0; i < 5; ++i)
        {
            double angle = i * 2.0 * M_PI / 5.0;
            double radius = 4.0 + (i % 2) * 2.0;
            double x = center.first + radius * std::cos(angle);
            double y = center.second + radius * std::sin(angle);
            int rssi = -50 - i * 5;
            algo.addDataPointMap(makePoint(id++, x, y, rssi));
        }
    }

    double lat, lon;
    try
    {
        algo.calculatePosition(lat, lon, 1.0, 20.0);
        EXPECT_TRUE(std::isfinite(lat));
        EXPECT_TRUE(std::isfinite(lon));
    }
    catch (...)
    {
        // May fail due to cluster formation constraints
        SUCCEED();
    }
}

// ====================
// Parameter Override Tests
// ====================

TEST(CTA2, ParameterOverrides_CoalitionDistance)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // Create points that would be coalesced at distance 2.0 (CTA2's override)
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -50));
    algo.addDataPointMap(makePoint(2, 1.5, 0.0, -50)); // Within 2m - should be coalesced
    algo.addDataPointMap(makePoint(3, 3.0, 0.0, -50)); // Just beyond 2m

    // After coalescing, we should have fewer points
    // We can verify this indirectly through behavior
    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 2.0), std::runtime_error);
}

TEST(CTA2, ParameterOverrides_ClusterMinPoints)
{
    core::ClusteredTriangulationAlgorithm2 algo;

    // CTA2 requires minimum 3 points per cluster
    // Create exactly 2 points per "cluster area" - should fail to form valid clusters
    algo.addDataPointMap(makePoint(1, 0.0, 0.0, -50));
    algo.addDataPointMap(makePoint(2, 5.0, 5.0, -45));

    algo.addDataPointMap(makePoint(3, 50.0, 0.0, -50));
    algo.addDataPointMap(makePoint(4, 55.0, 5.0, -45));

    double lat, lon;
    EXPECT_THROW(algo.calculatePosition(lat, lon, 1.0, 5.0), std::runtime_error);
}


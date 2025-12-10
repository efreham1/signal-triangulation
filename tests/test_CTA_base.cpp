#include "../src/core/ClusteredTriangulationBase.h"
#include "../src/core/Cluster.h"
#include "../src/core/PointDistanceCache.hpp"

#include <vector>
#include <random>
#include <cmath>
#include <iostream>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

// Disable logging for tests
static struct DisableLogging
{
    DisableLogging() { spdlog::set_level(spdlog::level::off); }
} _disableLogging;

// ====================
// Test Fixture: Concrete implementation for testing base class
// ====================

class TestableTriangulationBase : public core::ClusteredTriangulationBase
{
public:
    // Expose protected members for testing
    using ClusteredTriangulationBase::coalescePoints;
    using ClusteredTriangulationBase::estimateAoAForClusters;
    using ClusteredTriangulationBase::getCost;
    using ClusteredTriangulationBase::m_clusters;
    using ClusteredTriangulationBase::m_point_map;
    using ClusteredTriangulationBase::m_total_points;
    using ClusteredTriangulationBase::reorderDataPointsByDistance;

    void calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout) override
    {
        (void)precision;
        (void)timeout;
        out_latitude = 0.0;
        out_longitude = 0.0;
    }

    void clusterData()
    {
        // Simple clustering for testing: put all points from first device in one cluster
        for (auto &pair : m_point_map)
        {
            auto &points = pair.second;
            if (points.size() >= 3)
            {
                core::PointCluster cluster;
                for (const auto &p : points)
                {
                    cluster.addPoint(p);
                }
                m_clusters.push_back(cluster);
            }
        }
    }

    // Allow setting clusters directly for testing
    void setClusters(const std::vector<core::PointCluster> &clusters)
    {
        m_clusters = clusters;
    }

    // Helper to get all points flattened (for tests that need simple access)
    std::vector<core::DataPoint> getAllPoints() const
    {
        std::vector<core::DataPoint> all;
        for (const auto &pair : m_point_map)
        {
            all.insert(all.end(), pair.second.begin(), pair.second.end());
        }
        return all;
    }

    // Helper to get points for the default device
    std::vector<core::DataPoint> &getDefaultDevicePoints()
    {
        return m_point_map["default"];
    }
};

// Helper to create a DataPoint
static core::DataPoint makePoint(int id, double x, double y, double rssi, double lat = 57.0, double lon = 12.0)
{
    core::DataPoint dp;
    dp.point_id = id;
    dp.zero_latitude = lat;
    dp.zero_longitude = lon;
    dp.setX(x);
    dp.setY(y);
    dp.rssi = static_cast<int>(rssi);
    dp.timestamp_ms = id * 1000; // Use id as timestamp for ordering
    dp.computeCoordinates();
    return dp;
}

// Helper to create a single-point map for the new interface
static std::map<std::string, std::vector<core::DataPoint>> makePointMap(const core::DataPoint &p, const std::string &device = "default")
{
    std::map<std::string, std::vector<core::DataPoint>> m;
    m[device].push_back(p);
    return m;
}

// Helper to create a multi-point map for the new interface
static std::map<std::string, std::vector<core::DataPoint>> makePointMap(const std::vector<core::DataPoint> &points, const std::string &device = "default")
{
    std::map<std::string, std::vector<core::DataPoint>> m;
    m[device] = points;
    return m;
}

// ====================
// fitPlaneNormal Tests
// ====================

TEST(PlaneFit, NormalVectorAccuracy)
{
    const double a = 0.5;
    const double b = -0.25;
    const double c = 1.234;
    const int N = 100;
    const double tolerance = 1e-3;

    std::mt19937_64 rng(123456);
    std::uniform_real_distribution<double> unif(-10.0, 10.0);
    std::normal_distribution<double> noise(0.0, 0.01);

    std::vector<double> X(N), Y(N), Z(N);
    for (int i = 0; i < N; ++i)
    {
        double x = unif(rng);
        double y = unif(rng);
        double z = a * x + b * y + c + noise(rng);
        X[i] = x;
        Y[i] = y;
        Z[i] = z;
    }

    auto normal = core::fitPlaneNormal(X, Y, Z, 3);
    ASSERT_EQ(normal.size(), 3u) << "Normal vector size unexpected";

    std::vector<double> expected = {a, b, -1.0};

    auto normalize = [](const std::vector<double> &v)
    {
        double s = 0.0;
        for (double x : v)
            s += x * x;
        s = std::sqrt(s);
        std::vector<double> r(v.size());
        if (s > 0.0)
            for (size_t i = 0; i < v.size(); ++i)
                r[i] = v[i] / s;
        return r;
    };

    auto n_comp = normalize(normal);
    auto n_exp = normalize(expected);

    double dot = n_comp[0] * n_exp[0] + n_comp[1] * n_exp[1] + n_comp[2] * n_exp[2];
    double adot = std::abs(dot);

    EXPECT_GE(adot, 1.0 - tolerance) << "Normal vector mismatch (abs dot=" << adot << ")";
}

TEST(PlaneFit, MinimumPoints)
{
    std::vector<double> X = {0.0, 1.0, 0.0};
    std::vector<double> Y = {0.0, 0.0, 1.0};
    std::vector<double> Z = {0.0, 1.0, 2.0}; // z = x + 2y

    auto normal = core::fitPlaneNormal(X, Y, Z, 3);
    ASSERT_EQ(normal.size(), 3u);

    std::vector<double> expected = {1.0, 2.0, -1.0};

    auto normalize = [](const std::vector<double> &v)
    {
        double s = 0.0;
        for (double x : v)
            s += x * x;
        s = std::sqrt(s);
        std::vector<double> r(v.size());
        if (s > 0.0)
            for (size_t i = 0; i < v.size(); ++i)
                r[i] = v[i] / s;
        return r;
    };

    auto n_comp = normalize(normal);
    auto n_exp = normalize(expected);

    double dot = n_comp[0] * n_exp[0] + n_comp[1] * n_exp[1] + n_comp[2] * n_exp[2];
    EXPECT_GE(std::abs(dot), 0.99) << "Normal vector mismatch for minimum points";
}

TEST(PlaneFit, HorizontalPlane)
{
    std::vector<double> X = {0.0, 1.0, 2.0, 0.0, 1.0};
    std::vector<double> Y = {0.0, 0.0, 0.0, 1.0, 1.0};
    std::vector<double> Z = {5.0, 5.0, 5.0, 5.0, 5.0};

    auto normal = core::fitPlaneNormal(X, Y, Z, 3);
    ASSERT_EQ(normal.size(), 3u);

    EXPECT_NEAR(normal[0], 0.0, 0.01) << "X component should be ~0 for horizontal plane";
    EXPECT_NEAR(normal[1], 0.0, 0.01) << "Y component should be ~0 for horizontal plane";
    EXPECT_NE(normal[2], 0.0) << "Z component should be non-zero for horizontal plane";
}

TEST(PlaneFit, InsufficientPoints)
{
    std::vector<double> X = {0.0, 1.0};
    std::vector<double> Y = {0.0, 1.0};
    std::vector<double> Z = {0.0, 1.0};

    auto normal = core::fitPlaneNormal(X, Y, Z, 3);
    ASSERT_EQ(normal.size(), 3u);
    EXPECT_DOUBLE_EQ(normal[0], 0.0);
    EXPECT_DOUBLE_EQ(normal[1], 0.0);
    EXPECT_DOUBLE_EQ(normal[2], 0.0);
}

TEST(PlaneFit, MismatchedVectorSizes)
{
    std::vector<double> X = {0.0, 1.0, 2.0};
    std::vector<double> Y = {0.0, 1.0};
    std::vector<double> Z = {0.0, 1.0, 2.0};

    auto normal = core::fitPlaneNormal(X, Y, Z, 3);
    EXPECT_DOUBLE_EQ(normal[0], 0.0);
    EXPECT_DOUBLE_EQ(normal[1], 0.0);
    EXPECT_DOUBLE_EQ(normal[2], 0.0);
}

// ====================
// addDataPointMap Tests
// ====================

TEST(CTABase, ProcessDataPoint_SinglePoint)
{
    TestableTriangulationBase algo;
    auto p = makePoint(1, 10.0, 20.0, -50);

    algo.addDataPointMap(makePointMap(p), 57.0, 12.0);

    auto points = algo.getAllPoints();
    ASSERT_EQ(points.size(), 1u);
    EXPECT_DOUBLE_EQ(points[0].getX(), 10.0);
    EXPECT_DOUBLE_EQ(points[0].getY(), 20.0);
}

TEST(CTABase, ProcessDataPoint_MultipleDevices)
{
    TestableTriangulationBase algo;

    std::map<std::string, std::vector<core::DataPoint>> pointMap;
    pointMap["device1"].push_back(makePoint(1, 10.0, 10.0, -50));
    pointMap["device1"].push_back(makePoint(2, 20.0, 20.0, -60));
    pointMap["device2"].push_back(makePoint(3, 30.0, 30.0, -70));

    algo.addDataPointMap(pointMap, 57.0, 12.0);

    EXPECT_EQ(algo.m_point_map.size(), 2u);
    EXPECT_EQ(algo.m_point_map["device1"].size(), 2u);
    EXPECT_EQ(algo.m_point_map["device2"].size(), 1u);
    EXPECT_EQ(algo.m_total_points, 3u);
}

TEST(CTABase, ProcessDataPoint_OrderPreserved)
{
    TestableTriangulationBase algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 10.0, 10.0, -50));
    points.push_back(makePoint(2, 20.0, 20.0, -50));
    points.push_back(makePoint(3, 30.0, 30.0, -50));

    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    auto &devicePoints = algo.m_point_map["default"];
    ASSERT_EQ(devicePoints.size(), 3u);
    EXPECT_EQ(devicePoints[0].point_id, 1);
    EXPECT_EQ(devicePoints[1].point_id, 2);
    EXPECT_EQ(devicePoints[2].point_id, 3);
}

// ====================
// reset Tests
// ====================

TEST(CTABase, Reset_ClearsAll)
{
    TestableTriangulationBase algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 10.0, 20.0, -50));
    points.push_back(makePoint(2, 20.0, 30.0, -60));
    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    algo.reset();

    EXPECT_TRUE(algo.m_point_map.empty());
    EXPECT_TRUE(algo.m_clusters.empty());
}


// ====================
// reorderDataPointsByDistance Tests
// ====================

TEST(CTABase, ReorderByDistance_TooFewPoints)
{
    TestableTriangulationBase algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -50));
    points.push_back(makePoint(2, 10.0, 0.0, -50));
    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    // Should not throw with < 3 points
    algo.reorderDataPointsByDistance(points);

    EXPECT_EQ(algo.m_point_map["default"].size(), 2u);
}

TEST(CTABase, ReorderByDistance_OptimizesPath)
{
    TestableTriangulationBase algo;

    // Add points in a suboptimal order
    // Optimal path: 1 -> 2 -> 3 -> 4 (in a line)
    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -50));
    points.push_back(makePoint(3, 20.0, 0.0, -50));
    points.push_back(makePoint(2, 10.0, 0.0, -50));
    points.push_back(makePoint(4, 30.0, 0.0, -50));
    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    auto &devicePoints = algo.m_point_map["default"];
    algo.reorderDataPointsByDistance(devicePoints);


    // After optimization, consecutive points should be close
    double total_dist = 0.0;
    for (size_t i = 0; i < devicePoints.size() - 1; ++i)
    {
        total_dist += core::PointDistanceCache::getInstance().getDistance(devicePoints[i], devicePoints[i + 1]);
    }

    // Optimal path length is 30m (0->10->20->30)
    EXPECT_LE(total_dist, 35.0); // Allow some tolerance
}

// ====================
// coalescePoints Tests
// ====================

TEST(CTABase, CoalescePoints_MergesClosePoints)
{
    TestableTriangulationBase algo;

    // Two points within coalition distance
    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -40));
    points.push_back(makePoint(2, 0.5, 0.0, -60)); // 0.5m apart
    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    algo.coalescePoints(1.0, algo.m_point_map["default"]); // 1m threshold

    ASSERT_EQ(algo.m_point_map["default"].size(), 1u);
    EXPECT_NEAR(algo.m_point_map["default"][0].getX(), 0.25, 1e-9); // Midpoint
    EXPECT_EQ(algo.m_point_map["default"][0].rssi, -50);            // Average RSSI
}

TEST(CTABase, CoalescePoints_KeepsFarPoints)
{
    TestableTriangulationBase algo;

    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -40));
    points.push_back(makePoint(2, 5.0, 0.0, -60)); // 5m apart
    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    algo.coalescePoints(1.0, algo.m_point_map["default"]); // 1m threshold

    EXPECT_EQ(algo.m_point_map["default"].size(), 2u);
}

TEST(CTABase, CoalescePoints_ChainMerge)
{
    TestableTriangulationBase algo;

    // Three points in a line, each close to the next
    std::vector<core::DataPoint> points;
    points.push_back(makePoint(1, 0.0, 0.0, -40));
    points.push_back(makePoint(2, 0.5, 0.0, -50));
    points.push_back(makePoint(3, 1.0, 0.0, -60));
    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    algo.coalescePoints(0.6, algo.m_point_map["default"]); // Threshold slightly larger than spacing

    // Points 1 and 2 merge, then result may merge with 3
    EXPECT_LT(algo.m_point_map["default"].size(), 3u);
}

// ====================
// estimateAoAForClusters Tests
// ====================

TEST(CTABase, EstimateAoA_SetsGradient)
{
    TestableTriangulationBase algo;

    // Create a cluster with a clear gradient in x direction
    // z = x (RSSI increases with x)
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -60));
    cluster.addPoint(makePoint(2, 10.0, 0.0, -50));
    cluster.addPoint(makePoint(3, 20.0, 0.0, -40));
    cluster.addPoint(makePoint(4, 10.0, 5.0, -50));

    algo.setClusters({cluster});
    algo.estimateAoAForClusters(3);

    ASSERT_EQ(algo.m_clusters.size(), 1u);
    // Gradient should point in positive x direction (RSSI increases)
    EXPECT_GT(algo.m_clusters[0].aoa_x, 0.0);
}

TEST(CTABase, EstimateAoA_SkipsTooFewPoints)
{
    TestableTriangulationBase algo;

    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50));
    cluster.addPoint(makePoint(2, 10.0, 0.0, -60));
    // Only 2 points - not enough for plane fitting

    algo.setClusters({cluster});
    algo.estimateAoAForClusters(3);

    EXPECT_DOUBLE_EQ(algo.m_clusters[0].aoa_x, 0.0);
    EXPECT_DOUBLE_EQ(algo.m_clusters[0].aoa_y, 0.0);
}

// ====================
// getCost Tests
// ====================

TEST(CTABase, GetCost_ZeroForPointOnRay)
{
    TestableTriangulationBase algo;

    // Cluster at origin with AoA pointing in +x direction
    core::PointCluster cluster;
    cluster.centroid_x = 0.0;
    cluster.centroid_y = 0.0;
    cluster.aoa_x = 1.0;
    cluster.aoa_y = 0.0;
    cluster.score = 1.0;

    algo.setClusters({cluster});

    // Point along the ray (positive x)
    double cost = algo.getCost(10.0, 0.0, 0, 0);
    EXPECT_NEAR(cost, 0.0, 0.1);
}

TEST(CTABase, GetCost_HighForPointOffRay)
{
    TestableTriangulationBase algo;

    core::PointCluster cluster;
    cluster.centroid_x = 0.0;
    cluster.centroid_y = 0.0;
    cluster.aoa_x = 1.0;
    cluster.aoa_y = 0.0;
    cluster.score = 1.0;

    algo.setClusters({cluster});

    // Point perpendicular to ray
    double cost = algo.getCost(0.0, 10.0, 0, 0);
    EXPECT_GT(cost, 5.0);
}

TEST(CTABase, GetCost_PenalizesBehindCentroid)
{
    TestableTriangulationBase algo;

    core::PointCluster cluster;
    cluster.centroid_x = 0.0;
    cluster.centroid_y = 0.0;
    cluster.aoa_x = 1.0;
    cluster.aoa_y = 0.0;
    cluster.score = 1.0;

    algo.setClusters({cluster});

    // Point behind centroid (negative x)
    double cost_behind = algo.getCost(-10.0, 0.0, 0, 0);
    double cost_front = algo.getCost(10.0, 0.0, 0, 0);

    EXPECT_GT(cost_behind, cost_front);
}

TEST(CTABase, GetCost_SkipsZeroGradient)
{
    TestableTriangulationBase algo;

    core::PointCluster cluster;
    cluster.centroid_x = 0.0;
    cluster.centroid_y = 0.0;
    cluster.aoa_x = 0.0;
    cluster.aoa_y = 0.0; // Zero gradient
    cluster.score = 1.0;

    algo.setClusters({cluster});

    double cost = algo.getCost(10.0, 10.0, 0, 0);
    EXPECT_DOUBLE_EQ(cost, 0.0); // Cluster should be skipped
}

TEST(CTABase, GetCost_MultipleClusters)
{
    TestableTriangulationBase algo;

    core::PointCluster c1, c2;
    c1.centroid_x = 0.0;
    c1.centroid_y = 0.0;
    c1.aoa_x = 1.0;
    c1.aoa_y = 0.0;
    c1.score = 1.0;

    c2.centroid_x = 20.0;
    c2.centroid_y = 0.0;
    c2.aoa_x = -1.0; // Pointing back toward origin
    c2.aoa_y = 0.0;
    c2.score = 1.0;

    algo.setClusters({c1, c2});

    // Point between clusters should have low cost
    double cost_middle = algo.getCost(10.0, 0.0, 0, 0);
    // Point far from intersection should have high cost
    double cost_far = algo.getCost(10.0, 50.0, 0, 0);

    EXPECT_LT(cost_middle, cost_far);
}

// ====================
// Integration Test
// ====================

TEST(CTABase, FullPipeline)
{
    TestableTriangulationBase algo;

    // Add points forming a path
    std::vector<core::DataPoint> points;
    for (int i = 0; i < 10; ++i)
    {
        double x = i * 5.0;
        double y = std::sin(i * 0.5) * 10.0;
        double rssi = -50.0 - i; // Decreasing RSSI
        points.push_back(makePoint(i + 1, x, y, rssi));
    }
    algo.addDataPointMap(makePointMap(points), 57.0, 12.0);

    EXPECT_EQ(algo.m_total_points, 10u);

    algo.reorderDataPointsByDistance(points);
    algo.coalescePoints(2.0, algo.m_point_map["default"]);
    algo.clusterData();

    // Should have at least one cluster
    EXPECT_GE(algo.m_clusters.size(), 1u);

    algo.estimateAoAForClusters(3);

    // Cost function should work
    double cost = algo.getCost(25.0, 0.0, 0, 0);
    EXPECT_TRUE(std::isfinite(cost));
}
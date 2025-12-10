#include <gtest/gtest.h>
#include "../src/core/Cluster.h"
#include <cmath>

// Helper to create a DataPoint with x, y, rssi
static core::DataPoint makePoint(int id, double x, double y, double rssi)
{
    core::DataPoint dp;
    dp.point_id = id;
    dp.zero_latitude = 57.0;
    dp.zero_longitude = 11.0;
    dp.setX(x);
    dp.setY(y);
    dp.rssi = static_cast<int>(rssi);
    dp.timestamp_ms = id * 1000;
    return dp;
}

// ====================
// Basic Operations
// ====================

TEST(Cluster, DefaultConstruction)
{
    core::PointCluster cluster;
    EXPECT_EQ(cluster.size(), 0u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 0.0);
    EXPECT_DOUBLE_EQ(cluster.score, 0.0);
}

TEST(Cluster, AddSinglePoint)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 10.0, 20.0, -50.0));

    EXPECT_EQ(cluster.size(), 1u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, -50.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 10.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 20.0);
}

TEST(Cluster, AddMultiplePoints)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -40.0));
    cluster.addPoint(makePoint(2, 10.0, 0.0, -60.0));
    cluster.addPoint(makePoint(3, 10.0, 10.0, -50.0));

    EXPECT_EQ(cluster.size(), 3u);
    EXPECT_NEAR(cluster.avg_rssi, -50.0, 1e-9);
    EXPECT_NEAR(cluster.centroid_x, 20.0 / 3.0, 1e-9);
    EXPECT_NEAR(cluster.centroid_y, 10.0 / 3.0, 1e-9);
}

TEST(Cluster, RemovePoint)
{
    core::PointCluster cluster;
    auto p1 = makePoint(1, 0.0, 0.0, -40.0);
    auto p2 = makePoint(2, 10.0, 0.0, -60.0);

    cluster.addPoint(p1);
    cluster.addPoint(p2);
    EXPECT_EQ(cluster.size(), 2u);

    cluster.removePoint(p1);
    EXPECT_EQ(cluster.size(), 1u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, -60.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 10.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 0.0);
}

TEST(Cluster, RemoveAllPoints)
{
    core::PointCluster cluster;
    auto p1 = makePoint(1, 5.0, 5.0, -45.0);
    cluster.addPoint(p1);
    cluster.removePoint(p1);

    EXPECT_EQ(cluster.size(), 0u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 0.0);
}

TEST(Cluster, RemoveNonexistentPoint)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 10.0, 10.0, -50.0));

    auto nonexistent = makePoint(999, 0.0, 0.0, 0.0);
    cluster.removePoint(nonexistent); // Should not crash

    EXPECT_EQ(cluster.size(), 1u);
}

// ====================
// RSSI Variance
// ====================

TEST(Cluster, VarianceRSSI_SinglePoint)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));

    EXPECT_DOUBLE_EQ(cluster.varianceRSSI(), 0.0);
}

TEST(Cluster, VarianceRSSI_IdenticalValues)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 1.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 2.0, 0.0, -50.0));

    EXPECT_DOUBLE_EQ(cluster.varianceRSSI(), 0.0);
}

TEST(Cluster, VarianceRSSI_KnownValues)
{
    core::PointCluster cluster;
    // Values: -40, -50, -60. Mean = -50
    // Variance = ((10)^2 + (0)^2 + (-10)^2) / 2 = 200 / 2 = 100
    cluster.addPoint(makePoint(1, 0.0, 0.0, -40.0));
    cluster.addPoint(makePoint(2, 1.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 2.0, 0.0, -60.0));

    EXPECT_NEAR(cluster.varianceRSSI(), 100.0, 1e-9);
}

// ====================
// Overlap
// ====================

TEST(Cluster, Overlap_NoSharedPoints)
{
    core::PointCluster c1, c2;
    c1.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    c1.addPoint(makePoint(2, 1.0, 0.0, -50.0));
    c2.addPoint(makePoint(3, 2.0, 0.0, -50.0));
    c2.addPoint(makePoint(4, 3.0, 0.0, -50.0));

    EXPECT_DOUBLE_EQ(c1.overlapWith(c2), 0.0);
}

TEST(Cluster, Overlap_AllSharedPoints)
{
    core::PointCluster c1, c2;
    auto p1 = makePoint(1, 0.0, 0.0, -50.0);
    auto p2 = makePoint(2, 1.0, 0.0, -50.0);

    c1.addPoint(p1);
    c1.addPoint(p2);
    c2.addPoint(p1);
    c2.addPoint(p2);

    // 2 shared out of 4 total = 0.5
    EXPECT_DOUBLE_EQ(c1.overlapWith(c2), 0.5);
}

TEST(Cluster, Overlap_PartialShared)
{
    core::PointCluster c1, c2;
    auto shared = makePoint(1, 0.0, 0.0, -50.0);

    c1.addPoint(shared);
    c1.addPoint(makePoint(2, 1.0, 0.0, -50.0));
    c2.addPoint(shared);
    c2.addPoint(makePoint(3, 2.0, 0.0, -50.0));

    // 1 shared out of 4 total = 0.25
    EXPECT_DOUBLE_EQ(c1.overlapWith(c2), 0.25);
}

TEST(Cluster, Overlap_EmptyClusters)
{
    core::PointCluster c1, c2;
    EXPECT_DOUBLE_EQ(c1.overlapWith(c2), 0.0);
}

// ====================
// Bounding Box & Geometry
// ====================

TEST(Cluster, BoundingBox_TooFewPoints)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 1.0, 0.0, -50.0));

    auto bbox = cluster.bbox;
    EXPECT_FALSE(bbox.valid);
}

TEST(Cluster, BoundingBox_SquareCluster)
{
    core::PointCluster cluster;
    // Square: (0,0), (10,0), (10,10), (0,10)
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 10.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 10.0, 10.0, -50.0));
    cluster.addPoint(makePoint(4, 0.0, 10.0, -50.0));

    auto bbox = cluster.bbox;

    EXPECT_TRUE(bbox.valid);

    // For a square, the diagonal is the principal axis
    // Diagonal length = sqrt(200) ≈ 14.14
    // The perpendicular range depends on projection
    EXPECT_GT(bbox.range_u, 0.0);
    EXPECT_GT(bbox.range_v, 0.0);
}

TEST(Cluster, BoundingBox_ElongatedCluster)
{
    core::PointCluster cluster;
    // Elongated along x-axis
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 100.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 50.0, 1.0, -50.0));

    auto bbox = cluster.bbox;
    EXPECT_TRUE(bbox.valid);
    EXPECT_GT(bbox.range_u, bbox.range_v); // Principal axis is longer
}

TEST(Cluster, GeometricRatio_ElongatedCluster)
{
    core::PointCluster cluster;
    // Very elongated: 100 units long, 1 unit wide
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 100.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 50.0, 1.0, -50.0));

    double ratio = cluster.geometricRatio();
    EXPECT_GT(ratio, 0.0);
    EXPECT_LT(ratio, 0.1); // Should be small for elongated cluster
}

TEST(Cluster, GeometricRatio_SquareCluster)
{
    core::PointCluster cluster;
    // Square cluster should have ratio close to 1.0
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 10.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 10.0, 10.0, -50.0));
    cluster.addPoint(makePoint(4, 0.0, 10.0, -50.0));

    double ratio = cluster.geometricRatio();
    EXPECT_GT(ratio, 0.5); // Should be close to 1.0 for a square
}

TEST(Cluster, GeometricRatio_TooFewPoints)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));

    EXPECT_DOUBLE_EQ(cluster.geometricRatio(), 0.0);
}

TEST(Cluster, Area_ValidCluster)
{
    core::PointCluster cluster;
    // Roughly 10x10 area
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 10.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 10.0, 10.0, -50.0));
    cluster.addPoint(makePoint(4, 0.0, 10.0, -50.0));

    double a = cluster.area();
    EXPECT_GT(a, 0.0);
}

TEST(Cluster, Area_TooFewPoints)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));
    cluster.addPoint(makePoint(2, 1.0, 0.0, -50.0));

    EXPECT_DOUBLE_EQ(cluster.area(), 0.0);
}

// ====================
// Scoring
// ====================

TEST(Cluster, SetScore)
{
    core::PointCluster cluster;
    cluster.setScore(42.5);
    EXPECT_DOUBLE_EQ(cluster.score, 42.5);
}

TEST(Cluster, GetAndSetScore)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -40.0));
    cluster.addPoint(makePoint(2, 10.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 10.0, 10.0, -60.0));
    cluster.addPoint(makePoint(4, 0.0, 10.0, -50.0));

    double score = cluster.getAndSetScore(
        1.0,   // ideal_geometric_ratio
        100.0, // ideal_area
        50.0,  // ideal_rssi_variance
        1.0,   // gr_weight
        1.0,   // area_weight
        1.0,   // variance_weight
        -30.0, // bottom_rssi_threshold
        1.0    // rssi_weight
    );

    EXPECT_EQ(cluster.score, score);
    // Just verify it returns a finite number
    EXPECT_TRUE(std::isfinite(score));
}

TEST(Cluster, GetAndSetScore_EmptyCluster)
{
    core::PointCluster cluster;

    double score = cluster.getAndSetScore(
        1.0, 100.0, 50.0, 1.0, 1.0, 1.0, -30.0, 1.0);

    // Empty cluster should still return a valid (possibly 0) score
    EXPECT_TRUE(std::isfinite(score));
}

// ====================
// AoA Properties
// ====================

TEST(Cluster, AoAProperties)
{
    core::PointCluster cluster;

    // Initially zero
    EXPECT_DOUBLE_EQ(cluster.estimated_aoa, 0.0);
    EXPECT_DOUBLE_EQ(cluster.aoa_x, 0.0);
    EXPECT_DOUBLE_EQ(cluster.aoa_y, 0.0);

    // Can be set
    cluster.estimated_aoa = 1.57; // ~90 degrees
    cluster.aoa_x = 0.0;
    cluster.aoa_y = 1.0;

    EXPECT_DOUBLE_EQ(cluster.estimated_aoa, 1.57);
    EXPECT_DOUBLE_EQ(cluster.aoa_x, 0.0);
    EXPECT_DOUBLE_EQ(cluster.aoa_y, 1.0);
}
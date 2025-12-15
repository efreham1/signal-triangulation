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
    // Variance = ((10)^2 + (0)^2 + (-10)^2) / 2 = 200 / 3 = 200/3
    cluster.addPoint(makePoint(1, 0.0, 0.0, -40.0));
    cluster.addPoint(makePoint(2, 1.0, 0.0, -50.0));
    cluster.addPoint(makePoint(3, 2.0, 0.0, -60.0));

    EXPECT_NEAR(cluster.varianceRSSI(), 200.0/3.0, 1e-9);
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

// ====================
// Vectorized Clusters
// ====================

TEST(ClusterVectorized, AddPointVectorized_SinglePoint)
{
    core::PointCluster cluster;
    auto p = makePoint(1, 10.0, 20.0, -50.0);
    cluster.addPointVectorized(p, 0);

    EXPECT_EQ(cluster.x_dp_values.size(), 1u);
    EXPECT_DOUBLE_EQ(cluster.x_dp_values[0], 10.0);
    EXPECT_DOUBLE_EQ(cluster.y_dp_values[0], 20.0);
    EXPECT_DOUBLE_EQ(cluster.rssi_values[0], -50.0);
    EXPECT_EQ(cluster.point_idxs[0], 0);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, -50.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 10.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 20.0);
}

TEST(ClusterVectorized, AddPointVectorized_MultiplePoints)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -40.0), 0);
    cluster.addPointVectorized(makePoint(2, 10.0, 0.0, -60.0), 1);
    cluster.addPointVectorized(makePoint(3, 10.0, 10.0, -50.0), 2);

    EXPECT_EQ(cluster.x_dp_values.size(), 3u);
    EXPECT_NEAR(cluster.avg_rssi, -50.0, 1e-9);
    EXPECT_NEAR(cluster.centroid_x, 20.0 / 3.0, 1e-9);
    EXPECT_NEAR(cluster.centroid_y, 10.0 / 3.0, 1e-9);
}

TEST(ClusterVectorized, RemovePointVectorized)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -40.0), 0);
    cluster.addPointVectorized(makePoint(2, 10.0, 0.0, -60.0), 1);

    EXPECT_EQ(cluster.x_dp_values.size(), 2u);

    cluster.removePointVectorized(0);

    EXPECT_EQ(cluster.x_dp_values.size(), 1u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, -60.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 10.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 0.0);
}

TEST(ClusterVectorized, RemovePointVectorized_AllPoints)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 5.0, 5.0, -45.0), 0);
    cluster.removePointVectorized(0);

    EXPECT_EQ(cluster.x_dp_values.size(), 0u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 0.0);
}

TEST(ClusterVectorized, RemovePointVectorized_OutOfRange)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);

    EXPECT_THROW(cluster.removePointVectorized(5), std::out_of_range);
}

TEST(ClusterVectorized, RemovePointVectorized_EmptyCluster)
{
    core::PointCluster cluster;
    // Force vectorized mode by adding and removing
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.removePointVectorized(0);

    EXPECT_THROW(cluster.removePointVectorized(0), std::runtime_error);
}

TEST(ClusterVectorized, MixedMode_AddNonVectorizedToVectorized)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);

    EXPECT_THROW(cluster.addPoint(makePoint(2, 1.0, 0.0, -50.0)), std::runtime_error);
}

TEST(ClusterVectorized, MixedMode_RemoveNonVectorizedFromVectorized)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);

    EXPECT_THROW(cluster.removePoint(makePoint(1, 0.0, 0.0, -50.0)), std::runtime_error);
}

TEST(ClusterVectorized, MixedMode_RemoveVectorizedFromNonVectorized)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));

    EXPECT_THROW(cluster.removePointVectorized(0), std::runtime_error);
}

// ====================
// Vectorized Bounding Box
// ====================

TEST(ClusterVectorized, BoundingBox_TooFewPoints)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 1.0, 0.0, -50.0), 1);

    EXPECT_FALSE(cluster.bbox.valid);
}

TEST(ClusterVectorized, BoundingBox_ExactSize_HorizontalLine)
{
    core::PointCluster cluster;
    // Three points along horizontal line: (0,0), (100,0), (50,0)
    // Furthest pair: (0,0) and (100,0), distance = 100
    // All points on the line, so perpendicular range should be 0
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 100.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 50.0, 0.0, -50.0), 2);

    EXPECT_TRUE(cluster.bbox.valid);
    EXPECT_NEAR(cluster.furthest_distance, 100.0, 1e-9);
    EXPECT_NEAR(cluster.bbox.range_u, 100.0, 1e-9);
    EXPECT_NEAR(cluster.bbox.range_v, 0.0, 1e-9);
}

TEST(ClusterVectorized, BoundingBox_ExactSize_Rectangle)
{
    core::PointCluster cluster;
    // Rectangle: 20 units wide (x), 10 units tall (y)
    // Points: (0,0), (20,0), (20,10), (0,10)
    // Furthest pair: diagonal, distance = sqrt(20^2 + 10^2) = sqrt(500) ≈ 22.36
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 20.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 20.0, 10.0, -50.0), 2);
    cluster.addPointVectorized(makePoint(4, 0.0, 10.0, -50.0), 3);

    EXPECT_TRUE(cluster.bbox.valid);
    double expected_diagonal = std::sqrt(20.0 * 20.0 + 10.0 * 10.0);
    EXPECT_NEAR(cluster.furthest_distance, expected_diagonal, 1e-9);
    
    // The bounding box is computed in the principal axis coordinate system
    // For a rectangle, the principal axis is the diagonal
    // range_u should be the diagonal length, range_v should be the perpendicular spread
    EXPECT_GT(cluster.bbox.range_u, 0.0);
    EXPECT_GT(cluster.bbox.range_v, 0.0);
}

TEST(ClusterVectorized, BoundingBox_ExactSize_Square)
{
    core::PointCluster cluster;
    // Square: 10x10
    // Diagonal = sqrt(200) ≈ 14.14
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 10.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 10.0, 10.0, -50.0), 2);
    cluster.addPointVectorized(makePoint(4, 0.0, 10.0, -50.0), 3);

    EXPECT_TRUE(cluster.bbox.valid);
    double expected_diagonal = std::sqrt(200.0);
    EXPECT_NEAR(cluster.furthest_distance, expected_diagonal, 1e-9);
    
    // For a square rotated 45 degrees, range_u ≈ range_v ≈ diagonal
    // The geometric ratio should be close to 1.0
    EXPECT_NEAR(cluster.geometricRatio(), 1.0, 0.1);
}

TEST(ClusterVectorized, BoundingBox_ExactSize_ElongatedCluster)
{
    core::PointCluster cluster;
    // Very elongated: 100 units long, 2 units wide
    // Points: (0,0), (100,0), (50,1)
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 100.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 50.0, 1.0, -50.0), 2);

    EXPECT_TRUE(cluster.bbox.valid);
    EXPECT_NEAR(cluster.furthest_distance, 100.0, 1e-9);
    EXPECT_NEAR(cluster.bbox.range_u, 100.0, 1e-9);
    // The third point is 1 unit above the principal axis
    EXPECT_NEAR(cluster.bbox.range_v, 1.0, 1e-9);
    
    // Geometric ratio should be small (elongated)
    EXPECT_LT(cluster.geometricRatio(), 0.05);
}

TEST(ClusterVectorized, Area_ExactValue)
{
    core::PointCluster cluster;
    // Elongated cluster with known dimensions
    // range_u = 100, range_v = 1, area = 100
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 100.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 50.0, 1.0, -50.0), 2);

    EXPECT_NEAR(cluster.area(), 100.0, 1e-9);
}

TEST(ClusterVectorized, GeometricRatio_Elongated)
{
    core::PointCluster cluster;
    // range_u = 100, range_v = 1, ratio = 0.01
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 100.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 50.0, 1.0, -50.0), 2);

    EXPECT_NEAR(cluster.geometricRatio(), 0.01, 1e-9);
}

// ====================
// Vectorized Scoring
// ====================

TEST(ClusterVectorized, GetAndSetScore)
{
    core::PointCluster cluster;
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -40.0), 0);
    cluster.addPointVectorized(makePoint(2, 10.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 10.0, 10.0, -60.0), 2);
    cluster.addPointVectorized(makePoint(4, 0.0, 10.0, -50.0), 3);

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
    EXPECT_TRUE(std::isfinite(score));
}

TEST(ClusterVectorized, Score_MatchesNonVectorized)
{
    // Create identical clusters using both methods
    core::PointCluster vectorized;
    core::PointCluster nonVectorized;

    auto p1 = makePoint(1, 0.0, 0.0, -40.0);
    auto p2 = makePoint(2, 10.0, 0.0, -50.0);
    auto p3 = makePoint(3, 10.0, 10.0, -60.0);
    auto p4 = makePoint(4, 0.0, 10.0, -50.0);

    vectorized.addPointVectorized(p1, 0);
    vectorized.addPointVectorized(p2, 1);
    vectorized.addPointVectorized(p3, 2);
    vectorized.addPointVectorized(p4, 3);

    nonVectorized.addPoint(p1);
    nonVectorized.addPoint(p2);
    nonVectorized.addPoint(p3);
    nonVectorized.addPoint(p4);

    double scoreV = vectorized.getAndSetScore(1.0, 100.0, 50.0, 1.0, 1.0, 1.0, -30.0, 1.0);
    double scoreNV = nonVectorized.getAndSetScore(1.0, 100.0, 50.0, 1.0, 1.0, 1.0, -30.0, 1.0);

    EXPECT_NEAR(scoreV, scoreNV, 1e-9);
}

TEST(ClusterVectorized, Score_ComputesExpectedValue)
{
    core::PointCluster cluster;

    auto p1 = makePoint(1, 0.0, 0.0, -45.0);
    auto p2 = makePoint(2, 6.0, 0.0, -52.0);
    auto p3 = makePoint(3, 6.0, 4.0, -63.0);

    cluster.addPointVectorized(p1, 0);
    cluster.addPointVectorized(p2, 1);
    cluster.addPointVectorized(p3, 2);

    double ratio = cluster.geometricRatio();
    double area = cluster.area();
    double variance = cluster.varianceRSSI();
    double average_rssi = cluster.avg_rssi;

    double ideal_ratio = ratio * 1.25;
    double ideal_area = area * 0.75;
    double ideal_variance = variance * 1.5;

    double gr_weight = 0.4;
    double area_weight = 0.3;
    double variance_weight = 0.2;
    double rssi_weight = 0.1;
    double bottom_rssi = -70.0;

    double gr_score = 1.0 - std::abs(1.0 - ratio / ideal_ratio);
    double area_score = 1.0 - std::abs(1.0 - area / ideal_area);
    double variance_score = 1.0 - std::abs(1.0 - variance / ideal_variance);
    double rssi_score = 0.0;
    if (average_rssi > bottom_rssi)
    {
        rssi_score = 1.0 - (average_rssi / bottom_rssi);
    }

    double expected_score = gr_weight * gr_score +
                            area_weight * area_score +
                            variance_weight * variance_score +
                            rssi_weight * rssi_score;

    double score = cluster.getAndSetScore(ideal_ratio,
                                          ideal_area,
                                          ideal_variance,
                                          gr_weight,
                                          area_weight,
                                          variance_weight,
                                          bottom_rssi,
                                          rssi_weight);

    EXPECT_NEAR(score, expected_score, 1e-9);
}

TEST(ClusterVectorized, ValidityChecks_SizeAndOverlap)
{
    core::PointCluster smallCluster;
    auto a1 = makePoint(10, 0.0, 0.0, -50.0);
    auto a2 = makePoint(11, 5.0, 0.0, -55.0);

    smallCluster.addPointVectorized(a1, 0);
    smallCluster.addPointVectorized(a2, 1);

    EXPECT_EQ(smallCluster.size(), 2U);
    EXPECT_FALSE(smallCluster.bbox.valid);
    EXPECT_DOUBLE_EQ(smallCluster.geometricRatio(), 0.0);
    EXPECT_DOUBLE_EQ(smallCluster.area(), 0.0);

    core::PointCluster clusterA;
    core::PointCluster clusterB;

    auto shared1 = makePoint(20, 1.0, 0.0, -48.0);
    auto shared2 = makePoint(21, 2.5, 1.0, -52.0);
    auto uniqueA = makePoint(22, -1.0, 0.5, -55.0);
    auto uniqueB = makePoint(23, 4.0, 1.5, -60.0);

    clusterA.addPointVectorized(uniqueA, 0);
    clusterA.addPointVectorized(shared1, 1);
    clusterA.addPointVectorized(shared2, 2);

    clusterB.addPointVectorized(shared1, 1);
    clusterB.addPointVectorized(shared2, 2);
    clusterB.addPointVectorized(uniqueB, 3);

    EXPECT_EQ(clusterA.size(), 3U);
    EXPECT_EQ(clusterB.size(), 3U);
    EXPECT_TRUE(clusterA.bbox.valid);
    EXPECT_TRUE(clusterB.bbox.valid);

    double expected_overlap = 2.0 / static_cast<double>(clusterA.size() + clusterB.size());
    EXPECT_NEAR(clusterA.overlapWith(clusterB), expected_overlap, 1e-9);
    EXPECT_NEAR(clusterB.overlapWith(clusterA), expected_overlap, 1e-9);
}

TEST(ClusterVectorized, BoundingBox_MatchesNonVectorized)
{
    core::PointCluster vectorized;
    core::PointCluster nonVectorized;

    auto p1 = makePoint(1, 0.0, 0.0, -50.0);
    auto p2 = makePoint(2, 100.0, 0.0, -50.0);
    auto p3 = makePoint(3, 50.0, 1.0, -50.0);

    vectorized.addPointVectorized(p1, 0);
    vectorized.addPointVectorized(p2, 1);
    vectorized.addPointVectorized(p3, 2);

    nonVectorized.addPoint(p1);
    nonVectorized.addPoint(p2);
    nonVectorized.addPoint(p3);

    EXPECT_NEAR(vectorized.furthest_distance, nonVectorized.furthest_distance, 1e-9);
    EXPECT_NEAR(vectorized.bbox.range_u, nonVectorized.bbox.range_u, 1e-9);
    EXPECT_NEAR(vectorized.bbox.range_v, nonVectorized.bbox.range_v, 1e-9);
    EXPECT_NEAR(vectorized.geometricRatio(), nonVectorized.geometricRatio(), 1e-9);
    EXPECT_NEAR(vectorized.area(), nonVectorized.area(), 1e-9);
}

TEST(ClusterVectorized, VarianceRSSI_MatchesNonVectorized)
{
    core::PointCluster vectorized;
    core::PointCluster nonVectorized;

    auto p1 = makePoint(1, 0.0, 0.0, -40.0);
    auto p2 = makePoint(2, 1.0, 0.0, -50.0);
    auto p3 = makePoint(3, 2.0, 0.0, -60.0);

    vectorized.addPointVectorized(p1, 0);
    vectorized.addPointVectorized(p2, 1);
    vectorized.addPointVectorized(p3, 2);

    nonVectorized.addPoint(p1);
    nonVectorized.addPoint(p2);
    nonVectorized.addPoint(p3);

    EXPECT_NEAR(vectorized.varianceRSSI(), nonVectorized.varianceRSSI(), 1e-9);
}

TEST(ClusterVectorized, Centroid_MatchesNonVectorized)
{
    core::PointCluster vectorized;
    core::PointCluster nonVectorized;

    auto p1 = makePoint(1, 0.0, 0.0, -40.0);
    auto p2 = makePoint(2, 10.0, 0.0, -60.0);
    auto p3 = makePoint(3, 10.0, 10.0, -50.0);

    vectorized.addPointVectorized(p1, 0);
    vectorized.addPointVectorized(p2, 1);
    vectorized.addPointVectorized(p3, 2);

    nonVectorized.addPoint(p1);
    nonVectorized.addPoint(p2);
    nonVectorized.addPoint(p3);

    EXPECT_NEAR(vectorized.centroid_x, nonVectorized.centroid_x, 1e-9);
    EXPECT_NEAR(vectorized.centroid_y, nonVectorized.centroid_y, 1e-9);
    EXPECT_NEAR(vectorized.avg_rssi, nonVectorized.avg_rssi, 1e-9);
}
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
// BitVector Tests
// ====================

TEST(BitVector, DefaultConstruction)
{
    core::BitVector bv;
    EXPECT_EQ(bv.popcount(), 0u);
}

TEST(BitVector, SetSingleBit)
{
    core::BitVector bv;
    bv.setBit(5);
    EXPECT_TRUE(bv.getBit(5));
    EXPECT_FALSE(bv.getBit(4));
    EXPECT_FALSE(bv.getBit(6));
    EXPECT_EQ(bv.popcount(), 1u);
}

TEST(BitVector, SetMultipleBits)
{
    core::BitVector bv;
    bv.setBit(0);
    bv.setBit(10);
    bv.setBit(63);
    bv.setBit(64);
    bv.setBit(100);
    
    EXPECT_TRUE(bv.getBit(0));
    EXPECT_TRUE(bv.getBit(10));
    EXPECT_TRUE(bv.getBit(63));
    EXPECT_TRUE(bv.getBit(64));
    EXPECT_TRUE(bv.getBit(100));
    EXPECT_FALSE(bv.getBit(1));
    EXPECT_FALSE(bv.getBit(50));
    EXPECT_EQ(bv.popcount(), 5u);
}

TEST(BitVector, ClearBit)
{
    core::BitVector bv;
    bv.setBit(5);
    bv.setBit(10);
    EXPECT_EQ(bv.popcount(), 2u);
    
    bv.clearBit(5);
    EXPECT_FALSE(bv.getBit(5));
    EXPECT_TRUE(bv.getBit(10));
    EXPECT_EQ(bv.popcount(), 1u);
    
    bv.clearBit(10);
    EXPECT_FALSE(bv.getBit(10));
    EXPECT_EQ(bv.popcount(), 0u);
}

TEST(BitVector, ClearNonexistentBit)
{
    core::BitVector bv;
    bv.setBit(5);
    bv.clearBit(100); // Should not crash
    EXPECT_TRUE(bv.getBit(5));
    EXPECT_EQ(bv.popcount(), 1u);
}

TEST(BitVector, ClearAll)
{
    core::BitVector bv;
    bv.setBit(0);
    bv.setBit(10);
    bv.setBit(100);
    EXPECT_EQ(bv.popcount(), 3u);
    
    bv.clear();
    EXPECT_EQ(bv.popcount(), 0u);
    EXPECT_FALSE(bv.getBit(0));
    EXPECT_FALSE(bv.getBit(10));
    EXPECT_FALSE(bv.getBit(100));
}

TEST(BitVector, GetBitOutOfRange)
{
    core::BitVector bv;
    bv.setBit(5);
    EXPECT_FALSE(bv.getBit(1000)); // Should return false, not crash
}

TEST(BitVector, Reserve)
{
    core::BitVector bv;
    bv.reserve(200);
    
    // Should be able to set bits up to the reserved size without reallocation
    bv.setBit(199);
    EXPECT_TRUE(bv.getBit(199));
    EXPECT_EQ(bv.popcount(), 1u);
}

TEST(BitVector, PopcountEmpty)
{
    core::BitVector bv;
    EXPECT_EQ(bv.popcount(), 0u);
}

TEST(BitVector, PopcountMultipleWords)
{
    core::BitVector bv;
    // Set bits in first word (0-63)
    bv.setBit(0);
    bv.setBit(31);
    bv.setBit(63);
    // Set bits in second word (64-127)
    bv.setBit(64);
    bv.setBit(100);
    // Set bits in third word (128+)
    bv.setBit(200);
    
    EXPECT_EQ(bv.popcount(), 6u);
}

TEST(BitVector, SharedCountNoOverlap)
{
    core::BitVector bv1, bv2;
    bv1.setBit(0);
    bv1.setBit(10);
    bv1.setBit(20);
    
    bv2.setBit(5);
    bv2.setBit(15);
    bv2.setBit(25);
    
    EXPECT_EQ(bv1.sharedCount(bv2), 0u);
}

TEST(BitVector, SharedCountFullOverlap)
{
    core::BitVector bv1, bv2;
    bv1.setBit(5);
    bv1.setBit(10);
    bv1.setBit(15);
    
    bv2.setBit(5);
    bv2.setBit(10);
    bv2.setBit(15);
    
    EXPECT_EQ(bv1.sharedCount(bv2), 3u);
}

TEST(BitVector, SharedCountPartialOverlap)
{
    core::BitVector bv1, bv2;
    bv1.setBit(0);
    bv1.setBit(10);
    bv1.setBit(20);
    bv1.setBit(30);
    
    bv2.setBit(10);
    bv2.setBit(20);
    bv2.setBit(40);
    
    EXPECT_EQ(bv1.sharedCount(bv2), 2u); // 10 and 20 are shared
}

TEST(BitVector, SharedCountAcrossWords)
{
    core::BitVector bv1, bv2;
    // First word
    bv1.setBit(0);
    bv1.setBit(32);
    // Second word
    bv1.setBit(64);
    bv1.setBit(100);
    // Third word
    bv1.setBit(200);
    
    bv2.setBit(32);
    bv2.setBit(64);
    bv2.setBit(200);
    bv2.setBit(250);
    
    EXPECT_EQ(bv1.sharedCount(bv2), 3u); // 32, 64, 200
}

TEST(BitVector, SharedCountDifferentSizes)
{
    core::BitVector bv1, bv2;
    bv1.setBit(5);
    bv1.setBit(10);
    
    bv2.setBit(5);
    bv2.setBit(10);
    bv2.setBit(200); // Much larger index
    
    EXPECT_EQ(bv1.sharedCount(bv2), 2u);
    EXPECT_EQ(bv2.sharedCount(bv1), 2u); // Should be symmetric
}

TEST(BitVector, GetSetIndicesEmpty)
{
    core::BitVector bv;
    auto indices = bv.getSetIndices();
    EXPECT_TRUE(indices.empty());
}

TEST(BitVector, GetSetIndicesSingleBit)
{
    core::BitVector bv;
    bv.setBit(42);
    auto indices = bv.getSetIndices();
    ASSERT_EQ(indices.size(), 1u);
    EXPECT_EQ(indices[0], 42);
}

TEST(BitVector, GetSetIndicesMultipleBits)
{
    core::BitVector bv;
    bv.setBit(5);
    bv.setBit(10);
    bv.setBit(15);
    bv.setBit(100);
    bv.setBit(200);
    
    auto indices = bv.getSetIndices();
    ASSERT_EQ(indices.size(), 5u);
    
    // Indices should be in ascending order
    EXPECT_EQ(indices[0], 5);
    EXPECT_EQ(indices[1], 10);
    EXPECT_EQ(indices[2], 15);
    EXPECT_EQ(indices[3], 100);
    EXPECT_EQ(indices[4], 200);
}

TEST(BitVector, GetSetIndicesAcrossMultipleWords)
{
    core::BitVector bv;
    // First word (0-63)
    bv.setBit(0);
    bv.setBit(63);
    // Second word (64-127)
    bv.setBit(64);
    bv.setBit(127);
    // Third word (128-191)
    bv.setBit(128);
    
    auto indices = bv.getSetIndices();
    ASSERT_EQ(indices.size(), 5u);
    EXPECT_EQ(indices[0], 0);
    EXPECT_EQ(indices[1], 63);
    EXPECT_EQ(indices[2], 64);
    EXPECT_EQ(indices[3], 127);
    EXPECT_EQ(indices[4], 128);
}

TEST(BitVector, CopyFrom)
{
    core::BitVector bv1;
    bv1.setBit(5);
    bv1.setBit(10);
    bv1.setBit(100);
    
    core::BitVector bv2;
    bv2.copyFrom(bv1);
    
    EXPECT_EQ(bv2.popcount(), 3u);
    EXPECT_TRUE(bv2.getBit(5));
    EXPECT_TRUE(bv2.getBit(10));
    EXPECT_TRUE(bv2.getBit(100));
    EXPECT_FALSE(bv2.getBit(0));
}

TEST(BitVector, CopyFromModifyOriginal)
{
    core::BitVector bv1;
    bv1.setBit(5);
    bv1.setBit(10);
    
    core::BitVector bv2;
    bv2.copyFrom(bv1);
    
    // Modify original
    bv1.setBit(20);
    bv1.clearBit(5);
    
    // Copy should be unchanged
    EXPECT_EQ(bv2.popcount(), 2u);
    EXPECT_TRUE(bv2.getBit(5));
    EXPECT_TRUE(bv2.getBit(10));
    EXPECT_FALSE(bv2.getBit(20));
}

TEST(BitVector, SetSameBitMultipleTimes)
{
    core::BitVector bv;
    bv.setBit(5);
    bv.setBit(5);
    bv.setBit(5);
    
    EXPECT_EQ(bv.popcount(), 1u);
    EXPECT_TRUE(bv.getBit(5));
}

TEST(BitVector, ClearSameBitMultipleTimes)
{
    core::BitVector bv;
    bv.setBit(5);
    bv.clearBit(5);
    bv.clearBit(5);
    
    EXPECT_EQ(bv.popcount(), 0u);
    EXPECT_FALSE(bv.getBit(5));
}

TEST(BitVector, StressTest_1000Bits)
{
    core::BitVector bv;
    
    // Set every 10th bit up to 1000
    for (int i = 0; i < 1000; i += 10)
    {
        bv.setBit(i);
    }
    
    EXPECT_EQ(bv.popcount(), 100u);
    
    // Verify all set bits
    for (int i = 0; i < 1000; i++)
    {
        if (i % 10 == 0)
        {
            EXPECT_TRUE(bv.getBit(i)) << "Bit " << i << " should be set";
        }
        else
        {
            EXPECT_FALSE(bv.getBit(i)) << "Bit " << i << " should not be set";
        }
    }
    
    auto indices = bv.getSetIndices();
    EXPECT_EQ(indices.size(), 100u);
}

TEST(BitVector, StressTest_SharedCount_LargeVectors)
{
    core::BitVector bv1, bv2;
    
    // bv1: set bits 0, 2, 4, 6, ... 998 (even numbers)
    // bv2: set bits 0, 3, 6, 9, ... 999 (multiples of 3)
    // Shared: multiples of 6 (0, 6, 12, ..., 996) = 167 values
    
    for (int i = 0; i < 1000; i += 2)
    {
        bv1.setBit(i);
    }
    
    for (int i = 0; i < 1000; i += 3)
    {
        bv2.setBit(i);
    }
    
    // Count expected shared (multiples of 6)
    int expected_shared = 0;
    for (int i = 0; i < 1000; i += 6)
    {
        expected_shared++;
    }
    
    EXPECT_EQ(bv1.sharedCount(bv2), static_cast<size_t>(expected_shared));
}

TEST(BitVector, EdgeCase_Bit63And64)
{
    // Test boundary between first and second word
    core::BitVector bv;
    bv.setBit(63);
    bv.setBit(64);
    
    EXPECT_TRUE(bv.getBit(63));
    EXPECT_TRUE(bv.getBit(64));
    EXPECT_FALSE(bv.getBit(62));
    EXPECT_FALSE(bv.getBit(65));
    EXPECT_EQ(bv.popcount(), 2u);
    
    auto indices = bv.getSetIndices();
    ASSERT_EQ(indices.size(), 2u);
    EXPECT_EQ(indices[0], 63);
    EXPECT_EQ(indices[1], 64);
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
        0.0,   // min_geometric_ratio
        1.0,   // max_geometric_ratio
        100.0, // ideal_area
        0.0,   // min_area
        200.0, // max_area (2x ideal)
        50.0,  // ideal_rssi_variance
        0.0,   // min_rssi_variance
        100.0, // max_rssi_variance (2x ideal)
        1.0,   // gr_weight
        1.0,   // area_weight
        1.0,   // variance_weight
        -30.0, // bottom_rssi_threshold
        0.0,   // top_rssi
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
        1.0,   // ideal_geometric_ratio
        0.0,   // min_geometric_ratio
        1.0,   // max_geometric_ratio
        100.0, // ideal_area
        0.0,   // min_area
        200.0, // max_area
        50.0,  // ideal_rssi_variance
        0.0,   // min_rssi_variance
        100.0, // max_rssi_variance
        1.0, 1.0, 1.0,
        -30.0, // bottom_rssi
        0.0,   // top_rssi
        1.0);

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
    core::PointCluster cluster(1);
    auto p = makePoint(1, 10.0, 20.0, -50.0);
    cluster.addPointVectorized(p, 0);

    EXPECT_EQ(cluster.x_dp_values.size(), 1u);
    EXPECT_DOUBLE_EQ(cluster.x_dp_values[0], 10.0);
    EXPECT_DOUBLE_EQ(cluster.y_dp_values[0], 20.0);
    EXPECT_DOUBLE_EQ(cluster.rssi_values[0], -50.0);
    EXPECT_TRUE(cluster.point_bits.getBit(0));
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, -50.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 10.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 20.0);
}

TEST(ClusterVectorized, AddPointVectorized_MultiplePoints)
{
    core::PointCluster cluster(3);
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
    core::PointCluster cluster(2);
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -40.0), 0);
    cluster.addPointVectorized(makePoint(2, 10.0, 0.0, -60.0), 1);

    EXPECT_EQ(cluster.x_dp_values.size(), 2u);

    cluster.removePointVectorized(0, 0);

    EXPECT_EQ(cluster.x_dp_values.size(), 1u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, -60.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 10.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 0.0);
}

TEST(ClusterVectorized, RemovePointVectorized_AllPoints)
{
    core::PointCluster cluster(1);
    cluster.addPointVectorized(makePoint(1, 5.0, 5.0, -45.0), 0);
    cluster.removePointVectorized(0, 0);

    EXPECT_EQ(cluster.x_dp_values.size(), 0u);
    EXPECT_DOUBLE_EQ(cluster.avg_rssi, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_x, 0.0);
    EXPECT_DOUBLE_EQ(cluster.centroid_y, 0.0);
}

TEST(ClusterVectorized, RemovePointVectorized_OutOfRange)
{
    core::PointCluster cluster(1);
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);

    EXPECT_THROW(cluster.removePointVectorized(5, 0), std::out_of_range);
}

TEST(ClusterVectorized, RemovePointVectorized_OtherOutOfRange)
{
    core::PointCluster cluster(1);
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);

    EXPECT_THROW(cluster.removePointVectorized(0, 5), std::out_of_range);
}

TEST(ClusterVectorized, RemovePointVectorized_EmptyCluster)
{
    core::PointCluster cluster(1);
    // Force vectorized mode by adding and removing
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.removePointVectorized(0, 0);

    EXPECT_THROW(cluster.removePointVectorized(0, 0), std::runtime_error);
}

TEST(ClusterVectorized, MixedMode_AddNonVectorizedToVectorized)
{
    core::PointCluster cluster(1);
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);

    EXPECT_THROW(cluster.addPoint(makePoint(2, 1.0, 0.0, -50.0)), std::runtime_error);
}

TEST(ClusterVectorized, MixedMode_RemoveNonVectorizedFromVectorized)
{
    core::PointCluster cluster(1);
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);

    EXPECT_THROW(cluster.removePoint(makePoint(1, 0.0, 0.0, -50.0)), std::runtime_error);
}

TEST(ClusterVectorized, MixedMode_RemoveVectorizedFromNonVectorized)
{
    core::PointCluster cluster;
    cluster.addPoint(makePoint(1, 0.0, 0.0, -50.0));

    EXPECT_THROW(cluster.removePointVectorized(0, 0), std::runtime_error);
}

// ====================
// Vectorized Bounding Box
// ====================

TEST(ClusterVectorized, BoundingBox_TooFewPoints)
{
    core::PointCluster cluster(2);
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 1.0, 0.0, -50.0), 1);

    EXPECT_FALSE(cluster.bbox.valid);
}

TEST(ClusterVectorized, BoundingBox_ExactSize_HorizontalLine)
{
    core::PointCluster cluster(3);
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
    core::PointCluster cluster(4);
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
    core::PointCluster cluster(4);
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
    core::PointCluster cluster(3);
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
    core::PointCluster cluster(3);
    // Elongated cluster with known dimensions
    // range_u = 100, range_v = 1, area = 100
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -50.0), 0);
    cluster.addPointVectorized(makePoint(2, 100.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 50.0, 1.0, -50.0), 2);

    EXPECT_NEAR(cluster.area(), 100.0, 1e-9);
}

TEST(ClusterVectorized, GeometricRatio_Elongated)
{
    core::PointCluster cluster(3);
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
    core::PointCluster cluster(4);
    cluster.addPointVectorized(makePoint(1, 0.0, 0.0, -40.0), 0);
    cluster.addPointVectorized(makePoint(2, 10.0, 0.0, -50.0), 1);
    cluster.addPointVectorized(makePoint(3, 10.0, 10.0, -60.0), 2);
    cluster.addPointVectorized(makePoint(4, 0.0, 10.0, -50.0), 3);

    double score = cluster.getAndSetScore(
        1.0,   // ideal_geometric_ratio
        0.0,   // min_geometric_ratio
        1.0,   // max_geometric_ratio
        100.0, // ideal_area
        0.0,   // min_area
        200.0, // max_area (2x ideal)
        50.0,  // ideal_rssi_variance
        0.0,   // min_rssi_variance
        100.0, // max_rssi_variance (2x ideal)
        1.0,   // gr_weight
        1.0,   // area_weight
        1.0,   // variance_weight
        -30.0, // bottom_rssi_threshold
        0.0,   // top_rssi
        1.0    // rssi_weight
    );

    EXPECT_EQ(cluster.score, score);
    EXPECT_TRUE(std::isfinite(score));
}

TEST(ClusterVectorized, Score_MatchesNonVectorized)
{
    // Create identical clusters using both methods
    core::PointCluster vectorized(4);
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

    double scoreV = vectorized.getAndSetScore(
        1.0, 0.0, 1.0,
        100.0, 0.0, 200.0,
        50.0, 0.0, 100.0,
        1.0, 1.0, 1.0,
        -30.0, 0.0, 1.0);
    double scoreNV = nonVectorized.getAndSetScore(
        1.0, 0.0, 1.0,
        100.0, 0.0, 200.0,
        50.0, 0.0, 100.0,
        1.0, 1.0, 1.0,
        -30.0, 0.0, 1.0);

    EXPECT_NEAR(scoreV, scoreNV, 1e-9);
}

TEST(ClusterVectorized, Score_ComputesExpectedValue)
{
    core::PointCluster cluster(3);

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

    double score = cluster.getAndSetScore(
        ideal_ratio, 0.0, 1.0,
        ideal_area, 0.0, ideal_area * 2.0,
        ideal_variance, 0.0, ideal_variance * 2.0,
        gr_weight, area_weight, variance_weight,
        bottom_rssi, 0.0, rssi_weight);

    EXPECT_NEAR(score, expected_score, 1e-9);
}

TEST(ClusterVectorized, ValidityChecks_SizeAndOverlap)
{
    core::PointCluster smallCluster(2);
    auto a1 = makePoint(10, 0.0, 0.0, -50.0);
    auto a2 = makePoint(11, 5.0, 0.0, -55.0);

    smallCluster.addPointVectorized(a1, 0);
    smallCluster.addPointVectorized(a2, 1);

    EXPECT_EQ(smallCluster.size(), 2U);
    EXPECT_FALSE(smallCluster.bbox.valid);
    EXPECT_DOUBLE_EQ(smallCluster.geometricRatio(), 0.0);
    EXPECT_DOUBLE_EQ(smallCluster.area(), 0.0);

    core::PointCluster clusterA(3);
    core::PointCluster clusterB(3);

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
    core::PointCluster vectorized(3);
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
    core::PointCluster vectorized(3);
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
    core::PointCluster vectorized(3);
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

TEST(ClusterVectorized, Copy_CopyMatchesOriginal)
{
    core::PointCluster original(3);
    original.addPointVectorized(makePoint(1, 0.0, 0.0, -40.0), 0);
    original.addPointVectorized(makePoint(2, 10.0, 0.0, -60.0), 1);
    original.addPointVectorized(makePoint(3, 10.0, 10.0, -50.0), 2);

    core::PointCluster copy = original.copyVectorizedToVectorized();
    EXPECT_EQ(copy.x_dp_values.size(), original.x_dp_values.size());
    EXPECT_EQ(copy.y_dp_values.size(), original.y_dp_values.size());
    EXPECT_EQ(copy.rssi_values.size(), original.rssi_values.size());
    for (size_t i = 0; i < original.x_dp_values.size(); ++i)
    {
        EXPECT_DOUBLE_EQ(copy.x_dp_values[i], original.x_dp_values[i]);
        EXPECT_DOUBLE_EQ(copy.y_dp_values[i], original.y_dp_values[i]);
        EXPECT_DOUBLE_EQ(copy.rssi_values[i], original.rssi_values[i]);
        EXPECT_EQ(copy.point_bits.getBit(i), original.point_bits.getBit(i));
    }
    EXPECT_DOUBLE_EQ(copy.avg_rssi, original.avg_rssi);
    EXPECT_DOUBLE_EQ(copy.centroid_x, original.centroid_x);
    EXPECT_DOUBLE_EQ(copy.centroid_y, original.centroid_y);
    EXPECT_DOUBLE_EQ(copy.furthest_distance, original.furthest_distance);
    EXPECT_EQ(copy.bbox.valid, original.bbox.valid);
    EXPECT_DOUBLE_EQ(copy.bbox.range_u, original.bbox.range_u);
    EXPECT_DOUBLE_EQ(copy.bbox.range_v, original.bbox.range_v);
}

TEST(ClusterVectorized, Copy_NormalMatchesOriginal)
{
    core::PointCluster original(20);
    std::vector<core::DataPoint> points;

    for (size_t i = 0; i < 10; ++i)
    {
        auto p = makePoint(static_cast<int>(i), static_cast<double>(i), static_cast<double>(i * 3), -40.0 - static_cast<double>(i));
        points.push_back(p);
    }
    for (size_t i = 10; i < 20; ++i)
    {
        auto p = makePoint(static_cast<int>(i), static_cast<double>(i*3+5), static_cast<double>(i * 2+6), -40.0 - static_cast<double>(i));
        points.push_back(p);
    }

    std::vector<size_t> indices = {2, 1, 5, 10, 15, 3, 7, 12, 4, 0, 19};

    for (size_t i = 0; i < indices.size(); ++i)
    {
        original.addPointVectorized(points[indices[i]], indices[i]);
    }

    core::PointCluster copy = original.copyVectorizedToNormal(points);
    EXPECT_EQ(copy.points.size(), original.x_dp_values.size());
    EXPECT_DOUBLE_EQ(copy.avg_rssi, original.avg_rssi);
    EXPECT_DOUBLE_EQ(copy.centroid_x, original.centroid_x);
    EXPECT_DOUBLE_EQ(copy.centroid_y, original.centroid_y);
    EXPECT_DOUBLE_EQ(copy.furthest_distance, original.furthest_distance);
    EXPECT_EQ(copy.bbox.valid, original.bbox.valid);
    EXPECT_DOUBLE_EQ(copy.bbox.range_u, original.bbox.range_u);
    EXPECT_DOUBLE_EQ(copy.bbox.range_v, original.bbox.range_v);
}

// ====================
// Order Independence Tests for furthest_distance
// ====================

TEST(ClusterOrderInvariance, NonVectorized_ThreePoints_AllPermutations)
{
    // Test all 6 permutations of 3 points
    auto p1 = makePoint(1, 0.0, 0.0, -50.0);
    auto p2 = makePoint(2, 10.0, 0.0, -50.0);
    auto p3 = makePoint(3, 5.0, 8.66, -50.0); // Forms a triangle

    std::vector<std::vector<core::DataPoint>> permutations = {
        {p1, p2, p3}, {p1, p3, p2}, {p2, p1, p3}, 
        {p2, p3, p1}, {p3, p1, p2}, {p3, p2, p1}
    };

    std::vector<double> distances;
    for (const auto& perm : permutations)
    {
        core::PointCluster cluster;
        for (const auto& p : perm)
        {
            cluster.addPoint(p);
        }
        distances.push_back(cluster.furthest_distance);
    }

    // All permutations should yield the same furthest distance
    for (size_t i = 1; i < distances.size(); ++i)
    {
        EXPECT_NEAR(distances[0], distances[i], 1e-9) 
            << "Permutation " << i << " has different furthest_distance";
    }
}

TEST(ClusterOrderInvariance, Vectorized_ThreePoints_AllPermutations)
{
    // Test all 6 permutations of 3 points in vectorized mode
    std::vector<core::DataPoint> all_points;
    all_points.push_back(makePoint(0, 0.0, 0.0, -50.0));
    all_points.push_back(makePoint(1, 10.0, 0.0, -50.0));
    all_points.push_back(makePoint(2, 5.0, 8.66, -50.0));

    std::vector<std::vector<size_t>> permutations = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, 
        {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };

    std::vector<double> distances;
    for (const auto& perm : permutations)
    {
        core::PointCluster cluster(all_points.size());
        for (size_t idx : perm)
        {
            cluster.addPointVectorized(all_points[idx], idx);
        }
        distances.push_back(cluster.furthest_distance);
    }

    // All permutations should yield the same furthest distance
    for (size_t i = 1; i < distances.size(); ++i)
    {
        EXPECT_NEAR(distances[0], distances[i], 1e-9)
            << "Permutation " << i << " has different furthest_distance";
    }
}

TEST(ClusterOrderInvariance, NonVectorized_FourPoints_Rectangle)
{
    // Rectangle with known furthest distance (diagonal)
    auto p1 = makePoint(1, 0.0, 0.0, -50.0);
    auto p2 = makePoint(2, 20.0, 0.0, -50.0);
    auto p3 = makePoint(3, 20.0, 10.0, -50.0);
    auto p4 = makePoint(4, 0.0, 10.0, -50.0);

    double expected_diagonal = std::sqrt(20.0 * 20.0 + 10.0 * 10.0);

    // Test several different orderings
    std::vector<std::vector<core::DataPoint>> orderings = {
        {p1, p2, p3, p4},  // Sequential
        {p4, p3, p2, p1},  // Reverse
        {p1, p3, p2, p4},  // Cross pattern
        {p2, p4, p1, p3},  // Opposite corners first
        {p3, p1, p4, p2},  // Another cross pattern
    };

    for (size_t ord_idx = 0; ord_idx < orderings.size(); ++ord_idx)
    {
        core::PointCluster cluster;
        for (const auto& p : orderings[ord_idx])
        {
            cluster.addPoint(p);
        }
        EXPECT_NEAR(cluster.furthest_distance, expected_diagonal, 1e-9)
            << "Ordering " << ord_idx << " has incorrect furthest_distance";
    }
}

TEST(ClusterOrderInvariance, Vectorized_FourPoints_Rectangle)
{
    std::vector<core::DataPoint> all_points;
    all_points.push_back(makePoint(0, 0.0, 0.0, -50.0));
    all_points.push_back(makePoint(1, 20.0, 0.0, -50.0));
    all_points.push_back(makePoint(2, 20.0, 10.0, -50.0));
    all_points.push_back(makePoint(3, 0.0, 10.0, -50.0));

    double expected_diagonal = std::sqrt(20.0 * 20.0 + 10.0 * 10.0);

    // Test several different orderings
    std::vector<std::vector<size_t>> orderings = {
        {0, 1, 2, 3},  // Sequential
        {3, 2, 1, 0},  // Reverse
        {0, 2, 1, 3},  // Cross pattern
        {1, 3, 0, 2},  // Opposite corners first
        {2, 0, 3, 1},  // Another cross pattern
    };

    for (size_t ord_idx = 0; ord_idx < orderings.size(); ++ord_idx)
    {
        core::PointCluster cluster(all_points.size());
        for (size_t idx : orderings[ord_idx])
        {
            cluster.addPointVectorized(all_points[idx], idx);
        }
        EXPECT_NEAR(cluster.furthest_distance, expected_diagonal, 1e-9)
            << "Ordering " << ord_idx << " has incorrect furthest_distance";
    }
}

TEST(ClusterOrderInvariance, NonVectorized_LargeSet_RandomOrders)
{
    // Create a larger set of points
    std::vector<core::DataPoint> all_points;
    for (int i = 0; i < 10; ++i)
    {
        all_points.push_back(makePoint(i, i * 10.0, i * 5.0, -40.0 - i));
    }

    // Compute furthest distance with sequential order
    core::PointCluster reference_cluster;
    for (const auto& p : all_points)
    {
        reference_cluster.addPoint(p);
    }
    double reference_distance = reference_cluster.furthest_distance;

    // Test multiple random orderings
    std::vector<std::vector<int>> orderings = {
        {9, 8, 7, 6, 5, 4, 3, 2, 1, 0},          // Reverse
        {0, 9, 1, 8, 2, 7, 3, 6, 4, 5},          // Alternating ends
        {5, 4, 6, 3, 7, 2, 8, 1, 9, 0},          // From middle out
        {2, 7, 1, 9, 0, 5, 8, 3, 6, 4},          // Random-ish
        {4, 1, 7, 2, 9, 3, 6, 0, 8, 5},          // Another random-ish
    };

    for (size_t ord_idx = 0; ord_idx < orderings.size(); ++ord_idx)
    {
        core::PointCluster cluster;
        for (int idx : orderings[ord_idx])
        {
            cluster.addPoint(all_points[idx]);
        }
        EXPECT_NEAR(cluster.furthest_distance, reference_distance, 1e-9)
            << "Ordering " << ord_idx << " has different furthest_distance";
    }
}

TEST(ClusterOrderInvariance, Vectorized_LargeSet_RandomOrders)
{
    // Create a larger set of points
    std::vector<core::DataPoint> all_points;
    for (int i = 0; i < 10; ++i)
    {
        all_points.push_back(makePoint(i, i * 10.0, i * 5.0, -40.0 - i));
    }

    // Compute furthest distance with sequential order
    core::PointCluster reference_cluster(all_points.size());
    for (size_t i = 0; i < all_points.size(); ++i)
    {
        reference_cluster.addPointVectorized(all_points[i], i);
    }
    double reference_distance = reference_cluster.furthest_distance;

    // Test multiple random orderings
    std::vector<std::vector<size_t>> orderings = {
        {9, 8, 7, 6, 5, 4, 3, 2, 1, 0},          // Reverse
        {0, 9, 1, 8, 2, 7, 3, 6, 4, 5},          // Alternating ends
        {5, 4, 6, 3, 7, 2, 8, 1, 9, 0},          // From middle out
        {2, 7, 1, 9, 0, 5, 8, 3, 6, 4},          // Random-ish
        {4, 1, 7, 2, 9, 3, 6, 0, 8, 5},          // Another random-ish
    };

    for (size_t ord_idx = 0; ord_idx < orderings.size(); ++ord_idx)
    {
        core::PointCluster cluster(all_points.size());
        for (size_t idx : orderings[ord_idx])
        {
            cluster.addPointVectorized(all_points[idx], idx);
        }
        EXPECT_NEAR(cluster.furthest_distance, reference_distance, 1e-9)
            << "Ordering " << ord_idx << " has different furthest_distance";
    }
}

TEST(ClusterOrderInvariance, NonVectorized_CollinearPoints)
{
    // Collinear points - furthest should be end-to-end
    auto p1 = makePoint(1, 0.0, 0.0, -50.0);
    auto p2 = makePoint(2, 25.0, 0.0, -50.0);
    auto p3 = makePoint(3, 50.0, 0.0, -50.0);
    auto p4 = makePoint(4, 75.0, 0.0, -50.0);
    auto p5 = makePoint(5, 100.0, 0.0, -50.0);

    double expected_distance = 100.0;

    std::vector<std::vector<core::DataPoint>> orderings = {
        {p1, p2, p3, p4, p5},  // Sequential
        {p5, p4, p3, p2, p1},  // Reverse
        {p3, p1, p5, p2, p4},  // Middle first
        {p5, p1, p3, p4, p2},  // Ends first
    };

    for (size_t ord_idx = 0; ord_idx < orderings.size(); ++ord_idx)
    {
        core::PointCluster cluster;
        for (const auto& p : orderings[ord_idx])
        {
            cluster.addPoint(p);
        }
        EXPECT_NEAR(cluster.furthest_distance, expected_distance, 1e-9)
            << "Ordering " << ord_idx << " has incorrect furthest_distance";
    }
}

TEST(ClusterOrderInvariance, Vectorized_CollinearPoints)
{
    std::vector<core::DataPoint> all_points;
    all_points.push_back(makePoint(0, 0.0, 0.0, -50.0));
    all_points.push_back(makePoint(1, 25.0, 0.0, -50.0));
    all_points.push_back(makePoint(2, 50.0, 0.0, -50.0));
    all_points.push_back(makePoint(3, 75.0, 0.0, -50.0));
    all_points.push_back(makePoint(4, 100.0, 0.0, -50.0));

    double expected_distance = 100.0;

    std::vector<std::vector<size_t>> orderings = {
        {0, 1, 2, 3, 4},  // Sequential
        {4, 3, 2, 1, 0},  // Reverse
        {2, 0, 4, 1, 3},  // Middle first
        {4, 0, 2, 3, 1},  // Ends first
    };

    for (size_t ord_idx = 0; ord_idx < orderings.size(); ++ord_idx)
    {
        core::PointCluster cluster(all_points.size());
        for (size_t idx : orderings[ord_idx])
        {
            cluster.addPointVectorized(all_points[idx], idx);
        }
        EXPECT_NEAR(cluster.furthest_distance, expected_distance, 1e-9)
            << "Ordering " << ord_idx << " has incorrect furthest_distance";
    }
}

TEST(ClusterOrderInvariance, CrossCheck_VectorizedVsNonVectorized_SameOrder)
{
    // Verify vectorized and non-vectorized give same result with same order
    std::vector<core::DataPoint> all_points;
    for (int i = 0; i < 8; ++i)
    {
        all_points.push_back(makePoint(i, i * 7.5, (i * i) % 30, -45.0 - i * 2));
    }

    core::PointCluster vectorized(all_points.size());
    core::PointCluster nonVectorized;

    for (size_t i = 0; i < all_points.size(); ++i)
    {
        vectorized.addPointVectorized(all_points[i], i);
        nonVectorized.addPoint(all_points[i]);
    }

    EXPECT_NEAR(vectorized.furthest_distance, nonVectorized.furthest_distance, 1e-9);
    EXPECT_NEAR(vectorized.bbox.range_u, nonVectorized.bbox.range_u, 1e-9);
    EXPECT_NEAR(vectorized.bbox.range_v, nonVectorized.bbox.range_v, 1e-9);
}

TEST(ClusterOrderInvariance, CrossCheck_VectorizedVsNonVectorized_DifferentOrders)
{
    // Verify vectorized and non-vectorized give same result with different orders
    std::vector<core::DataPoint> all_points;
    for (int i = 0; i < 8; ++i)
    {
        all_points.push_back(makePoint(i, i * 7.5, (i * i) % 30, -45.0 - i * 2));
    }

    // Vectorized with one order
    std::vector<size_t> vec_order = {7, 2, 5, 0, 6, 1, 4, 3};
    core::PointCluster vectorized(all_points.size());
    for (size_t idx : vec_order)
    {
        vectorized.addPointVectorized(all_points[idx], idx);
    }

    // Non-vectorized with different order
    std::vector<size_t> nonvec_order = {3, 1, 6, 0, 5, 2, 7, 4};
    core::PointCluster nonVectorized;
    for (size_t idx : nonvec_order)
    {
        nonVectorized.addPoint(all_points[idx]);
    }

    // Both should have the same furthest_distance regardless of add order
    EXPECT_NEAR(vectorized.furthest_distance, nonVectorized.furthest_distance, 1e-9);
}

TEST(ClusterOrderInvariance, IncrementalAddition_VectorizedConsistency)
{
    // Test that furthest_distance is correctly updated as points are added incrementally
    std::vector<core::DataPoint> all_points;
    all_points.push_back(makePoint(0, 0.0, 0.0, -50.0));
    all_points.push_back(makePoint(1, 10.0, 0.0, -50.0));
    all_points.push_back(makePoint(2, 10.0, 10.0, -50.0));
    all_points.push_back(makePoint(3, 0.0, 10.0, -50.0));
    all_points.push_back(makePoint(4, 5.0, 5.0, -50.0));  // Center point

    core::PointCluster cluster(all_points.size());
    
    // Add first two points - distance should be 10
    cluster.addPointVectorized(all_points[0], 0);
    cluster.addPointVectorized(all_points[1], 1);
    EXPECT_NEAR(cluster.furthest_distance, 10.0, 1e-9);

    // Add third point - distance should become diagonal (sqrt(200))
    cluster.addPointVectorized(all_points[2], 2);
    EXPECT_NEAR(cluster.furthest_distance, std::sqrt(200.0), 1e-9);

    // Add fourth point - distance should stay diagonal
    cluster.addPointVectorized(all_points[3], 3);
    EXPECT_NEAR(cluster.furthest_distance, std::sqrt(200.0), 1e-9);

    // Add center point - distance should still be diagonal
    cluster.addPointVectorized(all_points[4], 4);
    EXPECT_NEAR(cluster.furthest_distance, std::sqrt(200.0), 1e-9);
}

TEST(ClusterOrderInvariance, IncrementalAddition_NonVectorizedConsistency)
{
    // Same test for non-vectorized
    auto p1 = makePoint(1, 0.0, 0.0, -50.0);
    auto p2 = makePoint(2, 10.0, 0.0, -50.0);
    auto p3 = makePoint(3, 10.0, 10.0, -50.0);
    auto p4 = makePoint(4, 0.0, 10.0, -50.0);
    auto p5 = makePoint(5, 5.0, 5.0, -50.0);

    core::PointCluster cluster;
    
    cluster.addPoint(p1);
    cluster.addPoint(p2);
    EXPECT_NEAR(cluster.furthest_distance, 10.0, 1e-9);

    cluster.addPoint(p3);
    EXPECT_NEAR(cluster.furthest_distance, std::sqrt(200.0), 1e-9);

    cluster.addPoint(p4);
    EXPECT_NEAR(cluster.furthest_distance, std::sqrt(200.0), 1e-9);

    cluster.addPoint(p5);
    EXPECT_NEAR(cluster.furthest_distance, std::sqrt(200.0), 1e-9);
}

TEST(ClusterOrderInvariance, StressTest_20Points_MultipleOrderings)
{
    // Stress test with 20 points in various orderings
    std::vector<core::DataPoint> all_points;
    for (int i = 0; i < 20; ++i)
    {
        double x = (i % 5) * 12.5;
        double y = (i / 5) * 8.3;
        all_points.push_back(makePoint(i, x, y, -40.0 - i));
    }

    // Compute reference distance
    core::PointCluster reference(all_points.size());
    for (size_t i = 0; i < all_points.size(); ++i)
    {
        reference.addPointVectorized(all_points[i], i);
    }
    double reference_distance = reference.furthest_distance;

    // Test 10 different orderings
    std::vector<std::vector<size_t>> orderings = {
        {19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},  // Reverse
        {0, 19, 1, 18, 2, 17, 3, 16, 4, 15, 5, 14, 6, 13, 7, 12, 8, 11, 9, 10},  // Alternating
        {10, 9, 11, 8, 12, 7, 13, 6, 14, 5, 15, 4, 16, 3, 17, 2, 18, 1, 19, 0},  // From middle
        {5, 15, 10, 0, 19, 3, 12, 7, 18, 1, 14, 8, 11, 6, 16, 2, 13, 4, 17, 9},  // Random 1
        {12, 4, 17, 9, 2, 14, 7, 19, 1, 11, 5, 16, 0, 13, 8, 18, 3, 15, 6, 10},  // Random 2
    };

    for (size_t ord_idx = 0; ord_idx < orderings.size(); ++ord_idx)
    {
        core::PointCluster cluster(all_points.size());
        for (size_t idx : orderings[ord_idx])
        {
            cluster.addPointVectorized(all_points[idx], idx);
        }
        EXPECT_NEAR(cluster.furthest_distance, reference_distance, 1e-9)
            << "Stress test ordering " << ord_idx << " has different furthest_distance";
    }
}
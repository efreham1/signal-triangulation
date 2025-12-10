#include <gtest/gtest.h>
#include "core/PointDistanceCache.hpp"
#include "core/DataPoint.h"

namespace core {

class PointDistanceCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        PointDistanceCache::getInstance().clear();
    }

    void TearDown() override {
        PointDistanceCache::getInstance().clear();
    }
};

TEST_F(PointDistanceCacheTest, SingletonInstance) {
    PointDistanceCache& instance1 = PointDistanceCache::getInstance();
    PointDistanceCache& instance2 = PointDistanceCache::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(PointDistanceCacheTest, CalculateDistance) {
    DataPoint p1;
    p1.point_id = 1;
    p1.setX(0.0);
    p1.setY(0.0);

    DataPoint p2;
    p2.point_id = 2;
    p2.setX(3.0);
    p2.setY(4.0);

    double dist = PointDistanceCache::getInstance().getDistance(p1, p2);
    EXPECT_DOUBLE_EQ(dist, 5.0);
}

TEST_F(PointDistanceCacheTest, CacheBehavior) {
    DataPoint p1;
    p1.point_id = 10;
    p1.setX(0.0);
    p1.setY(0.0);

    DataPoint p2;
    p2.point_id = 20;
    p2.setX(3.0);
    p2.setY(4.0);

    PointDistanceCache& cache = PointDistanceCache::getInstance();
    
    // Initial state
    EXPECT_EQ(cache.size(), 0);

    // First call - should calculate and cache
    double dist1 = cache.getDistance(p1, p2);
    EXPECT_DOUBLE_EQ(dist1, 5.0);
    EXPECT_EQ(cache.size(), 1);

    // Second call - should retrieve from cache
    double dist2 = cache.getDistance(p1, p2);
    EXPECT_DOUBLE_EQ(dist2, 5.0);
    EXPECT_EQ(cache.size(), 1);
}

TEST_F(PointDistanceCacheTest, Symmetry) {
    DataPoint p1;
    p1.point_id = 100;
    p1.setX(10.0);
    p1.setY(10.0);

    DataPoint p2;
    p2.point_id = 200;
    p2.setX(20.0);
    p2.setY(20.0);

    PointDistanceCache& cache = PointDistanceCache::getInstance();

    double dist1 = cache.getDistance(p1, p2);
    double dist2 = cache.getDistance(p2, p1);

    EXPECT_DOUBLE_EQ(dist1, dist2);
    EXPECT_EQ(cache.size(), 1); // Should be stored as one entry (min_id, max_id)
}

TEST_F(PointDistanceCacheTest, ClearCache) {
    DataPoint p1;
    p1.point_id = 1000;
    p1.setX(0.0);
    p1.setY(0.0);

    DataPoint p2;
    p2.point_id = 2000;
    p2.setX(1.0);
    p2.setY(1.0);

    PointDistanceCache& cache = PointDistanceCache::getInstance();
    cache.getDistance(p1, p2);
    EXPECT_EQ(cache.size(), 1);

    cache.clear();
    EXPECT_EQ(cache.size(), 0);
}

} // namespace core

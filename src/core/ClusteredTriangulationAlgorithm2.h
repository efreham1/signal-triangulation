#ifndef CLUSTERED_TRIANGULATION_ALGORITHM2_H
#define CLUSTERED_TRIANGULATION_ALGORITHM2_H

#include "ITriangulationAlgorithm.h"
#include "DataPoint.h"
#include "Cluster.hpp"

#include <vector>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace core
{

    /**
     * @class ClusteredTriangulationAlgorithm2
     * @brief Cluster-based triangulation algorithm implementing a brute force search
     * for position estimation. This class processes received measurements and applies
     * a brute force search to estimate the target position.
     */
    class ClusteredTriangulationAlgorithm2 : public ITriangulationAlgorithm
    {
    public:
        ClusteredTriangulationAlgorithm2();
        ~ClusteredTriangulationAlgorithm2() override;

        void processDataPoint(const DataPoint &point) override;
        void calculatePosition(double &out_latitude, double &out_longitude, double precision = 0.5, double timeout = -1.0) override;
        void reset() override;

        bool plottingEnabled = true;

    private:
        std::vector<DataPoint> m_points;
        std::vector<PointCluster> m_clusters;

        // Distance cache for path optimization
        struct PairHash
        {
            std::size_t operator()(const std::pair<int64_t, int64_t> &p) const
            {
                std::size_t seed = std::hash<int64_t>{}(p.first);
                seed ^= std::hash<int64_t>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                return seed;
            }
        };
        std::unordered_map<std::pair<int64_t, int64_t>, double, PairHash> distance_cache;

        std::pair<int64_t, int64_t> makeEdgeKey(int64_t id1, int64_t id2) const;
        void addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance);
        void reorderDataPointsByDistance();

        // Clustering evaluation
        struct ClusterMetrics
        {
            double squareness;  // 1.0 = perfect square, 0.0 = very elongated
            double area;        // Bounding box or variance-based area
            size_t point_count; // Number of points in cluster
            bool valid;         // Meets minimum requirements
        };

        struct PartitionScore
        {
            double squareness_score;    // Average squareness (higher is better)
            double area_score;          // How close to target area (lower deviation is better)
            double cluster_count_score; // How close to target cluster count
            double total_score;         // Combined score (lower is better)
        };

        // Calculate metrics for a single cluster segment [start_idx, end_idx]
        ClusterMetrics evaluateClusterMetrics(size_t start_idx, size_t end_idx) const;

        // Calculate score for entire partition
        PartitionScore evaluatePartition(const std::vector<std::pair<size_t, size_t>> &segments) const;

        // Determine target total area based on point spread
        double getTargetTotalArea(size_t num_clusters) const;

        // Exhaustive search for optimal contiguous clustering
        void findOptimalClustering();

        // Dynamic programming approach for larger datasets
        void dpClusterSearch();

        // Build m_clusters from segment indices
        void buildClustersFromSegments(const std::vector<std::pair<size_t, size_t>> &segments);

        // Original clustering and estimation
        void clusterData();
        void estimateAoAForClusters();
        void bruteForceSearch(double &global_best_x, double &global_best_y, double precision, double timeout);
        double getCost(double x, double y);
    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM2_H
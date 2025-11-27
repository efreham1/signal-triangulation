#ifndef CLUSTERED_TRIANGULATION_ALGORITHM2_H
#define CLUSTERED_TRIANGULATION_ALGORITHM2_H

#include "ITriangulationAlgorithm.h"
#include "DataPoint.h"
#include "Cluster.hpp"

#include <vector>
#include <cmath>
#include <limits>

namespace core
{

    /**
     * @class ClusteredTriangulationAlgorithm2
     * @brief Cluster-based triangulation algorithm implementing a brute force search
     * for position estimation. This class processes received measurements and applies
     * a brute force search to estimate the target position. Clustering and AoA estimation
     * are planned for future enhancements.
     */
    class ClusteredTriangulationAlgorithm2 : public ITriangulationAlgorithm
    {
    public:
        ClusteredTriangulationAlgorithm2();
        ~ClusteredTriangulationAlgorithm2() override;

        // ITriangulationAlgorithm interface
        void processDataPoint(const DataPoint &point) override;
        void calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout) override;
        void reset() override;

    private:
        // Storage for received measurements. Implement clustering and additional
        // state in later iterations.
        std::vector<DataPoint> m_points;

        std::vector<PointCluster> m_clusters;

        // Distance cache for optimization
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

        // Helper to create normalized edge key (smaller ID first)
        std::pair<int64_t, int64_t> makeEdgeKey(int64_t id1, int64_t id2) const;

        // Distance cache helpers
        void addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance);

        // Reorder points by distance (2-opt optimization)
        void reorderDataPointsByDistance();

        // Internal helpers (stubs)
        void clusterData();
        void estimateAoAForClusters();
        double getCost(double x, double y);
        void bruteForceSearch(double &out_x, double &out_y, double precision, double timeout);
    };

    std::vector<double> getNormalVector2(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z);

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM2_H

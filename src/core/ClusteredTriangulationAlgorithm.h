#ifndef CLUSTERED_TRIANGULATION_ALGORITHM_H
#define CLUSTERED_TRIANGULATION_ALGORITHM_H

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
     * @class ClusteredTriangulationAlgorithm
     * @brief Skeleton for the cluster-based triangulation algorithm described in the
     * design notes. This file provides method stubs and a minimal data model so the
     * class can be compiled and integrated; algorithmic details should be implemented
     * in subsequent iterations.
     */
    class ClusteredTriangulationAlgorithm : public ITriangulationAlgorithm
    {
    public:
        ClusteredTriangulationAlgorithm();
        ~ClusteredTriangulationAlgorithm() override;

        // ITriangulationAlgorithm interface
        void processDataPoint(const DataPoint &point) override;
        void calculatePosition(double &out_latitude, double &out_longitude) override;
        void reset() override;
        
        private:
        // Storage for received measurements. Implement clustering and additional
        // state in later iterations.
        std::vector<DataPoint> m_points;
        
        std::vector<PointCluster> m_clusters;

        // Memoization setup
        struct PairHash
        {
            static void hash_combine(std::size_t &seed, std::size_t value)
            {
                // Golden ratio hash combination
                int golden_ratio_const = 0x9e3779b9;
                seed ^= value + golden_ratio_const + (seed << 6) + (seed >> 2);
            }

            inline std::size_t operator()(const std::pair<int64_t, int64_t> &v) const
            {
                std::size_t seed = 0;
                hash_combine(seed, std::hash<int64_t>()(v.first));
                hash_combine(seed, std::hash<int64_t>()(v.second));
                return seed;
            }
        };
        std::unordered_map<std::pair<int64_t, int64_t>, double, PairHash> distance_cache;

        // Internal helpers
        void clusterData();
        void estimateAoAForClusters();
        std::pair<int64_t, int64_t> makeDistanceKey(int64_t id1, int64_t id2) const;
        void addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance);
        void reorderDataPointsByDistance();
        double getCost(double x, double y);
        void gradientDescent(double &out_x, double &out_y, std::vector<std::pair<double, double>> intersections);
        std::vector<std::pair<double, double>> findIntersections();
    };

    std::vector<double> getNormalVector(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z);

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM_H

#ifndef CLUSTERED_TRIANGULATION_BASE_H
#define CLUSTERED_TRIANGULATION_BASE_H

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
     * @class ClusteredTriangulationBase
     * @brief Base class for cluster-based triangulation algorithms.
     * Contains shared functionality for point processing, clustering utilities,
     * AoA estimation, and cost calculation.
     */
    class ClusteredTriangulationBase : public ITriangulationAlgorithm
    {
    public:
        ClusteredTriangulationBase();
        ~ClusteredTriangulationBase() override;

        // ITriangulationAlgorithm interface - common implementations
        void processDataPoint(const DataPoint &point) override;
        void reset() override;

        // calculatePosition() must be implemented by subclasses
        void calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout) override = 0;

    protected:
        // Shared data
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

        // Shared helper methods
        std::pair<int64_t, int64_t> makeDistanceKey(int64_t id1, int64_t id2) const;
        void addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance);
        double getDistance(const DataPoint &p1, const DataPoint &p2);

        // Shared processing methods
        void reorderDataPointsByDistance();
        void coalescePoints(double coalition_distance);
        void estimateAoAForClusters();

        // Cost calculation - can be overridden for custom weighting
        virtual double getCost(double x, double y) const;

        // Clustering - must be implemented by subclasses
        virtual void clusterData() = 0;

        // Debug/plotting helper
        void printPointsAndClusters() const;

        // Tunable parameters - subclasses can override these
        virtual double getCoalitionDistance() const { return 1.0; }
        virtual unsigned int getClusterMinPoints() const { return 3u; }
        virtual double getClusterRatioSplitThreshold() const { return 0.25; }

        // Cost function weights - subclasses can override
        virtual double getVarianceWeight() const { return 0.0; }
        virtual double getRssiWeight() const { return 0.0; }
        virtual double getBottomRssi() const { return -90.0; }
        virtual double getExtraWeight() const { return 1.0; }
    };

    // Shared utility function for plane fitting
    std::vector<double> fitPlaneNormal(
        const std::vector<double> &x,
        const std::vector<double> &y,
        const std::vector<double> &z,
        unsigned int min_points = 3u);

} // namespace core

#endif // CLUSTERED_TRIANGULATION_BASE_H
#ifndef CLUSTERED_TRIANGULATION_BASE_H
#define CLUSTERED_TRIANGULATION_BASE_H

#include "ITriangulationAlgorithm.h"
#include "DataPoint.h"
#include "Cluster.h"

#include <vector>
#include <map>

namespace core
{

    // Free function for plane fitting
    std::vector<double> fitPlaneNormal(
        const std::vector<double> &x,
        const std::vector<double> &y,
        const std::vector<double> &z,
        unsigned int min_points);

    class ClusteredTriangulationBase : public ITriangulationAlgorithm
    {
    public:
        ClusteredTriangulationBase();
        ~ClusteredTriangulationBase() override;

        void processDataPoint(const DataPoint &point) override;
        void reset() override;

    protected:
        // Utility methods - take parameters explicitly
        void reorderDataPointsByDistance();
        void coalescePoints(double coalition_distance);
        void estimateAoAForClusters(unsigned int min_points);
        double getCost(double x, double y, double extra_weight) const;

        // Distance caching
        double getDistance(const DataPoint &p1, const DataPoint &p2);

        // Debug
        void printPointsAndClusters() const;

        // Data
        std::vector<DataPoint> m_points;
        std::vector<PointCluster> m_clusters;

        std::map<std::pair<int64_t, int64_t>, double> distance_cache;
    private:
        std::pair<int64_t, int64_t> makeDistanceKey(int64_t id1, int64_t id2) const;
        void addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance);

    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_BASE_H
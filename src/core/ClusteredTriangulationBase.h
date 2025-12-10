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

        void addDataPointMap(std::map<std::string, std::vector<core::DataPoint>> dp_map, double zero_latitude, double zero_longitude) override;
        void reset() override;
        
        protected:
        // Utility methods - take parameters explicitly
        void reorderDataPointsByDistance(std::vector<DataPoint> &m_points);
        void coalescePoints(double coalition_distance, std::vector<DataPoint> &m_points);
        void estimateAoAForClusters(unsigned int min_points);
        double getCost(double x, double y, double extra_weight, double angle_weight) const;

        // Distance caching

        // Debug
        void printPointsAndClusters() const;

        // Data
        std::map<std::string, std::vector<core::DataPoint>> m_point_map;
        std::vector<PointCluster> m_clusters;
        size_t m_total_points = 0;
        double m_zero_latitude = 0.0;
        double m_zero_longitude = 0.0;

    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_BASE_H
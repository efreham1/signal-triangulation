#ifndef CLUSTERED_TRIANGULATION_ALGORITHM_H
#define CLUSTERED_TRIANGULATION_ALGORITHM_H

#include "ITriangulationAlgorithm.h"
#include "DataPoint.h"
#include "Cluster.hpp"

#include <vector>
#include <cmath>
#include <limits>

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
        
        // Internal helpers (stubs)
        void clusterData();
        void estimateAoAForClusters();
        void reorderDataPointsByDistance(bool filterOutliers = true);
        double getCost(double x, double y);
        void gradientDescent(double &out_x, double &out_y, std::vector<std::pair<double, double>> intersections);
        std::vector<std::pair<double, double>> findIntersections();
    };

    std::vector<double> getNormalVector(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z);

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM_H

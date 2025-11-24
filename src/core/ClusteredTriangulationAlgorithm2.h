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
     * @brief Skeleton for the cluster-based triangulation algorithm described in the
     * design notes. This file provides method stubs and a minimal data model so the
     * class can be compiled and integrated; algorithmic details should be implemented
     * in subsequent iterations.
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

        // Internal helpers (stubs)
        void clusterData();
        void estimateAoAForClusters();
        double getCost(double x, double y);
        void bruteForceSearch(double &out_x, double &out_y, double precision, double timeout);
    };

    std::vector<double> getNormalVector2(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z);

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM2_H

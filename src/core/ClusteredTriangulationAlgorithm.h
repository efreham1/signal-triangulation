#ifndef CLUSTERED_TRIANGULATION_ALGORITHM_H
#define CLUSTERED_TRIANGULATION_ALGORITHM_H

#include "TriangulationService.h"
#include "DataPoint.h"

#include <vector>

namespace core {

/**
 * @class ClusteredTriangulationAlgorithm
 * @brief Skeleton for the cluster-based triangulation algorithm described in the
 * design notes. This file provides method stubs and a minimal data model so the
 * class can be compiled and integrated; algorithmic details should be implemented
 * in subsequent iterations.
 */
class ClusteredTriangulationAlgorithm : public ITriangulationAlgorithm {
public:
    ClusteredTriangulationAlgorithm();
    ~ClusteredTriangulationAlgorithm() override;

    // ITriangulationAlgorithm interface
    bool processDataPoint(const DataPoint& point) override;
    bool calculatePosition(double& out_latitude, double& out_longitude) override;
    void reset() override;

private:

    class PointCluster
    {
    public:
        std::vector<DataPoint> points;
        double estimated_aoa;
        double avg_rssi;


        PointCluster()
        {
            avg_rssi = 0.0;
            estimated_aoa = 0.0;
        }
        ~PointCluster() = default;

        void addPoint(const DataPoint& point)
        {
            points.push_back(point);

            // Update average RSSI
            double previous_total = avg_rssi * static_cast<double>(points.size() - 1);
            avg_rssi = (previous_total + point.rssi) / static_cast<double>(points.size());
        }
    };

    // Storage for received measurements. Implement clustering and additional
    // state in later iterations.
    std::vector<DataPoint> m_points;

    std::vector<PointCluster> m_clusters;

    // Internal helpers (stubs)
    void clusterData();
    void estimateAoAForClusters();
    void buildCostFunction();
};

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM_H

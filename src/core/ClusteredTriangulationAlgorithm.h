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
        double centroid_x;
        double centroid_y;
        double aoa_x;
        double aoa_y;


        PointCluster()
        {
            avg_rssi = 0.0;
            estimated_aoa = 0.0;
            centroid_x = 0.0;
            centroid_y = 0.0;
            aoa_x = 0.0;
            aoa_y = 0.0;
        }
        ~PointCluster() = default;

        void addPoint(const DataPoint& point)
        {
            points.push_back(point);

            // Update average RSSI
            double previous_total = avg_rssi * static_cast<double>(points.size() - 1);
            avg_rssi = (previous_total + point.rssi) / static_cast<double>(points.size());

            // Update centroid
            double previous_total_x = centroid_x * static_cast<double>(points.size() - 1);
            double previous_total_y = centroid_y * static_cast<double>(points.size() - 1);
            centroid_x = (previous_total_x + point.getX()) / static_cast<double>(points.size());
            centroid_y = (previous_total_y + point.getY()) / static_cast<double>(points.size());
        }
    };

    class Point
    {
    public:
        double x;
        double y;
        Point(double x_in, double y_in) : x(x_in), y(y_in) {}
    };

    // Storage for received measurements. Implement clustering and additional
    // state in later iterations.
    std::vector<DataPoint> m_points;

    std::vector<PointCluster> m_clusters;

    // Internal helpers (stubs)
    void clusterData();
    void estimateAoAForClusters();
    double getCost(double x, double y);
    std::vector<ClusteredTriangulationAlgorithm::Point> findIntersections();
};

std::vector<double> getNormalVector(const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>& z);

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM_H

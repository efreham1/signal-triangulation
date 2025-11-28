#ifndef CLUSTERED_TRIANGULATION_ALGORITHM2_H
#define CLUSTERED_TRIANGULATION_ALGORITHM2_H

#include "ITriangulationAlgorithm.h"
#include "DataPoint.h"
#include "Cluster.hpp"

#include <vector>
#include <cmath>
#include <limits>
#include <optional>

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
        void setHyperparameters(
            std::optional<double> coalition_dist_meters,
            std::optional<int> cluster_min_points,
            std::optional<double> cluster_ratio_split_threshold,
            std::optional<double> normal_regularization_eps, 
            std::optional<double> gauss_elim_pivot_eps       
        );
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

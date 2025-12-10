#ifndef CLUSTERED_TRIANGULATION_ALGORITHM1_H
#define CLUSTERED_TRIANGULATION_ALGORITHM1_H

#include "ClusteredTriangulationBase.h"
#include "AlgorithmParameters.h"

namespace core
{

    /**
     * @class ClusteredTriangulationAlgorithm1
     * @brief Cluster-based triangulation using geometric ratio splitting for clustering
     * and gradient descent with intersection seeding for position estimation.
     */
    class ClusteredTriangulationAlgorithm1 : public ClusteredTriangulationBase
    {
    public:
        ClusteredTriangulationAlgorithm1();
        explicit ClusteredTriangulationAlgorithm1(const AlgorithmParameters &params);
        ~ClusteredTriangulationAlgorithm1() override;

        void calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout) override;

    private:
        void applyParameters(const AlgorithmParameters &params);

        // CTA1-specific methods
        void clusterData(std::vector<DataPoint> &m_points);
        std::vector<std::pair<double, double>> findIntersections();
        void gradientDescent(double &out_x, double &out_y,
                             std::vector<std::pair<double, double>> intersections,
                             double precision, double timeout);

        // Clustering
        double m_coalition_distance = 2.0;
        unsigned int m_cluster_min_points = 3;
        double m_cluster_ratio_threshold = 0.25;

        // Cost function
        double m_extra_weight = 1.0;
        double m_angle_weight = 10.0;
    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM1_H
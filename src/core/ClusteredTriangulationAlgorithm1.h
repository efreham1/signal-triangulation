#ifndef CLUSTERED_TRIANGULATION_ALGORITHM1_H
#define CLUSTERED_TRIANGULATION_ALGORITHM1_H

#include "ClusteredTriangulationBase.h"

#include <optional>

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
        ~ClusteredTriangulationAlgorithm1() override;

        void calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout) override;
    protected:
        // Override parameters for this algorithm
        double getCoalitionDistance() const override { return 1.0; }
        unsigned int getClusterMinPoints() const override { return 4u; }
        double getClusterRatioSplitThreshold() const override { return 0.25; }

        // No weighting in CTA1
        double getVarianceWeight() const override { return 0.0; }
        double getRssiWeight() const override { return 0.0; }
        double getExtraWeight() const override { return 1.0; }

        // Implement clustering
        void clusterData() override;

    private:
        // CTA1-specific methods
        std::vector<std::pair<double, double>> findIntersections();
        void gradientDescent(double &out_x, double &out_y,
                             std::vector<std::pair<double, double>> intersections,
                             double precision, double timeout);
    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM1_H
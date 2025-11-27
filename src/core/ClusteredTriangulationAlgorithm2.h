#ifndef CLUSTERED_TRIANGULATION_ALGORITHM2_H
#define CLUSTERED_TRIANGULATION_ALGORITHM2_H

#include "ClusteredTriangulationBase.h"

namespace core
{

    /**
     * @class ClusteredTriangulationAlgorithm2
     * @brief Cluster-based triangulation using geometric ratio splitting for clustering
     * and brute force grid search for position estimation, with cluster weighting.
     */
    class ClusteredTriangulationAlgorithm2 : public ClusteredTriangulationBase
    {
    public:
        ClusteredTriangulationAlgorithm2();
        ~ClusteredTriangulationAlgorithm2() override;

        void calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout) override;

    protected:
        // Override parameters for this algorithm
        double getCoalitionDistance() const override { return 2.0; }
        unsigned int getClusterMinPoints() const override { return 3u; }
        double getClusterRatioSplitThreshold() const override { return 0.25; }

        // CTA2 uses weighting
        double getVarianceWeight() const override { return 0.3; }
        double getRssiWeight() const override { return 0.1; }
        double getBottomRssi() const override { return -90.0; }
        double getExtraWeight() const override { return 1.0; }

        // Implement clustering
        void clusterData() override;

    private:
        // CTA2-specific methods
        void bruteForceSearch(double &out_x, double &out_y, double precision, double timeout);

        // Brute force search parameters
        static constexpr int HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS = 500; // TODO: should this also be a method?
    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM2_H
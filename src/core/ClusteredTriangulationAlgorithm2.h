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
        void findBestClusters();
        void getCandidates(int i, std::vector<int> &candidate_indices);
        bool checkCluster(PointCluster &cluster, PointCluster &best_cluster, double &best_score, int &best_seed_index, int i);

        static constexpr int MAX_CLUSTER_POINTS = 10;
        static constexpr double CLUSTER_FORMATION_TIMEOUT = 20.0; // seconds

        // CTA2-specific constants
        static constexpr int HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS = 500; // TODO: should this also be a method?

        // Brute-force clustering thresholds
        static constexpr int MAX_INTERNAL_CLUSTER_DISTANCE = 20; // Meters

        // Geometric ratio
        static constexpr double MIN_GEOMETRIC_RATIO_FOR_BEST_CLUSTER = 0.15;  // Prefer clusters closer to square
        static constexpr double IDEAL_GEOMETRIC_RATIO_FOR_BEST_CLUSTER = 1.0; // Perfect square

        // Area
        static constexpr double MIN_AREA_FOR_BEST_CLUSTER = 10.0;   // Square meters
        static constexpr double IDEAL_AREA_FOR_BEST_CLUSTER = 50.0; // Square meters
        static constexpr double MAX_AREA_FOR_BEST_CLUSTER = 1000.0; // Square meters

        // RSSI variance
        static constexpr double MIN_RSSI_VARIANCE_FOR_BEST_CLUSTER = 5.0; // (dBm)^2

        // Overlap
        static constexpr double MAX_OVERLAP_BETWEEN_CLUSTERS = 0.05; // 5%

        // Weights for best cluster selection
        static constexpr double WEIGHT_GEOMETRIC_RATIO = 1;
        static constexpr double WEIGHT_AREA = 1;
        static constexpr double WEIGHT_RSSI_VARIANCE = 1;
    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM2_H
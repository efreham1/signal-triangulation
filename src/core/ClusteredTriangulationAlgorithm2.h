#ifndef CLUSTERED_TRIANGULATION_ALGORITHM2_H
#define CLUSTERED_TRIANGULATION_ALGORITHM2_H

#include "ClusteredTriangulationBase.h"
#include <mutex>
#include <atomic>

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
        bool checkCluster(PointCluster &cluster, PointCluster &best_cluster, double &best_score);
        void logPerformanceSummary();

        // Performance counters (thread-safe versions)
        std::atomic<size_t> m_combinations_explored{0};
        std::atomic<size_t> m_clusters_evaluated{0};
        double m_clustering_time_ms = 0.0;

        // Per-seed metrics
        std::vector<size_t> m_combinations_per_seed;
        std::vector<double> m_time_per_seed_ms;
        std::vector<int> m_candidates_per_seed;
        std::vector<bool> m_seed_timed_out;

        // Mutex for thread-safe cluster insertion
        std::mutex m_clusters_mutex;

        static constexpr double PER_SEED_TIMEOUT = 1.0; // seconds

        // CTA2-specific constants
        static constexpr int HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS = 500; // 500x500 grid per precision step

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

        // RSSI strength
        static constexpr double BOTTOM_RSSI_FOR_BEST_CLUSTER = -90.0; // dBm

        // Weights for best cluster selection
        static constexpr double WEIGHT_GEOMETRIC_RATIO = 1;
        static constexpr double WEIGHT_AREA = 1;
        static constexpr double WEIGHT_RSSI_VARIANCE = 1;
        static constexpr double WEIGHT_RSSI = 1;
    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM2_H
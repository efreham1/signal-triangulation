#ifndef CLUSTERED_TRIANGULATION_ALGORITHM2_H
#define CLUSTERED_TRIANGULATION_ALGORITHM2_H

#include "ClusteredTriangulationBase.h"
#include "AlgorithmParameters.h"
#include <atomic>

namespace core
{

    class ClusteredTriangulationAlgorithm2 : public ClusteredTriangulationBase
    {
    public:
        ClusteredTriangulationAlgorithm2();
        explicit ClusteredTriangulationAlgorithm2(const AlgorithmParameters &params);
        ~ClusteredTriangulationAlgorithm2() override;

        void calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout) override;

    private:
        void clusterData();
        void applyParameters(const AlgorithmParameters &params);
        void bruteForceSearch(double &out_x, double &out_y, double precision, double timeout);
        void findBestClusters();
        void getCandidates(int i, std::vector<int> &candidate_indices);
        bool checkCluster(PointCluster &cluster, PointCluster &best_cluster, double &best_score);
        void logPerformanceSummary();

        // Performance counters
        std::atomic<size_t> m_combinations_explored{0};
        std::atomic<size_t> m_clusters_evaluated{0};
        double m_clustering_time_ms = 0.0;
        std::vector<size_t> m_combinations_per_seed;
        std::vector<double> m_time_per_seed_ms;
        std::vector<int> m_candidates_per_seed;
        std::vector<bool> m_seed_timed_out;

        // Timing
        double m_per_seed_timeout = 5.0;

        // Grid search
        int m_grid_half_size = 500;

        // Clustering
        double m_coalition_distance = 2.0;
        unsigned int m_cluster_min_points = 3;
        int m_max_internal_distance = 20;

        // Geometric ratio
        double m_min_geometric_ratio = 0.15;
        double m_ideal_geometric_ratio = 1.0;

        // Area
        double m_min_area = 10.0;
        double m_ideal_area = 50.0;
        double m_max_area = 1000.0;

        // RSSI
        double m_min_rssi_variance = 5.0;
        double m_bottom_rssi = -90.0;

        // Overlap
        double m_max_overlap = 0.05;

        // Weights
        double m_weight_geometric_ratio = 1.0;
        double m_weight_area = 1.0;
        double m_weight_rssi_variance = 1.0;
        double m_weight_rssi = 1.0;
        double m_extra_weight = 1.0;
        double m_angle_weight = 10.0;
    };

} // namespace core

#endif // CLUSTERED_TRIANGULATION_ALGORITHM2_H
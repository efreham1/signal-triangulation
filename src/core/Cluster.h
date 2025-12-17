#ifndef CLUSTER_H
#define CLUSTER_H

#include "DataPoint.h"

#include <vector>
#include <cstdint>

namespace core
{
    /**
     * @brief Compact bit vector for tracking point membership in clusters.
     * Uses 64-bit words for efficient bitwise operations (overlap via AND + popcount).
     */
    class BitVector
    {
    public:
        BitVector() = default;
        
        void setBit(size_t index);
        void clearBit(size_t index);
        bool getBit(size_t index) const;
        void clear();
        
        /**
         * @brief Pre-allocate capacity for n_points indices.
         * @param n_points Total number of points to reserve space for
         */
        void reserve(size_t n_points);
        
        size_t popcount() const;
        size_t sharedCount(const BitVector &other) const;
        
        /**
         * @brief Get all set bit indices for iteration.
         * @return Vector of indices where bits are set
         */
        std::vector<int> getSetIndices() const;
        
        /**
         * @brief Copy from another BitVector
         */
        void copyFrom(const BitVector &other);
        
    private:
        std::vector<uint64_t> m_words;
        
        void ensureCapacity(size_t index);
    };

    class PointCluster
    {
    public:
    /**
     * @brief Bounding box in principal axis coordinate system.
     * The principal axis is defined by the two furthest points in the cluster.
     */
    struct BoundingBox
    {
        double range_u; // Range along principal axis (longest dimension)
        double range_v; // Range along perpendicular axis
        bool valid;     // Whether computation succeeded
    };
        std::vector<DataPoint> points;
        std::vector<double> x_dp_values;
        std::vector<double> y_dp_values;
        std::vector<double> rssi_values;
        BitVector point_bits;  // Bit vector for point membership (fast overlap via AND + popcount)
        size_t num_points;
        double estimated_aoa;
        double avg_rssi;
        double centroid_x;
        double centroid_y;
        double aoa_x;
        double aoa_y;
        double score;
        BoundingBox bbox;
        size_t furthest_idx1;
        size_t furthest_idx2;
        double furthest_distance;


        PointCluster();
        PointCluster(size_t num_points);
        ~PointCluster() = default;

        void addPoint(const DataPoint &point);
        void removePoint(const DataPoint &point);
        double overlapWith(const PointCluster &other) const;
        double varianceRSSI();
        size_t size() const;
        void setScore(double input_score);

        double getAndSetScore(double ideal_geometric_ratio, double min_geometric_ratio, double max_geometric_ratio,
                double ideal_area, double min_area, double max_area,
                double ideal_rssi_variance, double min_rssi_variance, double max_rssi_variance,
                double gr_weight, double area_weight, double variance_weight,
                double bottom_rssi_threshold, double top_rssi, double rssi_weight);

        /**
         * @brief Ratio of perpendicular range to principal range.
         * A value close to 1.0 means the cluster is roughly square.
         * A value close to 0.0 means the cluster is elongated.
         * @return Geometric ratio (range_v / range_u), or 0.0 if invalid
         */
        double geometricRatio() const;

        /**
         * @brief Area of bounding box in principal axis coordinate system.
         * @return Area in square units, or 0.0 if invalid
         */
        double area() const;

        void addPointVectorized(const DataPoint &point, size_t index);
        void removePointVectorized(size_t cluster_index, size_t points_index);
        
        PointCluster copyVectorizedToNormal(std::vector<DataPoint> &all_points);
        
        PointCluster copyVectorizedToVectorized();
        
        /**
         * @brief Get indices of all points in the cluster (for iteration).
         * @return Vector of point indices
         */
        std::vector<int> getPointIndices() const;
        
    private:
        bool vectorized;
        bool rssi_variance_computed;
        double rssi_variance_value;
        void recomputeBoundingBox(size_t new_idx);
        void computeBoundingBox();
    };

} // namespace core

#endif // CLUSTER_H
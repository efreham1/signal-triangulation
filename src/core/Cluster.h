#ifndef CLUSTER_H
#define CLUSTER_H

#include "DataPoint.h"

#include <vector>

namespace core
{
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
        std::vector<int> point_idxs;
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
        ~PointCluster() = default;

        void addPoint(const DataPoint &point);
        void removePoint(const DataPoint &point);
        double overlapWith(const PointCluster &other) const;
        double varianceRSSI() const;
        size_t size() const;
        void setScore(double input_score);
        double getAndSetScore(double ideal_geometric_ratio, double ideal_area,
                        double ideal_rssi_variance, double gr_weight, double area_weight,
                        double variance_weight, double bottom_rssi_threshold, double rssi_weight);

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

        void addPointVectorized(const DataPoint &point, int index);
        void removePointVectorized(size_t index);
    private:
        bool currently_vectorized = false;
        void recomputeBoundingBox(size_t new_idx);
        void computeBoundingBox();
    };

} // namespace core

#endif // CLUSTER_H
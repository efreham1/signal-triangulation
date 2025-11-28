#ifndef CLUSTER_H
#define CLUSTER_H

#include "DataPoint.h"

#include <vector>

namespace core
{
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
        double score;

        PointCluster();
        ~PointCluster() = default;

        void addPoint(const DataPoint &point);
        void removePoint(const DataPoint &point);
        double overlapWith(const PointCluster &other) const;
        double varianceRSSI() const;
        size_t size() const;
        double getAndSetScore(double ideal_geometric_ratio, double ideal_area,
                        double ideal_rssi_variance, double gr_weight, double area_weight,
                        double variance_weight, double bottom_rssi_threshold, double rssi_weight);

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

        /**
         * @brief Compute bounding box aligned to principal axes.
         * Principal axis is the line connecting the two furthest points.
         * @return BoundingBox with range_u (principal) and range_v (perpendicular)
         */
        BoundingBox computePrincipalBoundingBox() const;

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
    };

} // namespace core

#endif // CLUSTER_HPP
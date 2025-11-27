#ifndef CLUSTER_HPP
#define CLUSTER_HPP

#include "DataPoint.h"

#include <limits>
#include <utility>
#include <vector>
#include <cmath>
#include <spdlog/spdlog.h>

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

        PointCluster()
        {
            avg_rssi = 0.0;
            estimated_aoa = 0.0;
            centroid_x = 0.0;
            centroid_y = 0.0;
            aoa_x = 0.0;
            aoa_y = 0.0;
        }
        ~PointCluster() = default;

        void addPoint(const DataPoint &point)
        {
            points.push_back(point);

            // Update average RSSI
            double previous_total = avg_rssi * static_cast<double>(points.size() - 1);
            avg_rssi = (previous_total + point.rssi) / static_cast<double>(points.size());

            // Update centroid
            double previous_total_x = centroid_x * static_cast<double>(points.size() - 1);
            double previous_total_y = centroid_y * static_cast<double>(points.size() - 1);
            centroid_x = (previous_total_x + point.getX()) / static_cast<double>(points.size());
            centroid_y = (previous_total_y + point.getY()) / static_cast<double>(points.size());

            spdlog::debug("PointCluster: added point (x={}, y={}, rssi={}), new centroid (x={}, y={}), avg_rssi={}", point.getX(), point.getY(), point.rssi, centroid_x, centroid_y, avg_rssi);
        }

        double geometricRatio() const
        {
            std::pair<int, int> points_furthest_between;
            double furthest_distance = 0.0;

            for (unsigned int i = 0; i < static_cast<unsigned int>(points.size()); ++i)
            {
                for (unsigned int j = i + 1; j < static_cast<unsigned int>(points.size()); ++j)
                {
                    double d = (points[i].getX() - points[j].getX()) * (points[i].getX() - points[j].getX()) +
                               (points[i].getY() - points[j].getY()) * (points[i].getY() - points[j].getY());
                    if (d > furthest_distance)
                    {
                        furthest_distance = d;
                        points_furthest_between = std::make_pair(i, j);
                        spdlog::debug("PointCluster: updated furthest distance to {} between points {} and {}", furthest_distance, i, j);
                    }
                }
            }

            if (furthest_distance == 0.0 || points.size() < 3)
            {
                return 0.0;
            }
            double ratio = 0.0;

            // Validate furthest-point indices; if invalid, recompute them.
            int idx1 = points_furthest_between.first;
            int idx2 = points_furthest_between.second;
            if (idx1 < 0 || idx2 < 0 || idx1 >= static_cast<int>(points.size()) || idx2 >= static_cast<int>(points.size()))
            {
                spdlog::debug("PointCluster: furthest point indices invalid ({} , {}), recomputing", idx1, idx2);
                double best_d = 0.0;
                for (int i = 0; i < static_cast<int>(points.size()); ++i)
                {
                    for (int j = i + 1; j < static_cast<int>(points.size()); ++j)
                    {
                        double dx = points[i].getX() - points[j].getX();
                        double dy = points[i].getY() - points[j].getY();
                        double d = dx * dx + dy * dy;
                        if (d > best_d)
                        {
                            best_d = d;
                            idx1 = i;
                            idx2 = j;
                        }
                    }
                }
                furthest_distance = best_d;
                points_furthest_between = std::make_pair(idx1, idx2);
                spdlog::debug("PointCluster: recomputed furthest distance to {} between points {} and {}", furthest_distance, idx1, idx2);
            }

            // Get unit vector along the line between the two furthest points
            double x1 = points[idx1].getX();
            double y1 = points[idx1].getY();
            double x2 = points[idx2].getX();
            double y2 = points[idx2].getY();
            double ux = x2 - x1;
            double uy = y2 - y1;
            double un = std::sqrt(ux * ux + uy * uy);
            if (un == 0.0)
            {
                return 0.0;
            }
            ux /= un;
            uy /= un;

            // Perpendicular unit vector
            double vx = -uy;
            double vy = ux;

            spdlog::debug("PointCluster: furthest points are {} (x={}, y={}) and {} (x={}, y={}), unit vector ({}, {}), perpendicular ({}, {})", idx1, x1, y1, idx2, x2, y2, ux, uy, vx, vy);

            // Project points into this coordinate system (origin at centroid)
            double min_u = std::numeric_limits<double>::infinity();
            double max_u = -std::numeric_limits<double>::infinity();
            double min_v = std::numeric_limits<double>::infinity();
            double max_v = -std::numeric_limits<double>::infinity();

            for (const auto &p : points)
            {
                double dx = p.getX() - centroid_x;
                double dy = p.getY() - centroid_y;
                double pu = dx * ux + dy * uy;
                double pv = dx * vx + dy * vy;
                if (pu < min_u)
                    min_u = pu;
                if (pu > max_u)
                    max_u = pu;
                if (pv < min_v)
                    min_v = pv;
                if (pv > max_v)
                    max_v = pv;
            }

            double range_u = max_u - min_u;
            double range_v = max_v - min_v;
            ratio = range_v / range_u;
            return ratio;
        }

        double varianceRSSI() const
        {
            if (points.size() < 2)
            {
                return 0.0;
            }
            double mean = avg_rssi;
            double sum_sq_diff = 0.0;
            for (const auto &p : points)
            {
                double diff = p.rssi - mean;
                sum_sq_diff += diff * diff;
            }
            return sum_sq_diff / static_cast<double>(points.size() - 1);
        }

        double getWeight(double variance_weight, double rssi_weight, double bottom_rssi) const
        {
            double variance_component = varianceRSSI();
            
            if (avg_rssi < bottom_rssi)
            {
                return variance_weight * variance_component;
            }
            
            return variance_weight * variance_component - (bottom_rssi-avg_rssi) * rssi_weight;
        }
    };

} // namespace core

#endif // CLUSTER_HPP
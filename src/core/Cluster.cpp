#include "Cluster.h"

#include <cmath>
#include <limits>
#include <spdlog/spdlog.h>

namespace core
{

    PointCluster::PointCluster()
        : estimated_aoa(0.0), avg_rssi(0.0), centroid_x(0.0), centroid_y(0.0), aoa_x(0.0), aoa_y(0.0)
    {
    }

    void PointCluster::addPoint(const DataPoint &point)
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

        spdlog::debug("PointCluster: added point (x={}, y={}, rssi={}), new centroid (x={}, y={}), avg_rssi={}",
                      point.getX(), point.getY(), point.rssi, centroid_x, centroid_y, avg_rssi);
    }

    double PointCluster::overlapWith(const PointCluster &other) const
    {
        size_t total_points = points.size() + other.points.size();
        if (total_points == 0)
        {
            return 0.0;
        }

        size_t shared_points = 0;
        for (const auto &p1 : points)
        {
            for (const auto &p2 : other.points)
            {
                if (p1.point_id == p2.point_id)
                {
                    shared_points++;
                    break;
                }
            }
        }

        return static_cast<double>(shared_points) / static_cast<double>(total_points);
    }

    double PointCluster::varianceRSSI() const
    {
        if (points.size() < 2)
        {
            return 0.0;
        }

        double sum_sq_diff = 0.0;
        for (const auto &p : points)
        {
            double diff = p.rssi - avg_rssi;
            sum_sq_diff += diff * diff;
        }

        return sum_sq_diff / static_cast<double>(points.size() - 1);
    }

    double PointCluster::getWeight(double variance_weight, double rssi_weight, double bottom_rssi) const
    {
        double variance_component = varianceRSSI();

        if (avg_rssi < bottom_rssi)
        {
            return variance_weight * variance_component;
        }

        return variance_weight * variance_component - (bottom_rssi - avg_rssi) * rssi_weight;
    }

    PointCluster::BoundingBox PointCluster::computePrincipalBoundingBox() const
    {
        BoundingBox bbox = {0.0, 0.0, false};

        if (points.size() < 3)
        {
            return bbox;
        }

        // Find the two furthest points
        int idx1 = 0, idx2 = 1;
        double furthest_distance = 0.0;

        for (size_t i = 0; i < points.size(); ++i)
        {
            for (size_t j = i + 1; j < points.size(); ++j)
            {
                double dx = points[i].getX() - points[j].getX();
                double dy = points[i].getY() - points[j].getY();
                double d = dx * dx + dy * dy;
                if (d > furthest_distance)
                {
                    furthest_distance = d;
                    idx1 = static_cast<int>(i);
                    idx2 = static_cast<int>(j);
                }
            }
        }

        if (furthest_distance == 0.0)
        {
            return bbox;
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
            return bbox;
        }

        ux /= un;
        uy /= un;

        // Perpendicular unit vector
        double vx = -uy;
        double vy = ux;

        spdlog::debug("PointCluster: furthest points are {} (x={}, y={}) and {} (x={}, y={}), "
                      "unit vector ({}, {}), perpendicular ({}, {})",
                      idx1, x1, y1, idx2, x2, y2, ux, uy, vx, vy);

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
            min_u = std::min(min_u, pu);
            max_u = std::max(max_u, pu);
            min_v = std::min(min_v, pv);
            max_v = std::max(max_v, pv);
        }

        bbox.range_u = max_u - min_u;
        bbox.range_v = max_v - min_v;
        bbox.valid = true;

        return bbox;
    }

    double PointCluster::geometricRatio() const
    {
        BoundingBox bbox = computePrincipalBoundingBox();

        if (!bbox.valid || bbox.range_u == 0.0)
        {
            return 0.0;
        }

        return bbox.range_v / bbox.range_u;
    }

    double PointCluster::area() const
    {
        BoundingBox bbox = computePrincipalBoundingBox();

        if (!bbox.valid)
        {
            return 0.0;
        }

        return bbox.range_u * bbox.range_v;
    }

} // namespace core
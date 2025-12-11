#include "Cluster.h"
#include "PointDistanceCache.hpp"

#include <cmath>
#include <limits>
#include <spdlog/spdlog.h>

namespace core
{

    PointCluster::PointCluster()
        : estimated_aoa(0.0), avg_rssi(0.0), centroid_x(0.0), centroid_y(0.0), aoa_x(0.0), aoa_y(0.0), score(0.0)
    {
    }

    void PointCluster::addPoint(const DataPoint &point)
    {
        if (currently_vectorized)
        {
            throw std::runtime_error("PointCluster: cannot add non-vectorized point to a vectorized cluster");
        }
        points.push_back(point);

        // Update average RSSI
        double previous_total = avg_rssi * static_cast<double>(points.size() - 1);
        avg_rssi = (previous_total + point.rssi) / static_cast<double>(points.size());

        // Update centroid
        double previous_total_x = centroid_x * static_cast<double>(points.size() - 1);
        double previous_total_y = centroid_y * static_cast<double>(points.size() - 1);
        centroid_x = (previous_total_x + point.getXUnsafe()) / static_cast<double>(points.size());
        centroid_y = (previous_total_y + point.getYUnsafe()) / static_cast<double>(points.size());

        spdlog::debug("PointCluster: added point (x={}, y={}, rssi={}), new centroid (x={}, y={}), avg_rssi={}",
                      point.getXUnsafe(), point.getYUnsafe(), point.rssi, centroid_x, centroid_y, avg_rssi);
        
        recomputeBoundingBox(points.size() - 1);
    }

    void PointCluster::addPointVectorized(const DataPoint &point, int index)
    {
        currently_vectorized = true;
        x_dp_values.push_back(point.getXUnsafe());
        y_dp_values.push_back(point.getYUnsafe());
        rssi_values.push_back(static_cast<double>(point.rssi));
        point_idxs.push_back(index);

        //update average RSSI
        double previous_total = avg_rssi * static_cast<double>(rssi_values.size() - 1);
        avg_rssi = (previous_total + static_cast<double>(point.rssi)) / static_cast<double>(rssi_values.size());

        //update centroid
        double previous_total_x = centroid_x * static_cast<double>(x_dp_values.size() - 1);
        double previous_total_y = centroid_y * static_cast<double>(y_dp_values.size() - 1);
        centroid_x = (previous_total_x + point.getXUnsafe()) / static_cast<double>(x_dp_values.size());
        centroid_y = (previous_total_y + point.getYUnsafe()) / static_cast<double>(y_dp_values.size());

        recomputeBoundingBox(x_dp_values.size() - 1);
    }

    void PointCluster::removePoint(const DataPoint &point)
    {
        if (currently_vectorized)
        {
            throw std::runtime_error("PointCluster: cannot remove non-vectorized point from a vectorized cluster");
        }
        
        auto it = std::find_if(points.begin(), points.end(),
                               [&point](const DataPoint &p) { return p.point_id == point.point_id; });

        if (it != points.end())
        {
            size_t idx = std::distance(points.begin(), it);
            points.erase(it);

            // Recalculate average RSSI
            if (!points.empty())
            {
                double total_rssi = 0.0;
                double total_x = 0.0;
                double total_y = 0.0;

                for (const auto &p : points)
                {
                    total_rssi += p.rssi;
                    total_x += p.getXUnsafe();
                    total_y += p.getYUnsafe();
                }

                avg_rssi = total_rssi / static_cast<double>(points.size());
                centroid_x = total_x / static_cast<double>(points.size());
                centroid_y = total_y / static_cast<double>(points.size());
            }
            else
            {
                avg_rssi = 0.0;
                centroid_x = 0.0;
                centroid_y = 0.0;
            }

            spdlog::debug("PointCluster: removed point (id={}), new centroid (x={}, y={}), avg_rssi={}",
                          point.point_id, centroid_x, centroid_y, avg_rssi);

            if (idx == furthest_idx1 || idx == furthest_idx2)
            {
                computeBoundingBox();
            }
        }
    }

    void PointCluster::removePointVectorized(size_t index)
    {
        if (x_dp_values.empty())
        {
            throw std::runtime_error("PointCluster: removePointVectorized called on empty cluster");
        }
        
        if (!currently_vectorized)
        {
            throw std::runtime_error("PointCluster: removePointVectorized called on non-vectorized cluster");
        }

        if (index >= x_dp_values.size())
        {
            throw std::out_of_range("PointCluster: removePointVectorized index out of range");
        }

        x_dp_values.erase(x_dp_values.begin() + index);
        y_dp_values.erase(y_dp_values.begin() + index);
        rssi_values.erase(rssi_values.begin() + index);
        point_idxs.erase(point_idxs.begin() + index);

        // Recalculate average RSSI
        if (!rssi_values.empty())
        {
            double total_rssi = 0.0;
            double total_x = 0.0;
            double total_y = 0.0;

            for (size_t i = 0; i < rssi_values.size(); ++i)
            {
                total_rssi += rssi_values[i];
                total_x += x_dp_values[i];
                total_y += y_dp_values[i];
            }

            avg_rssi = total_rssi / static_cast<double>(rssi_values.size());
            centroid_x = total_x / static_cast<double>(x_dp_values.size());
            centroid_y = total_y / static_cast<double>(y_dp_values.size());
        }
        else
        {
            avg_rssi = 0.0;
            centroid_x = 0.0;
            centroid_y = 0.0;
        }

        if (index == furthest_idx1 || index == furthest_idx2)
        {
            computeBoundingBox();
        }
    }

    double PointCluster::overlapWith(const PointCluster &other) const
    {
        size_t total_points = points.size() + other.points.size();
        if (total_points == 0)
        {
            return 0.0;
        }

        if (currently_vectorized && other.currently_vectorized)
        {
            size_t shared_points = 0;
            for (const auto &idx1 : point_idxs)
            {
                for (const auto &idx2 : other.point_idxs)
                {
                    if (idx1 == idx2)
                    {
                        shared_points++;
                        break;
                    }
                }
            }

            return static_cast<double>(shared_points) / static_cast<double>(total_points);
        }
        else if (!currently_vectorized && !other.currently_vectorized)
        {
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
        else
        {
            throw std::runtime_error("PointCluster: overlapWith called on mixed vectorized/non-vectorized clusters");
        }
    }

    double PointCluster::varianceRSSI() const
    {
        if (points.size() < 2)
        {
            return 0.0;
        }

        if (currently_vectorized)
        {
            double sum_sq_diff = 0.0;
            for (const auto &rssi_val : rssi_values)
            {
                double diff = rssi_val - avg_rssi;
                sum_sq_diff += diff * diff;
            }
            return sum_sq_diff / static_cast<double>(rssi_values.size() - 1);
        }

        double sum_sq_diff = 0.0;
        for (const auto &p : points)
        {
            double diff = p.rssi - avg_rssi;
            sum_sq_diff += diff * diff;
        }

        return sum_sq_diff / static_cast<double>(points.size() - 1);
    }

    void PointCluster::setScore(double input_score)
    {
        this->score = input_score;
    }

    double PointCluster::getAndSetScore(double ideal_geometric_ratio, double ideal_area,
                                    double ideal_rssi_variance, double gr_weight, double area_weight,
                                    double variance_weight, double bottom_rssi_threshold, double rssi_weight)
    {
        double gr_score = 1.0 - std::abs(1.0 - geometricRatio() / ideal_geometric_ratio);
        double area_score = 1.0 - std::abs(1.0 - area() / ideal_area);
        double variance_score = 1.0 - std::abs(1.0 - varianceRSSI() / ideal_rssi_variance);
        double rssi_score = 0.0;
        if (avg_rssi > bottom_rssi_threshold)
        {
            rssi_score = 1.0 - (avg_rssi / bottom_rssi_threshold);
        }
        
        score = gr_weight * gr_score + area_weight * area_score + variance_weight * variance_score + rssi_weight * rssi_score;
        return score;
    }

    size_t PointCluster::size() const
    {
        return points.size();
    }

    void PointCluster::recomputeBoundingBox(size_t new_idx)
    {

        if (points.size() < 3)
        {
            furthest_distance = 0.0;
            furthest_idx1 = 0;
            furthest_idx2 = 0;
            bbox.valid = false;
            return;
        }

        double sqrdist = furthest_distance*furthest_distance;
        size_t idx1 = furthest_idx1;;
        size_t idx2 = furthest_idx2;

        if (currently_vectorized)
        {
            for (size_t j = 0; j < x_dp_values.size(); ++j)
            {
                if (j == new_idx)
                {
                    continue;
                }

                double dx = x_dp_values[new_idx] - x_dp_values[j];
                double dy = y_dp_values[new_idx] - y_dp_values[j];
                double d = dx * dx + dy * dy;
                if (d > sqrdist)
                {
                    sqrdist = d;
                    idx1 = new_idx;
                    idx2 = j;
                }
            }
        }
        else
        {

            for (size_t j = 0; j < points.size(); ++j)
            {
                if (j == new_idx)
                {
                    continue;
                }
                
                double dx = points[new_idx].getXUnsafe() - points[j].getXUnsafe();
                double dy = points[new_idx].getYUnsafe() - points[j].getYUnsafe();
                double d = dx * dx + dy * dy;
                if (d > sqrdist)
                {
                    sqrdist = d;
                    idx1 = new_idx;
                    idx2 = j;
                }
            }
        }

        furthest_distance = std::sqrt(sqrdist);
        furthest_idx1 = idx1;
        furthest_idx2 = idx2;

        if (furthest_distance == 0.0)
        {
            bbox.valid = false;
            return;
        }

        double x1;
        double y1;
        double x2;
        double y2;

        // Get unit vector along the line between the two furthest points
        if (currently_vectorized)
        {
            x1 = x_dp_values[furthest_idx1];
            y1 = y_dp_values[furthest_idx1];
            x2 = x_dp_values[furthest_idx2];
            y2 = y_dp_values[furthest_idx2];
        }
        else
        {
            x1 = points[furthest_idx1].getXUnsafe();
            y1 = points[furthest_idx1].getYUnsafe();
            x2 = points[furthest_idx2].getXUnsafe();
            y2 = points[furthest_idx2].getYUnsafe();
        }

        double ux = x2 - x1;
        double uy = y2 - y1;
        double un = furthest_distance;

        ux /= un;
        uy /= un;

        // Perpendicular unit vector
        double vx = -uy;
        double vy = ux;

        // Project points into this coordinate system (origin at centroid)
        double min_u = std::numeric_limits<double>::infinity();
        double max_u = -std::numeric_limits<double>::infinity();
        double min_v = std::numeric_limits<double>::infinity();
        double max_v = -std::numeric_limits<double>::infinity();
        if (currently_vectorized)
        {
            for (size_t i = 0; i < x_dp_values.size(); ++i)
            {
                double dx = x_dp_values[i] - centroid_x;
                double dy = y_dp_values[i] - centroid_y;
                double pu = dx * ux + dy * uy;
                double pv = dx * vx + dy * vy;
                min_u = std::min(min_u, pu);
                max_u = std::max(max_u, pu);
                min_v = std::min(min_v, pv);
                max_v = std::max(max_v, pv);
            }
        }
        else
        {

            for (const auto &p : points)
            {
                double dx = p.getXUnsafe() - centroid_x;
                double dy = p.getYUnsafe() - centroid_y;
                double pu = dx * ux + dy * uy;
                double pv = dx * vx + dy * vy;
                min_u = std::min(min_u, pu);
                max_u = std::max(max_u, pu);
                min_v = std::min(min_v, pv);
                max_v = std::max(max_v, pv);
            }
        }

        bbox.range_u = max_u - min_u;
        bbox.range_v = max_v - min_v;
        bbox.valid = true;
    }

    void PointCluster::computeBoundingBox()
    {
        if (points.size() < 3)
        {
            furthest_distance = 0.0;
            furthest_idx1 = 0;
            furthest_idx2 = 0;
            bbox.valid = false;
            return;
        }
        
        double sqrdist = 0.0;
        size_t idx1 = 0;
        size_t idx2 = 0;

        if (currently_vectorized)
        {
            for (size_t i = 0; i < x_dp_values.size(); ++i)
            {
                for (size_t j = i + 1; j < x_dp_values.size(); ++j)
                {
                    double dx = x_dp_values[i] - x_dp_values[j];
                    double dy = y_dp_values[i] - y_dp_values[j];
                    double d = dx * dx + dy * dy;
                    if (d > sqrdist)
                    {
                        sqrdist = d;
                        idx1 = i;
                        idx2 = j;
                    }
                }
            }
        }
        else
        {
            for (size_t i = 0; i < points.size(); ++i)
            {
                for (size_t j = i + 1; j < points.size(); ++j)
                {
                    double dx = points[i].getXUnsafe() - points[j].getXUnsafe();
                    double dy = points[i].getYUnsafe() - points[j].getYUnsafe();
                    double d = dx * dx + dy * dy;
                    if (d > sqrdist)
                    {
                        sqrdist = d;
                        idx1 = i;
                        idx2 = j;
                    }
                }
            }
        }

        furthest_distance = std::sqrt(sqrdist);
        furthest_idx1 = idx1;
        furthest_idx2 = idx2;

        if (furthest_distance == 0.0)
        {
            bbox.valid = false;
            return;
        }

        double x1;
        double y1;
        double x2;
        double y2;

        // Get unit vector along the line between the two furthest points
        if (currently_vectorized)
        {
            x1 = x_dp_values[furthest_idx1];
            y1 = y_dp_values[furthest_idx1];
            x2 = x_dp_values[furthest_idx2];
            y2 = y_dp_values[furthest_idx2];
        }
        else
        {
            x1 = points[furthest_idx1].getXUnsafe();
            y1 = points[furthest_idx1].getYUnsafe();
            x2 = points[furthest_idx2].getXUnsafe();
            y2 = points[furthest_idx2].getYUnsafe();
        }

        double ux = x2 - x1;
        double uy = y2 - y1;
        double un = furthest_distance;

        ux /= un;
        uy /= un;

        // Perpendicular unit vector
        double vx = -uy;
        double vy = ux;

        // Project points into this coordinate system (origin at centroid)
        double min_u = std::numeric_limits<double>::infinity();
        double max_u = -std::numeric_limits<double>::infinity();
        double min_v = std::numeric_limits<double>::infinity();
        double max_v = -std::numeric_limits<double>::infinity();
        if (currently_vectorized)
        {
            for (size_t i = 0; i < x_dp_values.size(); ++i)
            {
                double dx = x_dp_values[i] - centroid_x;
                double dy = y_dp_values[i] - centroid_y;
                double pu = dx * ux + dy * uy;
                double pv = dx * vx + dy * vy;
                min_u = std::min(min_u, pu);
                max_u = std::max(max_u, pu);
                min_v = std::min(min_v, pv);
                max_v = std::max(max_v, pv);
            }
        }
        else
        {

            for (const auto &p : points)
            {
                double dx = p.getXUnsafe() - centroid_x;
                double dy = p.getYUnsafe() - centroid_y;
                double pu = dx * ux + dy * uy;
                double pv = dx * vx + dy * vy;
                min_u = std::min(min_u, pu);
                max_u = std::max(max_u, pu);
                min_v = std::min(min_v, pv);
                max_v = std::max(max_v, pv);
            }
        }

        bbox.range_u = max_u - min_u;
        bbox.range_v = max_v - min_v;
        bbox.valid = true;

    }

    double PointCluster::geometricRatio() const
    {
        if (!bbox.valid || bbox.range_u == 0.0)
        {
            return 0.0;
        }

        return bbox.range_v / bbox.range_u;
    }

    double PointCluster::area() const
    {
        if (!bbox.valid)
        {
            return 0.0;
        }

        return bbox.range_u * bbox.range_v;
    }

} // namespace core
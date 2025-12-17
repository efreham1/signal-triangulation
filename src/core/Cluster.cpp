#include "Cluster.h"
#include "PointDistanceCache.hpp"

#include <cmath>
#include <limits>
#include <spdlog/spdlog.h>

namespace core
{
    // ==================== BitVector Implementation ====================

    void BitVector::ensureCapacity(size_t index)
    {
        size_t word_idx = index / 64;
        if (word_idx >= m_words.size())
        {
            m_words.resize(word_idx + 1, 0);
        }
    }

    void BitVector::setBit(size_t index)
    {
        ensureCapacity(index);
        size_t word_idx = index / 64;
        size_t bit_idx = index % 64;
        m_words[word_idx] |= (1ULL << bit_idx);
    }

    void BitVector::clearBit(size_t index)
    {
        size_t word_idx = index / 64;
        if (word_idx < m_words.size())
        {
            size_t bit_idx = index % 64;
            m_words[word_idx] &= ~(1ULL << bit_idx);
        }
    }

    bool BitVector::getBit(size_t index) const
    {
        size_t word_idx = index / 64;
        if (word_idx >= m_words.size())
        {
            return false;
        }
        size_t bit_idx = index % 64;
        return (m_words[word_idx] & (1ULL << bit_idx)) != 0;
    }

    void BitVector::clear()
    {
        m_words.clear();
    }

    void BitVector::reserve(size_t n_points)
    {
        size_t n_words = (n_points + 63) / 64;
        m_words.resize(n_words, 0);
    }

    size_t BitVector::popcount() const
    {
        size_t count = 0;
        for (uint64_t word : m_words)
        {
            count += __builtin_popcountll(word);
        }
        return count;
    }

    size_t BitVector::sharedCount(const BitVector &other) const
    {
        size_t count = 0;
        size_t min_size = std::min(m_words.size(), other.m_words.size());
        for (size_t i = 0; i < min_size; ++i)
        {
            count += __builtin_popcountll(m_words[i] & other.m_words[i]);
        }
        return count;
    }

    std::vector<int> BitVector::getSetIndices() const
    {
        std::vector<int> indices;
        for (size_t word_idx = 0; word_idx < m_words.size(); ++word_idx)
        {
            uint64_t word = m_words[word_idx];
            while (word != 0)
            {
                int bit_pos = __builtin_ctzll(word); // Count trailing zeros
                indices.push_back(static_cast<int>(word_idx * 64 + bit_pos));
                word &= (word - 1); // Clear lowest set bit
            }
        }
        return indices;
    }

    void BitVector::copyFrom(const BitVector &other)
    {
        m_words = other.m_words;
    }

    // ==================== PointCluster Implementation ====================

    PointCluster::PointCluster()
        : num_points(0), estimated_aoa(0.0), avg_rssi(0.0), centroid_x(0.0), centroid_y(0.0), aoa_x(0.0), aoa_y(0.0), score(0.0), rssi_variance_computed(true), rssi_variance_value(0.0)
    {
        bbox = {0.0, 0.0, false};
        furthest_idx1 = 0;
        furthest_idx2 = 0;
        furthest_distance = 0.0;
        vectorized = false;
    }

    PointCluster::PointCluster(size_t num_points)
        : num_points(num_points), estimated_aoa(0.0), avg_rssi(0.0), centroid_x(0.0), centroid_y(0.0), aoa_x(0.0), aoa_y(0.0), score(0.0), rssi_variance_computed(true), rssi_variance_value(0.0)
    {
        bbox = {0.0, 0.0, false};
        furthest_idx1 = 0;
        furthest_idx2 = 0;
        furthest_distance = 0.0;
        point_bits.reserve(num_points);
        vectorized = true;
    }

    void PointCluster::addPoint(const DataPoint &point)
    {
        if (vectorized)
        {
            throw std::runtime_error("PointCluster: cannot add non-vectorized point to a vectorized cluster");
        }
        points.push_back(point);

        rssi_variance_computed = false;

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

    void PointCluster::addPointVectorized(const DataPoint &point, size_t index)
    {
        if (num_points == 0)
        {
            throw std::runtime_error("PointCluster: addPointVectorized called on cluster with zero capacity");
        }
        if (!vectorized)
        {
            throw std::runtime_error("PointCluster: addPointVectorized called on non-vectorized cluster");
        }

        rssi_variance_computed = false;

        x_dp_values.push_back(point.getXUnsafe());
        y_dp_values.push_back(point.getYUnsafe());
        rssi_values.push_back(static_cast<double>(point.rssi));
        point_bits.setBit(static_cast<size_t>(index));

        // update average RSSI
        double previous_total = avg_rssi * static_cast<double>(rssi_values.size() - 1);
        avg_rssi = (previous_total + static_cast<double>(point.rssi)) / static_cast<double>(rssi_values.size());

        // update centroid
        double previous_total_x = centroid_x * static_cast<double>(x_dp_values.size() - 1);
        double previous_total_y = centroid_y * static_cast<double>(y_dp_values.size() - 1);
        centroid_x = (previous_total_x + point.getXUnsafe()) / static_cast<double>(x_dp_values.size());
        centroid_y = (previous_total_y + point.getYUnsafe()) / static_cast<double>(y_dp_values.size());

        recomputeBoundingBox(x_dp_values.size() - 1);
    }

    void PointCluster::removePoint(const DataPoint &point)
    {
        if (vectorized)
        {
            throw std::runtime_error("PointCluster: cannot remove non-vectorized point from a vectorized cluster");
        }

        rssi_variance_computed = false;

        auto it = std::find_if(points.begin(), points.end(),
                               [&point](const DataPoint &p)
                               { return p.point_id == point.point_id; });

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

    void PointCluster::removePointVectorized(size_t cluster_index, size_t points_index)
    {
        if (x_dp_values.empty())
        {
            throw std::runtime_error("PointCluster: removePointVectorized called on empty cluster");
        }

        if (!vectorized)
        {
            throw std::runtime_error("PointCluster: removePointVectorized called on non-vectorized cluster");
        }

        if (cluster_index >= x_dp_values.size())
        {
            throw std::out_of_range("PointCluster: removePointVectorized index out of range");
        }

        if (points_index >= num_points)
        {
            throw std::out_of_range("PointCluster: removePointVectorized points_index out of range");
        }

        if (!point_bits.getBit(points_index))
        {
            throw std::runtime_error("PointCluster: removePointVectorized point not in cluster");
        }

        rssi_variance_computed = false;

        point_bits.clearBit(points_index);

        x_dp_values.erase(x_dp_values.begin() + cluster_index);
        y_dp_values.erase(y_dp_values.begin() + cluster_index);
        rssi_values.erase(rssi_values.begin() + cluster_index);

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

        if (cluster_index == furthest_idx1 || cluster_index == furthest_idx2)
        {
            computeBoundingBox();
        }
    }

    double PointCluster::overlapWith(const PointCluster &other) const
    {
        size_t total_points = size() + other.size();
        if (total_points == 0)
        {
            return 0.0;
        }

        if (vectorized && other.vectorized)
        {
            // Fast overlap using bitwise AND + popcount
            size_t shared_points = point_bits.sharedCount(other.point_bits);
            return static_cast<double>(shared_points) / static_cast<double>(total_points);
        }
        else if (!vectorized && !other.vectorized)
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
            // Mixed representations: compare the vectorized set indices to the point_id
            size_t shared_points = 0;
            if (vectorized && !other.vectorized)
            {
                std::vector<int> indices = point_bits.getSetIndices();
                for (int idx : indices)
                {
                    for (const auto &p : other.points)
                    {
                        if (p.point_id == idx)
                        {
                            shared_points++;
                            break;
                        }
                    }
                }
            }
            else if (!vectorized && other.vectorized)
            {
                std::vector<int> indices = other.point_bits.getSetIndices();
                for (int idx : indices)
                {
                    for (const auto &p : points)
                    {
                        if (p.point_id == idx)
                        {
                            shared_points++;
                            break;
                        }
                    }
                }
            }

            return static_cast<double>(shared_points) / static_cast<double>(total_points);
        }
    }
    double PointCluster::varianceRSSI()
    {
        if (rssi_variance_computed)
        {
            return rssi_variance_value;
        }
        if (size() < 2)
        {
            return 0.0;
        }

        double sum_sq_diff = 0.0;
        if (vectorized)
        {
            for (const auto &rssi_val : rssi_values)
            {
                double diff = rssi_val - avg_rssi;
                sum_sq_diff += diff * diff;
            }
        }
        else
        {
            for (const auto &p : points)
            {
                double diff = p.rssi - avg_rssi;
                sum_sq_diff += diff * diff;
            }
        }

        rssi_variance_value = sum_sq_diff / static_cast<double>(size());
        rssi_variance_computed = true;

        return rssi_variance_value;
    }

    void PointCluster::setScore(double input_score)
    {
        this->score = input_score;
    }

    double PointCluster::getAndSetScore(double ideal_geometric_ratio, double min_geometric_ratio, double max_geometric_ratio,
                                        double ideal_area, double min_area, double max_area,
                                        double ideal_rssi_variance, double min_rssi_variance, double max_rssi_variance,
                                        double gr_weight, double area_weight, double variance_weight,
                                        double bottom_rssi_threshold, double top_rssi, double rssi_weight)
    {
        // Triangular interpolation: 0 at min/max, 1 at ideal
        auto triangleScore = [](double value, double min_val, double ideal, double max_val) -> double
        {
            if (value < min_val || value > max_val)
                return 0.0;
            if (value <= ideal)
            {
                // Interpolate from min (0) to ideal (1)
                if (ideal == min_val)
                    return 1.0;
                return (value - min_val) / (ideal - min_val);
            }
            else
            {
                // Interpolate from ideal (1) to max (0)
                if (max_val == ideal)
                    return 1.0;
                return (max_val - value) / (max_val - ideal);
            }
        };

        double gr_score = triangleScore(geometricRatio(), min_geometric_ratio, ideal_geometric_ratio, max_geometric_ratio);

        double area_score = triangleScore(area(), min_area, ideal_area, max_area);

        double variance_score = triangleScore(varianceRSSI(), min_rssi_variance, ideal_rssi_variance, max_rssi_variance);

        // RSSI: interpolate between bottom threshold (worst) and top_rssi (best)
        double rssi_score = 0.0;
        if (avg_rssi > bottom_rssi_threshold)
        {
            if (top_rssi == bottom_rssi_threshold)
            {
                rssi_score = 1.0;
            }
            else
            {
                rssi_score = (avg_rssi - bottom_rssi_threshold) / (top_rssi - bottom_rssi_threshold);
            }
            if (rssi_score > 1.0)
                rssi_score = 1.0;
            if (rssi_score < 0.0)
                rssi_score = 0.0;
        }

        score = gr_weight * gr_score + area_weight * area_score + variance_weight * variance_score + rssi_weight * rssi_score;
        return score;
    }

    size_t PointCluster::size() const
    {
        if (vectorized)
        {
            return x_dp_values.size();
        }
        return points.size();
    }

    void PointCluster::recomputeBoundingBox(size_t new_idx)
    {

        double sqrdist = furthest_distance * furthest_distance;
        size_t idx1 = furthest_idx1;
        size_t idx2 = furthest_idx2;

        if (vectorized)
        {
            double x = x_dp_values[new_idx];
            double y = y_dp_values[new_idx];
            for (size_t j = 0; j < x_dp_values.size(); ++j)
            {
                double dx = x - x_dp_values[j];
                double dy = y - y_dp_values[j];
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
            double x = points[new_idx].getXUnsafe();
            double y = points[new_idx].getYUnsafe();
            for (size_t j = 0; j < points.size(); ++j)
            {
                double dx = x - points[j].getXUnsafe();
                double dy = y - points[j].getYUnsafe();
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

        if (size() < 3)
        {
            bbox.valid = false;
            return;
        }

        double x1;
        double y1;
        double x2;
        double y2;

        // Get unit vector along the line between the two furthest points
        if (vectorized)
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
        if (vectorized)
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

        double sqrdist = 0.0;
        size_t idx1 = 0;
        size_t idx2 = 0;

        if (vectorized)
        {
            for (size_t i = 0; i < x_dp_values.size(); ++i)
            {
                double x = x_dp_values[i];
                double y = y_dp_values[i];
                for (size_t j = i + 1; j < x_dp_values.size(); ++j)
                {
                    double dx = x - x_dp_values[j];
                    double dy = y - y_dp_values[j];
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
                double x = points[i].getXUnsafe();
                double y = points[i].getYUnsafe();
                for (size_t j = i + 1; j < points.size(); ++j)
                {
                    double dx = x - points[j].getXUnsafe();
                    double dy = y - points[j].getYUnsafe();
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

        if (size() < 3)
        {
            bbox.valid = false;
            return;
        }

        double x1;
        double y1;
        double x2;
        double y2;

        // Get unit vector along the line between the two furthest points
        if (vectorized)
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
        if (vectorized)
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

    std::vector<int> PointCluster::getPointIndices() const
    {
        if (vectorized)
        {
            return point_bits.getSetIndices();
        }
        else
        {
            // For non-vectorized clusters, return point_id values
            std::vector<int> indices;
            indices.reserve(points.size());
            for (const auto &p : points)
            {
                indices.push_back(p.point_id);
            }
            return indices;
        }
    }

    PointCluster PointCluster::copyVectorizedToNormal(std::vector<DataPoint> &all_points)
    {
        if (!vectorized)
        {
            throw std::runtime_error("PointCluster: copyVectorizedToNormal called on non-vectorized cluster");
        }

        PointCluster new_cluster;

        std::vector<int> indices = point_bits.getSetIndices();
        for (int idx : indices)
        {
            new_cluster.points.push_back(all_points[idx]);
        }

        new_cluster.estimated_aoa = estimated_aoa;
        new_cluster.avg_rssi = avg_rssi;
        new_cluster.centroid_x = centroid_x;
        new_cluster.centroid_y = centroid_y;
        new_cluster.aoa_x = aoa_x;
        new_cluster.aoa_y = aoa_y;
        new_cluster.computeBoundingBox();
        new_cluster.score = score;
        if (std::abs(new_cluster.furthest_distance - furthest_distance) > 1e-9)
        {
            throw std::runtime_error("PointCluster: copyVectorizedToNormal furthest distance mismatch: " + std::to_string(new_cluster.furthest_distance) + " != " + std::to_string(furthest_distance));
        }

        return new_cluster;
    }

    PointCluster PointCluster::copyVectorizedToVectorized()
    {
        if (!vectorized)
        {
            throw std::runtime_error("PointCluster: copyVectorizedToVectorized called on non-vectorized cluster");
        }

        PointCluster new_cluster(num_points);
        new_cluster.vectorized = true;
        new_cluster.x_dp_values = x_dp_values;
        new_cluster.y_dp_values = y_dp_values;
        new_cluster.rssi_values = rssi_values;
        new_cluster.point_bits.copyFrom(point_bits);
        new_cluster.estimated_aoa = estimated_aoa;
        new_cluster.avg_rssi = avg_rssi;
        new_cluster.centroid_x = centroid_x;
        new_cluster.centroid_y = centroid_y;
        new_cluster.aoa_x = aoa_x;
        new_cluster.aoa_y = aoa_y;
        new_cluster.score = score;
        new_cluster.bbox = bbox;
        new_cluster.furthest_idx1 = furthest_idx1;
        new_cluster.furthest_idx2 = furthest_idx2;
        new_cluster.furthest_distance = furthest_distance;

        return new_cluster;
    }

} // namespace core
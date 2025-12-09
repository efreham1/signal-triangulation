#include "ClusteredTriangulationBase.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <spdlog/spdlog.h>

namespace
{
    // Numeric tolerances
    static constexpr double NORMAL_REGULARIZATION_EPS = 1e-12;
    static constexpr double GAUSS_ELIM_PIVOT_EPS = 1e-15;
}

namespace core
{

    ClusteredTriangulationBase::ClusteredTriangulationBase() = default;
    ClusteredTriangulationBase::~ClusteredTriangulationBase() = default;

    void ClusteredTriangulationBase::processDataPoint(const DataPoint &point)
    {
        if (!point.validCoordinates())
        {
            throw std::invalid_argument("ClusteredTriangulationBase: invalid coordinates");
        }

        auto it = std::lower_bound(
            m_points.begin(), m_points.end(), point.timestamp_ms,
            [](const DataPoint &a, const int64_t t)
            { return a.timestamp_ms < t; });
        m_points.insert(it, point);

        spdlog::debug("ClusteredTriangulationBase: added DataPoint (x={}, y={}, rssi={}, timestamp={})",
                      point.getX(), point.getY(), point.rssi, point.timestamp_ms);
    }

    void ClusteredTriangulationBase::reset()
    {
        m_points.clear();
        m_clusters.clear();
        distance_cache.clear();
    }

    std::pair<int64_t, int64_t> ClusteredTriangulationBase::makeDistanceKey(int64_t id1, int64_t id2) const
    {
        return (id1 < id2) ? std::make_pair(id1, id2) : std::make_pair(id2, id1);
    }

    void ClusteredTriangulationBase::addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance)
    {
        distance_cache.try_emplace(makeDistanceKey(p1.point_id, p2.point_id), distance);
    }

    double ClusteredTriangulationBase::getDistance(const DataPoint &p1, const DataPoint &p2)
    {
        auto key = makeDistanceKey(p1.point_id, p2.point_id);
        auto it = distance_cache.find(key);
        if (it != distance_cache.end())
        {
            return it->second;
        }

        double dx = p1.getX() - p2.getX();
        double dy = p1.getY() - p2.getY();
        double distance = std::sqrt(dx * dx + dy * dy);
        addToDistanceCache(p1, p2, distance);
        return distance;
    }

    void ClusteredTriangulationBase::reorderDataPointsByDistance()
    {
        if (m_points.size() < 3)
        {
            return;
        }

        // Initial Solution: Greedy Nearest Neighbor
        std::vector<DataPoint> current_path;
        current_path.reserve(m_points.size());
        std::vector<DataPoint> remaining = m_points;

        current_path.push_back(remaining[0]);
        remaining.erase(remaining.begin());

        while (!remaining.empty())
        {
            const DataPoint &last = current_path.back();
            double best_dist = std::numeric_limits<double>::max();
            size_t best_idx = 0;

            for (size_t i = 0; i < remaining.size(); ++i)
            {
                double d = getDistance(last, remaining[i]);
                if (d < best_dist)
                {
                    best_dist = d;
                    best_idx = i;
                }
            }
            current_path.push_back(remaining[best_idx]);
            remaining.erase(remaining.begin() + best_idx);
        }

        // Calculate initial total distance
        double total_dist = 0.0;
        for (size_t i = 0; i < current_path.size() - 1; ++i)
        {
            total_dist += getDistance(current_path[i], current_path[i + 1]);
        }
        double initial_dist = total_dist;

        // 2-Opt Local Search optimization
        bool improved = true;
        int iterations = 0;
        const int MAX_ITERATIONS = 100;

        while (improved && iterations < MAX_ITERATIONS)
        {
            improved = false;
            iterations++;

            for (size_t i = 0; i < current_path.size() - 2; ++i)
            {
                for (size_t j = i + 1; j < current_path.size() - 1; ++j)
                {
                    double d_ab = getDistance(current_path[i], current_path[i + 1]);
                    double d_cd = getDistance(current_path[j], current_path[j + 1]);
                    double current_cost = d_ab + d_cd;

                    double d_ac = getDistance(current_path[i], current_path[j]);
                    double d_bd = getDistance(current_path[i + 1], current_path[j + 1]);
                    double new_cost = d_ac + d_bd;

                    if (new_cost < current_cost)
                    {
                        std::reverse(current_path.begin() + i + 1, current_path.begin() + j + 1);
                        total_dist -= (current_cost - new_cost);
                        improved = true;
                    }
                }
            }
        }

        m_points = std::move(current_path);

        spdlog::info("ClusteredTriangulationBase: optimized path. Length reduced from {:.2f}m to {:.2f}m ({} iterations)",
                     initial_dist, total_dist, iterations);
    }

    void ClusteredTriangulationBase::coalescePoints(double coalition_distance)
    {
        for (int i = 0; i < static_cast<int>(m_points.size()); ++i)
        {
            double old_x_i = m_points[i].getX();
            double old_y_i = m_points[i].getY();
            for (int j = i + 1; j < static_cast<int>(m_points.size()); ++j)
            {
                double dx = old_x_i - m_points[j].getX();
                double dy = old_y_i - m_points[j].getY();
                double dist2 = dx * dx + dy * dy;

                if (dist2 <= coalition_distance * coalition_distance)
                {
                    double new_x = (m_points[i].getX() + m_points[j].getX()) / 2.0;
                    double new_y = (m_points[i].getY() + m_points[j].getY()) / 2.0;
                    double new_rssi = (m_points[i].rssi + m_points[j].rssi) / 2.0;

                    spdlog::debug("ClusteredTriangulationBase: coalesced point (x={}, y={}, rssi={}) into (x={}, y={}, rssi={})",
                                  m_points[j].getX(), m_points[j].getY(), m_points[j].rssi,
                                  new_x, new_y, new_rssi);

                    m_points[i].setX(new_x);
                    m_points[i].setY(new_y);
                    m_points[i].rssi = static_cast<int>(new_rssi);

                    m_points.erase(m_points.begin() + j);
                    --j;
                }
            }
        }
    }

    void ClusteredTriangulationBase::estimateAoAForClusters(unsigned int min_points)
    {
        std::vector<double> X, Y, Z;

        for (auto &cluster : m_clusters)
        {
            if (cluster.points.size() < 3)
            {
                continue;
            }

            X.resize(cluster.points.size());
            Y.resize(cluster.points.size());
            Z.resize(cluster.points.size());

            for (size_t i = 0; i < cluster.points.size(); ++i)
            {
                const auto &point = cluster.points[i];
                X[i] = point.getX();
                Y[i] = point.getY();
                Z[i] = static_cast<double>(point.rssi);
            }

            std::vector<double> normal = fitPlaneNormal(X, Y, Z, min_points);

            if (normal[2] == 0.0)
            {
                continue;
            }

            double grad_x = -normal[0] / normal[2];
            double grad_y = -normal[1] / normal[2];
            cluster.aoa_x = grad_x;
            cluster.aoa_y = grad_y;
            cluster.estimated_aoa = atan2(grad_y, grad_x) * (180.0 / M_PI);

            spdlog::info("ClusteredTriangulationBase: cluster AoA estimated at {} degrees (grad_x={}, grad_y={})",
                         cluster.estimated_aoa, grad_x, grad_y);
        }
    }

    double ClusteredTriangulationBase::getCost(double x, double y, double extra_weight, double angle_weight) const
    {
        double total_cost = 0.0;

        for (const auto &cluster : m_clusters)
        {
            double cluster_grad[2] = {cluster.aoa_x, cluster.aoa_y};
            if (cluster_grad[0] == 0.0 && cluster_grad[1] == 0.0)
            {
                continue;
            }

            double point_to_centroid[2] = {x - cluster.centroid_x, y - cluster.centroid_y};
            double cross_prod_mag = std::abs(point_to_centroid[0] * cluster_grad[1] - point_to_centroid[1] * cluster_grad[0]);
            double cluster_grad_mag = std::sqrt(cluster_grad[0] * cluster_grad[0] + cluster_grad[1] * cluster_grad[1]);
            double dot_prod = point_to_centroid[0] * cluster_grad[0] + point_to_centroid[1] * cluster_grad[1];

            double ptc_norm = std::sqrt(point_to_centroid[0] * point_to_centroid[0] +
                                        point_to_centroid[1] * point_to_centroid[1]);
            
            if (ptc_norm < std::numeric_limits<double>::epsilon())
            {
                continue;
            }

            double cluster_cost = 0.0;
            if (dot_prod < 0)
            {
                cluster_cost = -dot_prod / cluster_grad_mag + ptc_norm;
            }
            else
            {
                cluster_cost = cross_prod_mag / cluster_grad_mag;
            }
            
            double cos_theta = dot_prod / (cluster_grad_mag * ptc_norm);

            if (cos_theta < -1.0 || cos_theta > 1.0)
            {
                spdlog::warn("ClusteredTriangulationBase: numerical issue in cost calculation, cos_theta={}", cos_theta);
                continue;
            }

            double theta = std::acos(cos_theta);

            double weight = extra_weight;
            weight += theta * angle_weight;
            
            if (cluster.score > 0.0)
            {
                weight += cluster.score;
            }
            cluster_cost *= weight;

            total_cost += cluster_cost;
        }

        return total_cost;
    }

    void ClusteredTriangulationBase::printPointsAndClusters() const
    {
        std::cout << "Data Points:" << std::endl;
        for (const auto &point : m_points)
        {
            std::cout << "  x: " << point.getX() << ", y: " << point.getY() << ", rssi: " << point.rssi << std::endl;
        }

        std::cout << "Clusters:" << std::endl;
        for (size_t i = 0; i < m_clusters.size(); ++i)
        {
            const auto &cluster = m_clusters[i];
            double ratio = cluster.geometricRatio();
            std::cout << "  Cluster " << i << ": centroid_x: " << cluster.centroid_x
                      << ", centroid_y: " << cluster.centroid_y
                      << ", avg_rssi: " << cluster.avg_rssi
                      << ", estimated_aoa: " << cluster.estimated_aoa
                      << ", ratio: " << ratio
                      << ", weight: " << cluster.score
                      << ", num_points: " << cluster.points.size() << std::endl;
            for (const auto &p : cluster.points)
            {
                std::cout << "    p " << p.getX() << " " << p.getY() << " " << i << std::endl;
            }
        }

        // Print point-to-cluster membership summary
        std::cout << "Point Membership:" << std::endl;
        for (const auto &point : m_points)
        {
            std::cout << "  point (" << point.getX() << ", " << point.getY() << ") in clusters:";
            for (size_t i = 0; i < m_clusters.size(); ++i)
            {
                for (const auto &cp : m_clusters[i].points)
                {
                    if (std::abs(cp.getX() - point.getX()) < 1e-9 &&
                        std::abs(cp.getY() - point.getY()) < 1e-9)
                    {
                        std::cout << " " << i;
                        break;
                    }
                }
            }
            std::cout << std::endl;
        }
    }

    // Free function for plane fitting
    std::vector<double> fitPlaneNormal(
        const std::vector<double> &x,
        const std::vector<double> &y,
        const std::vector<double> &z,
        unsigned int min_points)
    {
        if (x.size() < min_points || y.size() < min_points || z.size() < min_points ||
            x.size() != y.size() || x.size() != z.size())
        {
            return {0.0, 0.0, 0.0};
        }

        double Sxx = 0.0, Sxy = 0.0, Sx = 0.0;
        double Syy = 0.0, Sy = 0.0;
        double Sz = 0.0, Sxz = 0.0, Syz = 0.0;
        int N = static_cast<int>(x.size());

        for (int i = 0; i < N; ++i)
        {
            Sxx += x[i] * x[i];
            Sxy += x[i] * y[i];
            Sx += x[i];
            Syy += y[i] * y[i];
            Sy += y[i];
            Sz += z[i];
            Sxz += x[i] * z[i];
            Syz += y[i] * z[i];
        }

        double A00 = Sxx + NORMAL_REGULARIZATION_EPS;
        double A01 = Sxy;
        double A02 = Sx;
        double A10 = Sxy;
        double A11 = Syy + NORMAL_REGULARIZATION_EPS;
        double A12 = Sy;
        double A20 = Sx;
        double A21 = Sy;
        double A22 = static_cast<double>(N) + NORMAL_REGULARIZATION_EPS;

        double b0 = Sxz;
        double b1 = Syz;
        double b2 = Sz;

        double M[3][4] = {
            {A00, A01, A02, b0},
            {A10, A11, A12, b1},
            {A20, A21, A22, b2}};

        // Forward elimination with partial pivoting
        for (int col = 0; col < 3; ++col)
        {
            int pivot = col;
            double maxabs = std::fabs(M[pivot][col]);
            for (int r = col + 1; r < 3; ++r)
            {
                double v = std::fabs(M[r][col]);
                if (v > maxabs)
                {
                    maxabs = v;
                    pivot = r;
                }
            }
            if (pivot != col)
            {
                for (int c = col; c < 4; ++c)
                    std::swap(M[col][c], M[pivot][c]);
            }
            double piv = M[col][col];
            if (std::fabs(piv) < GAUSS_ELIM_PIVOT_EPS)
            {
                return {0.0, 0.0, 0.0};
            }
            for (int c = col; c < 4; ++c)
                M[col][c] /= piv;
            for (int r = col + 1; r < 3; ++r)
            {
                double factor = M[r][col];
                for (int c = col; c < 4; ++c)
                    M[r][c] -= factor * M[col][c];
            }
        }

        // Back substitution
        double xsol[3];
        for (int i = 2; i >= 0; --i)
        {
            double val = M[i][3];
            for (int j = i + 1; j < 3; ++j)
                val -= M[i][j] * xsol[j];
            xsol[i] = val / M[i][i];
        }

        std::vector<double> normal = {xsol[0], xsol[1], -1.0};
        double norm = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        if (norm > 0.0)
        {
            for (auto &v : normal)
                v /= norm;
        }
        return normal;
    }

} // namespace core
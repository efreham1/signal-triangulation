#include "ClusteredTriangulationAlgorithm2.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <spdlog/spdlog.h>
#include <chrono>

// File-local tunable constants (avoid magic numbers in code)
namespace
{
	// clustering
	static constexpr double DEFAULT_COALITION_DISTANCE_METERS = 2.0; // meters used to coalesce nearby points
	static constexpr unsigned int CLUSTER_MIN_POINTS = 3u;			 // minimum points to form a cluster (3 points needed for AoA estimation)
	static constexpr double CLUSTER_RATIO_SPLIT_THRESHOLD = 0.25;	 // geometric ratio threshold to split cluster

	// cluster weighting
	static constexpr double VARIANCE_WEIGHT = 0.3; // weight for variance in cluster weighting
	static constexpr double RSSI_WEIGHT = 0.1;	   // weight for RSSI component in cluster weighting
	static constexpr double BOTTOM_RSSI = -90.0;   // bottom RSSI threshold for cluster weighting
	static constexpr double EXTRA_WEIGHT = 1.0;	   // extra weight multiplier for cluster weighting

	// brute force search
	static constexpr int HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS = 500; // half the number of discrete steps per axis in brute force search

	// numeric tolerances
	static constexpr double NORMAL_REGULARIZATION_EPS = 1e-12; // regularize normal equations diagonal
	static constexpr double GAUSS_ELIM_PIVOT_EPS = 1e-15;	   // pivot threshold for Gaussian elimination
}

namespace core
{

	ClusteredTriangulationAlgorithm2::ClusteredTriangulationAlgorithm2() = default;

	ClusteredTriangulationAlgorithm2::~ClusteredTriangulationAlgorithm2() = default;

	std::pair<int64_t, int64_t> ClusteredTriangulationAlgorithm2::makeEdgeKey(int64_t id1, int64_t id2) const
	{
		if (id1 < id2)
		{
			return {id1, id2};
		}
		return {id2, id1};
	}

	void ClusteredTriangulationAlgorithm2::addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance)
	{
		distance_cache.try_emplace(makeEdgeKey(p1.point_id, p2.point_id), distance);
	}

	void ClusteredTriangulationAlgorithm2::reorderDataPointsByDistance()
	{
		if (m_points.size() < 3)
		{
			return; // Too few points to optimize
		}

		auto getDistance = [&](const DataPoint &p1, const DataPoint &p2) -> double
		{
			auto key = makeEdgeKey(p1.point_id, p2.point_id);
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
		};

		// Initial Solution: Greedy Nearest Neighbor
		std::vector<DataPoint> current_path;
		current_path.reserve(m_points.size());
		std::vector<DataPoint> remaining = m_points;

		// Start with the first point
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

		// Calculate initial total distance for logging
		double total_dist = 0.0;
		for (size_t i = 0; i < current_path.size() - 1; ++i)
		{
			double dist = getDistance(current_path[i], current_path[i + 1]);
			total_dist += dist;
		}

		double initial_dist = total_dist;

		// Optimization: 2-Opt Local Search
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

		spdlog::info("ClusteredTriangulationAlgorithm2: optimized path. Length reduced from {:.2f}m to {:.2f}m ({} iterations)",
					 initial_dist, total_dist, iterations);
	}

	void ClusteredTriangulationAlgorithm2::processDataPoint(const DataPoint &point)
	{
		// Basic validation; throw on invalid coordinates to make errors explicit
		if (!point.validCoordinates())
		{
			throw std::invalid_argument("ClusteredTriangulationAlgorithm2: invalid coordinates");
		}

		// insert point into m_points keeping the vector sorted by timestamp_ms (ascending)
		auto it = std::lower_bound(
			m_points.begin(), m_points.end(), point.timestamp_ms,
			[](const DataPoint &a, const int64_t t)
			{ return a.timestamp_ms < t; });
		m_points.insert(it, point);

		spdlog::debug("ClusteredTriangulationAlgorithm2: added DataPoint (x={}, y={}, rssi={}, timestamp={})", point.getX(), point.getY(), point.rssi, point.timestamp_ms);
	}

	void printPointsAndClusters2(const std::vector<DataPoint> &points, std::vector<PointCluster> &clusters)
	{
		std::cout << "Data Points:" << std::endl;
		for (const auto &point : points)
		{
			std::cout << "  x: " << point.getX() << ", y: " << point.getY() << ", rssi: " << point.rssi << std::endl;
		}

		std::cout << "Clusters:" << std::endl;
		for (size_t i = 0; i < clusters.size(); ++i)
		{
			auto &cluster = clusters[i];
			double ratio = cluster.geometricRatio();
			double weight = cluster.getWeight(VARIANCE_WEIGHT, RSSI_WEIGHT, BOTTOM_RSSI);
			std::cout << "  Cluster " << i << ": centroid_x: " << cluster.centroid_x
					  << ", centroid_y: " << cluster.centroid_y
					  << ", avg_rssi: " << cluster.avg_rssi
					  << ", estimated_aoa: " << cluster.estimated_aoa
					  << ", ratio: " << ratio
					  << ", weight: " << weight
					  << ", num_points: " << cluster.points.size() << std::endl;
			// Print points belonging to this cluster: one per line prefixed with 'p' (x y)
			for (const auto &p : cluster.points)
			{
				std::cout << "p " << p.getX() << " " << p.getY() << std::endl;
			}
		}
	}

	void ClusteredTriangulationAlgorithm2::bruteForceSearch(double &global_best_x, double &global_best_y, double precision, double timeout)
	{

		global_best_x = 0.0;
		global_best_y = 0.0;
		double global_best_cost = getCost(global_best_x, global_best_y);

		double zone_x = -precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS;
		double zone_y = -precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS;

		if (plottingEnabled)
		{
			std::cout << "Search Space Costs:" << std::endl;
		}

		auto start_time = std::chrono::steady_clock::now();

		std::set<std::pair<double, double>> visited_quadrants; // Defined by lower-left corner (x, y)

		while (true)
		{
			if (timeout > 0.0)
			{
				auto current_time = std::chrono::steady_clock::now();
				std::chrono::duration<double> elapsed = current_time - start_time;
				if (elapsed.count() > timeout)
				{
					spdlog::warn("ClusteredTriangulationAlgorithm2: timeout reached during brute force search");
					break;
				}
			}

			double best_x = global_best_x;
			double best_y = global_best_y;
			double best_cost = global_best_cost;

			for (int q = 0; q < 4; ++q)
			{
				// Check if this quadrant has been visited
				double quadrant_x = zone_x + (q % 2) * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS * precision;
				double quadrant_y = zone_y + (q / 2) * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS * precision;
				if (visited_quadrants.find(std::make_pair(quadrant_x, quadrant_y)) != visited_quadrants.end())
				{
					continue; // already visited
				}
				visited_quadrants.insert(std::make_pair(quadrant_x, quadrant_y));

				for (int ix = 0; ix < HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS; ++ix)
				{
					for (int iy = 0; iy < HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS; ++iy)
					{
						double x = quadrant_x + ix * precision;
						double y = quadrant_y + iy * precision;
						double cost = getCost(x, y);
						if (cost < best_cost)
						{
							best_cost = cost;
							best_x = x;
							best_y = y;
						}
						if (plottingEnabled)
						{
							std::cout << x << "," << y << "," << cost << std::endl;
						}
					}
				}
			}

			spdlog::info("ClusteredTriangulationAlgorithm2: brute force search iteration found best point (x={}, y={}) with cost {}", best_x, best_y, best_cost);

			// Check for convergence
			if (best_cost < global_best_cost)
			{
				global_best_cost = best_cost;
				global_best_x = best_x;
				global_best_y = best_y;

				zone_x += precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS * (best_x < zone_x + precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS ? -1.0 : 1.0);
				zone_y += precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS * (best_y < zone_y + precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS ? -1.0 : 1.0);
			}
			else
			{
				break;
			}
		}
		spdlog::info("ClusteredTriangulationAlgorithm2: brute force search completed with best point (x={}, y={}) and cost {}", global_best_x, global_best_y, global_best_cost);
	}

	void ClusteredTriangulationAlgorithm2::calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout)
	{
		reorderDataPointsByDistance();

		clusterData();
		estimateAoAForClusters();

		double global_best_x = 0.0;
		double global_best_y = 0.0;

		bruteForceSearch(global_best_x, global_best_y, precision, timeout);

		// print x and y of resulting point
		if (plottingEnabled)
		{
			printPointsAndClusters2(m_points, m_clusters);
			std::cout << "Resulting point after brute force search: x=" << global_best_x << ", y=" << global_best_y << std::endl;
		}

		DataPoint result_point;
		result_point.setX(global_best_x);
		result_point.setY(global_best_y);
		result_point.zero_latitude = m_points[0].zero_latitude;
		result_point.zero_longitude = m_points[0].zero_longitude;
		result_point.computeCoordinates();
		if (!result_point.validCoordinates())
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm2: computed invalid coordinates");
		}
		out_latitude = result_point.getLatitude();
		out_longitude = result_point.getLongitude();
	}

	void ClusteredTriangulationAlgorithm2::reset()
	{
		m_points.clear();
		m_clusters.clear();
	}

	void ClusteredTriangulationAlgorithm2::clusterData()
	{
        coalescePoints();

        // Find the best cluster formation based on hard requirements

		// Do this until no more clusters can be formed

		unsigned int cluster_id = 0;
		unsigned int current_cluster_size = 0;
		for (const auto &point : m_points)
		{
			if (current_cluster_size == 0)
			{
				m_clusters.emplace_back();
			}

			m_clusters[cluster_id].addPoint(point);
			current_cluster_size = static_cast<unsigned int>(m_clusters[cluster_id].points.size());
			auto &c = m_clusters[cluster_id];
			if (c.geometricRatio() > CLUSTER_RATIO_SPLIT_THRESHOLD && current_cluster_size >= CLUSTER_MIN_POINTS)
			{
				spdlog::debug("ClusteredTriangulationAlgorithm2: created new cluster (id={}) after splitting cluster (id={}) due to geometric ratio {}", cluster_id + 1, cluster_id, c.geometricRatio());
				cluster_id++;
				current_cluster_size = 0;
			}
		}
		spdlog::info("ClusteredTriangulationAlgorithm2: formed {} clusters from {} data points", m_clusters.size(), m_points.size());
		if (m_clusters.size() != cluster_id)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm2: last cluster formed does not meet requirements");
		}
		if (m_clusters.size() < 2)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm2: insufficient clusters formed for AoA estimation");
		}
		else if (m_clusters.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm2: only {} clusters formed; AoA estimation may be unreliable", m_clusters.size());
		}
    }

    void ClusteredTriangulationAlgorithm2::coalescePoints()
    {
        const double coalition_distance = DEFAULT_COALITION_DISTANCE_METERS;
        for (int i = 0; i < static_cast<int>(m_points.size()); ++i)
        {
            for (int j = i + 1; j < static_cast<int>(m_points.size()); ++j)
            {
                double dx = m_points[i].getX() - m_points[j].getX();
                double dy = m_points[i].getY() - m_points[j].getY();
                double dist2 = dx * dx + dy * dy;
                if (dist2 <= coalition_distance * coalition_distance)
                {
                    // Merge points by averaging their positions and RSSI into the earlier point (i)
                    double new_x = (m_points[i].getX() + m_points[j].getX()) / 2.0;
                    double new_y = (m_points[i].getY() + m_points[j].getY()) / 2.0;
                    double new_rssi = (m_points[i].rssi + m_points[j].rssi) / 2.0;
                    spdlog::debug("ClusteredTriangulationAlgorithm2: coalesced point (x={}, y={}, rssi={}) into existing point (x={}, y={}, rssi={})",
                                  m_points[j].getX(), m_points[j].getY(), m_points[j].rssi, new_x, new_y, new_rssi);

                    m_points[i].setX(new_x);
                    m_points[i].setY(new_y);
                    m_points[i].rssi = static_cast<int>(new_rssi);

                    // Remove the merged point j and adjust index to continue checking with the same i
                    m_points.erase(m_points.begin() + j);
                    --j;
                }
            }
        }
    }

    std::vector<double> getNormalVector2(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z)
	{
		if (x.size() < CLUSTER_MIN_POINTS || y.size() < CLUSTER_MIN_POINTS || z.size() < CLUSTER_MIN_POINTS ||
			x.size() != y.size() || x.size() != z.size())
		{
			return {0.0, 0.0, 0.0};
		}

		// Solve least-squares for plane z = a*x + b*y + c
		// Build normal equations: [A^T A] [a b c]^T = A^T z
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

		// Normal matrix (3x3)
		double A00 = Sxx;
		double A01 = Sxy;
		double A02 = Sx;
		double A10 = Sxy;
		double A11 = Syy;
		double A12 = Sy;
		double A20 = Sx;
		double A21 = Sy;
		double A22 = static_cast<double>(N);

		// Right-hand side
		double b0 = Sxz;
		double b1 = Syz;
		double b2 = Sz;

		// Add tiny regularization to diagonal to avoid singularity
		const double eps = NORMAL_REGULARIZATION_EPS;
		A00 += eps;
		A11 += eps;
		A22 += eps;

		// Solve 3x3 linear system by Gaussian elimination (in-place on augmented matrix)
		double M[3][4] = {
			{A00, A01, A02, b0},
			{A10, A11, A12, b1},
			{A20, A21, A22, b2}};

		// Forward elimination
		for (int col = 0; col < 3; ++col)
		{
			// Find pivot
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
				// singular; fallback: return zero vector
				return {0.0, 0.0, 0.0};
			}
			// normalize row
			for (int c = col; c < 4; ++c)
				M[col][c] /= piv;
			// eliminate below
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
			xsol[i] = val / M[i][i]; // M[i][i] should be 1.0 from normalization
		}

		double a = xsol[0];
		double b = xsol[1];
		// c = xsol[2]; not needed for normal

		// Plane normal for a*x + b*y - z + c = 0 is [a, b, -1]
		std::vector<double> normal = {a, b, -1.0};
		double norm = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
		if (norm > 0.0)
		{
			for (auto &v : normal)
				v /= norm;
		}
		return normal;
	}

	void ClusteredTriangulationAlgorithm2::estimateAoAForClusters()
	{
		// X, Y, and Z are separate parallel arrays containing x coordinates, y coordinates, and rssi values respectively.
		std::vector<double> X;
		std::vector<double> Y;
		std::vector<double> Z;
		for (auto &cluster : m_clusters)
		{
			if (cluster.points.size() < 3)
			{
				continue; // Need at least 3 points to estimate AoA
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
			};
			std::vector<double> normal = getNormalVector2(X, Y, Z);
			// The gradient components
			if (normal[2] == 0.0)
			{
				continue; // Avoid division by zero
			}
			double grad_x = -normal[0] / normal[2];
			double grad_y = -normal[1] / normal[2];
			cluster.aoa_x = grad_x;
			cluster.aoa_y = grad_y;
			cluster.estimated_aoa = atan2(grad_y, grad_x) * (180.0 / M_PI); // in degrees

			spdlog::info("ClusteredTriangulationAlgorithm2: cluster AoA estimated at {} degrees (grad_x={}, grad_y={})", cluster.estimated_aoa, grad_x, grad_y);
		}
		// Estimate AoA as the arctangent of the gradient
	}

	double ClusteredTriangulationAlgorithm2::getCost(double x, double y)
	{
		// d= ∥(p−a)×v∥ / ∥v∥
		// where p is the point (x, y), a is a point on the line (centroid), and v is the direction vector (AoA)
		double total_cost = 0.0;

		// Cost is defined as the sum distances from the guess to the lines defined by each cluster's AoA
		for (const auto &cluster : m_clusters)
		{
			double cluster_grad[2] = {cluster.aoa_x, cluster.aoa_y};
			if (cluster_grad[0] == 0.0 && cluster_grad[1] == 0.0)
			{
				continue; // Skip clusters without valid AoA
			}

			double point_to_centroid[2] = {x - cluster.centroid_x, y - cluster.centroid_y};

			// Cross product magnitude in 2D is |x1*y2 - y1*x2|
			double cross_prod_mag = std::abs(point_to_centroid[0] * cluster_grad[1] - point_to_centroid[1] * cluster_grad[0]);

			// Magnitude of the cluster gradient vector
			double cluster_grad_mag = std::sqrt(cluster_grad[0] * cluster_grad[0] + cluster_grad[1] * cluster_grad[1]);

			// t= (p−a)⋅v​ / ∥v∥^2
			double dot_prod = point_to_centroid[0] * cluster_grad[0] + point_to_centroid[1] * cluster_grad[1];

			// If the point is projected onto the line behind the cluster centroid, calculate cost as the distance to the centroid from (x,y)
			// + the distance from the centroid to the projection along the AoA direction
			if (dot_prod < 0)
			{
				// total_cost += projection_length + point_to_centroid_mag
				double cluster_cost = -dot_prod / cluster_grad_mag + std::sqrt(point_to_centroid[0] * point_to_centroid[0] + point_to_centroid[1] * point_to_centroid[1]);
				cluster_cost *= cluster.getWeight(VARIANCE_WEIGHT, RSSI_WEIGHT, BOTTOM_RSSI) + EXTRA_WEIGHT;
				spdlog::debug("ClusteredTriangulationAlgorithm2: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (behind centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, cluster_cost);
				total_cost += cluster_cost;
			}
			else
			{
				double cluster_cost = cross_prod_mag / cluster_grad_mag;
				cluster_cost *= cluster.getWeight(VARIANCE_WEIGHT, RSSI_WEIGHT, BOTTOM_RSSI) + EXTRA_WEIGHT;
				spdlog::debug("ClusteredTriangulationAlgorithm2: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (in front of centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, cluster_cost);
				total_cost += cluster_cost;
			}
		}
		return total_cost;
	}

} // namespace core

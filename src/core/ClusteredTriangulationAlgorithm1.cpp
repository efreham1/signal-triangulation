#include "ClusteredTriangulationAlgorithm1.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <spdlog/spdlog.h>
#include <optional>

// File-local tunable constants (avoid magic numbers in code)
namespace
{
	// clustering
	static double DEFAULT_COALITION_DISTANCE_METERS = 1.0; // meters used to coalesce nearby points
	static unsigned int CLUSTER_MIN_POINTS = 4u;			 // minimum points to form a cluster (3 points needed for AoA estimation)
	static double CLUSTER_RATIO_SPLIT_THRESHOLD = 0.25;	 // geometric ratio threshold to split cluster

	// numeric tolerances
	static double NORMAL_REGULARIZATION_EPS = 1e-12; // regularize normal equations diagonal
	static double GAUSS_ELIM_PIVOT_EPS = 1e-15;	   // pivot threshold for Gaussian elimination
}

namespace core
{

	ClusteredTriangulationAlgorithm1::ClusteredTriangulationAlgorithm1() = default;

	ClusteredTriangulationAlgorithm1::~ClusteredTriangulationAlgorithm1() = default;

	void ClusteredTriangulationAlgorithm1::setHyperparameters(
		std::optional<double> coalition_dist_meters,
		std::optional<int> cluster_min_points,
		std::optional<double> cluster_ratio_split_threshold,
		std::optional<double> normal_regularization_eps,
		std::optional<double> gauss_elim_pivot_eps
		)
	{
		if (coalition_dist_meters.has_value())
		{
			DEFAULT_COALITION_DISTANCE_METERS = coalition_dist_meters.value();
		}
		if (cluster_min_points.has_value())
		{
			CLUSTER_MIN_POINTS = static_cast<unsigned int>(cluster_min_points.value());
		}
		if (cluster_ratio_split_threshold.has_value())
		{
			CLUSTER_RATIO_SPLIT_THRESHOLD = cluster_ratio_split_threshold.value();
		}
		if (normal_regularization_eps.has_value())
		{
			NORMAL_REGULARIZATION_EPS = normal_regularization_eps.value();
		}
		if (gauss_elim_pivot_eps.has_value())
		{
			GAUSS_ELIM_PIVOT_EPS = gauss_elim_pivot_eps.value();
		}
		spdlog::info("ClusteredTriangulationAlgorithm1: hyperparameters set: coalition_distance={}, cluster_min_points={}, cluster_ratio_split_threshold={}, gradient_descent_step={}",
					 DEFAULT_COALITION_DISTANCE_METERS, CLUSTER_MIN_POINTS, CLUSTER_RATIO_SPLIT_THRESHOLD);
	}


	void ClusteredTriangulationAlgorithm1::processDataPoint(const DataPoint &point)
	{
		// Basic validation; throw on invalid coordinates to make errors explicit
		if (!point.validCoordinates())
		{
			throw std::invalid_argument("ClusteredTriangulationAlgorithm1: invalid coordinates");
		}

		// insert point into m_points keeping the vector sorted by timestamp_ms (ascending)
		auto it = std::lower_bound(
			m_points.begin(), m_points.end(), point.timestamp_ms,
			[](const DataPoint &a, const int64_t t)
			{ return a.timestamp_ms < t; });
		m_points.insert(it, point);

		spdlog::debug("ClusteredTriangulationAlgorithm1: added DataPoint (x={}, y={}, rssi={}, timestamp={})", point.getX(), point.getY(), point.rssi, point.timestamp_ms);
	}

	std::pair<int64_t, int64_t> ClusteredTriangulationAlgorithm1::makeDistanceKey(int64_t id1, int64_t id2) const
	{
		if (id1 < id2)
		{
			return {id1, id2};
		}
		return {id2, id1};
	}

	void ClusteredTriangulationAlgorithm1::addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance)
	{
		std::pair<int64_t, int64_t> key = makeDistanceKey(p1.point_id, p2.point_id);
		distance_cache.try_emplace(key, distance);
	}

	void ClusteredTriangulationAlgorithm1::reorderDataPointsByDistance()
	{
		if (m_points.size() < 3)
		{
			return; // Too few points
		}

		auto getDistance = [&](const DataPoint &p1, const DataPoint &p2) -> double
		{
			auto it = distance_cache.find(makeDistanceKey(p1.point_id, p2.point_id));
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
		// We look for segments to reverse that reduce total length.
		bool improved = true;
		int iterations = 0;
		const int MAX_ITERATIONS = 100;

		while (improved && iterations < MAX_ITERATIONS)
		{
			improved = false;
			iterations++;

			// Iterate through every possible segment of the path
			// We want to see if swapping edges (i, i+1) and (j, j+1)
			// to (i, j) and (i+1, j+1) improves the cost.
			// This is equivalent to reversing the segment [i+1, j].
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

		spdlog::info("ClusteredTriangulationAlgorithm: optimized path. Length reduced from {:.2f}m to {:.2f}m ({} iterations)",
					 initial_dist, total_dist, iterations);
	}

	void printPointsAndClusters(const std::vector<DataPoint> &points, std::vector<PointCluster> &clusters)
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
			std::cout << "  Cluster " << i << ": centroid_x: " << cluster.centroid_x
					  << ", centroid_y: " << cluster.centroid_y
					  << ", avg_rssi: " << cluster.avg_rssi
					  << ", estimated_aoa: " << cluster.estimated_aoa
					  << ", ratio: " << ratio
					  << ", num_points: " << cluster.points.size() << std::endl;
			// Print points belonging to this cluster: one per line prefixed with 'p' (x y)
			for (const auto &p : cluster.points)
			{
				std::cout << "p " << p.getX() << " " << p.getY() << std::endl;
			}
		}
	}

	void ClusteredTriangulationAlgorithm1::gradientDescent(double &out_x, double &out_y, std::vector<std::pair<double, double>> intersections, double precision, double timeout)
	{
		spdlog::debug("ClusteredTriangulationAlgorithm1: starting gradient descent with {} intersection points", intersections.size());

		double global_best_x = 0.0;
		double global_best_y = 0.0;
		double global_best_cost = std::numeric_limits<double>::max();

		auto start_time = std::chrono::steady_clock::now();

		for (const auto &inter : intersections)
		{
			if (timeout > 0.0)
			{
				auto current_time = std::chrono::steady_clock::now();
				std::chrono::duration<double> elapsed = current_time - start_time;
				if (elapsed.count() > timeout)
				{
					spdlog::warn("ClusteredTriangulationAlgorithm1: timeout reached during gradient descent");
					break;
				}
			}

			spdlog::debug("ClusteredTriangulationAlgorithm1: starting gradient descent from intersection point (x={}, y={})", inter.first, inter.second);
			bool continue_gradient_descent = true;
			double current_x = inter.first;
			double current_y = inter.second;
			double current_cost = getCost(current_x, current_y);

			std::set<std::pair<double, double>> visited_points;

			bool explored_new_point = true;

			while (continue_gradient_descent && explored_new_point)
			{
				if (timeout > 0.0)
				{
					auto current_time = std::chrono::steady_clock::now();
					std::chrono::duration<double> elapsed = current_time - start_time;
					if (elapsed.count() > timeout)
					{
						spdlog::warn("ClusteredTriangulationAlgorithm1: timeout reached during gradient descent loop");
						continue_gradient_descent = false;
						break;
					}
				}

				// Check neighboring points in a grid
				double best_cost = current_cost;
				double best_x = current_x;
				double best_y = current_y;
				explored_new_point = false;

				for (int dx = -1; dx <= 1; ++dx)
				{
					for (int dy = -1; dy <= 1; ++dy)
					{
						if (dx == 0 && dy == 0)
							continue; // Skip the center point
						double x = current_x + dx * precision;
						double y = current_y + dy * precision;
						if (visited_points.count({x, y}) > 0)
						{
							continue; // Already visited this point
						}
						visited_points.insert({x, y});
						explored_new_point = true;

						double neighbor_x = x;
						double neighbor_y = y;
						double neighbor_cost = getCost(neighbor_x, neighbor_y);

						if (neighbor_cost <= best_cost)
						{
							best_cost = neighbor_cost;
							best_x = neighbor_x;
							best_y = neighbor_y;
						}
					}
				}

				if (best_cost <= current_cost)
				{
					current_x = best_x;
					current_y = best_y;
					current_cost = best_cost;
				}
				else
				{
					continue_gradient_descent = false; // No better neighbors found
				}
			}
			if (current_cost < global_best_cost)
			{
				global_best_cost = current_cost;
				global_best_x = current_x;
				global_best_y = current_y;
			}
			else if (std::abs(current_cost - global_best_cost) < std::numeric_limits<double>::epsilon())
			{
				spdlog::warn("ClusteredTriangulationAlgorithm1: multiple local minima found with the same cost value.");
			}
			spdlog::debug("ClusteredTriangulationAlgorithm1: gradient descent from intersection (x={}, y={}) found local minimum at (x={}, y={}) with cost {}", inter.first, inter.second, current_x, current_y, current_cost);
		}
		out_x = global_best_x;
		out_y = global_best_y;

		spdlog::info("ClusteredTriangulationAlgorithm1: gradient descent completed, global minimum at (x={}, y={}) with cost {}", out_x, out_y, global_best_cost);
	}

	void ClusteredTriangulationAlgorithm1::calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout)
	{
		reorderDataPointsByDistance();

		clusterData();
		estimateAoAForClusters();

		std::vector<std::pair<double, double>> intersections = findIntersections();
		if (intersections.empty())
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm1: no intersections found between cluster AoA lines");
		}

		double global_best_x = 0.0;
		double global_best_y = 0.0;

		gradientDescent(global_best_x, global_best_y, intersections, precision, timeout);

		// print x and y of resulting point
		if (plottingEnabled)
		{
			printPointsAndClusters(m_points, m_clusters);
			std::cout << "Resulting point after gradient descent: x=" << global_best_x << ", y=" << global_best_y << std::endl;
		}

		DataPoint result_point;
		result_point.setX(global_best_x);
		result_point.setY(global_best_y);
		result_point.zero_latitude = m_points[0].zero_latitude;
		result_point.zero_longitude = m_points[0].zero_longitude;
		result_point.computeCoordinates();
		if (!result_point.validCoordinates())
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm1: computed invalid coordinates");
		}
		out_latitude = result_point.getLatitude();
		out_longitude = result_point.getLongitude();
	}

	void ClusteredTriangulationAlgorithm1::reset()
	{
		m_points.clear();
		m_clusters.clear();
		distance_cache.clear();
	}

	void ClusteredTriangulationAlgorithm1::clusterData()
	{
		const double coalition_distance = DEFAULT_COALITION_DISTANCE_METERS; // meters
		unsigned int cluster_id = 0;
		unsigned int current_cluster_size = 0;
		for (const auto &point : m_points)
		{
			if (current_cluster_size == 0)
			{
				m_clusters.emplace_back();
			}

			m_clusters[cluster_id].addPoint(point, coalition_distance);
			current_cluster_size = static_cast<unsigned int>(m_clusters[cluster_id].points.size());
			auto &c = m_clusters[cluster_id];
			if (c.geometricRatio() > CLUSTER_RATIO_SPLIT_THRESHOLD && current_cluster_size >= CLUSTER_MIN_POINTS)
			{
				spdlog::debug("ClusteredTriangulationAlgorithm1: created new cluster (id={}) after splitting cluster (id={}) due to geometric ratio {}", cluster_id + 1, cluster_id, c.geometricRatio());
				cluster_id++;
				current_cluster_size = 0;
			}
		}
		spdlog::info("ClusteredTriangulationAlgorithm1: formed {} clusters from {} data points", m_clusters.size(), m_points.size());
		if (m_clusters.size() != cluster_id)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm1: last cluster formed does not meet requirements");
		}
		if (m_clusters.size() < 2)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm1: insufficient clusters formed for AoA estimation");
		}
		else if (m_clusters.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm1: only {} clusters formed; AoA estimation may be unreliable", m_clusters.size());
		}
	}

	std::vector<double> getNormalVector(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z)
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

	void ClusteredTriangulationAlgorithm1::estimateAoAForClusters()
	{
		// column-major order: [x1, y1, rssi1, x2, y2, rssi2, ...]
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
			std::vector<double> normal = getNormalVector(X, Y, Z);
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

			spdlog::info("ClusteredTriangulationAlgorithm1: cluster AoA estimated at {} degrees (grad_x={}, grad_y={})", cluster.estimated_aoa, grad_x, grad_y);
		}
		// Estimate AoA as the arctangent of the gradient
	}

	// Should return a list of intersection points between all cluster AoA lines
	std::vector<std::pair<double, double>> ClusteredTriangulationAlgorithm1::findIntersections()
	{
		std::vector<std::pair<double, double>> intersections;
		for (size_t i = 0; i < m_clusters.size(); ++i)
		{
			for (size_t j = i + 1; j < m_clusters.size(); ++j)
			{
				// m_clusters[i].centroid_x + t1 * m_clusters[i].aoa_x = m_clusters[j].centroid_x + t2 * m_clusters[j].aoa_x
				// m_clusters[i].centroid_y + t1 * m_clusters[i].aoa_y = m_clusters[j].centroid_y + t2 * m_clusters[j].aoa_y
				double a1 = m_clusters[i].aoa_x;
				double b1 = -m_clusters[j].aoa_x;
				double c1 = m_clusters[j].centroid_x - m_clusters[i].centroid_x;

				double a2 = m_clusters[i].aoa_y;
				double b2 = -m_clusters[j].aoa_y;
				double c2 = m_clusters[j].centroid_y - m_clusters[i].centroid_y;

				double denom = a1 * b2 - a2 * b1;
				if (std::abs(denom) < std::numeric_limits<double>::epsilon())
				{
					continue; // Lines are parallel or nearly parallel
				}

				double t1 = (c1 * b2 - c2 * b1) / denom;
				double t2 = (a1 * c2 - a2 * c1) / denom;

				if (t1 < 0 || t2 < 0)
				{
					continue; // Intersection is behind one of the clusters
				}

				double intersect_x = m_clusters[i].centroid_x + t1 * m_clusters[i].aoa_x;
				double intersect_y = m_clusters[i].centroid_y + t1 * m_clusters[i].aoa_y;
				intersections.emplace_back(intersect_x, intersect_y);

				spdlog::debug("ClusteredTriangulationAlgorithm1: found intersection between cluster {} and {} at (x={}, y={})", i, j, intersect_x, intersect_y);
			}
		}
		if (intersections.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm1: only {} intersections found between cluster AoA lines", intersections.size());
		}
		return intersections;
	}

	double ClusteredTriangulationAlgorithm1::getCost(double x, double y)
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
				total_cost += -dot_prod / cluster_grad_mag + std::sqrt(point_to_centroid[0] * point_to_centroid[0] + point_to_centroid[1] * point_to_centroid[1]);
				spdlog::debug("ClusteredTriangulationAlgorithm1: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (behind centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, total_cost);
			}
			else
			{
				double distance = cross_prod_mag / cluster_grad_mag;
				total_cost += distance;
				spdlog::debug("ClusteredTriangulationAlgorithm1: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (in front of centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, total_cost);
			}
		}
		return total_cost;
	}

} // namespace core

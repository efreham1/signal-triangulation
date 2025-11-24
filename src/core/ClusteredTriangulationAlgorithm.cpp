#include "ClusteredTriangulationAlgorithm.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <spdlog/spdlog.h>

// File-local tunable constants (avoid magic numbers in code)
namespace
{
	// clustering
	static constexpr double DEFAULT_COALITION_DISTANCE_METERS = 1.0; // meters used to coalesce nearby points
	static constexpr unsigned int CLUSTER_MIN_POINTS = 4u;			 // minimum points to form a cluster (3 points needed for AoA estimation)
	static constexpr double CLUSTER_RATIO_SPLIT_THRESHOLD = 0.35;	 // geometric ratio threshold to split cluster

	// optimization / search
	static constexpr double GRADIENT_DESCENT_STEP_METERS = 0.1; // step size for grid-based gradient descent

	// numeric tolerances
	static constexpr double NORMAL_REGULARIZATION_EPS = 1e-12; // regularize normal equations diagonal
	static constexpr double GAUSS_ELIM_PIVOT_EPS = 1e-15;	   // pivot threshold for Gaussian elimination

	static constexpr double ISOLATION_THRESHOLD_METERS = 5.0; // Points further than this from ANY other point are discarded
}

namespace core
{

	ClusteredTriangulationAlgorithm::ClusteredTriangulationAlgorithm() = default;

	ClusteredTriangulationAlgorithm::~ClusteredTriangulationAlgorithm() = default;

	void ClusteredTriangulationAlgorithm::processDataPoint(const DataPoint &point)
	{
		// Basic validation; throw on invalid coordinates to make errors explicit
		if (!point.validCoordinates())
		{
			throw std::invalid_argument("ClusteredTriangulationAlgorithm: invalid coordinates");
		}

		// insert point into m_points keeping the vector sorted by timestamp_ms (ascending)
		auto it = std::lower_bound(
			m_points.begin(), m_points.end(), point.timestamp_ms,
			[](const DataPoint &a, const int64_t t)
			{ return a.timestamp_ms < t; });
		m_points.insert(it, point);

		spdlog::debug("ClusteredTriangulationAlgorithm: added DataPoint (x={}, y={}, rssi={}, timestamp={})", point.getX(), point.getY(), point.rssi, point.timestamp_ms);
	}

	void ClusteredTriangulationAlgorithm::addToDistanceCache(const DataPoint &p1, const DataPoint &p2, double distance,
															 std::unordered_map<std::pair<int64_t, int64_t>, double, ClusteredTriangulationAlgorithm::PairHash> &cache)
	{
		if (cache.find({p1.timestamp_ms, p2.timestamp_ms}) != cache.end())
		{
			return; // already cached
		}

		cache[{p1.timestamp_ms, p2.timestamp_ms}] = distance;
		cache[{p2.timestamp_ms, p1.timestamp_ms}] = distance;
	}

	void ClusteredTriangulationAlgorithm::reorderDataPointsByDistance()
	{
		if (m_points.empty())
		{
			return;
		}

		std::vector<DataPoint> points_to_process = m_points;
		std::vector<DataPoint> ordered_points;
		ordered_points.reserve(points_to_process.size());
		std::vector<bool> used(points_to_process.size(), false);

		// Start with the first point (preserves original time-based start if valid)
		ordered_points.push_back(points_to_process[0]);
		used[0] = true;

		while (ordered_points.size() < points_to_process.size())
		{
			const DataPoint &last_point = ordered_points.back();
			double best_dist = std::numeric_limits<double>::max();
			size_t best_idx = static_cast<size_t>(-1);

			for (size_t i = 0; i < points_to_process.size(); ++i)
			{
				if (used[i])
					continue;

				double dx = last_point.getX() - points_to_process[i].getX();
				double dy = last_point.getY() - points_to_process[i].getY();
				double dist = std::sqrt(dx * dx + dy * dy);
				
				addToDistanceCache(last_point, points_to_process[i], dist, distance_cache);

				if (dist < best_dist)
				{
					best_dist = dist;
					best_idx = i;
				}
			}

			if (best_idx != static_cast<size_t>(-1))
			{
				ordered_points.push_back(points_to_process[best_idx]);
				used[best_idx] = true;
			}
			else
			{
				break; // Should not happen
			}
		}

		m_points = std::move(ordered_points);
		spdlog::info("ClusteredTriangulationAlgorithm: reordered {} points using memoized distances.", m_points.size());
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

	void ClusteredTriangulationAlgorithm::gradientDescent(double &out_x, double &out_y, std::vector<std::pair<double, double>> intersections)
	{
		spdlog::debug("ClusteredTriangulationAlgorithm: starting gradient descent with {} intersection points", intersections.size());
		double resolution = GRADIENT_DESCENT_STEP_METERS;

		double global_best_x = 0.0;
		double global_best_y = 0.0;
		double global_best_cost = std::numeric_limits<double>::max();

		for (const auto &inter : intersections)
		{
			spdlog::debug("ClusteredTriangulationAlgorithm: starting gradient descent from intersection point (x={}, y={})", inter.first, inter.second);
			bool continue_gradient_descent = true;
			double current_x = inter.first;
			double current_y = inter.second;
			double current_cost = getCost(current_x, current_y);

			std::set<std::pair<double, double>> visited_points;

			bool explored_new_point = true;

			while (continue_gradient_descent && explored_new_point)
			{
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
						double x = current_x + dx * resolution;
						double y = current_y + dy * resolution;
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
				spdlog::warn("ClusteredTriangulationAlgorithm: multiple local minima found with the same cost value.");
			}
			spdlog::debug("ClusteredTriangulationAlgorithm: gradient descent from intersection (x={}, y={}) found local minimum at (x={}, y={}) with cost {}", inter.first, inter.second, current_x, current_y, current_cost);
		}
		out_x = global_best_x;
		out_y = global_best_y;

		spdlog::info("ClusteredTriangulationAlgorithm: gradient descent completed, global minimum at (x={}, y={}) with cost {}", out_x, out_y, global_best_cost);
	}

	void ClusteredTriangulationAlgorithm::calculatePosition(double &out_latitude, double &out_longitude)
	{
		reorderDataPointsByDistance();

		// Print all entries in the distance cache for debugging
		for (const auto &entry : distance_cache)
		{
			spdlog::info("Distance cache: points ({}, {}) -> distance {}", entry.first.first, entry.first.second, entry.second);
		}

		clusterData();
		estimateAoAForClusters();

		std::vector<std::pair<double, double>> intersections = findIntersections();
		if (intersections.empty())
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm: no intersections found between cluster AoA lines");
		}

		double global_best_x = 0.0;
		double global_best_y = 0.0;

		gradientDescent(global_best_x, global_best_y, intersections);

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
			throw std::runtime_error("ClusteredTriangulationAlgorithm: computed invalid coordinates");
		}
		out_latitude = result_point.getLatitude();
		out_longitude = result_point.getLongitude();
	}

	void ClusteredTriangulationAlgorithm::reset()
	{
		m_points.clear();
		m_clusters.clear();
	}

	void ClusteredTriangulationAlgorithm::clusterData()
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
				spdlog::debug("ClusteredTriangulationAlgorithm: created new cluster (id={}) after splitting cluster (id={}) due to geometric ratio {}", cluster_id + 1, cluster_id, c.geometricRatio());
				cluster_id++;
				current_cluster_size = 0;
			}
		}
		spdlog::info("ClusteredTriangulationAlgorithm: formed {} clusters from {} data points", m_clusters.size(), m_points.size());
		if (m_clusters.size() != cluster_id)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm: last cluster formed does not meet requirements");
		}
		if (m_clusters.size() < 2)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm: insufficient clusters formed for AoA estimation");
		}
		else if (m_clusters.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm: only {} clusters formed; AoA estimation may be unreliable", m_clusters.size());
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

	void ClusteredTriangulationAlgorithm::estimateAoAForClusters()
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

			spdlog::info("ClusteredTriangulationAlgorithm: cluster AoA estimated at {} degrees (grad_x={}, grad_y={})", cluster.estimated_aoa, grad_x, grad_y);
		}
		// Estimate AoA as the arctangent of the gradient
	}

	// Should return a list of intersection points between all cluster AoA lines
	std::vector<std::pair<double, double>> ClusteredTriangulationAlgorithm::findIntersections()
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

				spdlog::debug("ClusteredTriangulationAlgorithm: found intersection between cluster {} and {} at (x={}, y={})", i, j, intersect_x, intersect_y);
			}
		}
		if (intersections.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm: only {} intersections found between cluster AoA lines", intersections.size());
		}
		return intersections;
	}

	double ClusteredTriangulationAlgorithm::getCost(double x, double y)
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
				spdlog::debug("ClusteredTriangulationAlgorithm: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (behind centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, total_cost);
			}
			else
			{
				double distance = cross_prod_mag / cluster_grad_mag;
				total_cost += distance;
				spdlog::debug("ClusteredTriangulationAlgorithm: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (in front of centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, total_cost);
			}
		}
		return total_cost;
	}

} // namespace core

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
	static double DEFAULT_COALITION_DISTANCE_METERS = 2.0; // meters used to coalesce nearby points
	static unsigned int CLUSTER_MIN_POINTS = 3u;			 // minimum points to form a cluster (3 points needed for AoA estimation)
	static double CLUSTER_RATIO_SPLIT_THRESHOLD = 0.25;	 // geometric ratio threshold to split cluster

	
	// brute force search
	static constexpr int HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS = 500; // half the number of discrete steps per axis in brute force search

	// numeric tolerances
	static double NORMAL_REGULARIZATION_EPS = 1e-12; // regularize normal equations diagonal
	static double GAUSS_ELIM_PIVOT_EPS = 1e-15;	   // pivot threshold for Gaussian elimination
}

namespace core
{

	ClusteredTriangulationAlgorithm2::ClusteredTriangulationAlgorithm2() = default;

	ClusteredTriangulationAlgorithm2::~ClusteredTriangulationAlgorithm2() = default;


	void ClusteredTriangulationAlgorithm2::setHyperparameters(
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
		spdlog::info("ClusteredTriangulationAlgorithm2: hyperparameters set: coalition_distance={}, cluster_min_points={}, cluster_ratio_split_threshold={}, gradient_descent_step={}",
					 DEFAULT_COALITION_DISTANCE_METERS, CLUSTER_MIN_POINTS, CLUSTER_RATIO_SPLIT_THRESHOLD);
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
				spdlog::debug("ClusteredTriangulationAlgorithm2: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (behind centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, cluster_cost);
				total_cost += cluster_cost;
			}
			else
			{
				double cluster_cost = cross_prod_mag / cluster_grad_mag;
				spdlog::debug("ClusteredTriangulationAlgorithm2: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (in front of centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, cluster_cost);
				total_cost += cluster_cost;
			}
		}
		return total_cost;
	}

} // namespace core

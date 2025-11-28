#include "ClusteredTriangulationAlgorithm2.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <chrono>
#include <random>
#include <spdlog/spdlog.h>

namespace core
{

	ClusteredTriangulationAlgorithm2::ClusteredTriangulationAlgorithm2() = default;
	ClusteredTriangulationAlgorithm2::~ClusteredTriangulationAlgorithm2() = default;

	void ClusteredTriangulationAlgorithm2::calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout)
	{
		reorderDataPointsByDistance();
		clusterData();
		estimateAoAForClusters();

		double global_best_x = 0.0;
		double global_best_y = 0.0;

		bruteForceSearch(global_best_x, global_best_y, precision, timeout);

		if (plottingEnabled)
		{
			printPointsAndClusters();
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

	static std::vector<int> strideOrder(int n)
	{
		std::vector<int> order;
		order.reserve(n);

		// Choose stride as roughly sqrt(n) or a prime-like value
		int stride = std::max(2, static_cast<int>(std::sqrt(n)));
		// Make stride coprime with n for full coverage
		while (std::gcd(stride, n) != 1 && stride < n)
		{
			stride++;
		}

		int current = 0;
		for (int i = 0; i < n; ++i)
		{
			order.push_back(current);
			current = (current + stride) % n;
		}

		return order;
	}

	void ClusteredTriangulationAlgorithm2::findBestClusters()
	{
		double total_time_left = CLUSTER_FORMATION_TIMEOUT;

		int n_points = static_cast<int>(m_points.size());
		std::vector<int> point_order = strideOrder(n_points);

		auto start_time = std::chrono::steady_clock::now();
		for (int idx = 0; idx < n_points; ++idx)
		{
			int i = point_order[idx];
			auto current_time = std::chrono::steady_clock::now();
			std::chrono::duration<double> elapsed = current_time - start_time;
			total_time_left = CLUSTER_FORMATION_TIMEOUT - elapsed.count();

			double alloted_time = total_time_left / static_cast<double>(n_points - idx);
			bool found_valid = false;

			PointCluster best_cluster = PointCluster();
			double best_score = -std::numeric_limits<double>::max();
			int best_seed_index = -1;
			// Check timeout inside seed loop

			std::vector<int> candidate_indices;

			// Find ALL other points within distance
			getCandidates(i, candidate_indices);

			int n_candidates = static_cast<int>(candidate_indices.size());
			int min_additional = getClusterMinPoints() - 1;

			spdlog::info("ClusteredTriangulationAlgorithm2: forming clusters with seed point index {}, {} candidates found, alloted time {:.2f}s",
						 i, n_candidates, alloted_time);
			PointCluster cluster = PointCluster();
			cluster.addPoint(m_points[i]); // seed

			auto combinations_start_time = std::chrono::steady_clock::now();
			// Generate and evaluate combinations ON-THE-FLY
			for (int k = min_additional; k <= n_candidates; ++k)
			{
				bool timeout_reached = false;
				std::vector<int> indices(k);

				cluster = PointCluster();
				cluster.addPoint(m_points[i]); // seed

				// Initialize indices for combination of size k
				for (int idx = 0; idx < k; ++idx)
				{
					cluster.addPoint(m_points[candidate_indices[idx]]);
					indices[idx] = idx;
				}

				while (true)
				{
					// Evaluate current combination
					if (checkCluster(cluster, best_cluster, best_score, best_seed_index, i))
					{
						found_valid = true;
					}

					auto current_time = std::chrono::steady_clock::now();
					std::chrono::duration<double> elapsed = current_time - combinations_start_time;
					if (elapsed.count() > alloted_time)
					{
						spdlog::warn("ClusteredTriangulationAlgorithm2: timeout reached during cluster formation inside combinations ({:.2f}s)", elapsed.count());
						timeout_reached = true;
						break;
					}

					int pos = k - 1;
					while (pos >= 0 && indices[pos] == n_candidates - k + pos)
					{
						--pos;
					}

					if (pos < 0)
					{
						break;
					}

					// Increment and reset all following elements
					cluster.removePoint(m_points[candidate_indices[indices[pos]]]);
					indices[pos]++;
					cluster.addPoint(m_points[candidate_indices[indices[pos]]]);
					for (int j = pos + 1; j < k; ++j)
					{
						cluster.removePoint(m_points[candidate_indices[indices[j]]]);
						indices[j] = indices[j - 1] + 1;
						cluster.addPoint(m_points[candidate_indices[indices[j]]]);
					}
				}
				if (timeout_reached)
				{
					break;
				}
			}

			if (found_valid)
			{
				best_cluster.getAndSetScore(
					IDEAL_GEOMETRIC_RATIO_FOR_BEST_CLUSTER,
					IDEAL_AREA_FOR_BEST_CLUSTER,
					MIN_RSSI_VARIANCE_FOR_BEST_CLUSTER,
					WEIGHT_GEOMETRIC_RATIO,
					WEIGHT_AREA,
					WEIGHT_RSSI_VARIANCE,
					BOTTOM_RSSI_FOR_BEST_CLUSTER,
					WEIGHT_RSSI);
				m_clusters.push_back(best_cluster);
				spdlog::info("ClusteredTriangulationAlgorithm2: formed cluster with {} points, geometric ratio {:.3f}, area {:.2f}, seed index {}",
							 best_cluster.size(), best_cluster.geometricRatio(), best_cluster.area(), best_seed_index);
			}
			else
			{
				spdlog::info("ClusteredTriangulationAlgorithm2: no valid clusters found with seed point index {}", i);
			}
		}
	}

	void ClusteredTriangulationAlgorithm2::getCandidates(int i, std::vector<int> &candidate_indices)
	{
		for (int j = 0; j < static_cast<int>(m_points.size()); ++j)
		{
			if (i == j)
			{
				continue;
			}

			double distance = getDistance(m_points[i], m_points[j]);
			if (distance <= MAX_INTERNAL_CLUSTER_DISTANCE)
			{
				candidate_indices.push_back(j);
			}
		}
	}

	bool ClusteredTriangulationAlgorithm2::checkCluster(PointCluster &cluster, PointCluster &best_cluster, double &best_score, int &best_seed_index, int i)
	{
		double ratio = cluster.geometricRatio();
		double clusterArea = cluster.area();
		bool found_valid_for_this_seed = false;

		bool valid = (ratio >= MIN_GEOMETRIC_RATIO_FOR_BEST_CLUSTER &&
					  clusterArea >= MIN_AREA_FOR_BEST_CLUSTER &&
					  clusterArea <= MAX_AREA_FOR_BEST_CLUSTER);

		if (valid)
		{
			// Check overlap with existing clusters
			bool has_overlap = false;
			for (const auto &existing_cluster : m_clusters)
			{
				if (cluster.overlapWith(existing_cluster) > MAX_OVERLAP_BETWEEN_CLUSTERS)
				{
					has_overlap = true;
					break;
				}
			}

			if (!has_overlap)
			{
				found_valid_for_this_seed = true;
				double current_score = cluster.getAndSetScore(
					IDEAL_GEOMETRIC_RATIO_FOR_BEST_CLUSTER,
					IDEAL_AREA_FOR_BEST_CLUSTER,
					MIN_RSSI_VARIANCE_FOR_BEST_CLUSTER,
					WEIGHT_GEOMETRIC_RATIO,
					WEIGHT_AREA,
					WEIGHT_RSSI_VARIANCE,
					BOTTOM_RSSI_FOR_BEST_CLUSTER,
					WEIGHT_RSSI);

				if (current_score > best_score)
				{
					best_score = current_score;
					best_seed_index = i;
					best_cluster = PointCluster();
					for (const auto &pt : cluster.points)
					{
						best_cluster.addPoint(pt);
					}
				}
			}
		}
		return found_valid_for_this_seed;
	}

	void ClusteredTriangulationAlgorithm2::clusterData()
	{
		coalescePoints(getCoalitionDistance());

		findBestClusters();

		spdlog::info("ClusteredTriangulationAlgorithm2: formed {} clusters from {} data points", m_clusters.size(), m_points.size());

		if (m_clusters.size() < 2)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm2: insufficient clusters formed for AoA estimation");
		}
		else if (m_clusters.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm2: only {} clusters formed; AoA estimation may be unreliable", m_clusters.size());
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
		std::set<std::pair<double, double>> visited_quadrants;

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
				double quadrant_x = zone_x + (q % 2) * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS * precision;
				double quadrant_y = zone_y + (q / 2) * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS * precision;

				if (visited_quadrants.find({quadrant_x, quadrant_y}) != visited_quadrants.end())
				{
					continue;
				}
				visited_quadrants.insert({quadrant_x, quadrant_y});

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

			spdlog::info("ClusteredTriangulationAlgorithm2: brute force search iteration found best point (x={}, y={}) with cost {}",
						 best_x, best_y, best_cost);

			if (best_cost < global_best_cost)
			{
				global_best_cost = best_cost;
				global_best_x = best_x;
				global_best_y = best_y;

				zone_x += precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS *
						  (best_x < zone_x + precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS ? -1.0 : 1.0);
				zone_y += precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS *
						  (best_y < zone_y + precision * HALF_SQUARE_SIZE_NUMBER_OF_PRECISIONS ? -1.0 : 1.0);
			}
			else
			{
				break;
			}
		}

		spdlog::info("ClusteredTriangulationAlgorithm2: brute force search completed with best point (x={}, y={}) and cost {}",
					 global_best_x, global_best_y, global_best_cost);
	}

} // namespace core
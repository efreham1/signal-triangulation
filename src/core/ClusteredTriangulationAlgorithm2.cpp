#include "ClusteredTriangulationAlgorithm2.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <chrono>
#include <random>
#include <spdlog/spdlog.h>

#ifdef USE_OPENMP
#include <omp.h>
#endif

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
		auto clustering_start = std::chrono::high_resolution_clock::now();
		m_combinations_explored = 0;
		m_clusters_evaluated = 0;

		int n_points = static_cast<int>(m_points.size());
		std::vector<int> point_order = strideOrder(n_points);

		// Pre-allocate per-seed metrics (thread-safe access by index)
		std::vector<size_t> combinations_per_seed(n_points, 0);
		std::vector<double> time_per_seed_ms(n_points, 0.0);
		std::vector<int> candidates_per_seed(n_points, 0);
		std::vector<bool> seed_timed_out(n_points, false);

		// Thread-local results to avoid lock contention
		std::vector<std::pair<PointCluster, bool>> seed_results(n_points);

		// Calculate per-seed timeout (divide total timeout among seeds)
		double per_seed_timeout = PER_SEED_TIMEOUT;

#ifdef USE_OPENMP
		int num_threads = omp_get_max_threads();
		spdlog::info("ClusteredTriangulationAlgorithm2: using OpenMP with {} threads, per-seed timeout: {:.2f}s",
					 num_threads, per_seed_timeout);
#else
		spdlog::info("ClusteredTriangulationAlgorithm2: running single-threaded (OpenMP not available), per-seed timeout: {:.2f}s",
					 per_seed_timeout);
#endif

// Parallel loop over seed points
#ifdef USE_OPENMP
#pragma omp parallel for schedule(dynamic, 1)
#endif
		for (int idx = 0; idx < n_points; ++idx)
		{
			auto seed_start_time = std::chrono::high_resolution_clock::now();
			size_t seed_combinations = 0;
			bool timeout_reached = false;

			int i = point_order[idx];

			PointCluster best_cluster;
			double best_score = -std::numeric_limits<double>::max();
			bool found_valid = false;

			std::vector<int> candidate_indices;

			// Does not create a race condition since all distances are calculated beforehand
			// TODO: Could make an explicitly thread-safe version
			getCandidates(i, candidate_indices);

			int n_candidates = static_cast<int>(candidate_indices.size());
			candidates_per_seed[idx] = n_candidates;

			// Skip if not enough candidates
			if (n_candidates < static_cast<int>(getClusterMinPoints()) - 1)
			{
				seed_results[idx] = {PointCluster(), false};
				time_per_seed_ms[idx] = 0.0;
				combinations_per_seed[idx] = 0;
				continue;
			}

			PointCluster cluster;
			cluster.addPoint(m_points[i]); // seed

			std::vector<int> current_selection;
			current_selection.reserve(n_candidates);

			std::vector<int> stack;
			stack.push_back(0);

			// DFS exploration with timeout
			while (!stack.empty() && !timeout_reached)
			{
				// Check timeout periodically (every 100 iterations to reduce overhead)
				if (seed_combinations % 100 == 0)
				{
					auto current_time = std::chrono::high_resolution_clock::now();
					double elapsed_s = std::chrono::duration<double>(current_time - seed_start_time).count();
					if (elapsed_s > per_seed_timeout)
					{
						timeout_reached = true;
						seed_timed_out[idx] = true;
						break;
					}
				}

				int candidate_idx = stack.back();

				if (candidate_idx >= n_candidates)
				{
					stack.pop_back();
					if (!current_selection.empty())
					{
						cluster.removePoint(m_points[candidate_indices[current_selection.back()]]);
						current_selection.pop_back();
					}
					if (!stack.empty())
					{
						stack.back()++;
					}
					continue;
				}

				current_selection.push_back(candidate_idx);
				cluster.addPoint(m_points[candidate_indices[candidate_idx]]);

				int cluster_size = static_cast<int>(current_selection.size()) + 1;
				if (cluster_size >= static_cast<int>(getClusterMinPoints()))
				{
					seed_combinations++;
					// Evaluate cluster (thread-local, no locking needed here)
					if (checkCluster(cluster, best_cluster, best_score))
					{
						found_valid = true;
					}
				}

				if (candidate_idx + 1 < n_candidates)
				{
					stack.push_back(candidate_idx + 1);
				}
				else
				{
					cluster.removePoint(m_points[candidate_indices[current_selection.back()]]);
					current_selection.pop_back();
					stack.back()++;
				}
			}

			// Record per-seed metrics
			auto seed_end_time = std::chrono::high_resolution_clock::now();
			time_per_seed_ms[idx] = std::chrono::duration<double, std::milli>(seed_end_time - seed_start_time).count();
			combinations_per_seed[idx] = seed_combinations;

			// Store result for this seed
			seed_results[idx] = {best_cluster, found_valid};

			// Update atomic counters
			m_combinations_explored += seed_combinations;
		}

		// Count timeouts
		int total_timeouts = static_cast<int>(std::count(seed_timed_out.begin(), seed_timed_out.end(), true));
		if (total_timeouts > 0)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm2: {} seeds timed out (using best cluster found before timeout)",
						 total_timeouts);
		}

		// Sequential post-processing: merge results and check overlaps
		// TODO: In case there is a lot of overlap, consider a more sophisticated merging strategy
		for (int idx = 0; idx < n_points; ++idx)
		{
			const auto &[cluster, found_valid] = seed_results[idx];

			if (found_valid)
			{
				// Check overlap with already-added clusters
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
					m_clusters.push_back(cluster);
					m_clusters_evaluated++;
				}
			}
		}

		// Store metrics
		m_combinations_per_seed = std::move(combinations_per_seed);
		m_time_per_seed_ms = std::move(time_per_seed_ms);
		m_candidates_per_seed = std::move(candidates_per_seed);
		m_seed_timed_out = std::move(seed_timed_out);

		auto clustering_end = std::chrono::high_resolution_clock::now();
		m_clustering_time_ms = std::chrono::duration<double, std::milli>(clustering_end - clustering_start).count();

		// Log performance summary
		logPerformanceSummary();
	}

	void ClusteredTriangulationAlgorithm2::logPerformanceSummary()
	{
		spdlog::info("ClusteredTriangulationAlgorithm2: === Performance Summary ===");
		spdlog::info("  Total combinations explored: {}", m_combinations_explored.load());
		spdlog::info("  Total clustering time: {:.2f} ms", m_clustering_time_ms);
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

	bool ClusteredTriangulationAlgorithm2::checkCluster(PointCluster &cluster, PointCluster &best_cluster, double &best_score)
	{
		double ratio = cluster.geometricRatio();
		double clusterArea = cluster.area();
		bool found_valid = false;

		bool valid = (ratio >= MIN_GEOMETRIC_RATIO_FOR_BEST_CLUSTER &&
					  clusterArea >= MIN_AREA_FOR_BEST_CLUSTER &&
					  clusterArea <= MAX_AREA_FOR_BEST_CLUSTER);

		if (valid)
		{
			found_valid = true;
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
				best_cluster = PointCluster();
				for (const auto &pt : cluster.points)
				{
					best_cluster.addPoint(pt);
				}
			}
		}
		return found_valid;
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
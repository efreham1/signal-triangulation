#include "ClusteredTriangulationAlgorithm2.h"
#include "PointDistanceCache.hpp"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <chrono>
#include <random>
#include <mutex>
#include <shared_mutex>
#include <spdlog/spdlog.h>

#ifdef USE_OPENMP
#include <omp.h>
#endif

namespace core
{

	ClusteredTriangulationAlgorithm2::ClusteredTriangulationAlgorithm2() = default;
	ClusteredTriangulationAlgorithm2::ClusteredTriangulationAlgorithm2(const AlgorithmParameters &params)
	{
		applyParameters(params);
	}
	ClusteredTriangulationAlgorithm2::~ClusteredTriangulationAlgorithm2() = default;

	void ClusteredTriangulationAlgorithm2::applyParameters(const AlgorithmParameters &params)
	{
		// Only override if parameter was explicitly set
		// This keeps defaults from member initialization

		if (params.has("per_seed_timeout"))
			m_per_seed_timeout = params.get<double>("per_seed_timeout");

		if (params.has("grid_half_size"))
			m_grid_half_size = params.get<int>("grid_half_size");

		if (params.has("coalition_distance"))
			m_coalition_distance = params.get<double>("coalition_distance");

		if (params.has("cluster_min_points"))
			m_cluster_min_points = static_cast<unsigned int>(params.get<int>("cluster_min_points"));

		if (params.has("max_internal_distance"))
			m_max_internal_distance = params.get<int>("max_internal_distance");

		if (params.has("min_geometric_ratio"))
			m_min_geometric_ratio = params.get<double>("min_geometric_ratio");

		if (params.has("ideal_geometric_ratio"))
			m_ideal_geometric_ratio = params.get<double>("ideal_geometric_ratio");

		if (params.has("max_geometric_ratio"))
			m_max_geometric_ratio = params.get<double>("max_geometric_ratio");

		if (params.has("min_area"))
			m_min_area = params.get<double>("min_area");

		if (params.has("ideal_area"))
			m_ideal_area = params.get<double>("ideal_area");

		if (params.has("max_area"))
			m_max_area = params.get<double>("max_area");

		if (params.has("min_rssi_variance"))
			m_min_rssi_variance = params.get<double>("min_rssi_variance");

		if (params.has("max_rssi_variance"))
			m_max_rssi_variance = params.get<double>("max_rssi_variance");

		if (params.has("ideal_rssi_variance"))
			m_ideal_rssi_variance = params.get<double>("ideal_rssi_variance");

		if (params.has("bottom_rssi"))
			m_bottom_rssi = params.get<double>("bottom_rssi");

		if (params.has("top_rssi"))
			m_top_rssi = params.get<double>("top_rssi");

		if (params.has("max_overlap"))
			m_max_overlap = params.get<double>("max_overlap");

		if (params.has("weight_geometric_ratio"))
			m_weight_geometric_ratio = params.get<double>("weight_geometric_ratio");

		if (params.has("weight_area"))
			m_weight_area = params.get<double>("weight_area");

		if (params.has("weight_rssi_variance"))
			m_weight_rssi_variance = params.get<double>("weight_rssi_variance");

		if (params.has("weight_rssi"))
			m_weight_rssi = params.get<double>("weight_rssi");

		if (params.has("angle_weight"))
			m_angle_weight = params.get<double>("angle_weight");

		if (params.has("cluster_score_weight"))
			m_cluster_score_weight = params.get<double>("cluster_score_weight");

		spdlog::debug("CTA2: Parameters applied");
	}

	void ClusteredTriangulationAlgorithm2::calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout)
	{
		m_clusters.clear();

		for (auto &pair : m_point_map)
		{
			spdlog::info("ClusteredTriangulationAlgorithm2: Device '{}' has {} data points", pair.first, pair.second.size());

			auto &m_points = pair.second;

			reorderDataPointsByDistance(m_points);
			clusterData(m_points);
		}

		spdlog::info("ClusteredTriangulationAlgorithm2: formed a total of {} clusters from {} data points",
					 m_clusters.size(), m_total_points);

		estimateAoAForClusters(m_cluster_min_points);

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
		result_point.zero_latitude = m_zero_latitude;
		result_point.zero_longitude = m_zero_longitude;
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

	void ClusteredTriangulationAlgorithm2::findBestClusters(std::vector<DataPoint> &m_points)
	{
		auto clustering_start = std::chrono::high_resolution_clock::now();
		m_combinations_explored = 0;
		m_clusters_evaluated = 0;

		{
			std::unique_lock<std::shared_mutex> lock(working_clusters_mutex);
			working_clusters.clear();
		}

		int n_points = static_cast<int>(m_points.size());
		std::vector<int> point_order = strideOrder(n_points);

		// Pre-allocate per-seed metrics (thread-safe access by index)
		std::vector<size_t> combinations_per_seed(n_points, 0);
		std::vector<double> time_per_seed_ms(n_points, 0.0);
		std::vector<int> candidates_per_seed(n_points, 0);
		std::vector<bool> seed_timed_out(n_points, false);

		// Calculate per-seed timeout (divide total timeout among seeds)
		double per_seed_timeout = m_per_seed_timeout;

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

			PointCluster best_cluster(m_points.size());
			double best_score = -std::numeric_limits<double>::max();
			bool found_valid = false;

			std::vector<int> candidate_indices;

			// Does not create a race condition since all distances are calculated beforehand
			// TODO: Could make an explicitly thread-safe version
			getCandidates(i, candidate_indices, m_points);

			int n_candidates = static_cast<int>(candidate_indices.size());
			candidates_per_seed[idx] = n_candidates;

			// Skip if not enough candidates
			if (n_candidates < static_cast<int>(m_cluster_min_points) - 1)
			{
				time_per_seed_ms[idx] = 0.0;
				combinations_per_seed[idx] = 0;
				continue;
			}

			PointCluster cluster(m_points.size());
			cluster.addPointVectorized(m_points[i], i); // seed

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
						cluster.removePointVectorized(current_selection.size(), candidate_indices[current_selection.back()]);
						current_selection.pop_back();
					}
					if (!stack.empty())
					{
						stack.back()++;
					}
					continue;
				}

				current_selection.push_back(candidate_idx);
				cluster.addPointVectorized(m_points[candidate_indices[candidate_idx]], candidate_indices[candidate_idx]);

				int cluster_size = static_cast<int>(current_selection.size()) + 1;
				if (cluster_size >= static_cast<int>(m_cluster_min_points))
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
					cluster.removePointVectorized(current_selection.size(), candidate_indices[current_selection.back()]); // remove last added point, -1 for zero indexing +1 for seed
					current_selection.pop_back();
					stack.back()++;
				}
			}

			// Record per-seed metrics
			auto seed_end_time = std::chrono::high_resolution_clock::now();
			time_per_seed_ms[idx] = std::chrono::duration<double, std::milli>(seed_end_time - seed_start_time).count();
			combinations_per_seed[idx] = seed_combinations;
			if (found_valid)
			{
				std::unique_lock<std::shared_mutex> lock(working_clusters_mutex);
				working_clusters.push_back(best_cluster);
				spdlog::info("ClusteredTriangulationAlgorithm2: Seed point {} formed a valid cluster with score {:.4f}, size {} ({} combinations explored in {:.2f} ms)",
							 i, best_score, best_cluster.size(), seed_combinations, time_per_seed_ms[idx]);
			}

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

		for (auto &cluster : working_clusters)
		{
			PointCluster finalized_cluster = cluster.copyVectorizedToNormal(m_points);
			m_clusters.push_back(finalized_cluster);
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

	void ClusteredTriangulationAlgorithm2::getCandidates(int i, std::vector<int> &candidate_indices, const std::vector<DataPoint> &points)
	{
		for (int j = 0; j < static_cast<int>(points.size()); ++j)
		{
			if (i == j)
			{
				continue;
			}

			double distance = core::PointDistanceCache::getInstance().getDistance(points[i], points[j]);

			if (distance <= m_max_internal_distance)
			{
				candidate_indices.push_back(j);
			}
		}
	}

	bool ClusteredTriangulationAlgorithm2::checkCluster(PointCluster &cluster, PointCluster &best_cluster, double &best_score)
	{
		double ratio = cluster.geometricRatio();
		double clusterArea = cluster.area();

		bool valid = (ratio >= m_min_geometric_ratio && ratio <= m_max_geometric_ratio) &&
					 (clusterArea >= m_min_area && clusterArea <= m_max_area) &&
					 (cluster.varianceRSSI() >= m_min_rssi_variance && cluster.varianceRSSI() <= m_max_rssi_variance);

		if (!valid)
		{
			return false;
		}

		double current_score = cluster.getAndSetScore(
			m_ideal_geometric_ratio, m_min_geometric_ratio, m_max_geometric_ratio,
			m_ideal_area, m_min_area, m_max_area,
			m_ideal_rssi_variance, m_min_rssi_variance, m_max_rssi_variance,
			m_weight_geometric_ratio, m_weight_area, m_weight_rssi_variance,
			m_bottom_rssi, m_top_rssi, m_weight_rssi);

		if (current_score > best_score)
		{
			// check overlap with existing clusters
			{
				std::shared_lock<std::shared_mutex> lock(working_clusters_mutex);
				for (const auto &existing_cluster : working_clusters)
				{
					if (cluster.overlapWith(existing_cluster) > m_max_overlap)
					{
						return false;
					}
				}
			}
			best_score = current_score;
			// Copy cluster using the new copyFromVectorized method
			best_cluster = cluster.copyVectorizedToVectorized();
		}
		return true;
	}

	void ClusteredTriangulationAlgorithm2::clusterData(std::vector<DataPoint> &m_points)
	{
		coalescePoints(m_coalition_distance, m_points);

		size_t clusters_before = m_clusters.size();
		findBestClusters(m_points);
		size_t clusters_added = m_clusters.size() - clusters_before;

		spdlog::info("ClusteredTriangulationAlgorithm2: added {} clusters from this device ({} total)",
					 clusters_added, m_clusters.size());

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
		double global_best_cost = getCost(global_best_x, global_best_y, m_cluster_score_weight, m_angle_weight);

		double zone_x = -precision * m_grid_half_size;
		double zone_y = -precision * m_grid_half_size;

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
				double quadrant_x = zone_x + (q % 2) * m_grid_half_size * precision;
				double quadrant_y = zone_y + (q / 2) * m_grid_half_size * precision;

				if (visited_quadrants.find({quadrant_x, quadrant_y}) != visited_quadrants.end())
				{
					continue;
				}
				visited_quadrants.insert({quadrant_x, quadrant_y});

				for (int ix = 0; ix < m_grid_half_size; ++ix)
				{
					for (int iy = 0; iy < m_grid_half_size; ++iy)
					{
						double x = quadrant_x + ix * precision;
						double y = quadrant_y + iy * precision;
						double cost = getCost(x, y, m_cluster_score_weight, m_angle_weight);

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

				zone_x += precision * m_grid_half_size *
						  (best_x < zone_x + precision * m_grid_half_size ? -1.0 : 1.0);
				zone_y += precision * m_grid_half_size *
						  (best_y < zone_y + precision * m_grid_half_size ? -1.0 : 1.0);
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
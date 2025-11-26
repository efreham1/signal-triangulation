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
	// clustering - shape criteria
	static constexpr double TARGET_SQUARENESS = 1.0; // Perfect square ratio (width/height = 1)
	static constexpr double SQUARENESS_WEIGHT = 2.0; // Weight for squareness in scoring
	static constexpr double AREA_WEIGHT = 0.5;		 // Weight for area deviation in scoring
	static constexpr double MIN_SQUARENESS = 0.15;	 // Minimum acceptable squareness (reject worse)

	// clustering - size constraints
	static constexpr size_t CLUSTER_MIN_POINTS = 4u;   // Minimum points per cluster
	static constexpr size_t CLUSTER_MAX_POINTS = 100u; // Maximum points per cluster
	static constexpr size_t MIN_CLUSTERS = 4u;		   // Minimum clusters required
	static constexpr size_t MAX_CLUSTERS = 50u;		   // Maximum clusters allowed (relaxed)

	// clustering - area targets
	static constexpr double TARGET_AREA_PER_CLUSTER = 60.0; // Target area in square meters per cluster
	static constexpr double AREA_TOLERANCE = 0.5;		   // Acceptable deviation from target (fraction)

	// exhaustive search threshold
	static constexpr size_t EXHAUSTIVE_SEARCH_MAX_POINTS = 25; // Use brute force if n <= this	 // geometric ratio threshold to split cluster

	static constexpr double DEFAULT_COALITION_DISTANCE_METERS = 1.0; // meters used to coalesce nearby points

	// cluster weighting
	static constexpr double VARIANCE_WEIGHT = 0.5; // weight for variance in cluster weighting
	static constexpr double RSSI_WEIGHT = 0.3;	 // weight for RSSI component in cluster weighting
	static constexpr double BOTTOM_RSSI = -90.0;	 // bottom RSSI threshold for cluster weighting
	static constexpr double EXTRA_WEIGHT = 1.0;		 // extra weight multiplier for cluster weighting


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

		spdlog::info("ClusteredTriangulationAlgorithm2: starting brute force search for global minimum, data points: {}, clusters: {}",
					 m_points.size(), m_clusters.size());

		bruteForceSearch(global_best_x, global_best_y, precision, timeout);

		// print x and y of resulting point
		if (plottingEnabled)
		{
			spdlog::info("ClusteredTriangulationAlgorithm2: plotting enabled, printing {} points and {} clusters",
						 m_points.size(), m_clusters.size());
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

	double ClusteredTriangulationAlgorithm2::getTargetTotalArea(size_t num_clusters) const
	{
		return TARGET_AREA_PER_CLUSTER * static_cast<double>(num_clusters);
	}

	ClusteredTriangulationAlgorithm2::ClusterMetrics
	ClusteredTriangulationAlgorithm2::evaluateClusterMetrics(size_t start_idx, size_t end_idx) const
	{
		ClusterMetrics metrics = {0.0, 0.0, 0, false};

		size_t count = end_idx - start_idx + 1;
		metrics.point_count = count;

		// Check size constraints
		if (count < CLUSTER_MIN_POINTS || count > CLUSTER_MAX_POINTS)
		{
			return metrics; // Invalid, valid = false
		}

		// Calculate bounding box
		double min_x = std::numeric_limits<double>::max();
		double max_x = std::numeric_limits<double>::lowest();
		double min_y = std::numeric_limits<double>::max();
		double max_y = std::numeric_limits<double>::lowest();

		for (size_t i = start_idx; i <= end_idx; ++i)
		{
			double x = m_points[i].getX();
			double y = m_points[i].getY();
			min_x = std::min(min_x, x);
			max_x = std::max(max_x, x);
			min_y = std::min(min_y, y);
			max_y = std::max(max_y, y);
		}

		double width = max_x - min_x;
		double height = max_y - min_y;

		// Handle degenerate cases
		if (width < 1e-9 && height < 1e-9)
		{
			// All points at same location - perfect squareness but zero area
			metrics.squareness = 1.0;
			metrics.area = 0.0;
			metrics.valid = true;
			return metrics;
		}

		// Area from bounding box
		metrics.area = width * height;

		// Squareness: ratio of shorter side to longer side (1.0 = perfect square)
		double shorter = std::min(width, height);
		double longer = std::max(width, height);

		if (longer < 1e-9)
		{
			metrics.squareness = 1.0; // Degenerate case
		}
		else
		{
			metrics.squareness = shorter / longer;
		}

		// Check minimum squareness threshold
		if (metrics.squareness < MIN_SQUARENESS)
		{
			return metrics; // Invalid, valid = false
		}

		metrics.valid = true;
		return metrics;
	}

	ClusteredTriangulationAlgorithm2::PartitionScore
	ClusteredTriangulationAlgorithm2::evaluatePartition(const std::vector<std::pair<size_t, size_t>> &segments) const
	{
		PartitionScore score = {0.0, 0.0, 0.0, std::numeric_limits<double>::max()};

		size_t num_clusters = segments.size();

		// Check cluster count bounds (just for feasibility, not scoring)
		if (num_clusters < MIN_CLUSTERS || num_clusters > MAX_CLUSTERS)
		{
			return score; // Invalid partition
		}

		double total_squareness = 0.0;
		double total_area = 0.0;

		for (const auto &seg : segments)
		{
			ClusterMetrics metrics = evaluateClusterMetrics(seg.first, seg.second);
			if (!metrics.valid)
			{
				return score; // Invalid partition
			}
			total_squareness += metrics.squareness;
			total_area += metrics.area;
		}

		// Squareness score: average squareness (higher is better, so we invert for minimization)
		double avg_squareness = total_squareness / num_clusters;
		score.squareness_score = (TARGET_SQUARENESS - avg_squareness); // Lower is better

		// Area score: penalize deviation from TARGET_AREA_PER_CLUSTER for each cluster
		double target_total = getTargetTotalArea(num_clusters);
		if (target_total > 0.0)
		{
			double area_deviation = std::abs(total_area - target_total) / target_total;
			score.area_score = area_deviation;
		}
		else
		{
			score.area_score = 0.0;
		}

		// No cluster count penalty
		score.cluster_count_score = 0.0;

		// Combined score (lower is better) - only shape and area
		score.total_score = SQUARENESS_WEIGHT * score.squareness_score +
							AREA_WEIGHT * score.area_score;

		return score;
	}

	void ClusteredTriangulationAlgorithm2::findOptimalClustering()
	{
		size_t n = m_points.size();

		if (n < CLUSTER_MIN_POINTS * MIN_CLUSTERS)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm2: insufficient points for clustering");
		}

		if (n <= EXHAUSTIVE_SEARCH_MAX_POINTS)
		{
			// Exhaustive search: try all 2^(n-1) contiguous partitions
			size_t num_cuts = n - 1;
			size_t total_partitions = 1ULL << num_cuts;

			spdlog::info("ClusteredTriangulationAlgorithm2: exhaustive search over {} partitions", total_partitions);

			double best_total_score = std::numeric_limits<double>::max();
			std::vector<std::pair<size_t, size_t>> best_segments;

			for (size_t mask = 0; mask < total_partitions; ++mask)
			{
				std::vector<std::pair<size_t, size_t>> segments;
				size_t seg_start = 0;

				for (size_t i = 0; i < num_cuts; ++i)
				{
					if (mask & (1ULL << i))
					{
						segments.push_back({seg_start, i});
						seg_start = i + 1;
					}
				}
				segments.push_back({seg_start, n - 1});

				PartitionScore score = evaluatePartition(segments);
				if (score.total_score < best_total_score)
				{
					best_total_score = score.total_score;
					best_segments = segments;
				}
			}

			if (best_segments.empty())
			{
				throw std::runtime_error("ClusteredTriangulationAlgorithm2: no valid partition found");
			}

			spdlog::info("ClusteredTriangulationAlgorithm2: best partition has {} clusters with score {:.4f}",
						 best_segments.size(), best_total_score);

			buildClustersFromSegments(best_segments);
		}
		else
		{
			dpClusterSearch();
		}
	}

	void ClusteredTriangulationAlgorithm2::dpClusterSearch()
	{
		size_t n = m_points.size();
		size_t min_k = MIN_CLUSTERS;
		size_t max_k = std::min(MAX_CLUSTERS, n / CLUSTER_MIN_POINTS);

		spdlog::info("ClusteredTriangulationAlgorithm2: DP search for {} to {} clusters (n={}, min_points_per_cluster={})",
					 min_k, max_k, n, CLUSTER_MIN_POINTS);

		// Precompute cluster scores for all valid segments [i, j]
		// segment_score[i][len] = score for segment starting at i with length len
		std::vector<std::vector<double>> segment_score(n);
		std::vector<std::vector<double>> segment_area(n);

		for (size_t i = 0; i < n; ++i)
		{
			size_t max_len = std::min(CLUSTER_MAX_POINTS, n - i);
			segment_score[i].resize(max_len + 1, std::numeric_limits<double>::max());
			segment_area[i].resize(max_len + 1, 0.0);

			for (size_t len = CLUSTER_MIN_POINTS; len <= max_len; ++len)
			{
				size_t j = i + len - 1;
				ClusterMetrics metrics = evaluateClusterMetrics(i, j);
				if (metrics.valid)
				{
					segment_score[i][len] = (TARGET_SQUARENESS - metrics.squareness);
					segment_area[i][len] = metrics.area;
					spdlog::debug("ClusteredTriangulationAlgorithm2: segment [{}, {}] len={} valid, squareness={:.3f}, area={:.3f}",
								  i, j, len, metrics.squareness, metrics.area);
				}
			}
		}

		// dp[i][k] = {min_score, total_area} to partition [0..i-1] into k clusters
		struct DPState
		{
			double score;
			double area;
		};
		std::vector<std::vector<DPState>> dp(n + 1, std::vector<DPState>(max_k + 1, {std::numeric_limits<double>::max(), 0.0}));
		std::vector<std::vector<size_t>> parent(n + 1, std::vector<size_t>(max_k + 1, SIZE_MAX));

		dp[0][0] = {0.0, 0.0};

		for (size_t i = CLUSTER_MIN_POINTS; i <= n; ++i)
		{
			size_t max_clusters_for_i = i / CLUSTER_MIN_POINTS;
			for (size_t k = 1; k <= std::min(max_clusters_for_i, max_k); ++k)
			{
				// Try all valid last segments ending at i-1
				// Segment has length `len` and starts at position `j = i - len`
				for (size_t len = CLUSTER_MIN_POINTS; len <= std::min(CLUSTER_MAX_POINTS, i); ++len)
				{
					size_t j = i - len; // Start of last segment

					// Check that we can form k-1 clusters from [0, j-1]
					if (k > 1 && j < (k - 1) * CLUSTER_MIN_POINTS)
					{
						continue; // Not enough points for remaining clusters
					}

					if (dp[j][k - 1].score == std::numeric_limits<double>::max())
					{
						continue;
					}

					if (len >= segment_score[j].size() || segment_score[j][len] == std::numeric_limits<double>::max())
					{
						continue;
					}

					double new_score = dp[j][k - 1].score + segment_score[j][len];
					if (new_score < dp[i][k].score)
					{
						dp[i][k] = {new_score, dp[j][k - 1].area + segment_area[j][len]};
						parent[i][k] = j;
						spdlog::debug("ClusteredTriangulationAlgorithm2: dp[{}][{}] = {:.4f} via segment [{}, {}] len={}",
									  i, k, new_score, j, i - 1, len);
					}
				}
			}
		}

		// Find best k considering area target per cluster
		double best_combined_score = std::numeric_limits<double>::max();
		size_t best_k = 0;

		for (size_t k = min_k; k <= max_k; ++k)
		{
			if (dp[n][k].score == std::numeric_limits<double>::max())
			{
				spdlog::debug("ClusteredTriangulationAlgorithm2: k={} is infeasible", k);
				continue;
			}

			double target_total = getTargetTotalArea(k);
			double area_penalty = (target_total > 0.0) ? std::abs(dp[n][k].area - target_total) / target_total : 0.0;

			double combined = SQUARENESS_WEIGHT * dp[n][k].score +
							  AREA_WEIGHT * area_penalty;

			spdlog::debug("ClusteredTriangulationAlgorithm2: k={} score={:.4f} area={:.4f} target={:.4f} combined={:.4f}",
						  k, dp[n][k].score, dp[n][k].area, target_total, combined);

			if (combined < best_combined_score)
			{
				best_combined_score = combined;
				best_k = k;
			}
		}

		if (best_k == 0)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm2: no valid partition found via DP");
		}

		// Reconstruct segments
		std::vector<std::pair<size_t, size_t>> segments;
		size_t i = n;
		size_t k = best_k;
		while (k > 0)
		{
			size_t j = parent[i][k];
			if (j == SIZE_MAX)
			{
				throw std::runtime_error("ClusteredTriangulationAlgorithm2: DP reconstruction failed - invalid parent");
			}
			size_t seg_start = j;
			size_t seg_end = i - 1;
			size_t seg_len = seg_end - seg_start + 1;

			spdlog::debug("ClusteredTriangulationAlgorithm2: reconstructed segment [{}, {}] len={}",
						  seg_start, seg_end, seg_len);

			if (seg_len < CLUSTER_MIN_POINTS)
			{
				spdlog::error("ClusteredTriangulationAlgorithm2: reconstructed segment [{}, {}] has only {} points (min={})",
							  seg_start, seg_end, seg_len, CLUSTER_MIN_POINTS);
				throw std::runtime_error("ClusteredTriangulationAlgorithm2: DP reconstruction produced invalid segment");
			}

			segments.push_back({seg_start, seg_end});
			i = j;
			k--;
		}
		std::reverse(segments.begin(), segments.end());

		spdlog::info("ClusteredTriangulationAlgorithm2: DP found {} clusters with combined score {:.4f}",
					 best_k, best_combined_score);

		buildClustersFromSegments(segments);
	}

	void ClusteredTriangulationAlgorithm2::buildClustersFromSegments(
		const std::vector<std::pair<size_t, size_t>> &segments)
	{
		m_clusters.clear();
		m_clusters.reserve(segments.size());

		spdlog::info("ClusteredTriangulationAlgorithm2: building {} clusters from segments", segments.size());

		for (size_t seg_idx = 0; seg_idx < segments.size(); ++seg_idx)
		{
			const auto &seg = segments[seg_idx];

			PointCluster cluster;

			double sum_x = 0.0, sum_y = 0.0;
			int sum_rssi = 0;

			// Track bounding box for logging
			double min_x = std::numeric_limits<double>::max();
			double max_x = std::numeric_limits<double>::lowest();
			double min_y = std::numeric_limits<double>::max();
			double max_y = std::numeric_limits<double>::lowest();

			for (size_t i = seg.first; i <= seg.second; ++i)
			{
				cluster.addPoint(m_points[i]);
				sum_x += m_points[i].getX();
				sum_y += m_points[i].getY();
				sum_rssi += m_points[i].rssi;

				min_x = std::min(min_x, m_points[i].getX());
				max_x = std::max(max_x, m_points[i].getX());
				min_y = std::min(min_y, m_points[i].getY());
				max_y = std::max(max_y, m_points[i].getY());
			}

			// Ensure centroid and avg_rssi are set
			size_t count = seg.second - seg.first + 1;
			cluster.centroid_x = sum_x / count;
			cluster.centroid_y = sum_y / count;
			cluster.avg_rssi = static_cast<double>(sum_rssi) / count;

			// Calculate dimensions and area
			double width = max_x - min_x;
			double height = max_y - min_y;
			double area = width * height;
			double squareness = (std::max(width, height) > 1e-9)
									? std::min(width, height) / std::max(width, height)
									: 1.0;

			spdlog::info("ClusteredTriangulationAlgorithm2: cluster {} - {} points, dimensions: {:.2f}m x {:.2f}m, area: {:.2f}m², squareness: {:.3f}",
						 seg_idx, count, width, height, area, squareness);

			m_clusters.push_back(std::move(cluster));
		}
	}

	void ClusteredTriangulationAlgorithm2::clusterData()
	{
		findOptimalClustering();

		spdlog::info("ClusteredTriangulationAlgorithm2: formed {} clusters from {} data points",
					 m_clusters.size(), m_points.size());

		if (m_clusters.size() < 3)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm2: insufficient clusters for triangulation");
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

		size_t valid_aoa_count = 0;

		for (size_t cluster_idx = 0; cluster_idx < m_clusters.size(); ++cluster_idx)
		{
			auto &cluster = m_clusters[cluster_idx];

			if (cluster.points.size() < 3)
			{
				spdlog::warn("ClusteredTriangulationAlgorithm2: cluster {} has only {} points, skipping AoA estimation",
							 cluster_idx, cluster.points.size());
				continue; // Need at least 3 points to estimate AoA
			}

			X.resize(cluster.points.size());
			Y.resize(cluster.points.size());
			Z.resize(cluster.points.size());

			// Debug: check point spread and RSSI values
			double min_x = std::numeric_limits<double>::max();
			double max_x = std::numeric_limits<double>::lowest();
			double min_y = std::numeric_limits<double>::max();
			double max_y = std::numeric_limits<double>::lowest();
			double min_rssi = std::numeric_limits<double>::max();
			double max_rssi = std::numeric_limits<double>::lowest();

			for (size_t i = 0; i < cluster.points.size(); ++i)
			{
				const auto &point = cluster.points[i];
				X[i] = point.getX();
				Y[i] = point.getY();
				Z[i] = static_cast<double>(point.rssi);

				min_x = std::min(min_x, X[i]);
				max_x = std::max(max_x, X[i]);
				min_y = std::min(min_y, Y[i]);
				max_y = std::max(max_y, Y[i]);
				min_rssi = std::min(min_rssi, Z[i]);
				max_rssi = std::max(max_rssi, Z[i]);
			}

			spdlog::debug("ClusteredTriangulationAlgorithm2: cluster {} has {} points, x=[{:.2f}, {:.2f}], y=[{:.2f}, {:.2f}], rssi=[{:.0f}, {:.0f}]",
						  cluster_idx, cluster.points.size(), min_x, max_x, min_y, max_y, min_rssi, max_rssi);

			// Check if there's enough spread for plane fitting
			double x_spread = max_x - min_x;
			double y_spread = max_y - min_y;
			double rssi_spread = max_rssi - min_rssi;

			if (x_spread < 1e-6 && y_spread < 1e-6)
			{
				spdlog::warn("ClusteredTriangulationAlgorithm2: cluster {} has no spatial spread (all points at same location), skipping AoA",
							 cluster_idx);
				continue;
			}

			if (rssi_spread < 1e-6)
			{
				spdlog::warn("ClusteredTriangulationAlgorithm2: cluster {} has no RSSI variation (all same value: {}), skipping AoA",
							 cluster_idx, min_rssi);
				continue;
			}

			std::vector<double> normal = getNormalVector2(X, Y, Z);

			spdlog::debug("ClusteredTriangulationAlgorithm2: cluster {} normal vector = [{:.6f}, {:.6f}, {:.6f}]",
						  cluster_idx, normal[0], normal[1], normal[2]);

			// The gradient components
			if (std::abs(normal[2]) < 1e-9)
			{
				spdlog::warn("ClusteredTriangulationAlgorithm2: cluster {} has near-zero normal[2]={:.9f}, cannot compute gradient",
							 cluster_idx, normal[2]);
				continue; // Avoid division by zero
			}

			double grad_x = -normal[0] / normal[2];
			double grad_y = -normal[1] / normal[2];

			// Validate gradient is non-zero
			if (std::abs(grad_x) < 1e-9 && std::abs(grad_y) < 1e-9)
			{
				spdlog::warn("ClusteredTriangulationAlgorithm2: cluster {} has near-zero gradient ({:.9f}, {:.9f}), skipping",
							 cluster_idx, grad_x, grad_y);
				continue;
			}

			cluster.aoa_x = grad_x;
			cluster.aoa_y = grad_y;
			cluster.estimated_aoa = atan2(grad_y, grad_x) * (180.0 / M_PI); // in degrees

			valid_aoa_count++;

			spdlog::info("ClusteredTriangulationAlgorithm2: cluster {} AoA estimated at {:.2f} degrees (grad_x={:.6f}, grad_y={:.6f})",
						 cluster_idx, cluster.estimated_aoa, grad_x, grad_y);
		}

		spdlog::info("ClusteredTriangulationAlgorithm2: {} of {} clusters have valid AoA estimates",
					 valid_aoa_count, m_clusters.size());

		if (valid_aoa_count < 2)
		{
			spdlog::error("ClusteredTriangulationAlgorithm2: insufficient clusters with valid AoA ({} < 2), triangulation will fail",
						  valid_aoa_count);
		}
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
				cluster_cost *= cluster.getWeight(VARIANCE_WEIGHT, RSSI_WEIGHT, BOTTOM_RSSI)  + EXTRA_WEIGHT;
				spdlog::debug("ClusteredTriangulationAlgorithm2: cost for cluster at (centroid_x={}, centroid_y={}) with AoA ({}, {}) is {} (in front of centroid)", cluster.centroid_x, cluster.centroid_y, cluster.aoa_x, cluster.aoa_y, cluster_cost);
				total_cost += cluster_cost;
			}
		}
		return total_cost;
	}

} // namespace core

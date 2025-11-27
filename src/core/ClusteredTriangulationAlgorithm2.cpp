#include "ClusteredTriangulationAlgorithm2.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <chrono>
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

	void ClusteredTriangulationAlgorithm2::clusterData()
	{
		coalescePoints(getCoalitionDistance());

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

			if (c.geometricRatio() > getClusterRatioSplitThreshold() && current_cluster_size >= getClusterMinPoints())
			{
				spdlog::debug("ClusteredTriangulationAlgorithm2: created new cluster (id={}) after splitting (id={}) due to geometric ratio {}",
							  cluster_id + 1, cluster_id, c.geometricRatio());
				cluster_id++;
				current_cluster_size = 0;
			}
		}

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
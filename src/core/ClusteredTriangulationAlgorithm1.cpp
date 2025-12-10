#include "ClusteredTriangulationAlgorithm1.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>
#include <chrono>
#include <spdlog/spdlog.h>

namespace core
{

	ClusteredTriangulationAlgorithm1::ClusteredTriangulationAlgorithm1() = default;

	ClusteredTriangulationAlgorithm1::ClusteredTriangulationAlgorithm1(const AlgorithmParameters &params)
	{
		applyParameters(params);
	}

	ClusteredTriangulationAlgorithm1::~ClusteredTriangulationAlgorithm1() = default;

	void ClusteredTriangulationAlgorithm1::applyParameters(const AlgorithmParameters &params)
	{
		if (params.has("coalition_distance"))
			m_coalition_distance = params.get<double>("coalition_distance");

		if (params.has("cluster_min_points"))
			m_cluster_min_points = static_cast<unsigned int>(params.get<int>("cluster_min_points"));

		if (params.has("cluster_ratio_threshold"))
			m_cluster_ratio_threshold = params.get<double>("cluster_ratio_threshold");

		if (params.has("extra_weight"))
			m_extra_weight = params.get<double>("extra_weight");

		if (params.has("angle_weight"))
			m_angle_weight = params.get<double>("angle_weight");

		spdlog::debug("CTA1: Parameters applied");
	}

	void ClusteredTriangulationAlgorithm1::calculatePosition(double &out_latitude, double &out_longitude, double precision, double timeout)
	{
		m_clusters.clear();

		if (m_total_points < m_cluster_min_points)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm1: not enough data points");
		}

		for (auto &pair : m_point_map)
		{
			auto &m_points = pair.second;
			reorderDataPointsByDistance(m_points);
			clusterData(m_points);
		}
		estimateAoAForClusters(m_cluster_min_points);

		std::vector<std::pair<double, double>> intersections = findIntersections();
		if (intersections.empty())
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm1: no intersections found between cluster AoA lines");
		}

		double global_best_x = 0.0;
		double global_best_y = 0.0;

		gradientDescent(global_best_x, global_best_y, intersections, precision, timeout);

		if (plottingEnabled)
		{
			printPointsAndClusters();
			std::cout << "Resulting point after gradient descent: x=" << global_best_x << ", y=" << global_best_y << std::endl;
		}

		DataPoint result_point;
		result_point.setX(global_best_x);
		result_point.setY(global_best_y);
		result_point.zero_latitude = m_zero_latitude;
		result_point.zero_longitude = m_zero_longitude;
		result_point.computeCoordinates();

		if (!result_point.validCoordinates())
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm1: computed invalid coordinates");
		}

		out_latitude = result_point.getLatitude();
		out_longitude = result_point.getLongitude();
	}

	void ClusteredTriangulationAlgorithm1::clusterData(std::vector<DataPoint> &m_points)
	{
		coalescePoints(m_coalition_distance, m_points);

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

			if (c.geometricRatio() > m_cluster_ratio_threshold && current_cluster_size >= m_cluster_min_points)
			{
				spdlog::debug("ClusteredTriangulationAlgorithm1: created new cluster (id={}) after splitting (id={}) due to geometric ratio {}",
							  cluster_id + 1, cluster_id, c.geometricRatio());
				cluster_id++;
				current_cluster_size = 0;
			}
		}

		spdlog::info("ClusteredTriangulationAlgorithm1: formed {} clusters from {} data points", m_clusters.size(), m_total_points);

		if (m_clusters.size() < 2)
		{
			throw std::runtime_error("ClusteredTriangulationAlgorithm1: insufficient clusters formed for AoA estimation");
		}
		else if (m_clusters.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm1: only {} clusters formed; AoA estimation may be unreliable", m_clusters.size());
		}
	}

	std::vector<std::pair<double, double>> ClusteredTriangulationAlgorithm1::findIntersections()
	{
		std::vector<std::pair<double, double>> intersections;

		for (size_t i = 0; i < m_clusters.size(); ++i)
		{
			for (size_t j = i + 1; j < m_clusters.size(); ++j)
			{
				double a1 = m_clusters[i].aoa_x;
				double b1 = -m_clusters[j].aoa_x;
				double c1 = m_clusters[j].centroid_x - m_clusters[i].centroid_x;

				double a2 = m_clusters[i].aoa_y;
				double b2 = -m_clusters[j].aoa_y;
				double c2 = m_clusters[j].centroid_y - m_clusters[i].centroid_y;

				double denom = a1 * b2 - a2 * b1;
				if (std::abs(denom) < std::numeric_limits<double>::epsilon())
				{
					continue;
				}

				double t1 = (c1 * b2 - c2 * b1) / denom;
				double t2 = (a1 * c2 - a2 * c1) / denom;

				if (t1 < 0 || t2 < 0)
				{
					continue;
				}

				double intersect_x = m_clusters[i].centroid_x + t1 * m_clusters[i].aoa_x;
				double intersect_y = m_clusters[i].centroid_y + t1 * m_clusters[i].aoa_y;
				intersections.emplace_back(intersect_x, intersect_y);

				spdlog::debug("ClusteredTriangulationAlgorithm1: found intersection between cluster {} and {} at (x={}, y={})",
							  i, j, intersect_x, intersect_y);
			}
		}

		if (intersections.size() < 3)
		{
			spdlog::warn("ClusteredTriangulationAlgorithm1: only {} intersections found", intersections.size());
		}

		return intersections;
	}

	void ClusteredTriangulationAlgorithm1::gradientDescent(double &out_x, double &out_y,
														   std::vector<std::pair<double, double>> intersections,
														   double precision, double timeout)
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

			bool continue_gradient_descent = true;
			double current_x = inter.first;
			double current_y = inter.second;
			double current_cost = getCost(current_x, current_y, m_extra_weight, m_angle_weight);

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
						break;
					}
				}

				double best_cost = current_cost;
				double best_x = current_x;
				double best_y = current_y;
				explored_new_point = false;

				for (int dx = -1; dx <= 1; ++dx)
				{
					for (int dy = -1; dy <= 1; ++dy)
					{
						if (dx == 0 && dy == 0)
							continue;

						double x = current_x + dx * precision;
						double y = current_y + dy * precision;

						if (visited_points.count({x, y}) > 0)
							continue;

						visited_points.insert({x, y});
						explored_new_point = true;

						double neighbor_cost = getCost(x, y, m_extra_weight, m_angle_weight);
						if (neighbor_cost <= best_cost)
						{
							best_cost = neighbor_cost;
							best_x = x;
							best_y = y;
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
					continue_gradient_descent = false;
				}
			}

			if (current_cost < global_best_cost)
			{
				global_best_cost = current_cost;
				global_best_x = current_x;
				global_best_y = current_y;
			}
		}

		out_x = global_best_x;
		out_y = global_best_y;

		spdlog::info("ClusteredTriangulationAlgorithm1: gradient descent completed, global minimum at (x={}, y={}) with cost {}",
					 out_x, out_y, global_best_cost);
	}

} // namespace core
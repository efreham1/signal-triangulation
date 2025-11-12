#include "ClusteredTriangulationAlgorithm.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>



namespace core {

ClusteredTriangulationAlgorithm::ClusteredTriangulationAlgorithm() = default;

ClusteredTriangulationAlgorithm::~ClusteredTriangulationAlgorithm() = default;

bool ClusteredTriangulationAlgorithm::processDataPoint(const DataPoint& point)
{
	// Basic validation; throw on invalid coordinates to make errors explicit
	if (!point.validCoordinates() || (point.getLatitude() < -90.0 || point.getLatitude() > 90.0 ||
	    point.getLongitude() < -180.0 || point.getLongitude() > 180.0)) {
		throw std::invalid_argument("ClusteredTriangulationAlgorithm: invalid coordinates");
	}

	// insert point into m_points keeping the vector sorted by timestamp_ms (ascending)
	auto it = std::lower_bound(
		m_points.begin(), m_points.end(), point.timestamp_ms,
		[](const DataPoint& a, const int64_t t) { return a.timestamp_ms < t; });
	m_points.insert(it, point);
	return true;
}

bool ClusteredTriangulationAlgorithm::calculatePosition(double& out_latitude, double& out_longitude)
{
	clusterData();
	estimateAoAForClusters();
	buildCostFunction();
	std::cout << "x = [";
	for (const auto& point : m_points) {
		std::cout << point.getX() << ", ";
	}
	std::cout << "]" << std::endl;
	std::cout << "y = [";
	for (const auto& point : m_points) {
		std::cout << point.getY() << ", ";
	}
	std::cout << "]" << std::endl;
	std::cout << "rssi = [";
	for (const auto& point : m_points) {
		std::cout << point.rssi << ", ";
	}
	std::cout << "]" << std::endl;
	std::cout << "Clusterx = [";
	for (const auto& cluster : m_clusters) {
		std::cout << cluster.centroid_x << ", ";
	}
	std::cout << "]" << std::endl;
	std::cout << "Clustery = [";
	for (const auto& cluster : m_clusters) {
		std::cout << cluster.centroid_y << ", ";
	}
	std::cout << "]" << std::endl;
	std::cout << "AoAs = [";
	for (const auto& cluster : m_clusters) {
		std::cout << cluster.estimated_aoa << ", ";
	}
	std::cout << "]" << std::endl;
	// TODO: implement the cluster-based AoA triangulation algorithm.
	return true;
}

void ClusteredTriangulationAlgorithm::reset()
{
	m_points.clear();
}

void ClusteredTriangulationAlgorithm::clusterData()
{
	unsigned int cluster_id = 0;
	unsigned int current_cluster_size = 0;
	for (const auto& point : m_points) {
		if (current_cluster_size == 3) {
			cluster_id++;
			current_cluster_size = 0;
		}
		// Add point to the current cluster
		if (m_clusters.size() <= cluster_id) {
			m_clusters.emplace_back();
		}
		m_clusters[cluster_id].addPoint(point);
		current_cluster_size++;
	}
}

void ClusteredTriangulationAlgorithm::estimateAoAForClusters()
{
	double xs[3] = {0.0, 0.0, 0.0};
	double ys[3] = {0.0, 0.0, 0.0};
	double rssis[3] = {0, 0, 0};
	for (auto& cluster : m_clusters) {
		if (cluster.points.size() < 3) {
			continue; // Need at least 3 points to estimate AoA
		}
		for (size_t i = 0; i < 3; ++i) {
			xs[i] = cluster.points[i].getX();
			ys[i] = cluster.points[i].getY();
			rssis[i] = cluster.points[i].rssi;
		}
		// Calculate the gradient (slope) of the plane formed by the three points
		double vec1[3] = {xs[1] - xs[0], ys[1] - ys[0], rssis[1] - rssis[0]};
		double vec2[3] = {xs[2] - xs[0], ys[2] - ys[0], rssis[2] - rssis[0]};
		double normal[3] = {
			vec1[1] * vec2[2] - vec1[2] * vec2[1],
			vec1[2] * vec2[0] - vec1[0] * vec2[2],
			vec1[0] * vec2[1] - vec1[1] * vec2[0]
		};
		// The gradient components
		double grad_x = -normal[0] / normal[2];
		double grad_y = -normal[1] / normal[2];
		cluster.estimated_aoa = atan2(grad_y, grad_x) * (180.0 / M_PI); // in degrees
	}
		// Estimate AoA as the arctangent of the gradient
}

void ClusteredTriangulationAlgorithm::buildCostFunction()
{
	// Stub: cost function construction for optimization
}

} // namespace core


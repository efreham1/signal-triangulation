#include "ClusteredTriangulationAlgorithm.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>



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
	
	std::cout << "Received DataPoint: Lat=" << point.getLatitude()
	          << ", Lon=" << point.getLongitude()
	          << ", X=" << point.getX()
	          << ", Y=" << point.getY()
	          << ", RSSI=" << point.rssi
	          << ", Timestamp=" << point.timestamp_ms
	          << ", SSID=\"" << point.ssid << "\"" << std::endl;
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
	// Stub: AoA estimation logic to be implemented
}

void ClusteredTriangulationAlgorithm::buildCostFunction()
{
	// Stub: cost function construction for optimization
}

} // namespace core


#include "ClusteredTriangulationAlgorithm.h"

#include <stdexcept>
#include <algorithm>

namespace core {

ClusteredTriangulationAlgorithm::ClusteredTriangulationAlgorithm() = default;

ClusteredTriangulationAlgorithm::~ClusteredTriangulationAlgorithm() = default;

bool ClusteredTriangulationAlgorithm::processDataPoint(const DataPoint& point)
{
	// Basic validation; throw on invalid coordinates to make errors explicit
	if (point.latitude < -90.0 || point.latitude > 90.0 || point.longitude < -180.0 || point.longitude > 180.0) {
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
	// TODO: implement the cluster-based AoA triangulation algorithm.
	// For now, return a simple centroid if we have points; otherwise return false.
	if (m_points.empty()) return false;

	double sum_lat = 0.0;
	double sum_lon = 0.0;
	for (const auto& p : m_points) {
		sum_lat += p.latitude;
		sum_lon += p.longitude;
	}
	out_latitude = sum_lat / static_cast<double>(m_points.size());
	out_longitude = sum_lon / static_cast<double>(m_points.size());
	return true;
}

void ClusteredTriangulationAlgorithm::reset()
{
	m_points.clear();
}

void ClusteredTriangulationAlgorithm::clusterData()
{
	// Stub: clustering logic to be implemented
}

void ClusteredTriangulationAlgorithm::estimateAoAForCluster()
{
	// Stub: AoA estimation logic to be implemented
}

void ClusteredTriangulationAlgorithm::buildCostFunction()
{
	// Stub: cost function construction for optimization
}

} // namespace core


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
	if (!point.validCoordinates()) {
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

	std::vector<Point> intersections = findIntersections();
	if (intersections.empty()) {
		throw std::runtime_error("ClusteredTriangulationAlgorithm: no intersections found between cluster AoA lines");
	}

	double resolution = 0.1; // step size in meters
	double current_x;
	double current_y;

	for (const auto& inter : intersections) {
		bool continue_gradient_descent = true;
		current_x = inter.x;
		current_y = inter.y;
		double current_cost = getCost(current_x, current_y);

		while (continue_gradient_descent) {
			// Check neighboring points in a grid
			double best_cost = current_cost;
			double best_x = current_x;
			double best_y = current_y;

			for (int dx = -1; dx <= 1; ++dx) {
				for (int dy = -1; dy <= 1; ++dy) {
					if (dx == 0 && dy == 0) continue; // Skip the center point

					double neighbor_x = current_x + dx * resolution;
					double neighbor_y = current_y + dy * resolution;
					double neighbor_cost = getCost(neighbor_x, neighbor_y);

					if (neighbor_cost < best_cost) {
						best_cost = neighbor_cost;
						best_x = neighbor_x;
						best_y = neighbor_y;
					}
				}
			}

			if (best_cost < current_cost) {
				current_x = best_x;
				current_y = best_y;
				current_cost = best_cost;
			} else {
				continue_gradient_descent = false; // No better neighbors found
			}
		}
		
	}

	//print x and y of resulting point
	std::cout << "Resulting point after gradient descent: x=" << current_x << ", y=" << current_y << std::endl;
	
	DataPoint result_point;
	result_point.setX(current_x);
	result_point.setY(current_y);
	result_point.zero_latitude = m_points[0].zero_latitude;
	result_point.zero_longitude = m_points[0].zero_longitude;
	result_point.computeCoordinates();
	if (!result_point.validCoordinates()) {
		throw std::runtime_error("ClusteredTriangulationAlgorithm: computed invalid coordinates");
	}
	out_latitude = result_point.getLatitude();
	out_longitude = result_point.getLongitude();

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
		cluster.aoa_x = grad_x;
		cluster.aoa_y = grad_y;
		cluster.estimated_aoa = atan2(grad_y, grad_x) * (180.0 / M_PI); // in degrees
	}
		// Estimate AoA as the arctangent of the gradient
}

// Should return a list of intersection points between all cluster AoA lines
std::vector<ClusteredTriangulationAlgorithm::Point> ClusteredTriangulationAlgorithm::findIntersections()
{
	std::vector<Point> intersections;
	for (size_t i = 0; i < m_clusters.size(); ++i) {
		for (size_t j = i + 1; j < m_clusters.size(); ++j) {
			// m_clusters[i].centroid_x + t1 * m_clusters[i].aoa_x = m_clusters[j].centroid_x + t2 * m_clusters[j].aoa_x
			// m_clusters[i].centroid_y + t1 * m_clusters[i].aoa_y = m_clusters[j].centroid_y + t2 * m_clusters[j].aoa_y
			double a1 = m_clusters[i].aoa_x;
			double b1 = -m_clusters[j].aoa_x;
			double c1 = m_clusters[j].centroid_x - m_clusters[i].centroid_x;

			double a2 = m_clusters[i].aoa_y;
			double b2 = -m_clusters[j].aoa_y;
			double c2 = m_clusters[j].centroid_y - m_clusters[i].centroid_y;

			double denom = a1 * b2 - a2 * b1;
			if (std::abs(denom) < 1e-6) {
				continue; // Lines are parallel or nearly parallel
			}

			double t1 = (c1 * b2 - c2 * b1) / denom;
			double t2 = (a1 * c2 - a2 * c1) / denom;

			if (t1 < 0 || t2 < 0) {
				continue; // Intersection is behind one of the clusters
			}

			double intersect_x = m_clusters[i].centroid_x + t1 * m_clusters[i].aoa_x;
			double intersect_y = m_clusters[i].centroid_y + t1 * m_clusters[i].aoa_y;
			intersections.emplace_back(intersect_x, intersect_y);
		}
	}
	return intersections;
}

double ClusteredTriangulationAlgorithm::getCost(double x, double y)
{
	// d= ∥(p−a)×v∥ / ∥v∥
	// where p is the point (x, y), a is a point on the line (centroid), and v is the direction vector (AoA)
	double total_cost = 0.0;

	// Cost is defined as the sum distances from the guess to the lines defined by each cluster's AoA
	for (const auto& cluster : m_clusters) {
		double cluster_grad[2] = {cluster.aoa_x, cluster.aoa_y};
		if (cluster_grad[0] == 0.0 && cluster_grad[1] == 0.0) {
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
			total_cost += -dot_prod/cluster_grad_mag + std::sqrt(point_to_centroid[0]*point_to_centroid[0] + point_to_centroid[1]*point_to_centroid[1]);
		} else {
			double distance = cross_prod_mag / cluster_grad_mag;
			total_cost += distance;
		}
	}
	return total_cost;
}

} // namespace core


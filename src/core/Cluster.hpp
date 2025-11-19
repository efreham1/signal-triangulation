#ifndef CLUSTER_HPP
#define CLUSTER_HPP

#include "DataPoint.h"

#include <limits>
#include <utility>
#include <vector>

namespace core
{
    class PointCluster
    {
    public:
        std::vector<DataPoint> points;
        double estimated_aoa;
        double avg_rssi;
        double centroid_x;
        double centroid_y;
        double aoa_x;
        double aoa_y;
        std::pair<int, int> points_furthest_between;
        double furthest_distance;

        PointCluster()
        {
            avg_rssi = 0.0;
            estimated_aoa = 0.0;
            centroid_x = 0.0;
            centroid_y = 0.0;
            aoa_x = 0.0;
            aoa_y = 0.0;
            points_furthest_between = std::make_pair(-1, -1);
            furthest_distance = 0.0;
        }
        ~PointCluster() = default;

        void addPoint(const DataPoint &point)
        {
            for (unsigned int i = 0; i < static_cast<unsigned int>(points.size()); ++i)
            {
                double dist = (points[i].getX() - point.getX()) * (points[i].getX() - point.getX()) +
                              (points[i].getY() - point.getY()) * (points[i].getY() - point.getY());
                if (dist > furthest_distance)
                {
                    furthest_distance = dist;
                    points_furthest_between = std::make_pair(i, static_cast<unsigned int>(points.size()));
                }
            }
            points.push_back(point);

            // Update average RSSI
            double previous_total = avg_rssi * static_cast<double>(points.size() - 1);
            avg_rssi = (previous_total + point.rssi) / static_cast<double>(points.size());

            // Update centroid
            double previous_total_x = centroid_x * static_cast<double>(points.size() - 1);
            double previous_total_y = centroid_y * static_cast<double>(points.size() - 1);
            centroid_x = (previous_total_x + point.getX()) / static_cast<double>(points.size());
            centroid_y = (previous_total_y + point.getY()) / static_cast<double>(points.size());
        }

        void addPoint(const DataPoint &point, double coalition_distance)
        {
            for (int i = 0; i < static_cast<int>(points.size()); ++i)
            {
                DataPoint &existing_point = points[i];
                double dist = (existing_point.getX() - point.getX()) * (existing_point.getX() - point.getX()) +
                              (existing_point.getY() - point.getY()) * (existing_point.getY() - point.getY());
                if (dist <= coalition_distance * coalition_distance)
                {
                    // Point is close enough to be coalesced into this point
                    centroid_x = centroid_x * static_cast<double>(points.size()) - existing_point.getX();
                    centroid_y = centroid_y * static_cast<double>(points.size()) - existing_point.getY();

                    existing_point.setX((existing_point.getX() + point.getX()) / 2.0);
                    existing_point.setY((existing_point.getY() + point.getY()) / 2.0);

                    centroid_x = (centroid_x + existing_point.getX()) / static_cast<double>(points.size());
                    centroid_y = (centroid_y + existing_point.getY()) / static_cast<double>(points.size());

                    avg_rssi = avg_rssi * static_cast<double>(points.size()) - existing_point.rssi;

                    existing_point.rssi = (existing_point.rssi + point.rssi) / 2;

                    avg_rssi = (avg_rssi + existing_point.rssi) / static_cast<double>(points.size());

                    if (points_furthest_between.first == i || points_furthest_between.second == i)
                    {
                        // Recalculate furthest points
                        furthest_distance = 0.0;
                        for (int m = 0; m < static_cast<int>(points.size()); ++m)
                        {
                            for (int n = m + 1; n < static_cast<int>(points.size()); ++n)
                            {
                                double d = (points[m].getX() - points[n].getX()) * (points[m].getX() - points[n].getX()) +
                                           (points[m].getY() - points[n].getY()) * (points[m].getY() - points[n].getY());
                                if (d > furthest_distance)
                                {
                                    furthest_distance = d;
                                    points_furthest_between = std::make_pair(m, n);
                                }
                            }
                        }
                    }
                    return;
                }
            }
            // If we reach here, no existing point was close enough; add as new point
            addPoint(point);
        }

        double geometricRatio()
        {
            if (furthest_distance == 0.0 || points.size() < 3)
            {
                return 0.0;
            }
            double ratio = 0.0;

            // Validate furthest-point indices; if invalid, recompute them.
            int idx1 = points_furthest_between.first;
            int idx2 = points_furthest_between.second;
            if (idx1 < 0 || idx2 < 0 || idx1 >= static_cast<int>(points.size()) || idx2 >= static_cast<int>(points.size()))
            {
                double best_d = 0.0;
                for (int i = 0; i < static_cast<int>(points.size()); ++i)
                {
                    for (int j = i + 1; j < static_cast<int>(points.size()); ++j)
                    {
                        double dx = points[i].getX() - points[j].getX();
                        double dy = points[i].getY() - points[j].getY();
                        double d = dx * dx + dy * dy;
                        if (d > best_d)
                        {
                            best_d = d;
                            idx1 = i;
                            idx2 = j;
                        }
                    }
                }
                furthest_distance = best_d;
                points_furthest_between = std::make_pair(idx1, idx2);
            }

            // Get unit vector along the line between the two furthest points
            double x1 = points[idx1].getX();
            double y1 = points[idx1].getY();
            double x2 = points[idx2].getX();
            double y2 = points[idx2].getY();
            double ux = x2 - x1;
            double uy = y2 - y1;
            double un = std::sqrt(ux * ux + uy * uy);
            if (un == 0.0)
            {
                return 0.0;
            }
            ux /= un;
            uy /= un;

            // Perpendicular unit vector
            double vx = -uy;
            double vy = ux;

            // Project points into this coordinate system (origin at centroid)
            double min_u = std::numeric_limits<double>::infinity();
            double max_u = -std::numeric_limits<double>::infinity();
            double min_v = std::numeric_limits<double>::infinity();
            double max_v = -std::numeric_limits<double>::infinity();

            for (const auto &p : points)
            {
                double dx = p.getX() - centroid_x;
                double dy = p.getY() - centroid_y;
                double pu = dx * ux + dy * uy;
                double pv = dx * vx + dy * vy;
                if (pu < min_u)
                    min_u = pu;
                if (pu > max_u)
                    max_u = pu;
                if (pv < min_v)
                    min_v = pv;
                if (pv > max_v)
                    max_v = pv;
            }

            double range_u = max_u - min_u;
            double range_v = max_v - min_v;
            ratio = range_v / range_u;
            return ratio;
        }
    };

} // namespace core

#endif // CLUSTER_HPP
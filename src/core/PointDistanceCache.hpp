#ifndef POINT_DISTANCE_CACHE_HPP
#define POINT_DISTANCE_CACHE_HPP

#include "DataPoint.h"
#include <map>
#include <utility>
#include "spdlog/spdlog.h"

namespace core
{
    class PointDistanceCache
    {
    public:
        static PointDistanceCache& getInstance()
        {
            static PointDistanceCache instance;
            return instance;
        }

        // Delete copy constructor and assignment operator
        PointDistanceCache(const PointDistanceCache&) = delete;
        PointDistanceCache& operator=(const PointDistanceCache&) = delete;

        double getDistance(const DataPoint &p1, const DataPoint &p2)
        {
            auto key = makeDistanceKey(p1.point_id, p2.point_id);
            auto it = distance_cache.find(key);
            if (it != distance_cache.end())
            {
                return it->second;
            }
            else
            {
                double dx = p1.getXUnsafe() - p2.getXUnsafe();
                double dy = p1.getYUnsafe() - p2.getYUnsafe();
                double dist = std::sqrt(dx * dx + dy * dy);
                distance_cache.emplace(key, dist);
                return dist;
            }
        }

        size_t size() const
        {
            return distance_cache.size();
        }

        void clear()
        {
            distance_cache.clear();
        }
    private:
        PointDistanceCache() = default;
        ~PointDistanceCache() = default;

        std::pair<int64_t, int64_t> makeDistanceKey(int64_t id1, int64_t id2) const
        {
            return (id1 < id2) ? std::make_pair(id1, id2) : std::make_pair(id2, id1);
        }
        std::map<std::pair<int64_t, int64_t>, double> distance_cache;
    };
} // namespace core

#endif // POINT_DISTANCE_CACHE_HPP
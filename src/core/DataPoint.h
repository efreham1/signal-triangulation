#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <cstdint>
#include <string>
#include <cmath>
#include <atomic>

#define EARTH_RADIUS_METERS 6362475.0 // Earth radius in meters in Uppsala

namespace core
{

    /**
     * @class DataPoint
     * @brief Represents a signal measurement data point.
     */

    /// @brief Calculate the Haversine distance between two geographical points.
    /// @note EARTH_RADIUS_METERS is adjusted for Uppsala region.
    /// @param lat1 Latitude of point 1 in degrees.
    /// @param lon1 Longitude of point 1 in degrees.
    /// @param lat2 Latitude of point 2 in degrees.
    /// @param lon2 Longitude of point 2 in degrees.
    /// @param radius Radius of the sphere (default is EARTH_RADIUS_METERS).
    /// @return Distance in meters.
    inline static double distanceBetween(double lat1, double lon1, double lat2, double lon2, const double radius = EARTH_RADIUS_METERS)
    {
        const double toRad = M_PI / 180.0;
        double dlat = (lat2 - lat1) * toRad;
        double dlon = (lon2 - lon1) * toRad;
        double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(lat1 * toRad) * std::cos(lat2 * toRad) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
        return radius * c;
    }

    class DataPoint
    {
    private:
        double latitude;   ///< Geographical latitude
        double longitude;  ///< Geographical longitude
        double x;          ///< X coordinate in euclidean space
        double y;          ///< Y coordinate in euclidean space
        bool x_computed;   ///< Flag: x coordinate computed
        bool y_computed;   ///< Flag: y coordinate computed
        bool lat_computed; ///< Flag: latitude value is valid/available
        bool lon_computed; ///< Flag: longitude value is valid/available
        std::atomic<int> static next_point_id; ///< Static atomic counter for unique point IDs
    public:
        double zero_latitude;  ///< Latitude that represents zero point in the euclidean space
        double zero_longitude; ///< Longitude that represents zero point in the euclidean space
        int rssi;              ///< Received Signal Strength Indicator
        int64_t timestamp_ms;  ///< Measurement timestamp in milliseconds
        std::string ssid;      ///< Optional SSID identifier for the measured network
        std::string dev_id;    ///< Optional device identifier
        int point_id;          ///< Unique point identifier

        // Default constructor
        DataPoint();

        // Parameterized constructors
        DataPoint(double lat, double lon, double zero_lat, double zero_lon, int signal_strength, int64_t time, const std::string &ssid_in = "", const std::string &dev_id = "");

        void setX(double x_val);
        void setY(double y_val);

        double getX() const;
        double getY() const;

        void setLatitude(double lat);
        void setLongitude(double lon);

        double getLatitude() const;
        double getLongitude() const;

        void computeCoordinates();

        bool validCoordinates() const;
    };

} // namespace core

#endif // DATAPOINT_H
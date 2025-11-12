#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <cstdint>
#include <string>

#define EARTH_RADIUS_METERS 6362475.0 // Earth radius in meters in Uppsala

namespace core {

/**
 * @class DataPoint
 * @brief Represents a signal measurement data point.
 */
class DataPoint {
public:
    double zero_latitude;     ///< Latitude that represents zero point in the euclidean space
    double zero_longitude;    ///< Longitude that represents zero point in the euclidean space
    int rssi;                 ///< Received Signal Strength Indicator
    int64_t timestamp_ms;     ///< Measurement timestamp in milliseconds
    std::string ssid;         ///< Optional SSID identifier for the measured network

    // Default constructor
    DataPoint();

    // Parameterized constructors
    DataPoint(double lat, double lon, double zero_lat, double zero_lon, int signal_strength, int64_t time, const std::string& ssid_in = "");

    void setX(double x_val);
    void setY(double y_val);

    double getX() const; 
    double getY() const;

    void setLatitude(double lat);
    void setLongitude(double lon);
    
    double getLatitude() const;
    double getLongitude() const;

    void computeCoordinates();

private:
    double latitude;            ///< Geographical latitude
    double longitude;           ///< Geographical longitude
    double x;                   ///< X coordinate in euclidean space
    double y;                   ///< Y coordinate in euclidean space
    bool x_computed;            ///< Flag: x coordinate computed
    bool y_computed;            ///< Flag: y coordinate computed
    bool lat_computed;          ///< Flag: latitude value is valid/available
    bool lon_computed;          ///< Flag: longitude value is valid/available
};

} // namespace core

#endif // DATAPOINT_H
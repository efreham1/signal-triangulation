#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <cstdint>
#include <string>

namespace core {

/**
 * @struct DataPoint
 * @brief Represents a signal measurement data point.
 */
struct DataPoint {
    double latitude;          ///< Geographical latitude
    double longitude;         ///< Geographical longitude
    int rssi;                 ///< Received Signal Strength Indicator
    int64_t timestamp_ms;     ///< Measurement timestamp in milliseconds
    std::string ssid;         ///< Optional SSID identifier for the measured network

    // Default constructor
    DataPoint()
        : latitude(0.0)
        , longitude(0.0)
        , rssi(0)
        , timestamp_ms(0)
        , ssid()
    {
    }

    // Parameterized constructor
    DataPoint(double lat, double lon, int signal_strength, int64_t time, const std::string& ssid_in = "")
        : latitude(lat)
        , longitude(lon)
        , rssi(signal_strength)
        , timestamp_ms(time)
        , ssid(ssid_in)
    {
    }
};

} // namespace core

#endif // DATAPOINT_H
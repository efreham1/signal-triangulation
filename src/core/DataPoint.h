#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <cstdint>

namespace core {

/**
 * @struct DataPoint
 * @brief Represents a signal measurement data point.
 */
struct DataPoint {
    double latitude;          ///< Geographical latitude
    double longitude;         ///< Geographical longitude
    int rssi;                ///< Received Signal Strength Indicator
    int64_t timestamp_ms;    ///< Measurement timestamp in milliseconds

    // Default constructor
    DataPoint() : latitude(0.0), longitude(0.0), rssi(0), timestamp_ms(0) {}

    // Parameterized constructor
    DataPoint(double lat, double lon, int signal_strength, int64_t time)
        : latitude(lat)
        , longitude(lon)
        , rssi(signal_strength)
        , timestamp_ms(time) {}
};

} // namespace core

#endif // DATAPOINT_H
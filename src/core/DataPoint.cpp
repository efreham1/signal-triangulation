#include "DataPoint.h"
#include <stdexcept>
#include <cmath>
#include <iostream>
namespace core {
    DataPoint::DataPoint()
        : zero_latitude(0.0)
        , zero_longitude(0.0)
        , rssi(0)
        , timestamp_ms(0)
        , ssid()
        , latitude(0.0)
        , longitude(0.0)
        , x(0.0)
        , y(0.0)
        , x_computed(false)
        , y_computed(false)
        , lat_computed(false)
        , lon_computed(false)
    {
    }

    DataPoint::DataPoint(double lat, double lon, double zero_lat, double zero_lon, int signal_strength, int64_t time, const std::string& ssid_in)
        : zero_latitude(zero_lat)
        , zero_longitude(zero_lon)
        , rssi(signal_strength)
        , timestamp_ms(time)
        , ssid(ssid_in)
        , latitude(lat)
        , longitude(lon)
        , x(0.0)
        , y(0.0)
        , x_computed(false)
        , y_computed(false)
        , lat_computed(true)
        , lon_computed(true)
    {
        // compute x/y from provided latitude/longitude
        computeCoordinates();
    }

    void DataPoint::setX(double x_val)
    {
        x = x_val;
        x_computed = true;
        // setting x invalidates computed lat/lon
        lat_computed = false;
        lon_computed = false;
    }

    void DataPoint::setY(double y_val)
    {
        y = y_val;
        y_computed = true;
        // setting y invalidates computed lat/lon
        lat_computed = false;
        lon_computed = false;
    }

    double DataPoint::getX() const
    {
        if (!x_computed) {
            throw std::runtime_error("DataPoint: x coordinate not computed");
        }
        return x;
    }

    double DataPoint::getY() const
    {
        if (!y_computed) {
            throw std::runtime_error("DataPoint: y coordinate not computed");
        }
        return y;
    }

    void DataPoint::computeCoordinates()
    {
        double lat_rad;
        double lon_rad;
        if (lat_computed && lon_computed && !x_computed && !y_computed)
        {
            lat_rad = (latitude - zero_latitude) * (M_PI / 180.0);
            lon_rad = (longitude - zero_longitude) * (M_PI / 180.0);

            x = EARTH_RADIUS_METERS * lon_rad * cos(zero_latitude * (M_PI / 180.0));
            y = EARTH_RADIUS_METERS * lat_rad;
            x_computed = true;
            y_computed = true;
        } else if (!lat_computed && !lon_computed && x_computed && y_computed)
        {
            lat_rad = y / EARTH_RADIUS_METERS;
            lon_rad = x / (EARTH_RADIUS_METERS * cos(zero_latitude * (M_PI / 180.0)));

            latitude = zero_latitude + (lat_rad * 180.0 / M_PI);
            longitude = zero_longitude + (lon_rad * 180.0 / M_PI);

            std::cout << "Computed latitude: " << latitude << ", longitude: " << longitude << std::endl;
            std::cout << "Radians: lat_rad=" << lat_rad << ", lon_rad=" << lon_rad << std::endl;

            lat_computed = true;
            lon_computed = true;
        } else if (lat_computed && lon_computed && x_computed && y_computed)
        {           
            // all values are already computed; nothing to do
            // skip
        }
        
        else
        {
            throw std::runtime_error("DataPoint: insufficient data to compute coordinates");
        }
    }

    void DataPoint::setLatitude(double lat)
    {
        latitude = lat;
        lat_computed = true;
        // setting latitude invalidates x/y
        x_computed = false;
        y_computed = false;
    }

    void DataPoint::setLongitude(double lon)
    {
        longitude = lon;
        lon_computed = true;
        // setting longitude invalidates x/y
        x_computed = false;
        y_computed = false;
    }

    double DataPoint::getLatitude() const
    {
        if (!lat_computed) {
            throw std::runtime_error("DataPoint: latitude not computed");
        }
        return latitude;
    }

    double DataPoint::getLongitude() const
    {
        if (!lon_computed) {
            throw std::runtime_error("DataPoint: longitude not computed");
        }
        return longitude;
    }

} // namespace core
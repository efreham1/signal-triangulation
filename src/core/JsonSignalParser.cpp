#include "JsonSignalParser.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace core
{
    std::map<std::string, std::vector<core::DataPoint>> JsonSignalParser::parseFileToVector(const std::string &path, double &zero_latitude, double &zero_longitude)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open JSON file: " + path);
        }

        nlohmann::json j;
        file >> j;

        if (!j.contains("measurements") || !j["measurements"].is_array())
        {
            throw std::runtime_error("JSON does not contain a measurements array");
        }

        const auto &arr = j["measurements"];

        if (arr.size() == 0)
        {
            throw std::runtime_error("JSON measurements array is empty");
        }
        // Set zero latitude and longitude to the first entry's values
        zero_latitude = arr[0].value("latitude", 0.0);
        zero_longitude = arr[0].value("longitude", 0.0);

        std::map<std::string, std::vector<core::DataPoint>> result;

        for (const auto &item : arr)
        {
            double lat = item.value("latitude", 0.0);
            double lon = item.value("longitude", 0.0);
            int rssi = item.value("rssi", 0);
            int64_t timestamp = item.value("timestamp", int64_t{0});
            std::string ssid_in = item.value("ssid", "");
            std::string dev_id = item.value("deviceID", "");

            DataPoint dp(lat, lon, zero_latitude, zero_longitude, rssi, timestamp, ssid_in, dev_id);

            result[dev_id].push_back(dp);
        }

        return result;
    }

    std::pair<double, double> JsonSignalParser::parseFileToSourcePos(const std::string &path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open JSON file: " + path);
        }
        nlohmann::json j;
        file >> j;
        if (!j.contains("source_pos") || !j["source_pos"].is_object())
        {
            throw std::runtime_error("JSON does not contain a source_pos object");
        }
        const auto &src = j["source_pos"];
        if (!src.contains("x") || !src.contains("y"))
        {
            throw std::runtime_error("source_pos object does not contain x and y fields");
        }
        double lat = src["x"].get<double>();
        double lon = src["y"].get<double>();
        if (!j.contains("measurements") || !j["measurements"].is_array())
        {
            throw std::runtime_error("JSON does not contain a measurements array");
        }

        const auto &arr = j["measurements"];

        if (arr.size() == 0)
        {
            throw std::runtime_error("JSON measurements array is empty");
        }
        // Set zero latitude and longitude to the first entry's values
        double zero_latitude = arr[0].value("latitude", 0.0);
        double zero_longitude = arr[0].value("longitude", 0.0);
        // Convert source_pos from lat/lon to x/y using DataPoint projection
        DataPoint src_point(lat, lon, zero_latitude, zero_longitude, 0, 0);

        src_point.computeCoordinates();

        return std::make_pair(src_point.getX(), src_point.getY());
    }

} // namespace core

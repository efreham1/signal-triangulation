#include "JsonSignalParser.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace core {
    std::vector<DataPoint> JsonSignalParser::parseFileToVector(const std::string &path)
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

        // Set zero latitude and longitude to the first entry's values
        double zero_latitude = arr[0].value("latitude", 0.0);
        double zero_longitude = arr[0].value("longitude", 0.0);

        std::vector<DataPoint> result;
        result.reserve(arr.size());

        for (const auto &item : arr)
        {
            double lat = item.value("latitude", 0.0);
            double lon = item.value("longitude", 0.0);
            int rssi = item.value("rssi", 0);
            int64_t timestamp = item.value("timestamp", int64_t{0});
            std::string ssid_in = item.value("ssid", "");
            std::string dev_id = item.value("deviceID", "");

            DataPoint dp(lat, lon, zero_latitude, zero_longitude, rssi, timestamp, ssid_in, dev_id);

            result.push_back(dp);
        }

        return result;
    }

} // namespace core

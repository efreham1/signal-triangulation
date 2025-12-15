#include "AlgorithmRunner.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include "../core/ClusteredTriangulationAlgorithm2.h"
#include "../core/JsonSignalParser.h"
#include <memory>
#include <map>

namespace rest
{
    static nlohmann::json mergeJsonInputs(const std::vector<std::string> &json_inputs)
    {
        if (json_inputs.empty())
            throw std::runtime_error("No JSON inputs provided");

        // Parse the first JSON as the base
        nlohmann::json merged = nlohmann::json::parse(json_inputs[0]);

        // Ensure "measurements" exists and is an array
        if (!merged.contains("measurements") || !merged["measurements"].is_array())
            throw std::runtime_error("First JSON does not contain a 'measurements' array");

        // Append measurements from the rest
        for (size_t i = 1; i < json_inputs.size(); ++i)
        {
            nlohmann::json j = nlohmann::json::parse(json_inputs[i]);
            if (j.contains("measurements") && j["measurements"].is_array())
            {
                for (const auto &m : j["measurements"])
                    merged["measurements"].push_back(m);
            }
        }

        return merged;
    }

    std::string AlgorithmRunner::runFromJsons(const std::vector<std::string> &json_inputs)
    {
        std::cout << "Running algorithm on " << json_inputs.size() << " JSON inputs." << std::endl;

        if (json_inputs.empty())
            return R"({"error":"No input JSONs provided."})";

        // 1. Merge JSONs
        nlohmann::json merged_json;
        try
        {
            merged_json = mergeJsonInputs(json_inputs);
        }
        catch (const std::exception &ex)
        {
            return std::string(R"({"error":"Failed to merge JSONs: )") + ex.what() + "\"}";
        }

        // 2. Parse merged JSON to data points
        std::map<std::string, std::vector<core::DataPoint>> points;
        double zero_latitude = 0.0, zero_longitude = 0.0;
        try
        {
            points = core::JsonSignalParser::parseJsonToVector(merged_json, zero_latitude, zero_longitude);
        }
        catch (const std::exception &ex)
        {
            return std::string(R"({"error":"Failed to parse merged JSON: )") + ex.what() + "\"}";
        }

        // 3. Prepare algorithm (CTA2, default args)
        auto algorithm = std::make_unique<core::ClusteredTriangulationAlgorithm2>();
        for (auto &dev : points)
            for (auto &dp : dev.second)
                dp.computeCoordinates();
        algorithm->addDataPointMap(points, zero_latitude, zero_longitude);

        // 4. Run calculation
        double lat = 0.0, lon = 0.0;
        try
        {
            // TODO: Magic numbers
            algorithm->calculatePosition(lat, lon, 0.1, 60); // Default precision and timeout
        }
        catch (const std::exception &ex)
        {
            return std::string(R"({"error":"Calculation error: )") + ex.what() + "\"}";
        }

        // 5. Return result as JSON
        nlohmann::json result;
        result["latitude"] = lat;
        result["longitude"] = lon;

        // Print result
        std::cout << "Calculated Position: Latitude = "
                  << std::setprecision(10) << lat
                  << ", Longitude = " << std::setprecision(10) << lon
                  << std::endl;

        return result.dump();
    }

} // namespace rest
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <regex>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include "../src/core/DataPoint.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

#ifndef APP_BIN_PATH
#error "APP_BIN_PATH not defined"
#endif

static std::string runAppAndCapture(const std::string &exe, const std::string &arg1, const std::string &arg2)
{
    std::string cmd = std::string("\"") + exe + "\" " + arg1 + " " + arg2;
    std::string output;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return {};
    char buffer[4096]; // Can be adjusted if needed
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output += buffer;
    pclose(pipe);
    return output;
}

TEST(Triangulation, DistanceCheckForFile)
{
    const char *envFile = std::getenv("JSON_FILE");
    if (!envFile)
    {
        GTEST_SKIP() << "JSON_FILE env var not set (this test is meant to be run via CTest add_test entries).";
    }
    const std::string filePath = envFile;

    ASSERT_TRUE(fs::exists(APP_BIN_PATH)) << "Application binary not found at: " << APP_BIN_PATH;
    ASSERT_TRUE(fs::is_regular_file(filePath)) << "JSON file not found: " << filePath;

    // Read source_pos from JSON
    std::ifstream ifs(filePath);
    ASSERT_TRUE(ifs) << "Failed to open JSON: " << filePath;
    json j;
    ASSERT_NO_THROW(ifs >> j) << "JSON parse error for " << filePath;
    ASSERT_TRUE(j.contains("source_pos") && j["source_pos"].contains("x") && j["source_pos"].contains("y"))
        << "JSON missing source_pos.x/y in " << filePath;

    double srcLat = j["source_pos"]["x"].get<double>();
    double srcLon = j["source_pos"]["y"].get<double>();

    // Run app and capture stdout (expects the line: "New position calculated: Lat=..., Lon=...")
    std::string out = runAppAndCapture(APP_BIN_PATH, std::string("--signals-file2"), std::string("\"" + filePath + "\"");
    std::regex re(R"(Calculated Position: Latitude\s*=\s*([0-9\.\-eE]+)\s*,\s*Longitude\s*=\s*([0-9\.\-eE]+))");
    std::smatch m;
    ASSERT_TRUE(std::regex_search(out, m, re) && m.size() >= 3)
        << "Could not parse app output for " << filePath << "\nOutput:\n"
        << out;

    double lat = std::stod(m[1].str());
    double lon = std::stod(m[2].str());
    double dist = core::distanceBetween(srcLat, srcLon, lat, lon);

    const double MAX_ALLOWED_METERS = 3.0;
    EXPECT_LT(dist, MAX_ALLOWED_METERS) << "Distance: " << dist << " m";
}